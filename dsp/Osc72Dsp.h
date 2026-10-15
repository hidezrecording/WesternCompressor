#pragma once
// ─────────────────────────────────────────────────────────────────────
// Osc72Dsp — framework-free C++17 DSP core for OSC-72.
//
// OSC-72 is a one-knob stereo limiter built as a reconstruction of the
// documented session trick behind a well-known 1972 Record Plant hit:
// a prototype feed-forward FET limiter, built by a British electronics
// engineer, was used IN PARALLEL with the finished stereo mix — the
// mix was split to extra tracks, run through the limiter until it was
// totally crushed, and the crushed tracks were raised underneath the
// originals until they EQUALLED the originals in level.
//
// WHAT IS VERIFIED FROM RESEARCH (see README.md for the source map):
//  * The historical unit was a prototype limiter; feed-forward FET
//    gain reduction is the documented design family behaviour.
//  * Positive and negative peaks are controlled separately, and the
//    stereo image is held under heavy gain reduction (linked GR).
//  * There is NO ratio control and no fixed ratio: gain reduction is
//    dynamic — the effective ratio grows continuously (described as
//    exponential) as the signal goes further over threshold.
//  * The parallel routing and the "crushed = dry" blend endpoint are
//    from the primary witness account of the session.
//  * Carmen (the artist) valued how the low end surged back into gaps
//    in the arrangement — i.e. a fast-musical release, not a slow
//    programme-dependent one.
//
// WHAT IS RECONSTRUCTION / INFERENCE (never published anywhere):
//  * Every exact time constant. No attack/release values in ms were
//    ever published for the prototype or its production relatives
//    (the manufacturer publishes switch positions, not times).
//  * The threshold/makeup/blend knob law below.
//  * The gentle Class-A-style saturation curve (kept deliberately
//    small: density/weight, no hard flat-top at nominal levels).
//
// WHAT IS DELIBERATELY NOT MODELLED:
//  * 456HD tape simulation (a modern reissue addition — anachronism).
//  * Transformers / 48 V supply as "features" (the vintage unit's I/O
//    arrangement is itself uncertain; see report §2.1).
// ─────────────────────────────────────────────────────────────────────
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace osc72 {

inline float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }
inline float linearToDb(float lin) {
    return 20.0f * std::log10(lin > 1.0e-12f ? lin : 1.0e-12f);
}
// One-pole smoothing coefficient for a time constant in SECONDS.
inline float tauCoef(float tauSec, double fs) {
    return 1.0f - (float)std::exp(-1.0 / ((double)tauSec * fs));
}
inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ─────────────────────────────────────────────────────────────────────
// Fixed internal time constants — RECONSTRUCTION (see header),
// RETUNED in audio v2, then RE-TUNED AGAIN in audio v3 (Nathan's
// ear verdicts on the shipped 1.0.2: the release was "way too
// slow — it needs to be like five times quicker at least to
// pump", the gain action on continuous material was "lost", and
// he asked for auto makeup that CRANKS with the push). v2
// measured release t63 of 68 ms (centre) / 78 ms (max); v3
// targets the low teens. The v3 ballistics:
//   * GR ATTACK stays slow-to-medium (20 ms, easing to 10 ms at
//     max) so transients punch through BEFORE the clamp — the
//     gain ballistics live in the GR smoother; the detector
//     itself still peak-catches at 50 µs (documented feed-
//     forward FET behaviour).
//   * GR RELEASE is ~8x faster than v2 at the pole: a 6 ms pole
//     (3.8 ms at max), so the gain surges back BETWEEN hits even
//     at mix density — pumping as a trajectory, not a posture.
//     Measured step-release t63 (the number includes bleeding
//     the detector envelope down through the over-range, which
//     is deeper at max): 19 ms centre / 27 ms max, vs v2's
//     68 / 78 ms.
//   * PROGRAMME-DEPENDENT RELEASE (v3, the vari-mu / Sta-Level
//     principle — see docs/audio-v3-diagnosis.md): a slow
//     "stress" memory of sustained GR stretches the release up
//     to ~1.9x while the box is being held down continuously,
//     so isolated hits recover at full speed but a passage that
//     is being slammed non-stop doesn't chatter.
//   * The DETECTOR HANG drops 12 ms -> 3 ms base, stretched x4
//     by a programme-PRESENCE state while anything is actually
//     playing (so the envelope cannot collapse inside an LF
//     cycle and ride the gain — the fast-release LF-distortion
//     mechanism) and collapsing the moment programme stops
//     (with a 2 ms true-silence cut), so perceived recovery
//     matches the release number instead of a hang owning it.
//   * The DETECTOR feed is high-passed (~75 Hz, detector only —
//     the audio path is untouched): sustained bass no longer
//     parks the gain computer. Measured on the diagnosis drum
//     loop, a continuous 55 Hz line alone held ~15 dB of GR
//     floor under v2, squeezing all hit movement into a ~9 dB
//     band on top of it. (The Distressor's detector HP filter
//     exists for exactly this; see the diagnosis doc.)
// See docs/audio-v2-retune.md (v2) and docs/audio-v3-diagnosis.md
// (v3) for the before/after measurements.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kAttackTauSec   = 0.020f;   // 20 ms GR attack (base)
inline constexpr float kReleaseTauSec  = 0.006f;   // 6 ms GR release base (v3: v2 was 50 ms)
inline constexpr float kDetectAttackTauSec  = 0.00005f; // 50 µs peak catch
inline constexpr float kDetectReleaseTauSec = 0.003f;   // 3 ms detector hang base (v3; presence-stretched x4 while programme plays)
inline constexpr float kMakeupTauSec   = 0.300f;   // slow average-GR makeup
inline constexpr float kDetectHpTauSec = 0.00212f; // ~75 Hz detector sidechain HPF
inline constexpr float kStressTauSec   = 1.5f;     // programme memory for the release law
inline constexpr float kStressStretch  = 0.9f;     // max release stretch at full stress

