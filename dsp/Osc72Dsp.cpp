// Osc72Dsp.cpp — see Osc72Dsp.h for the verified/reconstruction map.
#include "Osc72Dsp.h"

namespace osc72 {

void StereoLimiter::prepare(double sampleRate) {
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    xfmrInL_.prepare(fs_);
    xfmrInR_.prepare(fs_);
    reset();
}

void StereoLimiter::reset() {
    knobS_ = knobTarget_;
    posEnvL_ = negEnvL_ = posEnvR_ = negEnvR_ = 0.0f;
    grDb_ = 0.0f;
    makeupDb_ = 0.0f;
    xfmrInL_.reset(); xfmrInR_.reset();
    tdFast_ = tdSlow_ = 0.0f;
    tdGain_ = 1.0f;
}

// One detector pole: fast attack, slower hang. The target is the
// current half-wave excursion (positive part or negative part), so
// positive and negative peaks are controlled separately — the
// documented reason the design holds together on asymmetric material
// and preserves the stereo centre under heavy reduction.
float StereoLimiter::envStep(float env, float target) const {
    const float c = (target > env) ? tauCoef(kDetectAttackTauSec, fs_)
                                   : tauCoef(kDetectReleaseTauSec, fs_);
    env += (target - env) * c;
    return env < 1.0e-15f ? 0.0f : env; // denormal flush
}

// Transient-designer envelope pole (v9): the fast envelope chases
// the signal (sub-ms attack, 25 ms release); the slow one remembers
// (25 ms attack, 350 ms release). Same flush discipline as envStep.
float StereoLimiter::tdEnvStep(float env, float target, bool fast) const {
    const float atk = fast ? kTdFastAttackTauSec : kTdSlowAttackTauSec;
    const float rel = fast ? kTdFastReleaseTauSec : kTdSlowReleaseTauSec;
    const float c = (target > env) ? tauCoef(atk, fs_) : tauCoef(rel, fs_);
    env += (target - env) * c;
    return env < 1.0e-15f ? 0.0f : env; // denormal flush
}

void StereoLimiter::process(float inL, float inR, float& outL, float& outR) {
    // Smooth the knob itself (~5 ms) so rides never click.
    knobS_ += (knobTarget_ - knobS_) * tauCoef(0.005f, fs_);
    if (std::fabs(knobTarget_ - knobS_) < 1.0e-5f) knobS_ = knobTarget_;
    const float k = knobS_;

    // ── Wet branch, stage 1: Lundahl input transformer (v9) ───────
    // Per channel, first thing in the wet path. Depth ramps in over
    // the first eighth of the knob so knob 0 stays bit-transparent.
    const float xDepth = xfmrDepth();
    const float xinL = xfmrInL_.process(inL, xDepth);
    const float xinR = xfmrInR_.process(inR, xDepth);

    // ── Wet branch, stage 2: transient designer (v9) ──────────────
    // Linked envelopes on the transformed signal; gain follows the
    // fast/slow ratio DOWN only (sustain cut, no attack boost), the
    // gain itself smoothed so the cut can never click. Depth is
    // ganged to the knob: at knob 0 the gain is pinned at exactly 1.
    {
        const float aL = xinL < 0.0f ? -xinL : xinL;
        const float aR = xinR < 0.0f ? -xinR : xinR;
        const float sig = aL > aR ? aL : aR;
        tdFast_ = tdEnvStep(tdFast_, sig, true);
        tdSlow_ = tdEnvStep(tdSlow_, sig, false);
        const float depth = tdDepthForKnob(k);
        float gTarget = 1.0f;
        if (depth > 0.0f && tdSlow_ > 1.0e-9f) {
            float ratio = tdFast_ / tdSlow_;
            if (ratio > 1.0f) ratio = 1.0f;
            gTarget = std::pow(ratio, depth * kTdMaxExponent);
            if (gTarget < kTdFloorGain) gTarget = kTdFloorGain;
        }
        const float gcTd = (gTarget < tdGain_) ? tauCoef(kTdGainAttackTauSec, fs_)
                                               : tauCoef(kTdGainReleaseTauSec, fs_);
        tdGain_ += (gTarget - tdGain_) * gcTd;
    }
    const float wetInL = xinL * tdGain_;
    const float wetInR = xinR * tdGain_;

    // ── Feed-forward detector ──────────────────────────────────────
    // The control signal is derived from the wet-path INPUT (post
    // transformer, post TD), before the gain cell (documented
    // feed-forward topology) — not from the output.
    posEnvL_ = envStep(posEnvL_, wetInL > 0.0f ? wetInL : 0.0f);
    negEnvL_ = envStep(negEnvL_, wetInL < 0.0f ? -wetInL : 0.0f);
    posEnvR_ = envStep(posEnvR_, wetInR > 0.0f ? wetInR : 0.0f);
    negEnvR_ = envStep(negEnvR_, wetInR < 0.0f ? -wetInR : 0.0f);

    // Linked stereo: ONE level for both channels (max of all four
    // half-wave envelopes) -> one gain -> centre image cannot wander.
    float level = posEnvL_;
    if (negEnvL_ > level) level = negEnvL_;
    if (posEnvR_ > level) level = posEnvR_;
    if (negEnvR_ > level) level = negEnvR_;

    // ── Dynamic-ratio gain computer (no ratio control, by design) ─
    // v2 ballistics: the smoothing pole IS the compressor's attack
    // and release (slow-to-medium attack — transients punch before
    // the clamp; super-quick release — the gain rides back between
    // hits and pumps). Past centre both quicken and the GR ceiling
    // rises (the Level-Loc lean).
    const float overDb = linearToDb(level) - thresholdDbForKnob(k);
    const float grTarget = dynamicGainReductionDb(overDb, grCapForKnob(k));

    const float gc = (grTarget > grDb_) ? tauCoef(attackTauForKnob(k), fs_)
                                        : tauCoef(releaseTauForKnob(k), fs_);
    grDb_ += (grTarget - grDb_) * gc;
    if (grDb_ < 1.0e-9f) grDb_ = 0.0f;

    // ── Overshoot guard (v2) ─────────────────────────────────────
    // The slow attack's price: for the first ~10-20 ms of a loud
    // hit the programme GR has not yet arrived, and the raw wet
    // peak would sail past full scale — a flat-top disaster the
    // v1 build's 0.2 ms attack never allowed. The dangerous case
    // is a hit after a gap: the fast release has let the GR fall
    // to ~0 while the slow makeup (300 ms relax) is still tens of
    // dB up, so the hit enters at a large NET positive gain
    // (measured without the guard: peak 1.58 on a pre-charged
    // unit). The detector is feed-forward, so the would-be peak
    // is known exactly — including the makeup term: require
    // grDb >= makeupDb + 20*log10(peak x drive) so the net
    // saturator input can never exceed the clean ceiling (1.0).
    // The shortfall is pulled in fast (0.3 ms pole). The guard is
    // silent in steady state (settled GR exceeds the requirement
    // by tens of dB) and releases through the normal release pole
    // above, so it cannot chatter on its own. (makeupDb_ here is
    // last sample's value — it updates below; 1-sample lag is
    // immaterial at a 300 ms time constant.)
    // The peak is read PRE-TD (the transformer outputs xinL/xinR),
    // not from wetIn: after a gap the TD's fast envelope needs
    // ~2 ms to climb back, and in that window wetIn is ducked by
    // ~20 dB while the net gain is at its highest — a wetIn-based
    // guard wakes up late and the ducked residue, amplified by
    // the full net gain, still overshoots (measured: 1.58). The
    // TD only ever cuts (gain <= 1), so the pre-TD peak is the
    // conservative, correct basis: it is the level the gain cell
    // will see the moment the TD lets go.
    const float satD = satDriveForKnob(k);
    {
        const float pkL = xinL < 0.0f ? -xinL : xinL;
        const float pkR = xinR < 0.0f ? -xinR : xinR;
        const float pk = (pkL > pkR ? pkL : pkR) * satD;
        if (pk > 0.05f) {
            const float guardDb = makeupDb_ + linearToDb(pk);
            if (guardDb > grDb_)
                grDb_ += (guardDb - grDb_) * tauCoef(0.0003f, fs_);
        }
    }

    // Makeup follows the SLOW average of the gain reduction, so the
    // crushed path stays hot and dense without chasing peaks (which
    // would undo the peak control). 0.8 factor, capped (cap rises
    // slightly in the top half of the knob with the deeper squash).
    // The makeup ENGAGES with a 120 ms rise and relaxes with the
    // documented 300 ms tau: settled density is unchanged, but a
    // burst after silence blooms back within ~100 ms instead of
    // staying ducked while a 300 ms average fills.
    const float mkCap = makeupCapForKnob(k);
    const float mkTarget = grDb_ * 0.8f > mkCap ? mkCap : grDb_ * 0.8f;
    const float mkTau = (mkTarget > makeupDb_) ? 0.120f : kMakeupTauSec;
    makeupDb_ += (mkTarget - makeupDb_) * tauCoef(mkTau, fs_);

    const float gainLin = dbToLinear(-grDb_ + makeupDb_);

    // ── Parallel blend — the knob IS the mix (v9 law) ──────────────
    // Dry path passes untouched. Wet path = transformed, TD'd,
    // limited, gently saturated (Class-A-style density; driven a
    // touch harder in the top half — the Comptortion grit). At knob
    // 0 the wet weight is exactly 0, the TD gain is exactly 1 and
    // the transformer depth is exactly 0: bit-transparent.
    const float w = wetWeightForKnob(k);
    const float d = 1.0f - w;
    // (satD computed earlier, at the overshoot guard.)

    const float wetL = classASaturate(wetInL * gainLin * satD);
    const float wetR = classASaturate(wetInR * gainLin * satD);
    outL = inL * d + wetL * w;
    outR = inR * d + wetR * w;
}

// ─────────────────────────────────────────────────────────────────────
// WarbleChain implementation — see Osc72Dsp.h for the design notes
// and ../research-lofi-loop-junky.md for the source map.
// ─────────────────────────────────────────────────────────────────────

void Biquad::setLowpass(double fs, float freq, float q) {
    const double w0 = 2.0 * 3.14159265358979323846 * (double)freq / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * (double)q);
    const double a0 = 1.0 + alpha;
    b0 = (float)((1.0 - cw) * 0.5 / a0);
    b1 = (float)((1.0 - cw) / a0);
    b2 = b0;
    a1 = (float)(-2.0 * cw / a0);
    a2 = (float)((1.0 - alpha) / a0);
}

