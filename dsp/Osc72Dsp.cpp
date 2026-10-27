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
    lawGrowth_ = lawGrowthT_; lawGrowthKnee_ = lawGrowthKneeT_;
    lawSlowSlope_ = lawSlowSlopeT_; lawSlowToe_ = lawSlowToeT_; lawSlowSense_ = lawSlowSenseT_;
    lawSlowGrowth_ = lawSlowGrowthT_; lawSlowGrowthKnee_ = lawSlowGrowthKneeT_;
    lawSlowGrowth2_ = lawSlowGrowth2T_; lawSlowGrowthKnee2_ = lawSlowGrowthKnee2T_;
    lawSlowRef_ = lawSlowRefT_;
    // v64: no timing-scale state to settle — the scales
    // are compile-time constants, live from sample 0.
    ironInL_.reset();  ironInR_.reset();
    ironMidL_.reset(); ironMidR_.reset();
    ironOutL_.reset(); ironOutR_.reset();
    hpLpL_ = hpLpR_ = 0.0f;
    lpL_ = lpR_ = 0.0f;
    sag_ = 0.0f;
    detEnv_ = 0.0f;
    detSlowV_ = 0.0f;
    peakHoldV_ = 0.0f;
    peakFastV_ = 0.0f;
    susW_ = 0.0f;
    lastPreL_ = lastPreR_ = 0.0f;
    prePeak_ = 0.0f;
    grSingle_ = grDual1_ = grDual2_ = 0.0f;
    grSelFast_ = releaseFast_; grSelBlend_ = 1.0f; grSelFrom_ = 0.0f;
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

    // PHASE 3: the path works in VOLTS between the two
    // terminal crossings (in: x * kVoltsPerFs; out: / the
    // same at the return). Every stage limit is a soft
    // shoulder in volts, far above the levels the settled
    // loop allows (DESIGN.md §3) — no hard clip exists
    // anywhere in the engine.
    // 1. Input transformer (volts).
    float y = ironIn.process(x * kVoltsPerFs);
    // 2. Drive + the vari-mu stage's gain (the 6SK7 pair
    //    under the shared control voltage), then the pair's
    //    plate swing — a soft shoulder at 55 V, asymptote
    //    105 V (+21.6 dB over the terminal reference).
    y = y * driveLin * grLin;
    y = softClipV(y, 55.0f, 105.0f);
    //    Residual push-pull imbalance, growing with depth
    //    (the hardware truth the BAL trim pots exist to
    //    null; kept at the musical end of "ragged").
    const float depth = clampf(grDb_ / 30.0f, 0.0f, 1.0f);
    y += 0.02f * depth * (y * y / (1.0f + (y / 30.0f) * (y / 30.0f))) / kVoltsPerFs;
    // 3. Interstage transformer.
    y = ironMid.process(y);
    // 4. 6J7 driver (swing scale 60 V).
    y = tubeSatV(y, 0.55f, 0.05f, 60.0f);
    // 5. 6V6 push-pull output (swing scale 48 V; asymptote
    //    ~68.6 V ≈ +17.9 dB over the terminal reference,
    //    the +30 dBm-class swing), B+ sag as shipped
    //    (charge ~25 ms / recover ~240 ms; driven in
    //    processCore from the output level, and because
    //    the detector reads the sagged output the loop
    //    feels it too).
    y = tubeSatV(y, 0.7f * (1.0f + 0.30f * sag_), 0.015f, 48.0f);
    y *= (1.0f - 0.05f * sag_);
    // 6. Output transformer (the fat one).
    y = ironOut.process(y);
    // 7. Response shaping: the family's ±2 dB / 30 Hz–15 kHz
    //    envelope as a gentle LF droop (~16 Hz) + HF droop
    //    (~19 kHz). Also strips any DC the imbalance term
    //    made. (The poles are linear, so running them in
    //    volts and converting at the return is identical
    //    to the shipped normalised placement.)
    hpLp += (y - hpLp) * tauCoef(0.00995f, fs_);
    y -= hpLp;
    lp += (y - lp) * tauCoef(0.00000838f, fs_);
    return lp / kVoltsPerFs;
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
    // header. FULL2: module mode (applyMakeup_ false) skips
    // it with the rest of the make-up application — the
    // module output is the raw post-transformer signal,
    // and the ceiling discipline lives at the Chain's bus,
    // the final output, where no decimation follows it.
    if (applyMakeup_) {
        const float ipk = std::max(std::fabs(accL), std::fabs(accR));
        if (ipk > kBa6aMakeupPeakCeil) {
            const float g = kBa6aMakeupPeakCeil / ipk;
            accL *= g; accR *= g;
        }
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
    lawGrowth_ += (lawGrowthT_ - lawGrowth_) * lawCoef_;
    lawGrowthKnee_ += (lawGrowthKneeT_ - lawGrowthKnee_) * lawCoef_;
    lawSlowSlope_ += (lawSlowSlopeT_ - lawSlowSlope_) * lawCoef_;
    lawSlowToe_   += (lawSlowToeT_   - lawSlowToe_)   * lawCoef_;
    lawSlowSense_ += (lawSlowSenseT_ - lawSlowSense_) * lawCoef_;
    lawSlowGrowth_ += (lawSlowGrowthT_ - lawSlowGrowth_) * lawCoef_;
    lawSlowGrowthKnee_ += (lawSlowGrowthKneeT_ - lawSlowGrowthKnee_) * lawCoef_;
    lawSlowGrowth2_ += (lawSlowGrowth2T_ - lawSlowGrowth2_) * lawCoef_;
    lawSlowGrowthKnee2_ += (lawSlowGrowthKnee2T_ - lawSlowGrowthKnee2_) * lawCoef_;
    lawSlowRef_   += (lawSlowRefT_   - lawSlowRef_)   * lawCoef_;

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
    // PHASE 3: the sense is carried in VOLTS (the tap
    // reads the same post-transformer point; lastPre is
    // normalised, so x kVoltsPerFs at the tap). Nothing
    // bounds this signal inside the working range — the
    // detector's authority is proportional across the
    // whole drive range, which is the design's point.
    const float peakV = std::max(std::fabs(lastPreL_), std::fabs(lastPreR_))
                      * kVoltsPerFs;
    const float peak = lawSense_ * peakV;
    const bool detRising = peak > detEnv_;
    float detTau;
    if (detRising) {
        if (attackFast_) {
            detTau = kBa6aDetFastTauSec;
        } else {
            // SLOW shaped tau: the demand is read in the
            // fixed COMP-sense domain (DESIGN.md §5), as
            // the shipped design reads its fixed law in
            // both modes — the seat's dynamics never
            // change with the mode.
            const float peakC = kBa6aCompSense * peakV;
            const float envC = detEnv_ * (kBa6aCompSense / lawSense_);
            detTau = ba6aDetSlowTau(peakC, envC);
        }
    } else detTau = kBa6aDetDecayTauSec;
    // AB2: the rising charge takes its seat's scale (FAST
    // or the SLOW shaped tau); the decay keeps its own
    // held scale (release-side infrastructure).
    // TEST10: with the FAST release seat selected, the
    // decay takes an additional x0.08 — the seat-gated
    // control-loop discharge of the release re-voice
    // (see the TEST10 note at the release constants in
    // the header). The SLOW seat's detector is untouched.
    const float detScale = detRising ? (attackFast_ ? kBa6aAttFastScale
                                                    : kBa6aAttSlowScale)
                                     : kBa6aDetDecayScale * (releaseFast_ ? 0.08f : 1.0f);
    detEnv_ += (peak - detEnv_) * tauCoef(detTau * detScale, fs_);

    // DUAL-SENSE (Phase 3b): the sustained sense -- a slow
    // follower of the same rectified tap, in RAW volts. It
    // reads what the signal has BEEN, where detEnv_ reads
    // what it IS.
    {
        // Gap dump: a ~30 ms max-hold of the tap. On any
        // sustained tone the hold rides at the tone peak
        // (~1.47x the slow sense, safe margin down to
        // 20 Hz); when the signal truly stops it falls
        // below 0.35x the slow sense in ~45 ms, and the
        // sustained memory discharges quickly so the slow
        // demand exits with the signal instead of tailing
        // the release networks.
        // TEST10: the FAST release seat accelerates the
        // dump (hold 6 ms, discharge 12 ms) as part of
        // its control-loop discharge; the SLOW seat keeps
        // the stock 30 ms / 60 ms figures.
        const float phDecay = tauCoef(releaseFast_ ? 0.006f : 0.030f, fs_);
        peakHoldV_ = peakV > peakHoldV_ ? peakV
                       : peakHoldV_ + (0.0f - peakHoldV_) * phDecay;
        // TEST13 Part A: the seat-independent fast peak of
        // the same raw tap (fixed taus — NOT the attack
        // seat's shaped detector): the sustainedness
        // blend's fast reference below.
        peakFastV_ += (peakV - peakFastV_) * tauCoef(
            peakV > peakFastV_ ? 0.005f : 0.060f, fs_);
        const float dnTau = (peakV <= detSlowV_
                             && peakHoldV_ < 0.35f * detSlowV_)
                                ? (releaseFast_ ? 0.012f : 0.06f) : kBa6aSlowSenseDnTauSec;
        detSlowV_ += (peakV - detSlowV_) * tauCoef(
            peakV > detSlowV_ ? kBa6aSlowSenseUpTauSec : dnTau, fs_);
    }

    // TEST8: the per-mode / per-seat reference compensation
    // (kBa6aRef* constants; DIAGNOSIS §D's verified combined
    // set, in the probes' exact association). The seat is
    // the ATTACK seat, as probed.
    const float threshV = kBa6aThreshVolts * dbToLinear(threshTrimDb_)
        * ((compMode_ ? kBa6aRefCompModeFactor : kBa6aRefLimitModeFactor)
           * (attackFast_ ? 1.0f
                          : (compMode_ ? 1.0f : kBa6aRefLimitSlowSeatFactor)));
    const float targetFast = ba6aVariMuDb(detEnv_, lawSlope_, lawToe_,
                                      threshV * lawSense_,
                                      lawGrowth_, lawGrowthKnee_,
                                      compMode_ ? kBa6aCompKneeDb : kBa6aLimitKneeDb,
                                      compMode_ ? kBa6aCompKneeCtrDb : kBa6aLimitKneeCtrDb,
                                      compMode_ ? kBa6aCompKneeSlopeDb : kBa6aLimitKneeSlopeDb);
    const float targetSlow = ba6aVariMuDb2(detSlowV_ * lawSlowSense_,
                                      lawSlowSlope_, lawSlowToe_,
                                      threshV * lawSlowSense_ * lawSlowRef_,
                                      lawSlowGrowth_, lawSlowGrowthKnee_,
                                      lawSlowGrowth2_, lawSlowGrowthKnee2_,
                                      compMode_ ? kBa6aCompSlowKneeDb : kBa6aLimitSlowKneeDb,
                                      compMode_ ? kBa6aCompSlowKneeCtrDb : kBa6aLimitSlowKneeCtrDb,
                                      compMode_ ? kBa6aCompKneeSlopeDb : kBa6aLimitKneeSlopeDb);
    // Sustainedness blend: rho = slow/fast envelope ratio
    // (both senses read the same tap). w ducks fast (a hit
    // is programme, the fast law keeps it), recovers slower
    // (sustained material earns the slow law's static
    // curve).
    // TEST13 Part A: through TEST12 the fast reference was
    // detEnv_ — the ATTACK-SHAPED detector — so the blend
    // (and therefore the release's demand composition)
    // inherited the attack seat: with FAST attack detEnv
    // rode the phrase peaks, rho sat near 0 and a release
    // began from the fast law; with SLOW attack rho and
    // detSlowV both sat hotter and the same release seat
    // exited through the slow law — Nathan heard the
    // release change speed with the attack seat (TEST12
    // session). The reference is now peakFastV_, a fixed-
    // tau peak follower of the raw tap: identical for both
    // attack seats (up to the feedback tap's own physics),
    // so a release is composed the same way regardless of
    // how the phrase was attacked. On sustained tones the
    // follower reads the same peak detEnv did, so the
    // settled blend (w -> 1) and the statics are unchanged.
    {
        const float peakRaw = peakFastV_;
        const float rho = detSlowV_ / (peakRaw > 0.05f ? peakRaw : 0.05f);
        float wT = (rho - kBa6aSusRho0) / (kBa6aSusRho1 - kBa6aSusRho0);
        wT = wT < 0.0f ? 0.0f : (wT > 1.0f ? 1.0f : wT);
        susW_ += (wT - susW_) * tauCoef(wT < susW_ ? kBa6aSusDnTauSec
                                                  : kBa6aSusUpTauSec, fs_);
    }
    const float targetDemand = targetFast + susW_ * (targetSlow - targetFast);
    // TEST9: deep-GR softening (ba6aDeepSoften; header
    // note) — the demanded depth is stretched above 5 dB
    // before the control networks see it. Identity at and
    // below 5 dB, so onsets and shallow ballistics are the
    // TEST8 calibration exactly; the networks' state (and
    // therefore the meter) is the applied GR.
    const float target = ba6aDeepSoften(targetDemand);

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
    // PHASE 3 (select blend): the shipped design runs both
    // recovery networks continuously so a RELEASE flip never
    // jumps -- true while both networks sit on a common
    // target. The voltage engine has a true zero-target
    // region: mid-release from a deep excursion the networks
    // legitimately sit dBs apart (measured 6.5 dB on the A3
    // bench history), and a hard select would step the gain.
    // The select therefore blends over ~15 ms from the
    // pre-flip value; the networks, attack path and all
    // ballistics are untouched.
    {
        const float sel = releaseFast_ ? grSingle_ : grDual1_ + grDual2_;
        if (releaseFast_ != grSelFast_) {
            grSelFast_ = releaseFast_;
            grSelFrom_ = grDb_;
            grSelBlend_ = 0.0f;
        }
        if (grSelBlend_ < 1.0f) {
            grSelBlend_ = std::min(1.0f, grSelBlend_ + 1.0f / (0.015f * (float)fs_));
            const float t = grSelBlend_;
            const float sm = t * t * (3.0f - 2.0f * t);
            grDb_ = grSelFrom_ + (sel - grSelFrom_) * sm;
        } else grDb_ = sel;
    }

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
    // the discipline an operator applies by ear. FULL2: this
    // computation runs in BOTH modes (the readout and the
    // direct-limiter behaviour are unchanged); only the
    // APPLICATION below is gated — in module mode the Chain
    // re-applies this same law at the output bus. ──
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
    // FULL2 module mode (applyMakeup_ false, set by the
    // Chain): the module's output IS the pre-make-up
    // signal — the make-up law above has run (its states
    // and readout stay live) but nothing is applied here;
    // the Chain's bus applies the law to the summed feeds.
    if (!applyMakeup_) {
        outL = preL;
        outR = preR;
        return;
    }
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
    // v1.4.1: park the state machine CONSISTENTLY with the
    // surviving engagement target. The pre-fix code forced
    // off here while target_ survived true — and
    // setEnabled() early-returns on "no change", so a host
    // re-prepare (which re-applies the same target) could
    // never re-arm the machine: the fade stayed pinned at
    // 0 and the insert returned its input forever (the
    // Luna power-cycle parking defect). A reset whose
    // target is ON now re-arms exactly like a fresh
    // engage — fade-in from 0 over the same 30 ms. Fresh
    // instances are unaffected: target_ is false until
    // the first setEnabled.
    state_ = target_ ? State::fadingIn : State::off;
    fadeGain_ = 0.0f;
    delay1Ms_ = lastDelay1Ms_ = kAdtBaseMs + kAdtDeck1StaggerMs;
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
        return kAdtBaseMs + kAdtDeck1StaggerMs   // FULL2 stagger (header)
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
    // TEST8: store the wet sum's exact addends for the
    // Chain's MOD MIX law (unmodulated SRC / modulated
    // deck term, pre-fade).
    lastWetSrcL_ = srcL;            lastWetSrcR_ = srcR;
    lastWetDeckL_ = kAdtDeckGain * deck2;
    lastWetDeckR_ = kAdtDeckGain * deck1;

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
    // v1.4.1: park consistently with the surviving target,
    // exactly as AdtChain::reset — a target-ON reset re-arms
    // into the fade-in instead of parking off forever (the
    // host re-prepare defect: setEnabled cannot re-arm a
    // machine whose target never changed).
    state_ = target_ ? State::fadingIn : State::off;
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

void Phaser90::processBoth(float inL, float inR, float& outL, float& outR,
                           float& effL, float& effR)
{
    processImpl(inL, inR, outL, outR, effL, effR);
}

// ═════════════════════════════════════════════════════════════
// Chain
// ═════════════════════════════════════════════════════════════

void Chain::prepare(double sampleRate)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    // FULL2: the chain's limiter is the MODULE — its own
    // make-up application is disabled from here on.
    // (Through TEST7 the bus applied the make-up law to
    // the summed channels; TEST8 removed the law — the
    // bus now applies only the fixed staging trim and
    // the soft-ceiling knee.)
    limiter_.setApplyMakeup(false);
    // v1.4.1: SAME-RATE RE-PREPARE PRESERVATION. A host can
    // re-call prepareToPlay on a live instance without
    // reloading it (Luna's top-bar power cycle does, via
    // the JUCE AU wrapper). Rebuilding under a live stream
    // zeroed every ring and — through the inserts' reset —
    // parked the MOD section (see AdtChain::reset). At a
    // repeated rate there is nothing to rebuild: every
    // ring, phase, fade and tracker keeps its live state
    // and the re-call is an inaudible non-event. The first
    // prepare, and any genuine rate change, still build
    // from scratch below.
    if (prepared_ && rate == preparedFs_)
        return;
    fs_ = rate;
    limiter_.prepare(fs_);
    adt_.prepare(fs_);
    phaser_.prepare(fs_);
    // FULL2: the COMP channel's alignment ring — sized
    // for the ADT's 20 ms offset at this rate plus margin,
    // power of two.
    int need = (int)std::ceil(0.025 * fs_) + 64;
    int sz = 256;
    while (sz < need) sz <<= 1;
    compRingL_.assign((size_t)sz, 0.0f);
    compRingR_.assign((size_t)sz, 0.0f);
    // TEST8: the dry arm's rings, same sizing (the deepest
    // dry read — oversampling delay + ADT offset — is
    // shallower than the sizing's headroom by construction:
    // 32 + 20 ms < 25 ms + 64).
    dryRingL_.assign((size_t)sz, 0.0f);
    dryRingR_.assign((size_t)sz, 0.0f);
    compMask_ = sz - 1;
    reset();
    preparedFs_ = fs_;
    prepared_ = true;
}

