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
    detLpL_ = detLpR_ = 0.0f;
    detPresence_ = 0.0f;
    detQuietRun_ = 0;
    pkEnv_ = 0.0f;
    grDb_ = 0.0f;
    makeupDb_ = 0.0f;
    makeupAvgDb_ = 0.0f;
    stressDb_ = 0.0f;
    xfmrInL_.reset(); xfmrInR_.reset();
    tdFast_ = tdSlow_ = 0.0f;
    tdGain_ = 1.0f;
}

// One detector pole: fast attack, slower hang (the hang tau is
// supplied per sample — v3 makes it LF-adaptive, see process()).
// The target is the current half-wave excursion (positive part or
// negative part), so positive and negative peaks are controlled
// separately — the documented reason the design holds together on
// asymmetric material and preserves the stereo centre under heavy
// reduction.
float StereoLimiter::envStep(float env, float target, float relTauSec) const {
    const float c = (target > env) ? tauCoef(kDetectAttackTauSec, fs_)
                                   : tauCoef(relTauSec, fs_);
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
    // v3: the detector feed is high-passed (~75 Hz one-pole,
    // detector only) so sustained bass cannot park the gain
    // computer — see the constants block in the header.
    const float hpC = tauCoef(kDetectHpTauSec, fs_);
    detLpL_ += (wetInL - detLpL_) * hpC;
    detLpR_ += (wetInR - detLpR_) * hpC;
    const float detL = wetInL - detLpL_;
    const float detR = wetInR - detLpR_;
    // Presence-keyed hang (v3): the hang stretches to ~12 ms
    // while programme is present at the detector (within ~14 dB
    // of the running envelope — sustained tones and hits alike,
    // so the envelope cannot collapse inside an LF cycle), and
    // collapses to the 3 ms base the moment programme stops
    // (presence decays with a 15 ms tau), so the release the
    // step test — and a listener between hits — hears stays at
    // full speed. (An LF-content-keyed version was tried first:
    // its share estimate saturated during decays and slowed the
    // step release to 63 ms. Presence is the honest
    // discriminator: it asks "is anything still playing?", not
    // "what is playing?".)
    float pkNow = detL < 0.0f ? -detL : detL;
    { const float aR = detR < 0.0f ? -detR : detR; if (aR > pkNow) pkNow = aR; }
    {
        float envPk = posEnvL_;
        if (negEnvL_ > envPk) envPk = negEnvL_;
        if (posEnvR_ > envPk) envPk = posEnvR_;
        if (negEnvR_ > envPk) envPk = negEnvR_;
        if (pkNow > 0.2f * envPk) { detPresence_ = 1.0f; detQuietRun_ = 0; }
        else {
            detPresence_ += (0.0f - detPresence_) * tauCoef(0.006f, fs_);
            // True-silence cut: programme never sits below 1% of
            // its own envelope for 2 ms straight (even a 60 Hz
            // tone crosses that band for only ~0.1 ms at a time),
            // but a stopped programme does — drop presence at
            // once so the release bleed runs at full speed.
            if (pkNow < 0.01f * envPk) {
                if (++detQuietRun_ > 96) detPresence_ = 0.0f;
            } else detQuietRun_ = 0;
        }
    }
    const float hangTau = kDetectReleaseTauSec * (1.0f + 3.0f * detPresence_);
    posEnvL_ = envStep(posEnvL_, detL > 0.0f ? detL : 0.0f, hangTau);
    negEnvL_ = envStep(negEnvL_, detL < 0.0f ? -detL : 0.0f, hangTau);
    posEnvR_ = envStep(posEnvR_, detR > 0.0f ? detR : 0.0f, hangTau);
    negEnvR_ = envStep(negEnvR_, detR < 0.0f ? -detR : 0.0f, hangTau);

    // Linked stereo: ONE level for both channels (max of all four
    // half-wave envelopes) -> one gain -> centre image cannot wander.
    float level = posEnvL_;
    if (negEnvL_ > level) level = negEnvL_;
    if (posEnvR_ > level) level = posEnvR_;
    if (negEnvR_ > level) level = negEnvR_;

    // ── Dynamic-ratio gain computer (no ratio control, by design) ─
    // The smoothing pole IS the compressor's attack and release.
    // v3 ballistics (Nathan: release "five times quicker at
    // least"): release pole 11 ms (7 ms at max) — see the header
    // constants — with the vari-mu programme dependence on top:
    // the stress memory (a ~1.5 s average of the GR itself)
    // stretches the release up to ~2.6x while the box is being
    // held down continuously, so (a) isolated hits still recover
    // at full speed, (b) non-stop slamming doesn't chatter, and
    // (c) sustained-LF waveform riding at the fast release is
    // contained. The attack side is untouched by stress.
    const float overDb = linearToDb(level) - thresholdDbForKnob(k);
    const float grTarget = dynamicGainReductionDb(overDb, grCapForKnob(k));

    stressDb_ += (grDb_ - stressDb_) * tauCoef(kStressTauSec, fs_);
    const float stress01 = clampf(stressDb_ / 50.0f, 0.0f, 1.0f);
    const float relTau = releaseTauForKnob(k) * (1.0f + kStressStretch * stress01);

    const float gc = (grTarget > grDb_) ? tauCoef(attackTauForKnob(k), fs_)
                                        : tauCoef(relTau, fs_);
    grDb_ += (grTarget - grDb_) * gc;
    if (grDb_ < 1.0e-9f) grDb_ = 0.0f;

    // ── Overshoot ceiling (v2 guard, v3 form) ────────────────────
    // The slow attack's price: for the first ~10-20 ms of a loud
    // hit the programme GR has not yet arrived, and if the makeup
    // is still tens of dB up from before a gap, the hit enters at
    // a large NET positive gain. v3 makes this state NORMAL (auto
    // makeup = average GR + crank, and the release is now fast
    // enough that GR sits near 0 in every gap), so the v2 guard's
    // mechanism — pulling the GR state up with a 0.3 ms pole —
    // is too slow by two orders of magnitude: measured on the
    // pre-charged gap scenario, the net gain reached 711x in the
    // first samples and the output peaked at 1.58 while the pole
    // was still closing a ~57 dB gap (v2 only passed because its
    // slow release kept GR parked high through the gap).
    // v3 form: the ceiling is an INSTANTANEOUS feed-forward cap
    // on the applied gain, computed at the gain stage below from
    // THIS sample's pre-TD peak: effGrDb = max(grDb, makeupDb +
    // 20*log10(peak x drive)), so the saturator input can never
    // exceed the clean ceiling (1.0) on any sample, however the
    // states are arranged. The GR state itself is untouched (the
    // display and the release law see the honest programme GR);
    // the cap is active only while the programme GR is below the
    // ceiling requirement — the transient window, plus loud
    // steady material at high crank, where it acts as the
    // brickwall a Level-Loc-style box is expected to be.
    // The peak is read PRE-TD (the transformer outputs xinL/xinR),
    // not from wetIn: after a gap the TD's fast envelope needs
    // ~2 ms to climb back, and in that window wetIn is ducked by
    // ~20 dB while the net gain is at its highest — a wetIn-based
    // ceiling wakes up late. The TD only ever cuts (gain <= 1),
    // so the pre-TD peak is the conservative, correct basis.
    const float satD = satDriveForKnob(k);
    // Ceiling basis: a PEAK HOLD of the pre-TD signal —
    // instantaneous up, 25 ms release — not the raw sample and
    // not a pole envelope. The first v3 cut used the raw sample
    // and the ceiling then modulated the gain within every cycle
    // of a steady tone (gain proportional to 1/|x| — measured
    // 36% THD on a 60 Hz tone at max). A 50 µs attack pole was
    // tried next: it lagged a rising transient edge by ~17%
    // (1.8 dB), enough to let a cold hit reach 1.054. The peak
    // hold is exact on both sides of the problem: on a rising
    // edge it IS the max-so-far (the value a peak ceiling must
    // answer to), and on a steady tone it stays pinned at the
    // cycle peak across the whole cycle, so steady tones are
    // limited, not reshaped.
    {
        const float pkL = xinL < 0.0f ? -xinL : xinL;
        const float pkR = xinR < 0.0f ? -xinR : xinR;
        const float pkIn = pkL > pkR ? pkL : pkR;
        if (pkIn > pkEnv_) pkEnv_ = pkIn;
        else {
            pkEnv_ += (pkIn - pkEnv_) * tauCoef(0.025f, fs_);
            if (pkEnv_ < 1.0e-15f) pkEnv_ = 0.0f;
        }
    }
    const float pkSat = pkEnv_ * satD;

    // AUTO MAKEUP (v3 form — Nathan's explicit ask on 1.0.2).
    // makeupAvgDb_ is a slow average (120 ms rise / 300 ms relax)
    // of the PROGRAMME GR — the gain computer's curve output
    // grTarget, BEFORE the overshoot guard's additions. The
    // applied makeup is that average (FULL compensation, factor
    // 1.0 — v1/v2 used 0.8 of the lagged total) PLUS the knob-tied
    // crank (makeupCrankDbForKnob), capped. It tracks grTarget
    // rather than the smoothed grDb_ because the smoother's slow
    // attack / fast release biases its time-average LOW on
    // exactly the rhythmic material that pumps (the state spends
    // the gaps near zero), which would under-compensate where it
    // matters; the curve output's average is the honest settled
    // depth. (An earlier v3 cut tracked the instantaneous total
    // and, through the old pole guard, ratcheted to 78 dB — the
    // failure that forced the ceiling's instantaneous form.)
    // Because the makeup moves only at average speed, the
    // hit-rate pumping trajectory passes at full depth — the
    // makeup sets the loudness the pumping plays around, and
    // that loudness now RISES with the knob instead of falling
    // (v2 measured net −1.8 dB at centre falling to −7.4 dB at
    // max on program; see docs/audio-v3-diagnosis.md). The
    // ceiling below checks peaks against this full cranked
    // makeup on every sample.
    const float mkAvgTau = (grTarget > makeupAvgDb_) ? 0.120f : kMakeupTauSec;
    makeupAvgDb_ += (grTarget - makeupAvgDb_) * tauCoef(mkAvgTau, fs_);
    makeupDb_ = makeupAvgDb_ + makeupCrankDbForKnob(k);
    const float mkCap = makeupCapForKnob(k);
    if (makeupDb_ > mkCap) makeupDb_ = mkCap;

    // Applied gain: programme GR, raised instantaneously to the
    // overshoot ceiling's requirement whenever that is higher
    // (see the ceiling note above). pkSat <= ~0.001 needs no cap
    // (the requirement is far below any real GR there).
    float effGrDb = grDb_;
    if (pkSat > 0.001f) {
        const float ceilGrDb = makeupDb_ + linearToDb(pkSat);
        if (ceilGrDb > effGrDb) effGrDb = ceilGrDb;
    }
    const float gainLin = dbToLinear(-effGrDb + makeupDb_);

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
    ringM_.assign((size_t)sz, 0.0f);
    latency_ = (int)std::lround((double)kWarbleBaseDelayMs * 0.001 * fs_);

    const float q = 0.7071f; // two of these in series = 4th-order rolloff
    lpInS_.setLowpass(fs_, kWarbleLowpassHz, q);
    lpOutS_.setLowpass(fs_, kWarbleLowpassHz, q);
    hpOutS_.setHighpass(fs_, kWarbleHighpassHz, q);
    lpInM_.setLowpass(fs_, kWarbleLowpassHz, q);
    lpOutM_.setLowpass(fs_, kWarbleLowpassHz, q);
    hpOutM_.setHighpass(fs_, kWarbleHighpassHz, q);
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
    std::fill(ringM_.begin(), ringM_.end(), 0.0f);
    writePos_ = 0;
    lpInS_.reset();
    lpOutS_.reset();
    hpOutS_.reset();
    lpInM_.reset();
    lpOutM_.reset();
    hpOutM_.reset();
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
        // v3: the lines are fed continuously while off (see
        // process()), so the transport is already live and
        // coherent — engaging just crossfades into it. (v1/v2
        // cleared the line here; with warm lines there is
        // nothing stale to clear.)
        if (state_ == State::off)
            fadeGain_ = 0.0f;
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
    // ── Mid/side (v10 topology, v3 mid share) ─────────────────────
    // The (L+R)/2, (L−R)/2 encode is exactly invertible, so nothing
    // is lost by working here. The SIDE rides the transport at full
    // depth; the MID takes a half-depth share of the SAME transport
    // (v3 — see the header topology note for why the v10 dry-centre
    // guarantee was retired). Both lines are written on EVERY call,
    // including while off, so the transport is always live.
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
    ringM_[(size_t)writePos_] = lpInM_.process(mid);
    const float wetSide = hpOutS_.process(
        lpOutS_.process(readDelay(ringS_.data(), delaySamples)));
    const float wetMidLine = hpOutM_.process(
        lpOutM_.process(readDelay(ringM_.data(), delaySamples)));
    writePos_ = (writePos_ + 1) & ringMask_;

    if (state_ == State::off) {
        // Fully out of the path: bit-exact copy. (The transport
        // above still ran, so a later engage is click-free.)
        outL = inL;
        outR = inR;
        return;
    }

    const float midBlend = mid + kWarbleMidDepth * (wetMidLine - mid);
    const float wetL = midBlend + wetSide;
    const float wetR = midBlend - wetSide;

    // Crossfade bookkeeping (30 ms linear ramp).
    const float step = 1.0f / (kWarbleFadeSec * (float)fs_);
    if (state_ == State::fadingIn) {
        fadeGain_ += step;
        if (fadeGain_ >= 1.0f) { fadeGain_ = 1.0f; state_ = State::on; }
    } else if (state_ == State::fadingOut) {
        fadeGain_ -= step;
        if (fadeGain_ <= 0.0f) { fadeGain_ = 0.0f; state_ = State::off; }
    }
    // Subtractive blend form: out = in + g·(wet − in). (In v10
    // this was also exact for mono/centre content at any fade
    // position; with the v3 mid share the wet path genuinely
    // differs from the input there too — the 30 ms ramp is what
    // keeps the engage click-free now, and the warm lines keep
    // the wet signal coherent from the first faded sample.)
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