void Biquad::setHighpass(double fs, float freq, float q) {
    const double w0 = 2.0 * 3.14159265358979323846 * (double)freq / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * (double)q);
    const double a0 = 1.0 + alpha;
    b0 = (float)((1.0 + cw) * 0.5 / a0);
    b1 = (float)(-(1.0 + cw) / a0);
    b2 = b0;
    a1 = (float)(-2.0 * cw / a0);
    a2 = (float)((1.0 - alpha) / a0);
}

void WarbleChain::prepare(double sampleRate) {
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    // Ring sized for 70 ms at this rate (max delay is 24 ms), pow2.
    int need = (int)std::ceil(fs_ * 0.070);
    int sz = 1;
    while (sz < need) sz <<= 1;
    ringSize_ = sz;
    ringMask_ = sz - 1;
    ringS_.assign((size_t)sz, 0.0f);
    latency_ = (int)std::lround((double)kWarbleBaseDelayMs * 0.001 * fs_);

    const float q = 0.7071f; // two of these in series = 4th-order rolloff
    lpInS_.setLowpass(fs_, kWarbleLowpassHz, q);
    lpOutS_.setLowpass(fs_, kWarbleLowpassHz, q);
    hpOutS_.setHighpass(fs_, kWarbleHighpassHz, q);
    reset();
}