// ─────────────────────────────────────────────────────────────────────
// Knob law — RECONSTRUCTION, v9 (Nathan, 2026-10-14). The one knob
// gangs THREE things, all rising together:
//   1. SQUASH — the limiter below (Mayer/Raspberries action; the top
//      of the travel leans on the Shure Level-Loc's violent AGC:
//      deeper GR cap; audio v2: a FASTER, harder-pumping release
//      and a slower attack — see the time-constant block below).
//   2. TRANSIENT DESIGNER — before the limiter in the wet path,
//      cutting natural sustain (drying ambience) increasingly.
//   3. MIX — the knob IS the mix law: wet weight == knob value.
//        0    -> 100% dry, no squash, no TD, bit-exact.
//        0.5  -> CENTRE ("straight up"): exactly 50/50 dry vs the
//                super-squashed wet — the documented Raspberries
//                endpoint (crushed == dry) now sits at centre. The
//                squash AT CENTRE equals what the pre-v9 law reached
//                at 0.7 (same threshold), by construction.
//        1.0  -> 100% wet, squash beyond the Raspberries reference,
//                TD sustain cut at its deepest.
// squashU() maps the knob onto the pre-v9 law's travel (u = old
// knob position) for the first half, then extends past it (u to
// 1.25), so the centre inherits the old 0.7 threshold exactly.
// ─────────────────────────────────────────────────────────────────────
inline float squashU(float k) {
    k = clampf(k, 0.0f, 1.0f);
    if (k <= 0.5f) return 0.7f * (k * 2.0f);
    return 0.7f + 0.55f * ((k - 0.5f) * 2.0f);
}
// 0 below centre -> 1 at max: how far past the Raspberries
// reference the knob is (drives the Level-Loc lean, below).
inline float topFactorForKnob(float k) {
    k = clampf(k, 0.0f, 1.0f);
    return k <= 0.5f ? 0.0f : (k - 0.5f) * 2.0f;
}
inline float thresholdDbForKnob(float k) {
    // v2: slope 42 -> 55 — substantially deeper squash across the
    // whole travel (centre threshold ≈ −29.9 dBFS, was −21.4;
    // max ≈ −65.9 dBFS, was −48.9). See docs/audio-v2-retune.md.
    return 6.0f - 55.0f * std::pow(squashU(k), 1.2f);
}
inline float wetWeightForKnob(float k) {
    return clampf(k, 0.0f, 1.0f); // the knob IS the mix (v9 law)
}
// Level-Loc lean in the top half, v2 form: past centre everything
// gets MORE violent in the direction Nathan asked for on the 1.0.0
// build — the release SPEEDS UP (50 -> 25 ms: fast recovery is the
// pumping he wants; v1's stretch to ~340 ms was the opposite), the
// attack quickens a little as the depth grows (20 -> 10 ms — a
// full slow attack into 60 dB of GR would overshoot for tens of
// ms), the GR ceiling rises 50 -> 64 dB, the makeup cap rises so
// the deeper squash stays hot, and the Class-A saturator is
// driven a touch harder (the Comptortion's grit — its compression
// ran "to completely squashed", ~15:1 per period reviews).
inline float attackTauForKnob(float k) {
    return kAttackTauSec * (1.0f - 0.5f * topFactorForKnob(k));
}
inline float releaseTauForKnob(float k) {
    // v3: past centre the release is faster still (6 -> 3.8 ms);
    // the programme-stress stretch is applied on top of this in
    // the gain computer (see StereoLimiter::process).
    return kReleaseTauSec * (1.0f - 0.36f * topFactorForKnob(k));
}
inline float grCapForKnob(float k) {
    return 50.0f + 14.0f * topFactorForKnob(k);
}
inline float makeupCapForKnob(float k) {
    // v3: the cap must clear full average-GR compensation PLUS
    // the knob crank (below): at max that is ~64 + 8.
    return 44.0f + 30.0f * topFactorForKnob(k);
}
// AUTO MAKEUP, v3 form (Nathan, on 1.0.2: "add an auto makeup
// gain so that as you push the signal, the input starts cranking
// with the compressed signal"). Two terms:
//   1. FULL compensation of the slow-average GR (factor 1.0, was
//      0.8 in v1/v2): the wet path's average loudness is restored,
//      so what the knob adds is DENSITY at level — and because
//      the average can't follow hit-rate movement, the pumping
//      trajectory passes at full depth instead of being levelled
//      away. (Measured v2 failure: net gain on a drum loop sat
//      at −1.8 dB at centre and FELL to −7.4 dB at max — the
//      harder you pushed, the quieter the crush got.)
//   2. A knob-tied CRANK: extra makeup that exists purely
//      because the knob is up — +2 dB at the bottom of the
//      travel rising to +8 dB at max — so pushing the knob
//      audibly turns the crushed signal UP, as asked. (At knob
//      0 the wet weight is 0, so the crank can never disturb
//      the bit-exact dry guarantee.)
inline float makeupCrankDbForKnob(float k) {
    return 2.0f * clampf(k, 0.0f, 1.0f) + 6.0f * topFactorForKnob(k);
}
inline float satDriveForKnob(float k) {
    const float t = topFactorForKnob(k);
    return 1.0f + 0.9f * t * t;
}
// Transient-designer depth: 0 at knob 0 (off), deep at max.
inline float tdDepthForKnob(float k) {
    return std::pow(clampf(k, 0.0f, 1.0f), 1.3f);
}

