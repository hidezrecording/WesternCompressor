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
    hostFs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    fs_ = hostFs_ * (double)kBa6aOsRatio; // v53: the core runs oversampled

    // Oversampling prototype (v53): windowed-sinc low-pass,
    // cutoff at the host Nyquist, Blackman-Harris window.
    // Sum-normalised so each polyphase up phase sums to ~1
    // and the down filter's DC gain is 1.
    {
        const int c = (kOsTaps - 1) / 2;
        double sum = 0.0;
        for (int j = 0; j < kOsTaps; ++j) {
            const double x = ((double)j - c) / (double)kBa6aOsRatio;
            const double sinc = x == 0.0 ? 1.0
                : std::sin(3.14159265358979323846 * x)
                / (3.14159265358979323846 * x);
            const double ph = 2.0 * 3.14159265358979323846 * j / (kOsTaps - 1);
            const double w = 0.35875 - 0.48829 * std::cos(ph)
                           + 0.14128 * std::cos(2 * ph)
                           - 0.01168 * std::cos(3 * ph);
            osCoefUp_[j] = (float)(sinc * w);
            sum += sinc * w;
        }
        for (int j = 0; j < kOsTaps; ++j) {
            osCoefDn_[j] = (float)(osCoefUp_[j] / sum);
            osCoefUp_[j] = (float)(osCoefUp_[j] * (kBa6aOsRatio / sum));
        }
    }
    lawCoef_ = tauCoef(kBa6aLawSlewTauSec, fs_);

    ironInL_.prepare(fs_);  ironInR_.prepare(fs_);
    ironMidL_.prepare(fs_); ironMidR_.prepare(fs_);
    ironOutL_.prepare(fs_); ironOutR_.prepare(fs_);
    reset();
}

void Ba6aLimiter::reset()
{
    for (int i = 0; i < kOsHist; ++i) osInL_[i] = osInR_[i] = 0.0f;
    osInPos_ = 0;
    for (int i = 0; i < kOsTaps + kBa6aOsRatio + 8; ++i)
        osHighL_[i] = osHighR_[i] = 0.0f;
    osHighPos_ = 0;
    lawSlope_ = lawSlopeT_; lawToe_ = lawToeT_; lawSense_ = lawSenseT_;
    // v64: no timing-scale state to settle — the scales
    // are compile-time constants, live from sample 0.
    ironInL_.reset();  ironInR_.reset();
    ironMidL_.reset(); ironMidR_.reset();
    ironOutL_.reset(); ironOutR_.reset();
    hpLpL_ = hpLpR_ = 0.0f;
    lpL_ = lpR_ = 0.0f;
    sag_ = 0.0f;
    detEnv_ = 0.0f;
    lastPreL_ = lastPreR_ = 0.0f;
    prePeak_ = 0.0f;
    grSingle_ = grDual1_ = grDual2_ = 0.0f;
    stress_ = 0.0f;
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
    // Oversampled wrapper (v53): upsample x2 (polyphase),
    // run the core at the high rate, decimate. The pair's
    // group delay is 32 host samples, reported via
    // osDelaySamples().
    constexpr int HS = kOsTaps + kBa6aOsRatio + 8;
    osInL_[osInPos_] = inL;
    osInR_[osInPos_] = inR;
    for (int p = 0; p < kBa6aOsRatio; ++p) {
        float upL = 0.0f, upR = 0.0f;
        for (int idx = p, k = 0; idx < kOsTaps; idx += kBa6aOsRatio, ++k) {
            const float xvL = osInL_[(osInPos_ - k) & (kOsHist - 1)];
            const float xvR = osInR_[(osInPos_ - k) & (kOsHist - 1)];
            upL += osCoefUp_[idx] * xvL;
            upR += osCoefUp_[idx] * xvR;
        }
        float dnL, dnR;
        processCore(upL, upR, dnL, dnR);
        osHighPos_ = (osHighPos_ + 1) % HS;
        osHighL_[osHighPos_] = dnL;
        osHighR_[osHighPos_] = dnR;
    }
    osInPos_ = (osInPos_ + 1) & (kOsHist - 1);
    float accL = 0.0f, accR = 0.0f;
    for (int j = 0; j < kOsTaps; ++j) {
        const int idx = ((osHighPos_ - j) % HS + HS) % HS;
        accL += osCoefDn_[j] * osHighL_[idx];
        accR += osCoefDn_[j] * osHighR_[idx];
    }
    // The shipped peak discipline, re-applied at the output
    // rate (v53): decimation ringing on ceiling-limited
    // peaks can overshoot the 1.600 bound (Gibbs); this is
    // the makeup ceiling's own instant law and touches only
    // samples above it — see the oversampling note in the
    // header.
    const float ipk = std::max(std::fabs(accL), std::fabs(accR));
    if (ipk > kBa6aMakeupPeakCeil) {
        const float g = kBa6aMakeupPeakCeil / ipk;
        accL *= g; accR *= g;
    }
    outL = accL;
    outR = accR;
}