void WarbleChain::reset() {
    target_ = false;
    state_ = State::off;
    fadeGain_ = 0.0f;
    resetChainState();
}

void WarbleChain::resetChainState() {
    std::fill(ringS_.begin(), ringS_.end(), 0.0f);
    writePos_ = 0;
    lpInS_.reset();
    lpOutS_.reset();
    hpOutS_.reset();
    wowPhase_ = vibPhase_ = fltPhase_ = 0.0f;
    drift_ = driftTarget_ = 0.0f;
    driftHold_ = 0;
    rngState_ = 0x2545f491u;
    lastDelayMs_ = kWarbleBaseDelayMs;
}

void WarbleChain::setEnabled(bool on) {
    if (on == target_) return;
    target_ = on;
    if (on) {
        if (state_ == State::off) {
            // Fresh engage: clear the line so nothing stale can
            // burst out; the 30 ms crossfade covers the fill time.
            resetChainState();
            fadeGain_ = 0.0f;
        }
        state_ = State::fadingIn;
    } else {
        if (state_ == State::on || state_ == State::fadingIn)
            state_ = State::fadingOut;
    }
}

// Layered modulation — wow (slow sine), the main vibrato (triangle,
// the waveform Z.Vex's realtime emulation names as traditional),
// flutter (fast, tiny), and a smoothed random walk for the RC-clock
// irregularity. Returns the instantaneous delay in ms, hard-clamped
// to the safe band so the read position can never cross the write
// position or go negative.
float WarbleChain::modulationDelayMs() {
    const float inv = 1.0f / (float)fs_;
    wowPhase_ += kWarbleWowHz * inv;
    if (wowPhase_ >= 1.0f) wowPhase_ -= 1.0f;
    vibPhase_ += kWarbleVibHz * inv;
    if (vibPhase_ >= 1.0f) vibPhase_ -= 1.0f;
    fltPhase_ += kWarbleFlutterHz * inv;
    if (fltPhase_ >= 1.0f) fltPhase_ -= 1.0f;

    if (--driftHold_ <= 0) {
        driftHold_ = (int)(0.37 * fs_);
        rngState_ = rngState_ * 1664525u + 1013904223u;
        driftTarget_ = ((float)((rngState_ >> 9) & 0x7fffffu) / 4194303.5f) - 1.0f;
    }
    drift_ += (driftTarget_ - drift_) * tauCoef(0.35f, fs_);

    const float wow = std::sin(wowPhase_ * 6.283185307179586f);
    const float tri = 2.0f * std::fabs(2.0f * (vibPhase_ - std::floor(vibPhase_ + 0.5f))) - 1.0f;
    const float flt = std::sin(fltPhase_ * 6.283185307179586f);

    float d = kWarbleBaseDelayMs + kWarbleWowMs * wow
            + kWarbleVibMs * tri + kWarbleFlutterMs * flt
            + kWarbleDriftMs * drift_;
    if (d < kWarbleMinDelayMs) d = kWarbleMinDelayMs;
    if (d > kWarbleMaxDelayMs) d = kWarbleMaxDelayMs;
    lastDelayMs_ = d;
    return d;
}