// Dynamic-ratio gain reduction — VERIFIED PRINCIPLE, reconstructed
// curve. There is no fixed ratio: the fraction of the over-threshold
// excursion that is removed grows continuously towards ~100% as the
// excursion grows (gain reduction grows "exponentially" above
// threshold in the manufacturer's own description). Returns GR in dB
// (>= 0) for an over-threshold amount in dB (>= 0).
inline float dynamicGainReductionDb(float overDb, float capDb = 50.0f) {
    if (overDb <= 0.0f) return 0.0f;
    // v2: divisor 8 -> 6.5 — the removed fraction approaches 100%
    // sooner, so the curve bites harder at moderate overdrive.
    const float frac = 1.0f - std::exp(-overDb / 6.5f);
    float gr = overDb * frac;
    return gr > capDb ? capDb : gr;
}

// Gentle Class-A-style soft saturation — RECONSTRUCTION, deliberately
// small. Asymmetric rational curve (2nd-harmonic dominant): derivative
// at 0 is exactly 1 (small-signal unity preserved), no hard flat-top:
// |y| approaches ~2.5 asymptotically only at absurd drive, and at
// nominal levels (|x| <= 1) the bend is a few percent at most.
inline float classASaturate(float x) {
    if (x >= 0.0f) return x / (1.0f + 0.10f * x * x);
    return x / (1.0f + 0.16f * x * x);
}