void Ba6aLimiter::processCore(float inL, float inR, float& outL, float& outR)
{
    // Ratio-law slew (v53): the mode's slope/toe/sense glide
    // to their targets (~5 ms) so a mode toggle never steps
    // the gain computer.
    lawSlope_ += (lawSlopeT_ - lawSlope_) * lawCoef_;
    lawToe_   += (lawToeT_   - lawToe_)   * lawCoef_;
    lawSense_ += (lawSenseT_ - lawSense_) * lawCoef_;

    // Baked timing scales (v64 / 1.3.1: the v62 selector's
    // settled 2X value as one compile-time constant; AB2
    // test build: one calibrated constant per seat — see
    // the kBa6aAttFastScale note in the header). Compile-
    // time, so the scales apply from the first sample (the
    // selector's ~46 ms load glide is gone with the
    // selector).

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
    // AB3: the v41 sidechain low-cut is deleted with its
    // pot (repurposed as COMP MIX) — the detector hears
    // the raw sense, exactly as it did at the feature's
    // 10 Hz bottom stop, so this line is the shipped one.
    const float peak = lawSense_
                     * std::max(std::fabs(lastPreL_), std::fabs(lastPreR_));
    const bool detRising = peak > detEnv_;
    const float detTau = detRising
        ? (attackFast_ ? kBa6aDetFastTauSec : ba6aDetSlowTau(peak, detEnv_))
        : kBa6aDetDecayTauSec;
    // AB2: the rising charge takes its seat's scale (FAST
    // or the SLOW shaped tau); the decay keeps its own
    // held scale (release-side infrastructure).
    const float detScale = detRising ? (attackFast_ ? kBa6aAttFastScale
                                                    : kBa6aAttSlowScale)
                                     : kBa6aDetDecayScale;
    detEnv_ += (peak - detEnv_) * tauCoef(detTau * detScale, fs_);

    const float target = ba6aVariMuDb(detEnv_, lawSlope_, lawToe_);

    // ── Control state: BOTH recovery networks run every
    // sample; the toggle selects which one drives the stage,
    // so flipping RELEASE mid-programme never jumps. ──
    const float attTau = (attackFast_ ? kBa6aAttFastTauSec : kBa6aAttSlowTauSec)
                       * (attackFast_ ? kBa6aAttFastScale : kBa6aAttSlowScale);
    if (target > grSingle_)
        grSingle_ += (target - grSingle_) * tauCoef(attTau, fs_);
    else
        grSingle_ += (target - grSingle_) * tauCoef(kBa6aRelSingleTauSec * kBa6aRelSingleScale, fs_);
    const float t1 = kBa6aRelDualFastShare * target;
    const float t2 = (1.0f - kBa6aRelDualFastShare) * target;
    if (t1 > grDual1_)
        grDual1_ += (t1 - grDual1_) * tauCoef(attTau, fs_);
    else
        grDual1_ += (t1 - grDual1_) * tauCoef(kBa6aRelDualFastTauSec * kBa6aRelDualScale, fs_);
    // The Dual tail pole carries the T4 memory: the stress
    // state (integrated GR history, updated below from the
    // PREVIOUS sample's GR — the cell remembers what it has
    // already done) stretches the pole monotonically.
    const float dualSlowTau = kBa6aRelDualSlowTauSec
        * (1.0f + kBa6aMemStretch * clampf(stress_ / kBa6aMemRefDb, 0.0f, 1.0f)) * kBa6aRelDualScale;
    if (t2 > grDual2_)
        grDual2_ += (t2 - grDual2_) * tauCoef(attTau, fs_);
    else
        grDual2_ += (t2 - grDual2_) * tauCoef(dualSlowTau, fs_);
    grDb_ = releaseFast_ ? grSingle_ : grDual1_ + grDual2_;

    // ── T4 memory state (v45): charges toward the GR being
    // done (tau ~1.2 s), forgets in quiet (tau ~8 s). Bounded
    // by the GR cap itself; only ever read as a 0..1 stretch
    // of the Dual tail pole. ──
    stress_ += (grDb_ - stress_) * tauCoef(
        (grDb_ > stress_ ? kBa6aMemChargeTauSec : kBa6aMemDecayTauSec) * kBa6aRelDualScale, fs_);

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
// Phaser90 (v55) — see the header for the circuit basis.
// One-pole allpass stage: H(z) = (z^-1 - c) / (1 - c z^-1),
// the bilinear image of the analogue stage (s - wb)/(s + wb)
// with c = (1 - t)/(1 + t), t = tan(pi * fb / fs): the stage
// phase is exactly -90 degrees at the break fb, 0 at DC and
// -180 at Nyquist. Four ganged stages sum 50/50 with the dry:
// notches where the cascade phase is -180 / -540 degrees,
// i.e. at fb * tan(22.5 deg) and fb * tan(67.5 deg) in the
// warped domain (0.4142 / 2.4142 x fb before warping).
// ═════════════════════════════════════════════════════════════

void Phaser90::prepare(double sampleRate)
{
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void Phaser90::reset()
{
    state_ = State::off;
    fadeGain_ = 0.0f;
    lfoPhase_ = 0.0f;
    breakHz_ = kPhaserBreakLoHz;
    for (int ch = 0; ch < 2; ++ch)
        for (int st = 0; st < 4; ++st)
            sx_[ch][st] = sy_[ch][st] = 0.0f;
}

void Phaser90::setEnabled(bool on)
{
    if (on == target_) return;
    target_ = on;
    state_ = on ? State::fadingIn : State::fadingOut;
}

void Phaser90::processImpl(float inL, float inR, float& outL, float& outR,
                           float& effL, float& effR)
{
    // The LFO turns continuously, engaged or not (the
    // pedal's oscillator never stops), and the stages track
    // the input every sample so engaging fades into a live
    // machine — the ADT transports' discipline, mirrored.
    if (!lfoFrozen_) {
        lfoPhase_ += kPhaserRateHz / (float)fs_;
        if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
    }
    // Log-domain sweep: the break travels its decade at a
    // constant pitch rate, bottom to top and back per cycle.
    const float s = std::sin(lfoPhase_ * 6.283185307179586f);
    breakHz_ = kPhaserBreakLoHz
        * std::pow(kPhaserBreakHiHz / kPhaserBreakLoHz,
                   0.5f * (s + 1.0f));
    // One coefficient for all eight stage instances: ganged
    // breaks, one LFO, both channels — the pedal is mono.
    const float t = (float)std::tan(3.14159265358979323846
                                    * (double)breakHz_ / fs_);
    const float c = (1.0f - t) / (1.0f + t);

    float wetL = inL, wetR = inR;
    for (int st = 0; st < 4; ++st) {
        wetL = stage(wetL, 0, st, c);
        wetR = stage(wetR, 1, st, c);
    }
    // The pedal's output mixer: phased path summed 50/50
    // with the dry. |(1 + e^jphi)/2| <= 1 everywhere, so the
    // mix never exceeds the dry level on a steady tone.
    const float mixL = 0.5f * (inL + wetL);
    const float mixR = 0.5f * (inR + wetR);

    if (state_ == State::off) {
        outL = inL;
        outR = inR;
        effL = inL;   // AB3: the effect arm at fade 0 is the
        effR = inR;   // input exactly, like the classic arm.
        return;
    }
    const float step = 1.0f / (kPhaserFadeSec * (float)fs_);
    if (state_ == State::fadingIn) {
        fadeGain_ += step;
        if (fadeGain_ >= 1.0f) { fadeGain_ = 1.0f; state_ = State::on; }
    } else if (state_ == State::fadingOut) {
        fadeGain_ -= step;
        if (fadeGain_ <= 0.0f) { fadeGain_ = 0.0f; state_ = State::off; }
    }
    // Subtractive blend: exact dry at g=0, exact mix at g=1.
    const float g = fadeGain_;
    outL = inL + (mixL - inL) * g;
    outR = inR + (mixR - inR) * g;
    // AB3 effect arm: the same engage fade, taken toward
    // the bare cascade — hard-selected at the extremes so
    // a settled machine delivers the cascade bit-for-bit
    // (at g=1) and the input bit-for-bit (at g=0).
    if (g >= 1.0f)      { effL = wetL; effR = wetR; }
    else if (g <= 0.0f) { effL = inL;  effR = inR; }
    else {
        effL = inL + (wetL - inL) * g;
        effR = inR + (wetR - inR) * g;
    }
}

void Phaser90::process(float inL, float inR, float& outL, float& outR)
{
    float effL, effR;
    processImpl(inL, inR, outL, outR, effL, effR);
}

void Phaser90::processEffect(float inL, float inR, float& outL, float& outR)
{
    float classicL, classicR;
    processImpl(inL, inR, classicL, classicR, outL, outR);
}

// ═════════════════════════════════════════════════════════════
// Chain
// ═════════════════════════════════════════════════════════════

void Chain::prepare(double sampleRate)
{
    fs_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    limiter_.prepare(fs_);
    adt_.prepare(fs_);
    phaser_.prepare(fs_);
    // AB3: the mod stage's dry ring — sized for the ADT's
    // 20 ms offset at this rate plus margin, power of two.
    int need = (int)std::ceil(0.025 * fs_) + 64;
    int sz = 256;
    while (sz < need) sz <<= 1;
    compRingL_.assign((size_t)sz, 0.0f);
    compRingR_.assign((size_t)sz, 0.0f);
    compMask_ = sz - 1;
    reset();
}

void Chain::reset()
{
    limiter_.reset();
    adt_.reset();
    phaser_.reset();
    modBlend_ = modPhaser_ ? 1.0f : 0.0f;
    // AB3: both mix slews snap to their targets and the
    // dry taps start settled — a fresh instance is at its
    // panel state from the first sample, no load glide
    // (the retired whole-chain mix's reset discipline).
    compMixCur_ = compMixTarget_;
    modMixCur_ = modMixTarget_;
    for (int i = 0; i < kDry32Size; ++i) dry32L_[i] = dry32R_[i] = 0.0f;
    dry32Write_ = 0;
    std::fill(compRingL_.begin(), compRingL_.end(), 0.0f);
    std::fill(compRingR_.begin(), compRingR_.end(), 0.0f);
    compWrite_ = 0;
    modDelayA_ = modDelayB_ = modExtraTarget();
    modTapFade_ = 1.0f;
}

void Chain::process(float inL, float inR, float& outL, float& outR)
{
    // The limiter section is always engaged (v53: the old
    // in/out bypass became the COMP/LIMIT ratio mode switch,
    // which lives inside the limiter's gain computer).
    float midL, midR;
    limiter_.process(inL, inR, midL, midR);

    // ── AB3 stage 1 — COMP MIX: the raw input delayed by
    // exactly the limiter's 32-sample oversampling delay
    // (a fixed tap, written every sample) against the
    // limiter's output. Linear crossfade, hard selects at
    // the extremes: at 1 the limiter output passes
    // untouched (the shipped path), at 0 the output of
    // this stage is the input delayed by exactly 32. ──
    dry32L_[dry32Write_] = inL;
    dry32R_[dry32Write_] = inR;
    const int rd32 = (dry32Write_ - limiter_.osDelaySamples()) & (kDry32Size - 1);
    const float dryL = dry32L_[rd32], dryR = dry32R_[rd32];
    dry32Write_ = (dry32Write_ + 1) & (kDry32Size - 1);
    compMixCur_ += (compMixTarget_ - compMixCur_) * 0.002f;
    if (std::fabs(compMixTarget_ - compMixCur_) < 2.0e-5f)
        compMixCur_ = compMixTarget_;
    float compL, compR;
    if (compMixCur_ >= 1.0f)      { compL = midL; compR = midR; }
    else if (compMixCur_ <= 0.0f) { compL = dryL; compR = dryR; }
    else {
        compL = dryL * (1.0f - compMixCur_) + midL * compMixCur_;
        compR = dryR * (1.0f - compMixCur_) + midR * compMixCur_;
    }

    // MOD insert: both machines run continuously on the
    // post-comp signal; the flavour blend selects between
    // their WET ARMS. While MOD is out each arm returns
    // its input bit-exact once settled (the ADT by its
    // state machine, the phaser likewise), so the stage-2
    // blend is the post-comp signal alone in either
    // flavour — the MOD MIX pot is inert, as briefed.
    //   A arm: the ADT's natural output (SRC centre +
    //     decks), exactly as authored in 1.3.1.
    //   P arm: the phaser's bare cascade (processEffect)
    //     — the pot re-creates the classic 50/50 pedal at
    //     0.5, the pure phased signal at 1.0.
    float aL, aR, pL, pR;
    adt_.process(compL, compR, aL, aR);
    phaser_.processEffect(compL, compR, pL, pR);
    const float target = modPhaser_ ? 1.0f : 0.0f;
    if (modBlend_ != target) {
        const float step = 1.0f / (kAdtFadeSec * (float)fs_);
        modBlend_ = target > modBlend_
            ? std::min(target, modBlend_ + step)
            : std::max(target, modBlend_ - step);
    }
    // Hard-select at the extremes: a settled flavour is its
    // own output, bit-for-bit (the subtractive blend is the
    // transition law, as in the inserts themselves).
    float wetL, wetR;
    if (modBlend_ <= 0.0f) {
        wetL = aL; wetR = aR;
    } else if (modBlend_ >= 1.0f) {
        wetL = pL; wetR = pR;
    } else {
        wetL = aL + (pL - aL) * modBlend_;
        wetR = aR + (pR - aR) * modBlend_;
    }

    // ── AB3 stage 2 — MOD MIX: the post-comp signal
    // delayed by the inserts' own latency (the offset
    // target tracks MOD toggle and A/P flavour; a change
    // starts the v44-style 30 ms offset crossfade between
    // the old and new reads) against the wet arm. Linear
    // crossfade, hard selects at the extremes: at 0 the
    // comp path alone, aligned to the reported latency;
    // at 1 the wet arm untouched. ──
    compRingL_[(size_t)compWrite_] = compL;
    compRingR_[(size_t)compWrite_] = compR;
    const int extra = modExtraTarget();
    if (extra != modDelayB_) {
        modDelayA_ = modDelayB_;
        modDelayB_ = extra;
        modTapFade_ = 0.0f;
    }
    float modDryL, modDryR;
    if (modTapFade_ >= 1.0f && modDelayB_ == 0) {
        modDryL = compL; modDryR = compR;   // settled, no offset
    } else {
        const float bL = readCompRing(compRingL_, modDelayB_);
        const float bR = readCompRing(compRingR_, modDelayB_);
        if (modTapFade_ >= 1.0f) {
            modDryL = bL; modDryR = bR;
        } else {
            const float aLagL = readCompRing(compRingL_, modDelayA_);
            const float aLagR = readCompRing(compRingR_, modDelayA_);
            modDryL = aLagL + (bL - aLagL) * modTapFade_;
            modDryR = aLagR + (bR - aLagR) * modTapFade_;
            modTapFade_ = std::min(1.0f, modTapFade_ + 1.0f / (kAdtFadeSec * (float)fs_));
        }
    }
    compWrite_ = (compWrite_ + 1) & compMask_;
    modMixCur_ += (modMixTarget_ - modMixCur_) * 0.002f;
    if (std::fabs(modMixTarget_ - modMixCur_) < 2.0e-5f)
        modMixCur_ = modMixTarget_;
    if (modMixCur_ >= 1.0f)      { outL = wetL; outR = wetR; }
    else if (modMixCur_ <= 0.0f) { outL = modDryL; outR = modDryR; }
    else {
        outL = modDryL * (1.0f - modMixCur_) + wetL * modMixCur_;
        outR = modDryR * (1.0f - modMixCur_) + wetR * modMixCur_;
    }
}

} // namespace osc72