void Chain::reset()
{
    limiter_.reset();
    adt_.reset();
    phaser_.reset();
    modBlend_ = modPhaser_ ? 1.0f : 0.0f;
    // FULL2: both pot slews snap to their targets and
    // the engagement gate snaps to the held MOD target —
    // a fresh instance is at its panel state from the
    // first sample, no load glide (the retired whole-
    // chain mix's reset discipline). The inserts' own state machines park
    // through their reset() consistently with their
    // surviving targets (v1.4.1: off when the target is
    // out, re-armed into the fade-in when it is in) — in
    // the K7 harness protocol the targets are still out
    // at reset time, so they park off there exactly as
    // before.
    compMixCur_ = compMixTarget_;
    modMixCur_ = modMixTarget_;
    modGate_ = adt_.enabled() ? 1.0f : 0.0f;
    std::fill(compRingL_.begin(), compRingL_.end(), 0.0f);
    std::fill(compRingR_.begin(), compRingR_.end(), 0.0f);
    std::fill(dryRingL_.begin(), dryRingL_.end(), 0.0f);
    std::fill(dryRingR_.begin(), dryRingR_.end(), 0.0f);
    compWrite_ = 0;
    modDelayA_ = modDelayB_ = modExtraTarget();
    modTapFade_ = 1.0f;
    // TEST8: no make-up law to start cold — the bus's net
    // staging gain is the fixed trim from the first sample.
    busMakeupDb_ = kBusStagingTrimDb;
    lastFeedL_ = lastFeedR_ = 0.0f;
}