// ─────────────────────────────────────────────────────────────────────
// LundahlStage — one transformer stage (Nathan, 2026-10-14: the
// transformers are LUNDAHL, voiced on the closest Lundahl parts to
// the Neve Marinair iron — see docs/audio-v9-design.md for the
// research: input voiced on Lundahl's hi-fi line-input class —
// LL1540 datasheet behaviour, with the LL1538 documented as the
// Marinair-10468 substitute; output voiced on the LL2734, Lundahl's
// own "based on the Neve LO1166A" line output part).
//
// Character: "super fat but dry" (Nathan). Fat = low-end weight:
// core saturation that lives at LOW frequencies (real core flux is
// the integral of the signal, so it falls steeply with frequency —
// the top end stays hi-fi clean, the Lundahl signature), subtle at
// nominal level, thickening when driven. Dry = Nathan's OP-6 sense:
// the colour starts and stops WITH the signal. The model has no
// resonance, no ring and no tail: its only memory is the core flux
// state, a ~30 Hz magnetisation average that discharges with a
// ~5 ms time constant, so colouration collapses within a few ms
// of the drive stopping — no overhang, no low-mid honk build-up.
//
// Model (RECONSTRUCTION — a behavioural voicing, not a circuit
// sim): flux_ = one-pole LF average of the input; drive rises with
// |flux|; the deviation from clean is a soft odd/even shape of the
// driven signal, GATED by the flux magnitude (no flux, no colour),
// and blended by an engagement depth (0 = bit-exact pass-through).
// The input stage is the cleaner of the two (hi-fi line input);
// the output stage is fatter (Marinair-style iron saturates
// earlier: ~4% THD at 20 Hz at very high level per period tests
// cited in the design note).
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kXfmrFluxTauSec = 0.0053f; // ~30 Hz flux average
inline constexpr float kXfmrGateKnee   = 0.12f;   // flux gate knee
inline constexpr float kXfmrInKFlux    = 0.20f;   // input stage (cleaner)
inline constexpr float kXfmrInOdd      = 0.14f;
inline constexpr float kXfmrInAsym     = 0.010f;
inline constexpr float kXfmrOutKFlux   = 0.38f;   // output stage (fatter)
inline constexpr float kXfmrOutOdd     = 0.24f;
inline constexpr float kXfmrOutAsym    = 0.020f;

inline float tanhPade(float z) { // Padé approximant, good to |z| < 2.5
    return z * (27.0f + z * z) / (27.0f + 9.0f * z * z);
}

class LundahlStage {
public:
    enum class Voicing { input, output };
    explicit LundahlStage(Voicing v = Voicing::input) { setVoicing(v); }
    void setVoicing(Voicing v) {
        if (v == Voicing::input) { kFlux_ = kXfmrInKFlux; odd_ = kXfmrInOdd; asym_ = kXfmrInAsym; }
        else                     { kFlux_ = kXfmrOutKFlux; odd_ = kXfmrOutOdd; asym_ = kXfmrOutAsym; }
    }
    void prepare(double fs) { fs_ = fs > 0.0 ? fs : 48000.0; }
    void reset() { flux_ = 0.0f; }
    // depth: 0..1 engagement. depth == 0 returns x bit-exactly.
    float process(float x, float depth) {
        flux_ += (x - flux_) * tauCoef(kXfmrFluxTauSec, fs_);
        if (depth <= 0.0f) return x;
        const float aFlux = flux_ < 0.0f ? -flux_ : flux_;
        const float drive = 1.0f + kFlux_ * aFlux;
        float z = x * drive;
        z += asym_ * z * z;                       // even-order warmth
        const float shaped = z + odd_ * (tanhPade(z) - z);
        const float gate = aFlux / (aFlux + kXfmrGateKnee);
        return x + depth * gate * (shaped - x);
    }
    float flux() const { return flux_; }
private:
    double fs_ = 48000.0;
    float flux_ = 0.0f;
    float kFlux_ = kXfmrInKFlux, odd_ = kXfmrInOdd, asym_ = kXfmrInAsym;
};