float WarbleChain::readDelay(const float* ring, float delaySamples) const {
    float pos = (float)writePos_ - delaySamples;
    while (pos < 0.0f) pos += (float)ringSize_;
    const int i0 = (int)pos;
    const float frac = pos - (float)i0;
    const float a = ring[i0 & ringMask_];
    const float b = ring[(i0 + 1) & ringMask_];
    return a + (b - a) * frac;
}

void WarbleChain::process(float inL, float inR, float& outL, float& outR) {
    if (state_ == State::off) {
        // Fully out of the path: bit-exact copy, no other work.
        outL = inL;
        outR = inR;
        return;
    }

    // ── Mid/side (v10): centre dry, side takes the transport ──────
    // The (L+R)/2, (L−R)/2 encode is exactly invertible, so nothing
    // is lost by working here. The mid is NEVER written into the
    // delay line and never filtered: the wet signal is the untouched
    // mid plus/minus the warbled side. For a mono input the side is
    // exactly 0, the delay line and filters process exact zeros, and
    // the wet signal equals the input sample for sample.
    const float mid  = (inL + inR) * 0.5f;
    const float side = (inL - inR) * 0.5f;

    const float delayMs = modulationDelayMs();
    float delaySamples = delayMs * 0.001f * (float)fs_;
    {
        const float lo = 2.0f, hi = (float)ringSize_ * 0.5f;
        if (delaySamples < lo) delaySamples = lo;
        if (delaySamples > hi) delaySamples = hi;
    }

    ringS_[(size_t)writePos_] = lpInS_.process(side);
    const float wetSide = hpOutS_.process(
        lpOutS_.process(readDelay(ringS_.data(), delaySamples)));
    writePos_ = (writePos_ + 1) & ringMask_;

    const float wetL = mid + wetSide;
    const float wetR = mid - wetSide;

    // Crossfade bookkeeping (30 ms linear ramp).
    const float step = 1.0f / (kWarbleFadeSec * (float)fs_);
    if (state_ == State::fadingIn) {
        fadeGain_ += step;
        if (fadeGain_ >= 1.0f) { fadeGain_ = 1.0f; state_ = State::on; }
    } else if (state_ == State::fadingOut) {
        fadeGain_ -= step;
        if (fadeGain_ <= 0.0f) { fadeGain_ = 0.0f; state_ = State::off; }
    }
    // Subtractive blend form: out = in + g·(wet − in) is exact
    // whenever wet == in (mono input, centre content), at ANY fade
    // position — the dry centre cannot be perturbed by the fade.
    const float g = fadeGain_;
    outL = inL + (wetL - inL) * g;
    outR = inR + (wetR - inR) * g;
}

