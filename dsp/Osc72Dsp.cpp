// ─────────────────────────────────────────────────────────────────────
// Osc72Dsp.cpp — engine v4 implementation. See Osc72Dsp.h for
// the architecture, the research/design docs for the why.
// ─────────────────────────────────────────────────────────────────────
#include "Osc72Dsp.h"

namespace osc72 {

// ═════════════════════════════════════════════════════════════
// Ba6aLimiter
// ═════════════════════════════════════════════════════════════

void Ba6aLimiter::prepare(double sampleRate)
{
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    setScCutoff(scCutoffHz_);   // v41: refresh the sidechain coefficient at the real rate
    ironInL_.prepare(fs_);  ironInR_.prepare(fs_);
    ironMidL_.prepare(fs_); ironMidR_.prepare(fs_);
    ironOutL_.prepare(fs_); ironOutR_.prepare(fs_);
    reset();
}

void Ba6aLimiter::reset()
{
    ironInL_.reset();  ironInR_.reset();
    ironMidL_.reset(); ironMidR_.reset();
    ironOutL_.reset(); ironOutR_.reset();
    hpLpL_ = hpLpR_ = 0.0f;
    lpL_ = lpR_ = 0.0f;
    sag_ = 0.0f;
    scLpL_ = scLpR_ = 0.0f;
    detEnv_ = 0.0f;
    lastPreL_ = lastPreR_ = 0.0f;
    prePeak_ = 0.0f;
    grSingle_ = grDual1_ = grDual2_ = 0.0f;
    grDb_ = 0.0f;
    avgGrDb_ = 0.0f;
    makeupDb_ = 0.0f;
    knobS_ = knobTarget_;
}

float Ba6aLimiter::processChannel(float x, int ch, float grLin, float driveLin)
{
    IronStage& ironIn  = ch == 0 ? ironInL_  : ironInR_;
    IronStage& ironMid = ch == 0 ? ironMidL_ : ironMidR_;
    IronStage& ironOut = ch == 0 ? ironOutL_ : ironOutR_;
    float& hpLp = ch == 0 ? hpLpL_ : hpLpR_;
    float& lp   = ch == 0 ? lpL_   : lpR_;

    // 1. Input transformer.
    float y = ironIn.process(x);
    // 2. Drive + the vari-mu stage's gain (the 6SK7 pair under
    //    the shared control voltage), then the pair's own
    //    plate-swing limit — a tube stage cannot pass more
    //    than its swing, however hard it is driven; that
    //    ceiling is part of the squash at extreme drive.
    y = y * driveLin * grLin;
    y = tubeSat(y, 0.9f, 0.0f);
    //    Residual push-pull imbalance, growing with depth (the
    //    hardware truth the BAL trim pots exist to null; kept
    //    at the musical end of "ragged").
    const float depth = clampf(grDb_ / 30.0f, 0.0f, 1.0f);
    y += 0.02f * depth * y * y;
    // 3. Interstage transformer.
    y = ironMid.process(y);
    // 4. 6J7 driver: soft saturation (ceiling ~1.8 — it bends
    //    late, approaching the output stage's territory).
    y = tubeSat(y, 0.55f, 0.05f);
    // 5. 6V6 push-pull output: odd-dominant; its asymptote
    //    (~1.43) IS the unit's output ceiling — the bend a
    //    +30 dBm-class stage gives approaching full swing.
    //    B+ sag (v1.1.2, study §2.3a): the 5R4-era rail sags
    //    under programme current demand while the OD3 holds
    //    the screens/bias stiff — so the sag lands here, on
    //    the output stage: under load it bends earlier and
    //    gives back a fraction of a dB (x(1 - 0.05.sag),
    //    -0.45 dB max). The state is driven by the output
    //    level itself (in process()), charges in ~25 ms and
    //    recovers over ~240 ms — the box breathes with the
    //    programme, and because the detector reads the
    //    sagged output the loop feels it too.
    y = tubeSat(y, 0.7f * (1.0f + 0.30f * sag_), 0.015f);
    y *= (1.0f - 0.05f * sag_);
    // 6. Output transformer (the fat one).
    y = ironOut.process(y);
    // 7. Response shaping: the family's ±2 dB / 30 Hz–15 kHz
    //    envelope as a gentle LF droop (~16 Hz) + HF droop
    //    (~19 kHz). Also strips any DC the imbalance term made
    //    (the LF pole's settle is the iron's natural recovery —
    //    measured in the dryness test, suite E2).
    hpLp += (y - hpLp) * tauCoef(0.00995f, fs_);
    y -= hpLp;
    lp += (y - lp) * tauCoef(0.00000838f, fs_);
    return lp;
}

void Ba6aLimiter::process(float inL, float inR, float& outL, float& outR)
{
    // Knob smoothing (5 ms): the attenuator is stepped on the
    // hardware; a short slew keeps moves click-free without
    // changing the law.
    knobS_ += (knobTarget_ - knobS_) * tauCoef(0.005f, fs_);

    // ── Supply sag state (v1.1.2, study §2.3a): driven by
    // last sample's pre-make-up output level — the output
    // stage's own current demand — like the detector beside
    // it. Signal-driven and signal-decaying (dryness): with
    // no signal the rail is simply full. Bounded 0..1, level
    // effect small and monotone (predictability). ──
    {
        const float outPeak = std::max(std::fabs(lastPreL_), std::fabs(lastPreR_));
        const float sagTgt = clampf(outPeak / 0.85f, 0.0f, 1.0f);
        sag_ += (sagTgt - sag_) * tauCoef(sagTgt > sag_ ? 0.025f : 0.240f, fs_);
    }

    // ── Detector (6H6): reads LAST sample's post-transformer,
    // pre-make-up output — output-referenced feedback, with
    // the loop's inherent one-sample delay (the analogue loop
    // is continuous; one sample at audio rates is the honest
    // discrete equivalent and keeps the loop stable). ──
    // v41 sidechain low-cut: the per-channel LP states track
    // the sense every sample (engaging the filter never
    // steps); above the 10 Hz bottom stop the detector hears
    // sense − LP (the high-passed sense), at the stop it
    // hears the raw sense exactly as before the feature.
    scLpL_ += (lastPreL_ - scLpL_) * scCoef_;
    scLpR_ += (lastPreR_ - scLpR_) * scCoef_;
    const bool scIn = scCutoffHz_ > 10.5f;
    const float senL = scIn ? lastPreL_ - scLpL_ : lastPreL_;
    const float senR = scIn ? lastPreR_ - scLpR_ : lastPreR_;
    const float peak = kBa6aDetSense
                     * std::max(std::fabs(senL), std::fabs(senR));
    const float detTau = peak > detEnv_
        ? (attackFast_ ? kBa6aDetFastTauSec : kBa6aDetSlowTauSec)
        : kBa6aDetDecayTauSec;
    detEnv_ += (peak - detEnv_) * tauCoef(detTau, fs_);

    const float target = ba6aVariMuDb(detEnv_);

    // ── Control state: BOTH recovery networks run every
    // sample; the toggle selects which one drives the stage,
    // so flipping RELEASE mid-programme never jumps. ──
    const float attTau = attackFast_ ? kBa6aAttFastTauSec : kBa6aAttSlowTauSec;
    if (target > grSingle_)
        grSingle_ += (target - grSingle_) * tauCoef(attTau, fs_);
    else
        grSingle_ += (target - grSingle_) * tauCoef(kBa6aRelSingleTauSec, fs_);
    const float t1 = kBa6aRelDualFastShare * target;
    const float t2 = (1.0f - kBa6aRelDualFastShare) * target;
    if (t1 > grDual1_)
        grDual1_ += (t1 - grDual1_) * tauCoef(attTau, fs_);
    else
        grDual1_ += (t1 - grDual1_) * tauCoef(kBa6aRelDualFastTauSec, fs_);
    if (t2 > grDual2_)
        grDual2_ += (t2 - grDual2_) * tauCoef(attTau, fs_);
    else
        grDual2_ += (t2 - grDual2_) * tauCoef(kBa6aRelDualSlowTauSec, fs_);
    grDb_ = releaseFast_ ? grSingle_ : grDual1_ + grDual2_;

    // ── Auto make-up (design §3.2): tracks the ACTUAL GR
    // (slow average) + a crank term in the knob; applied after
    // the detector tap, like the hardware's OUTPUT control.
    // Ceilinged against the pre-make-up peak so the automated
    // output never pushes peaks past the unit's headroom —
    // the discipline an operator applies by ear. ──
    const float avgTau = grDb_ > avgGrDb_ ? kBa6aMakeupAvgChargeTauSec
                                          : kBa6aMakeupAvgDecayTauSec;
    avgGrDb_ += (grDb_ - avgGrDb_) * tauCoef(avgTau, fs_);
    const float peakNow = std::max(std::fabs(lastPreL_), std::fabs(lastPreR_));
    if (peakNow > prePeak_)
        prePeak_ += (peakNow - prePeak_) * tauCoef(0.0002f, fs_);
    else
        prePeak_ += (peakNow - prePeak_) * tauCoef(0.6f, fs_);
    const float desired = kBa6aMakeupFactor * avgGrDb_
                        + kBa6aMakeupCrankDb * knobS_ * knobS_;
    const float peakCapDb = linearToDb(kBa6aMakeupPeakCeil
                                       / std::max(prePeak_, 0.05f));
    const float makeupTarget = clampf(std::min(desired, peakCapDb),
                                      0.0f, kBa6aMakeupCapDb);
    // Make-up slew: falls instantly (the ceiling discipline is
    // never late), rises over ~50 ms (no gain-step zipper).
    if (makeupTarget < makeupDb_)
        makeupDb_ = makeupTarget;
    else
        makeupDb_ += (makeupTarget - makeupDb_) * tauCoef(0.05f, fs_);

    const float driveLin = dbToLinear(ba6aDriveDb(knobS_));
    const float grLin = dbToLinear(-grDb_);

    const float preL = processChannel(inL, 0, grLin, driveLin);
    const float preR = processChannel(inR, 1, grLin, driveLin);
    lastPreL_ = preL;
    lastPreR_ = preR;
    // Instant ceiling (design §3.2): the slow cap above keeps
    // the make-up honest on programme; this per-sample bound
    // covers the transient case — a hit's first overshoot
    // peak arriving before the slow peak state registers it.
    // |out| <= ceiling whenever it binds, by construction.
    // (Linear domain: no log/pow in the sample loop.)
    const float instPeak = std::max(std::max(std::fabs(preL), std::fabs(preR)), 0.05f);
    const float stateLin = dbToLinear(makeupDb_);
    const float appliedLin = std::min(stateLin, kBa6aMakeupPeakCeil / instPeak);
    outL = preL * appliedLin;
    outR = preR * appliedLin;
}

// ═════════════════════════════════════════════════════════════
// AdtChain
// ═════════════════════════════════════════════════════════════

void AdtChain::prepare(double sampleRate)
{
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    latency_ = (int)std::lround(0.001 * (double)kAdtBaseMs * fs_);
    int need = (int)std::ceil(0.055 * fs_) + 8; // > max total delay (~34 ms)
    int sz = 64;
    while (sz < need) sz <<= 1;
    ringSize_ = sz;
    ringMask_ = sz - 1;
    ringL_.assign((size_t)sz, 0.0f);
    ringR_.assign((size_t)sz, 0.0f);
    reset();
}

void AdtChain::reset()
{
    std::fill(ringL_.begin(), ringL_.end(), 0.0f);
    std::fill(ringR_.begin(), ringR_.end(), 0.0f);
    writePos_ = 0;
    state_ = State::off;
    fadeGain_ = 0.0f;
    delay1Ms_ = lastDelay1Ms_ = kAdtBaseMs;
    delay2Ms_ = lastDelay2Ms_ = kAdtDeck2BaseMs;
    lfo1Phase_ = 0.0f;
    wow1Phase_ = 0.0f; wow2Phase_ = 0.25f;
    flt1Phase_ = 0.0f; flt2Phase_ = 0.4f;
    rand2_ = rand2Target_ = 0.0f;
    rand2Hold_ = 0;
    rngState_ = 0x1f123bb5u;
    hp1_ = lp1_ = hp2_ = lp2_ = 0.0f;
}

void AdtChain::setEnabled(bool on)
{
    if (on == target_) return;
    target_ = on;
    state_ = on ? State::fadingIn : State::fadingOut;
}

float AdtChain::readDelay(int channel, float delaySamples) const
{
    const std::vector<float>& ring = channel == 0 ? ringL_ : ringR_;
    float pos = (float)writePos_ - delaySamples;
    while (pos < 0.0f) pos += (float)ringSize_;
    const int i0 = (int)pos;
    const float frac = pos - (float)i0;
    const float a = ring[(size_t)(i0 & ringMask_)];
    const float b = ring[(size_t)((i0 + 1) & ringMask_)];
    return a + (b - a) * frac;
}

float AdtChain::deckTargetMs(int deck)
{
    const float inv = 1.0f / (float)fs_;
    if (deck == 1) {
        lfo1Phase_ += kAdtLfo1Hz * inv;
        if (lfo1Phase_ >= 1.0f) lfo1Phase_ -= 1.0f;
        return kAdtBaseMs
             + kAdtLfo1Ms * std::sin(lfo1Phase_ * 6.283185307179586f);
    }
    // Deck 2: smoothed random walk (the manual's "random" LFO
    // shape) — a new target every ~0.8 s, slewed by the same
    // motor pole as everything else.
    if (--rand2Hold_ <= 0) {
        rand2Hold_ = (int)(0.8 * fs_);
        rngState_ = rngState_ * 1664525u + 1013904223u;
        rand2Target_ = ((float)((rngState_ >> 9) & 0x7fffffu) / 4194303.5f - 1.0f)
                     * kAdtLfo2Ms;
    }
    rand2_ += (rand2Target_ - rand2_) * tauCoef(0.35f, fs_);
    return kAdtDeck2BaseMs + rand2_;
}

void AdtChain::process(float inL, float inR, float& outL, float& outR)
{
    // Both lines are written on EVERY call, and the transports
    // advance, so the machines are live whenever we fade in.
    ringL_[(size_t)writePos_] = inL;
    ringR_[(size_t)writePos_] = inR;

    const float inv = 1.0f / (float)fs_;
    wow1Phase_ += 0.45f * inv; if (wow1Phase_ >= 1.0f) wow1Phase_ -= 1.0f;
    wow2Phase_ += 0.62f * inv; if (wow2Phase_ >= 1.0f) wow2Phase_ -= 1.0f;
    flt1Phase_ += 5.8f * inv;  if (flt1Phase_ >= 1.0f) flt1Phase_ -= 1.0f;
    flt2Phase_ += 7.3f * inv;  if (flt2Phase_ >= 1.0f) flt2Phase_ -= 1.0f;

    // Motor inertia: the varispeed (base + LFO) slews through
    // the ~180 ms pole — the manual's modelled "physical
    // engine acceleration". Wow/flutter are the residual
    // mechanical wobbles and ride on top of the slewed delay.
    delay1Ms_ += (deckTargetMs(1) - delay1Ms_) * tauCoef(kAdtMotorTauSec, fs_);
    delay2Ms_ += (deckTargetMs(2) - delay2Ms_) * tauCoef(kAdtMotorTauSec, fs_);
    const float d1 = clampf(delay1Ms_
        + kAdtWowMs * std::sin(wow1Phase_ * 6.283185307179586f)
        + kAdtFlutterMs * std::sin(flt1Phase_ * 6.283185307179586f),
        kAdtMinDelayMs, 45.0f);
    const float d2 = clampf(delay2Ms_
        + kAdtWowMs * std::sin(wow2Phase_ * 6.283185307179586f)
        + kAdtFlutterMs * std::sin(flt2Phase_ * 6.283185307179586f),
        kAdtMinDelayMs, 45.0f);
    lastDelay1Ms_ = d1;
    lastDelay2Ms_ = d2;

    const float msToSamples = 0.001f * (float)fs_;
    const float maxSamp = (float)ringSize_ * 0.5f;
    const float srcSamp = clampf(kAdtBaseMs * msToSamples, 2.0f, maxSamp);
    const float srcL = readDelay(0, srcSamp);
    const float srcR = readDelay(1, srcSamp);

    // Deck feeds: the source mono sum at each deck's delay.
    const float d1Samp = clampf(d1 * msToSamples, 2.0f, maxSamp);
    const float d2Samp = clampf(d2 * msToSamples, 2.0f, maxSamp);
    float deck1 = 0.5f * (readDelay(0, d1Samp) + readDelay(1, d1Samp));
    float deck2 = 0.5f * (readDelay(0, d2Samp) + readDelay(1, d2Samp));

    // Tape bandwidth (~35 Hz high-pass, ~8.5 kHz low-pass) and
    // a gentle tape saturation per deck.
    hp1_ += (deck1 - hp1_) * tauCoef(0.00455f, fs_);
    deck1 -= hp1_;
    lp1_ += (deck1 - lp1_) * tauCoef(0.0000187f, fs_);
    deck1 = tubeSat(lp1_, 1.5f, 0.04f);
    hp2_ += (deck2 - hp2_) * tauCoef(0.00455f, fs_);
    deck2 -= hp2_;
    lp2_ += (deck2 - lp2_) * tauCoef(0.0000187f, fs_);
    deck2 = tubeSat(lp2_, 1.5f, 0.04f);

    writePos_ = (writePos_ + 1) & ringMask_;

    if (state_ == State::off) {
        outL = inL;
        outR = inR;
        return;
    }

    // Widest arrangement: source full-stereo in the middle,
    // deck 1 hard RIGHT, deck 2 hard LEFT, matched gains.
    const float wetL = srcL + kAdtDeckGain * deck2;
    const float wetR = srcR + kAdtDeckGain * deck1;

    const float step = 1.0f / (kAdtFadeSec * (float)fs_);
    if (state_ == State::fadingIn) {
        fadeGain_ += step;
        if (fadeGain_ >= 1.0f) { fadeGain_ = 1.0f; state_ = State::on; }
    } else if (state_ == State::fadingOut) {
        fadeGain_ -= step;
        if (fadeGain_ <= 0.0f) { fadeGain_ = 0.0f; state_ = State::off; }
    }
    // Subtractive blend: exact dry at g=0, exact wet at g=1.
    const float g = fadeGain_;
    outL = inL + (wetL - inL) * g;
    outR = inR + (wetR - inR) * g;
}

// ═════════════════════════════════════════════════════════════
// Chain
// ═════════════════════════════════════════════════════════════

void Chain::prepare(double sampleRate)
{
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    limiter_.prepare(fs_);
    adt_.prepare(fs_);
    reset();
}

void Chain::reset()
{
    limiter_.reset();
    adt_.reset();
    limitTarget_ = true;
    limitFade_ = 1.0f;
}

void Chain::setLimitIn(bool in)
{
    limitTarget_ = in;
}

void Chain::process(float inL, float inR, float& outL, float& outR)
{
    // The limiter always runs on the input: detector, make-up
    // and knob smoothing stay live so re-engaging is seamless.
    float limL, limR;
    limiter_.process(inL, inR, limL, limR);

    // Limit crossfade (45 ms linear ramp).
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
    // ADT insert (bit-exact copy when mod is out).
    adt_.process(midL, midR, outL, outR);
}

} // namespace osc72