// ─────────────────────────────────────────────────────────────────────
// Transient designer (v9) — sustain cut only, living in the wet
// path BEFORE the limiter (input transformer -> TD -> limiter).
// Classic envelope-difference design: a fast envelope and a slow
// envelope follow the (linked) signal; the gain follows their ratio
// DOWN only — where the slow envelope remembers level the fast one
// has already left (sustain, reverb tails, ambience), gain falls.
// Attacks pass at ~unity: a transient's own fast envelope leads.
// The limiter behind it then forces the level back up flattened,
// so the net is dried ambience + ultra-squashed body. Stereo-
// linked (one gain, both channels), click-free (the gain itself is
// smoothed), depth ganged to the knob (0 at knob 0).
// RECONSTRUCTION — the topology is the classic published one; all
// constants are voicing choices, labelled as such.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kTdFastAttackTauSec  = 0.0005f; // 0.5 ms
inline constexpr float kTdFastReleaseTauSec = 0.025f;  // 25 ms
inline constexpr float kTdSlowAttackTauSec  = 0.025f;  // 25 ms
inline constexpr float kTdSlowReleaseTauSec = 0.350f;  // 350 ms memory
inline constexpr float kTdGainAttackTauSec  = 0.002f;  // gain falls fast
inline constexpr float kTdGainReleaseTauSec = 0.008f;  // and recovers smooth
inline constexpr float kTdMaxExponent       = 2.0f;    // depth scaling
inline constexpr float kTdFloorGain         = 0.10f;   // -20 dB max cut

// ─────────────────────────────────────────────────────────────────────
// StereoLimiter — one shared (linked) gain computer for both
// channels, exactly like the documented stereo behaviour: the stereo
// centre holds because both channels always receive the same gain.
// v9 signal flow INSIDE the limiter: the wet branch is
//   input -> Lundahl input transformer -> transient designer ->
//   feed-forward FET gain cell -> Class-A saturator,
// blended against the untouched dry input by the knob-as-mix law.
// The limiter's detector reads the TD'd wet signal (feed-forward:
// control derived before the gain cell, as documented).
// ─────────────────────────────────────────────────────────────────────
class StereoLimiter {
public:
    void prepare(double sampleRate);
    void reset();

    // knob: 0..1 (plugin parameter 0..10 divided by 10)
    void setKnob(float knob01) { knobTarget_ = clampf(knob01, 0.0f, 1.0f); }

    void process(float inL, float inR, float& outL, float& outR);

    // Introspection (meters / tests)
    float gainReductionDb() const { return grDb_; }
    // Currently applied auto makeup (slow average GR + knob
    // crank, capped) — the other half of the net-gain readout
    // the tests use to prove the pumping trajectory.
    float appliedMakeupDb() const { return makeupDb_; }
    float wetWeight() const { return wetWeightForKnob(knobS_); }
    float knob() const { return knobS_; }
    // Current transient-designer gain (1 = no cut) and cut in dB.
    float tdGain() const { return tdGain_; }
    float tdCutDb() const { return -linearToDb(tdGain_); }
    // Colour engagement of the wet-path transformers (0 at knob 0).
    float xfmrDepth() const {
        const float d = knobS_ * 8.0f;
        return d > 1.0f ? 1.0f : d;
    }

private:
    double fs_ = 48000.0;
    float knobTarget_ = 0.0f;
    float knobS_ = 0.0f;      // smoothed knob (click-free rides)

    // Feed-forward peak detectors: positive and negative excursions
    // tracked separately, per channel, then linked by max().
    float posEnvL_ = 0.0f, negEnvL_ = 0.0f;
    float posEnvR_ = 0.0f, negEnvR_ = 0.0f;