void Chain::process(float inL, float inR, float& outL, float& outR)
{
    // ── FULL2 bus architecture (Nathan; see the class
    // note in the header). The limiter section is always
    // engaged (v53: the old in/out bypass became the
    // COMP/LIMIT ratio mode switch, inside the limiter's
    // gain computer) and runs in MODULE mode: its output
    // is the raw post-transformer signal, pre-make-up. ──
    float modL, modR;
    limiter_.process(inL, inR, modL, modR);

    // TEST8: the two pot slews run FIRST (the retired
    // whole-chain mix's law, as in AB3: one-pole 0.002,
    // snap 2e-5) — every consumer below (the MOD blend
    // laws, the COMP crossfade, the dry arm's complement)
    // reads the same per-sample values. Through TEST7 the
    // slews sat at the bus sum and the pots were bare
    // gains; with static pots (every pinned render) the
    // values are identical either way.
    compMixCur_ += (compMixTarget_ - compMixCur_) * 0.002f;
    if (std::fabs(compMixTarget_ - compMixCur_) < 2.0e-5f)
        compMixCur_ = compMixTarget_;
    modMixCur_ += (modMixTarget_ - modMixCur_) * 0.002f;
    if (std::fabs(modMixTarget_ - modMixCur_) < 2.0e-5f)
        modMixCur_ = modMixTarget_;

    // ── TEST13 Part D: SERIAL BLEND SEMANTICS (Nathan, on
    // TEST12). Through TEST12 the bus ADDED the wet arm on
    // top of the comp blend and the inserts were fed the
    // raw module output unconditionally — so COMP MIX at
    // dry + MOD MIX 100 was never fully wet, and the MOD
    // section processed the compressor's raw output (hot,
    // pre-trim) even when the listener's blend was the
    // clean driven dry. Now: the comp blend is formed
    // FIRST (dry <-> comp, as built in TEST8), the inserts
    // are fed THAT blend, and MOD MIX crossfades the blend
    // against its modulated version. ──
    // The rings are written first: the send is tapped at
    // the module's own timeline (the oversampling delay
    // only); the bus's alignment offset below delays the
    // through path by the same amount the insert adds, so
    // the ADT's internal latency lands exactly where it
    // did — the reported latency is unchanged.
    compRingL_[(size_t)compWrite_] = modL;
    compRingR_[(size_t)compWrite_] = modR;
    dryRingL_[(size_t)compWrite_] = inL;
    dryRingR_[(size_t)compWrite_] = inR;
    // ── TEST16: THE SEND IS THE DRIVEN SIGNAL, IN
    // PARALLEL (Nathan's confirmed semantics, 2026-10-26,
    // superseding the TEST13 serial feed and the TEST15
    // staged dry). The insert feed is the clean post-drive
    // gain copy — the input sample at the module's own
    // timeline (the oversampling delay) x the limiter's
    // smoothed drive law, the same level the compressor
    // itself receives — on its own path, BYPASSING the
    // compressor entirely: independent of COMP MIX, of
    // the blend and of the applied GR. Driving INPUT
    // drives the effect, exactly as it drives the
    // compressor. No shoulders, no staging law, no GR
    // reference: a gain copy cannot compress and cannot
    // breathe. (TEST15's feed was the comp blend, whose
    // dry share was the staged driven dry; both are
    // retired here — the blend no longer feeds anything.)
    // TEST17 (Nathan, 2026-10-27): the copy carries ONE
    // added constant — the -3 dB MOD feed pad
    // (kModFeedPadLin, header): the drive was hitting
    // the MOD section with a little too much force. The
    // pad sits at the tap, before both machines; nothing
    // downstream is re-gained. ──
    const float driveDbNow = limiter_.driveDb();
    const float driveLinNow = dbToLinear(driveDbNow);
    const float feedGain = driveLinNow * kModFeedPadLin;
    float feedL, feedR;
    {
        const int osD = limiter_.osDelaySamples();
        feedL = readDryRing(dryRingL_, osD) * feedGain;
        feedR = readDryRing(dryRingR_, osD) * feedGain;
    }
    lastFeedL_ = feedL;   // the send tap: the driven copy
    lastFeedR_ = feedR;   // itself, at every pot position

    // ── The MOD send: both machines run continuously on
    // the driven copy (the send tap above). The flavour
    // blend selects between the two WET signals.
    // TEST13 took the ADT's full output and the Phase
    // 90's BARE CASCADE, reasoning that the serial
    // crossfade trades full-strength signals so no
    // staging loss exists by construction. Nathan's
    // TEST13 session disproved the conclusion for both
    // flavours ("the phase is not even audible now…
    // same with the ADT, very difficult to pick out
    // even with the mix all the way up"), and the
    // measurements say why (REPORT-TEST14):
    //  - the bare cascade's magnitude response is
    //    identically 1 — it carries no notches, only
    //    phase rotation; its band energy measured
    //    identical to the through signal in every
    //    0.5 s chunk. The effect was ABSENT, not
    //    quiet. The notches live in the interference
    //    between the cascade and the dry path.
    //  - the ADT's SRC coincides sample-for-sample
    //    with the through signal in the serial bus,
    //    so the whole on/off difference was the deck
    //    term at ~-12 dB rel — against TEST11's
    //    additive presentation of the full wet at
    //    ~0 dB rel.
    // TEST14 therefore forms the wet legs explicitly:
    //  - ADT: SRC + kAdtWetDeckBoost x deck, from the
    //    machine's exposed components (the machine's
    //    own engage fade is dropped here; the Chain's
    //    engagement gate below ramps the same 30 ms).
    //  - Phase 90: the pedal's CLASSIC output
    //    (0.5 . (in + cascade)) — the full pedal, as
    //    through TEST12 — under the fixed wet-leg
    //    staging trim kPhaserWetTrimLin.
    // TEST16 keeps both formations (and the flavour
    // crossfade below) exactly; what changed is the
    // feed (the driven copy, above) and the bus algebra
    // (the wet now ADDS to the comp blend under MOD
    // MIX as a level — see the bus block below). ──
    float aL, aR, pL, pR, eL, eR;
    adt_.process(feedL, feedR, aL, aR);
    aL = adt_.lastWetSrcL() + kAdtWetDeckBoost * adt_.lastWetDeckL();
    aR = adt_.lastWetSrcR() + kAdtWetDeckBoost * adt_.lastWetDeckR();
    phaser_.processBoth(feedL, feedR, pL, pR, eL, eR);
    eL = kPhaserWetTrimLin * pL;
    eR = kPhaserWetTrimLin * pR;
    // TEST16: the wet-leg staging law (header: the
    // static, knob-referenced presentation of each
    // flavour's formed wet, fitted so the wet-solo
    // level tracks the compressed path's across the
    // knob — the driven feed's level parts from the
    // module's as drive rises). Applied per flavour
    // BEFORE the flavour crossfade, so a mid-blend
    // trades two correctly-presented legs.
    {
        const float knob01Now = limiter_.knob();
        const float aSt = ba6aWetStagingLin(kAdtWetStagingDb, knob01Now);
        const float pSt = ba6aWetStagingLin(kPhaserWetStagingDb, knob01Now);
        aL *= aSt; aR *= aSt;
        eL *= pSt; eR *= pSt;
    }
    const float target = modPhaser_ ? 1.0f : 0.0f;
    if (modBlend_ != target) {
        const float step = 1.0f / (kAdtFadeSec * (float)fs_);
        modBlend_ = target > modBlend_
            ? std::min(target, modBlend_ + step)
            : std::max(target, modBlend_ - step);
    }
    float wetL, wetR;
    if (modBlend_ <= 0.0f) {
        wetL = aL; wetR = aR;
    } else if (modBlend_ >= 1.0f) {
        wetL = eL; wetR = eR;
    } else {
        wetL = aL + (eL - aL) * modBlend_;
        wetR = aR + (eR - aR) * modBlend_;
    }
    // The engagement gate: linear over the inserts' own
    // 30 ms span, exactly 0 settled-out, exactly 1
    // settled-in. (On a fresh engage the gate and the
    // machines' internal fades ramp together; on disengage
    // the gate's fall covers the machines' fade-out tail.)
    {
        const float gateTarget = adt_.enabled() ? 1.0f : 0.0f;
        const float step = 1.0f / (kAdtFadeSec * (float)fs_);
        if (modGate_ < gateTarget)
            modGate_ = std::min(gateTarget, modGate_ + step);
        else if (modGate_ > gateTarget)
            modGate_ = std::max(gateTarget, modGate_ - step);
    }

    // ── The COMP channel: the module output, delayed at
    // the bus by the engaged insert's own latency so both
    // channels arrive together (the offset target tracks
    // MOD toggle and A/P flavour; a change starts the
    // v44-style 30 ms offset crossfade between the old and
    // new reads — the machinery's third home, its purpose
    // unchanged). (TEST13: the rings were written at the
    // top of this function, where the send is tapped; the
    // reads below are unchanged.) ──
    // (TEST13: the dry ring was written in lockstep at the
    // top of this function, with the comp ring.)
    const int extra = modExtraTarget();
    if (extra != modDelayB_) {
        modDelayA_ = modDelayB_;
        modDelayB_ = extra;
        modTapFade_ = 0.0f;
    }
    // The offset crossfade's phase for THIS sample, saved
    // before the comp reads advance it: the dry arm below
    // reads through the identical A/B law.
    const float tapFade = modTapFade_;
    float compL, compR;
    if (modTapFade_ >= 1.0f && modDelayB_ == 0) {
        compL = modL; compR = modR;   // settled, no offset
    } else {
        const float bL = readCompRing(compRingL_, modDelayB_);
        const float bR = readCompRing(compRingR_, modDelayB_);
        if (modTapFade_ >= 1.0f) {
            compL = bL; compR = bR;
        } else {
            const float aLagL = readCompRing(compRingL_, modDelayA_);
            const float aLagR = readCompRing(compRingR_, modDelayA_);
            compL = aLagL + (bL - aLagL) * modTapFade_;
            compR = aLagR + (bR - aLagR) * modTapFade_;
            modTapFade_ = std::min(1.0f, modTapFade_ + 1.0f / (kAdtFadeSec * (float)fs_));
        }
    }
    // ── TEST8: the DRY ARM. The chain input, read at the
    // comp arm's TOTAL latency — the limiter's
    // oversampling delay (32 host samples at any rate)
    // plus the comp channel's bus offset, through the
    // same A/B offset crossfade — so the dry sample and
    // the comp sample it blends against are the SAME
    // input sample. The reported latency does
    // not change: the dry arm's delay IS the comp path's
    // latency, in every MOD state.
    // TEST16 (Nathan's confirmed semantics, 2026-10-26):
    // the dry arm is the arriving signal ITSELF — the
    // raw ring read, no drive law, no shoulder staging,
    // no trim. TEST8 staged it by the drive law, TEST15
    // by the module's shoulders (ba6aDryStaging, now
    // retired with both): every staged form made COMP
    // MIX 0 something other than what arrived. Now the
    // blend's zero end is the plugin's input at input
    // level, delay/phase-aligned — "as if the plugin
    // isn't even there" — and it cannot distort, pump
    // or breathe, because nothing is done to it at all.
    // The A/B crossfade blends two raw reads exactly as
    // it crossfades the comp channel. ──
    float dryArmL, dryArmR;
    {
        const int osD = limiter_.osDelaySamples();
        if (tapFade >= 1.0f && modDelayB_ == 0) {
            dryArmL = readDryRing(dryRingL_, osD);
            dryArmR = readDryRing(dryRingR_, osD);
        } else {
            const float bL = readDryRing(dryRingL_, osD + modDelayB_);
            const float bR = readDryRing(dryRingR_, osD + modDelayB_);
            if (tapFade >= 1.0f) {
                dryArmL = bL;
                dryArmR = bR;
            } else {
                const float aL = readDryRing(dryRingL_, osD + modDelayA_);
                const float aR = readDryRing(dryRingR_, osD + modDelayA_);
                dryArmL = aL + (bL - aL) * tapFade;
                dryArmR = aR + (bR - aR) * tapFade;
            }
        }
    }
    compWrite_ = (compWrite_ + 1) & compMask_;

    // ── The bus (TEST16 — Nathan's confirmed semantics,
    // 2026-10-26, superseding the TEST13 serial crossfade
    // and every earlier bus algebra). The two pots now
    // operate DIFFERENTLY, by his definition:
    //  - COMP MIX is a BLEND: the COMP channel crossfaded
    //    against the dry arm (the arriving input, aligned)
    //    with complementary weights on the same slewed
    //    value. 0 = the input itself; 100 = the fully
    //    compressed path.
    //  - MOD MIX is a LEVEL: the wet leg (the inserts on
    //    the driven copy) ADDS to the blend at this gain,
    //    under the engagement gate. 0 = silence from the
    //    section; 100 = its full output. It is NOT a
    //    crossfade — nothing is traded away as it rises.
    // The processed sum (comp share + wet share) passes
    // the bus's output staging (the fixed trim and the
    // soft ceiling below — the TEST15 output stage,
    // unchanged law) and the dry share is added AFTER
    // it, untouched: the arriving signal is never
    // trimmed or ceilinged by the bus that carries it.
    // Endpoints, exact in the settled state: 0/0 = the
    // aligned input itself; 100/0 = the comp path alone,
    // bit-identical to TEST15's; 0/100 = the aligned
    // input + the full wet(driven); 100/100 = the comp
    // path + the full wet combined. ──
    const float wMod = modGate_ * modMixCur_;
    const float procL = compMixCur_ * compL + wMod * wetL;
    const float procR = compMixCur_ * compR + wMod * wetR;

    // ── TEST8: NO AUTO MAKE-UP AT THE BUS (Nathan:
    // "act like a normal compressor"). The FULL2 bus law
    // (0.92 x avgGR + 7 x knob^2, peak-referenced park
    // cap, riding tracker) is REMOVED — the diagnosis
    // proved the detector never read it (the 3.5 V
    // calibration and every GR trace survive untouched)
    // and that its net action on this engine was
    // attenuation against a hot raw staging. In its
    // place, where the law sat: the FIXED post-bus
    // staging trim (kBusStagingTrimDb, -15.6 dB
    // measured), which lands insertion at INPUT 5 flat
    // on Nathan's vocal. Level recovery is the OUTPUT
    // pots' job.
    // The telemetry readout reports the trim. ──
    busMakeupDb_ = kBusStagingTrimDb;
    // Soft ceiling (STAGING-FIX CANDIDATE, kept): the
    // shipped per-sample instant clamp at 1.600 is
    // replaced by a LINKED soft knee whose onset is the
    // park ceiling. Below it the pair's gain is exactly
    // 1; above it the pair's peak bends along a tanh
    // toward the knee asymptote (park + span = 0.981),
    // one gain for both channels taken from the pair's
    // peak — overshoot arrives as progressive squash,
    // never a flat top, and the 1.600 absolute bound is
    // never reached. A ceiling, not make-up: it only
    // ever bends peaks DOWN toward the park. TEST16: the
    // ceiling reads the PROCESSED sum (comp + wet shares)
    // — the dry share of the blend is added after it and
    // is never bent (it is the arriving signal itself).
    float oL = procL * kBusStagingTrimLin, oR = procR * kBusStagingTrimLin;
    const float pairPk = std::max(std::fabs(oL), std::fabs(oR));
    if (pairPk > kBusParkCeilLin) {
        const float soft = kBusParkCeilLin
            + kBusKneeSpan * (float)std::tanh(
                  (double)(pairPk - kBusParkCeilLin) / (double)kBusKneeSpan);
        const float g = soft / pairPk;
        oL *= g; oR *= g;
    }
    outL = oL + (1.0f - compMixCur_) * dryArmL;
    outR = oR + (1.0f - compMixCur_) * dryArmR;
}

} // namespace osc72