// ─────────────────────────────────────────────────────────────────────
// Chain implementation — see Osc72Dsp.h for the guarantee list.
// ─────────────────────────────────────────────────────────────────────

void Chain::prepare(double sampleRate) {
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    limiter_.prepare(fs_);
    warble_.prepare(fs_);
    xfmrOutL_.prepare(fs_);
    xfmrOutR_.prepare(fs_);
    reset();
}

void Chain::reset() {
    limiter_.reset();
    warble_.reset();
    xfmrOutL_.reset();
    xfmrOutR_.reset();
    limitTarget_ = true;
    limitFade_ = 1.0f;
}

void Chain::setLimitIn(bool in) {
    limitTarget_ = in;
}

void Chain::process(float inL, float inR, float& outL, float& outR) {
    // The limiter always runs on the input: its detectors, makeup
    // and knob smoothing stay live so re-engaging is seamless.
    float limL, limR;
    limiter_.process(inL, inR, limL, limR);

    // Advance the limit crossfade (30 ms linear ramp, the same law
    // as the mod toggle's fade).
    const float step = 1.0f / (kLimitFadeSec * (float)fs_);
    if (limitTarget_ && limitFade_ < 1.0f)
        limitFade_ = std::min(1.0f, limitFade_ + step);
    else if (!limitTarget_ && limitFade_ > 0.0f)
        limitFade_ = std::max(0.0f, limitFade_ - step);

    float midL, midR;
    if (limitFade_ <= 0.0f) {
        midL = inL;  midR = inR;             // limiter fully out: exact dry
    } else if (limitFade_ >= 1.0f) {
        midL = limL; midR = limR;            // limiter fully in: exact path
    } else {
        const float d = 1.0f - limitFade_;
        midL = inL * d + limL * limitFade_;
        midR = inR * d + limR * limitFade_;
    }
    // Post-limiter insert (bit-exact copy when mod is out).
    float postL, postR;
    warble_.process(midL, midR, postL, postR);

    // ── Lundahl output transformer (v9): last in the engaged path ─
    // Engaged when either section is in. Skipped outright when both
    // are out (limitFade_ == 0 and the warble chain fully off), so
    // the both-out path stays bit-exact. Depth: full when mod is
    // in; otherwise the limiter's colour depth — which is exactly
    // 0 at knob 0, keeping knob-0 transparency end to end (the
    // stage also returns its input bit-exactly at depth 0).
    const bool modActive = warble_.active();
    if (limitFade_ > 0.0f || modActive) {
        const float depth = modActive ? 1.0f : limiter_.xfmrDepth();
        outL = xfmrOutL_.process(postL, depth);
        outR = xfmrOutR_.process(postR, depth);
    } else {
        outL = postL;
        outR = postR;
    }
}

} // namespace osc72