    // v3: detector sidechain high-pass states (one low-pass state
    // per channel; the detector reads signal − lowpass ≈ the
    // signal above ~75 Hz). Detector only — the audio path never
    // sees this filter.
    float detLpL_ = 0.0f, detLpR_ = 0.0f;
    // v3: programme-presence state for the detector hang (see
    // process()): while programme is actually present at the
    // detector the hang stretches (the envelope cannot collapse
    // between LF cycles and ride the gain — the fast-release LF
    // distortion mechanism); the moment programme stops, the
    // hang collapses and the envelope bleeds at full speed —
    // programme-dependent ballistics, not one fixed compromise.
    float detPresence_ = 0.0f;
    int detQuietRun_ = 0;   // consecutive near-silent samples
    // v3: peak envelope of the pre-TD wet-path signal — the
    // overshoot ceiling's basis (an envelope, not the raw
    // instantaneous sample, so the ceiling cannot waveshape
    // steady tones within a cycle).
    float pkEnv_ = 0.0f;

    float grDb_ = 0.0f;       // smoothed gain reduction (dB, >= 0)
    float makeupDb_ = 0.0f;   // derived makeup actually applied (dB)
    float makeupAvgDb_ = 0.0f; // v3: slow average of the PROGRAMME GR (curve output, pre-guard)
    float stressDb_ = 0.0f;   // v3: slow memory of sustained GR (programme-dependent release)

    // v9 wet-path stages: input transformers (per channel — real
    // iron is per-channel) and the linked transient designer.
    LundahlStage xfmrInL_{LundahlStage::Voicing::input};
    LundahlStage xfmrInR_{LundahlStage::Voicing::input};
    float tdFast_ = 0.0f, tdSlow_ = 0.0f;  // linked envelopes
    float tdGain_ = 1.0f;

    float envStep(float env, float target, float relTauSec) const;
    float tdEnvStep(float env, float target, bool fast) const;
};

// ─────────────────────────────────────────────────────────────────────
// WarbleChain — switchable Lo-Fi Loop Junky-style warble insert
// (Nathan, 2026-10-14; research: ../research-lofi-loop-junky.md).
// NOT part of the 1972 reconstruction — a separate, deliberately
// lo-fi playback effect bolted on behind the limiter, OFF by
// default. In the original pedal the pitch wobble is the storage
// chip's playback clock being modulated (pitch == clock rate), and
// its bandwidth is hard-limited: nothing above 2.6 kHz. The accepted
// realtime equivalent (per Z.Vex's own Instant Lo-Fi Junky) is a
// short delay line with a modulated read position — which is what
// this is: one shared "transport" (like one tape / one clock),
// applied to the SIDE component only (see MID/SIDE TOPOLOGY below).
//
// Modulation is layered so it reads as tired tape / a wobbling RC
// clock, not a clean chorus LFO: slow wow + a triangle-wave main
// vibrato (the waveform Z.Vex's manual names as the traditional
// Loop Junky setting) + a whisper of flutter + slow random drift
// that never repeats. Depths/rates are judgement calls from the
// qualitative published descriptions — Z.Vex never published Hz or
// cents figures (see the research note).
//
// When OFF the chain is fully out of the path: process() copies
// input to output bit-exactly. (v3: the transport still runs
// silently underneath — lines fed, modulation advancing — so
// engaging crossfades into a live transport over 30 ms with no
// fill transient.) Reported latency: 0 when off; the base delay
// (12 ms) when on — the instantaneous delay wobbles up to
// ~±10 ms around that figure (v2 depths), which is the effect
// itself.
//
// MID/SIDE TOPOLOGY (v10 MOD revision, Nathan 2026-10-14: "the mod
// function should be hard left right warble so it sounds really
// wide, and the center should remain the dry signal"). The insert
// works in mid/side: the SIDE component rides the modulated-delay
// transport above — delaying the side is precisely antiphase L/R
// warble, so the image swings hard left/right — and originally the
// MID passed through completely DRY. Strong consequence of that
// design: a mono input (side ≡ 0) passed through the engaged
// insert bit-exactly, and everything centred in a stereo mix was
// numerically untouched.
//
// v3 SUPERSESSION (Nathan, on the shipped 1.0.2: "I'm not hearing
// the mod have any effect whatsoever"): processor-level
// measurement proved the side warble is enormous on wide material
// (−0.43 dB difference-vs-output) and clearly present on typical
// stereo (−9 dB) — but on mono and centre-anchored material there
// is, by the v10 design, literally NOTHING to hear: the sounds a
// listener anchors on never move. The least invasive correction
// that keeps the architecture: the MID now takes a HALF-DEPTH
// share of the SAME transport (same delay line timing, same
// 2.6 kHz lo-fi bandwidth, blended 50/50 with the dry mid), so
// centred content sways coherently with the sides instead of
// sitting outside the effect — the centre stays anchored in
// level and image (it is a blend, never a replacement, and both
// components share one transport so they cannot tear apart),
// while the side keeps the full-depth hard L/R warble. THE
// TRADE-OFF, stated plainly: the v10 guarantees "mono passes
// bit-exactly" and "the centre is numerically untouched" are
// RETIRED. With MOD in, mono material now warbles audibly (that
// is the point of the change), and the centre of stereo material
// moves at half depth. MOD out remains bit-exact end to end.
// See docs/audio-v3-diagnosis.md, Issue 1.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kWarbleBaseDelayMs = 12.0f;
// v2 depths (Nathan, on the shipped 1.0.0: the warble was "almost
// inaudible… needs to be absolutely crazy"): every layer deepened
// ~3-4x from v1 (wow 1.8, vib 0.55, flutter 0.12, drift 1.1 ms —
// ~5.2% peak-to-peak pitch, chorus-subtle). The worst-case sum of
// excursions is now ~10.05 ms around the unchanged 12 ms base, so
// the safety floor drops 4.0 -> 2.0 ms to give the troughs room
// (the read position still can never approach the write position:
// the process() clamp keeps >= 2 samples). Base delay unchanged,
// so the reported latency stays 576 samples @ 48 kHz. The 2.6 kHz
// side bandwidth stays too: it is the pedal's published brick
// wall, and the v1 inaudibility was depth, not dullness — pitch
// wobble lives in timing and reads on any content that survives
// 2.6 kHz. See docs/audio-v2-retune.md.
inline constexpr float kWarbleWowHz       = 0.6f;
inline constexpr float kWarbleWowMs       = 5.0f;
inline constexpr float kWarbleVibHz       = 4.9f;   // triangle
inline constexpr float kWarbleVibMs       = 2.4f;
inline constexpr float kWarbleFlutterHz   = 9.3f;
inline constexpr float kWarbleFlutterMs   = 0.45f;
inline constexpr float kWarbleDriftMs     = 2.2f;   // random-walk depth
inline constexpr float kWarbleLowpassHz   = 2600.0f; // published pedal limit
inline constexpr float kWarbleHighpassHz  = 100.0f;
inline constexpr float kWarbleFadeSec     = 0.030f;
inline constexpr float kWarbleMinDelayMs  = 2.0f;    // hard safety floor (v2)
inline constexpr float kWarbleMaxDelayMs  = 24.0f;   // hard safety ceiling
inline constexpr float kWarbleMidDepth    = 0.4f;    // v3: mid blend depth on the same transport (see topology note)

// Minimal RBJ biquad (the warble path's bandwidth limit).
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    void setLowpass(double fs, float freq, float q);
    void setHighpass(double fs, float freq, float q);
    void reset() { x1 = x2 = y1 = y2 = 0.0f; }
    float process(float x) {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y < 1.0e-20f && y > -1.0e-20f ? 0.0f : y; // denormal flush
    }
};

class WarbleChain {
public:
    void prepare(double sampleRate);
    void reset();

    void setEnabled(bool on);
    bool enabled() const { return target_; }
    bool active() const { return state_ != State::off; }
    // Truthful latency: base delay in samples when on, 0 when off.
    int latencySamples() const { return latency_; }

    void process(float inL, float inR, float& outL, float& outR);

    // Introspection (tests)
    float currentDelayMs() const { return lastDelayMs_; }
    float fadeGain() const { return fadeGain_; }

private:
    enum class State { off, fadingIn, on, fadingOut };
    void resetChainState();
    float modulationDelayMs();
    float readDelay(const float* ring, float delaySamples) const;

    double fs_ = 48000.0;
    bool target_ = false;
    State state_ = State::off;
    float fadeGain_ = 0.0f;
    int latency_ = 0;

    // Two rings on ONE shared transport (v3): the SIDE at full
    // depth (v10 M/S topology) and the MID at half depth (v3 —
    // see the topology note above the constants). Both lines are
    // fed continuously, even while the insert is off, so engaging
    // crossfades into a live, coherent transport — no fill
    // transient, no stale burst. (Off still returns the input
    // bit-exactly; the lines just stay warm.)
    std::vector<float> ringS_;
    std::vector<float> ringM_;
    int ringSize_ = 0;      // power of two
    int ringMask_ = 0;
    int writePos_ = 0;

    Biquad lpInS_;   // into the delay (record-side limit)
    Biquad lpOutS_;  // out of the delay (playback-side limit)
    Biquad hpOutS_;  // low-end tidy
    Biquad lpInM_;   // the mid line wears the same lo-fi limits
    Biquad lpOutM_;
    Biquad hpOutM_;

    float wowPhase_ = 0.0f, vibPhase_ = 0.0f, fltPhase_ = 0.0f;
    float drift_ = 0.0f, driftTarget_ = 0.0f;
    int driftHold_ = 0;
    uint32_t rngState_ = 0x2545f491u;
    float lastDelayMs_ = kWarbleBaseDelayMs;
};

// ─────────────────────────────────────────────────────────────────────
// Chain — the plugin's complete signal path (v8): input → limiter
// (bypassable via the 'limit' in/out toggle) → warble insert (the
// 'mod' toggle) → output. The processor and the tests both run
// THIS code, so the bypass guarantees below are the shipped ones,
// not a test-side replica.
//
// Guarantees:
//  * limit OUT + mod OUT: output is BIT-EXACT dry input. The
//    limiter keeps running on the input (its state stays live, so
//    switching back in has no stale-state surprise), but its
//    samples are not used; the warble chain is fully out.
//  * limit toggling crossfades over 30 ms (same law as the mod
//    toggle) — click-free mid-tone switching, in both directions.
//    At the fade extremes the selected path is used exactly, so
//    limit IN + mod OUT is the plain limiter, sample for sample.
//  * The magic-eye readout follows the limit fade: gainReductionDb()
//    returns 0 whenever limit is out, whatever the limiter's own
//    (still-running) gain computer says.
//  * Latency: 0 unless mod is in; then the warble base delay.
//  * v9: the Lundahl output transformer sits after BOTH sections
//    (see the member note below); it is skipped when both are out,
//    and its depth is 0 at knob 0 with mod out, so the knob-0
//    transparency also holds end to end through the chain.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kLimitFadeSec = 0.045f;

class Chain {
public:
    void prepare(double sampleRate);
    void reset();

    void setKnob(float knob01) { limiter_.setKnob(knob01); }

    void setLimitIn(bool in);
    bool limitIn() const { return limitTarget_; }
    void setModIn(bool in) { warble_.setEnabled(in); }
    bool modIn() const { return warble_.enabled(); }

    int latencySamples() const
    {
        return warble_.enabled() ? warble_.latencySamples() : 0;
    }
    // GR for the magic eye: the limiter's own GR, weighted by the
    // limit fade (exactly 0 when limit is out).
    float gainReductionDb() const
    {
        return limiter_.gainReductionDb() * limitFade_;
    }
    const StereoLimiter& limiter() const { return limiter_; }
    const WarbleChain& warble() const { return warble_; }

    void process(float inL, float inR, float& outL, float& outR);

private:
    double fs_ = 48000.0;
    StereoLimiter limiter_;
    WarbleChain warble_;
    bool limitTarget_ = true;
    float limitFade_ = 1.0f;   // 1 = limiter fully in, 0 = fully out

    // v9: Lundahl OUTPUT transformer — last thing in the engaged
    // path, after the limiter AND after the mod insert, so
    // everything the box outputs passes through it when either
    // section is engaged. Depth: full when mod is in (the box is
    // unmistakably engaged); when only the limiter is engaged it
    // follows the limiter's own colour depth (0 at knob 0, which
    // keeps the knob-0 bit-exact guarantee end to end). Both
    // toggles out: the stage is skipped entirely — bit-exact dry.
    LundahlStage xfmrOutL_{LundahlStage::Voicing::output};
    LundahlStage xfmrOutR_{LundahlStage::Voicing::output};
};

} // namespace osc72
