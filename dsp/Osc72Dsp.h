#pragma once
// ─────────────────────────────────────────────────────────────────────
// Osc72Dsp — framework-free C++17 DSP core for the Western
// Compressor. ENGINE v4 (2026-10-15): a from-scratch engine
// behind the unchanged shell (Chain interface, parameters,
// latency reporting, eye GR feed). The v1–v3 engine (feed-
// forward FET limiter + transient designer + Lundahl stages +
// mid/side warble) is RETIRED wholesale by Nathan's directive:
//
//   LIMIT = the most authentic possible end-to-end behavioural
//           emulation of the classic RCA BA-6A limiting
//           amplifier (research: docs/engine-v4-research.md;
//           design: docs/engine-v4-design.md).
//   MOD   = the widest documented configuration of Abbey Road
//           Reel ADT (Artificial Double Tracking): a delayed
//           source plus TWO independent varispeed tape decks
//           panned to opposite extremes.
//
// BA-6A path (per channel, hardware order, one linked control
// voltage — the hardware is mono; the plugin is a stereo pair
// under one control voltage so the image cannot tear):
//   input transformer (flux colour) -> DRIVE (the one knob =
//   the hardware's INPUT attenuator) -> push-pull 6SK7 vari-mu
//   gain (law from the tube's remote-cutoff characteristic) ->
//   interstage transformer -> 6J7 driver saturation -> 6V6
//   push-pull output saturation -> output transformer ->
//   response shaping -> OUTPUT (auto make-up, §3.2 of the
//   design doc). The 6H6 detector reads the signal AFTER the
//   output transformer and before make-up — the hardware's
//   output-referenced limiting. Ballistics: fast detector
//   charge (stock attack), Single/Dual recovery network (the
//   hardware's own selector), plus an ATTACK SLOW position
//   that is an era detector mod, NOT a factory setting — see
//   the research doc, §A.6, and design §8 for every flag.
//
// Governing criteria (Nathan, from PA-200/DN1000): density and
// weight over paper accuracy; always-on colour at nominal
// levels; dryness (colour starts/stops with the signal);
// predictable, monotone level behaviour; first-note analog
// indistinguishability. Design doc §0/§8.
//
// PHASE 3 (2026-10-24, Nathan's engine-rebuild directive):
// the gain computer + stage chain are re-implemented in a
// VOLTAGE domain (0 dBFS == 8.7 Vpk at the terminals; stage
// shoulders at tens of volts), restoring the hardware
// proportions the normalised v4 engine lost — detector
// authority across the whole drive range, stage headroom
// above the loop's working levels, and a threshold that is
// one reference voltage with a small, safe trim. Topology
// is unchanged (true BA-6A feedback: detector post-output-
// transformer, vari-mu element in the path); ballistics
// networks, taper, bus staging, MOD, meter and every
// external behaviour are the shipped ones, held to the
// compatibility contract (western-compressor-release-
// 1.4.2/CONTRACT.md; design: DESIGN.md in the same folder).
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
inline float tanhPade(float z) { // Padé approximant, good to |z| < 2.5
    return z * (27.0f + z * z) / (27.0f + 9.0f * z * z);
}

// Soft tube-stage saturation: tanh core (small-signal slope
// exactly 1 after the /drive normalisation) plus a small
// even-order term (asym: push-pull stages run it near zero,
// single-ended-ish driver stages carry a little more).
inline float tubeSat(float x, float drive, float asym) {
    const float z = x * drive;
    const float az = z < 0.0f ? -z : z;
    const float even = asym * z * az / (1.0f + az);
    return ((float)std::tanh((double)z) + even) / drive;
}

// ── PHASE 3 (2026-10-24): voltage-domain stage helpers ──
// Soft shoulder (unmodeled-characteristics gate item G4):
// linear to the knee, tanh approach to the ceiling above it
// — a limit approached along a curve, never a category
// change. Used for the vari-mu pair's plate swing.
inline float softClipV(float x, float knee, float ceil) {
    const float a = x < 0.0f ? -x : x;
    if (a <= knee) return x;
    const float s = x < 0.0f ? -1.0f : 1.0f;
    return s * (knee + (ceil - knee) * (float)std::tanh(
        (double)(a - knee) / (double)(ceil - knee)));
}
// Tube stage in VOLTS: tanh core normalised to normV (the
// stage's swing scale), small-signal slope exactly 1 after
// the /drive normalisation, plus a bounded even-order term
// (the prototype's re-anchored form of tubeSat above).
inline float tubeSatV(float x, float drive, float asym, float normV) {
    const float z = x * drive;
    const float az = z < 0.0f ? -z : z;
    const float even = asym * 40.5f * (z / 9.0f) * (az / 9.0f) / (1.0f + (az / 9.0f) * (az / 9.0f)) * 0.5f;
    return ((float)(normV * std::tanh((double)z / (double)normV)) + even) / drive;
}

// ─────────────────────────────────────────────────────────────────────
// IronStage — one transformer in the BA-6A path (input,
// interstage, output voicings). Behavioural flux model: the
// core flux state is a ~30 Hz magnetisation average of the
// signal (real flux is the signal's integral, so colour lives
// at LF and the top stays clean); flux gates a soft odd/even
// waveshape, with the v52 flux-relief knee backing the gate
// off as the core saturates (kIronReliefKneeV). The
// model's ONLY memory is that flux state,
// discharging in ~5 ms — colour starts and stops with the
// signal (criterion 3, dryness: no ring, no tail). Voiced FAT
// per criteria 1/2 (density/weight, always-on colour): these
// stages run slightly hotter at LF than a mint unit's <0.4 %
// ticket — the deviation is documented in the design doc §8.
// ─────────────────────────────────────────────────────────────────────
// ── PHASE 3 (2026-10-24): the voltage anchor ────────────
// 0 dBFS at the terminals ≡ 8.7 Vpk (+18 dBu-class, the
// shipped design doc §8.5 calibration). The new engine
// carries the whole path in volts; stage limits are soft
// shoulders at tens of volts (DESIGN.md §3), restoring the
// hardware proportions the normalised engine lost.
inline constexpr float kVoltsPerFs = 8.7f;

inline constexpr float kIronFluxTauSec = 0.0053f; // ~30 Hz flux average
// Flux relief knee (v52 mechanism, PHASE 3 domain): the
// knee now reads the flux state in VOLTS. 3.0 V sits at
// the same operating region the normalised 0.35 knee
// served (storm-flux LF at high drive), because the flux
// state is now the signal's own volt-domain average.
inline constexpr float kIronReliefKneeV = 3.0f;

class IronStage {
public:
    enum class Voicing { input, interstage, output };
    explicit IronStage(Voicing v = Voicing::input) { setVoicing(v); }
    void setVoicing(Voicing v) {
        switch (v) {
            case Voicing::input:      kFlux_ = 0.30f; odd_ = 0.20f; asym_ = 0.012f; ceil_ = 30.0f; break;
            case Voicing::interstage: kFlux_ = 0.18f; odd_ = 0.11f; asym_ = 0.007f; ceil_ = 34.0f; break;
            case Voicing::output:     kFlux_ = 0.46f; odd_ = 0.30f; asym_ = 0.018f; ceil_ = 26.0f; break;
        }
    }
    void prepare(double fs) { fs_ = fs > 0.0 ? fs : 48000.0; }
    void reset() { flux_ = 0.0f; }
    float process(float x) {
        flux_ += (x - flux_) * tauCoef(kIronFluxTauSec, fs_);
        const float aFlux = flux_ < 0.0f ? -flux_ : flux_;
        if (aFlux < 1.0e-6f) return x;
        const float drive = 1.0f + kFlux_ * aFlux / kVoltsPerFs;
        float z = clampf(x * drive, -90.0f, 90.0f);
        { const float az = z < 0.0f ? -z : z;
          z += asym_ * 49.0f * (z / 7.0f) * (az / 7.0f) / (1.0f + (az / 7.0f) * (az / 7.0f)) * 0.5f; }
        const float shaped = z + odd_ * (ceil_ * (float)std::tanh((double)z / (double)ceil_) - z);
        const float relief = 1.0f / (1.0f + (aFlux / kIronReliefKneeV)
                                          * (aFlux / kIronReliefKneeV));
        const float gate = aFlux / (aFlux + 0.5f) * relief;
        return x + gate * (shaped - x);
    }
    float flux() const { return flux_; }
private:
    double fs_ = 48000.0;
    float flux_ = 0.0f;
    float kFlux_ = 0.30f, odd_ = 0.20f, asym_ = 0.012f, ceil_ = 30.0f;
};

// ─────────────────────────────────────────────────────────────────────
// BA-6A model constants. Sources and flags: research doc §A,
// design doc §2/§3/§8. In short:
//  * CEILING is the fixed internal limiting verge (envelope-
//    peak dB at the detector reference). The hardware has no
//    threshold control; the verge is the circuit's.
//  * DRIVE law = the hardware's INPUT attenuator ahead of a
//    fixed-gain box, normalised to the plugin reference
//    (0 dBFS ≡ +18 dBu-class terminal level — a calibration
//    invention, design §8.5).
//  * The vari-mu curve is the 6SK7 remote-cutoff law in the
//    dB domain: removed fraction of the over-verge excursion
//    grows continuously (soft knee -> limiting), capped at
//    the pair's useful ~38 dB.
// ─────────────────────────────────────────────────────────────────────
// PHASE 3: the threshold is ONE detector reference
// VOLTAGE (pre-sense), a signal level at the 6H6 tap —
// not a fudge factor against a bounded envelope. A trim
// in dB multiplies this voltage (Ba6aLimiter::
// setThreshTrimDb); measured on the prototype, +-1 dB
// of trim moves onset -+ ~0.3 knob uniformly across
// materials and source levels (DESIGN.md §4).
// TEST7 (Nathan's file-anchored directive, 2026-10-24):
// 3.5 V = -15.35 dB vs the TEST6 20.5 V. Calibrated by
// binary search on Nathan's actual Luna-session vocal
// (PV_verb_test_-_MAIN_0_axb2.wav dry L channel at
// native level + the session's -0.8 dB clip gain) so
// compression onsets at knob ~5 through the real
// processor path; both senses' references scale
// proportionally (single seam, nothing else changed).
// Full evidence: REPORT-TEST7.md.
inline constexpr float kBa6aThreshVolts = 3.5f;
// TEST8 (Nathan, 2026-10-25): per-mode / per-seat REFERENCE
// compensation, costed and verified by renders in
// DIAGNOSIS-test7-insertion-gain.md §D on the same file.
// The 3.5 V calibration centred the FAST seats near 5 but
// left the seat spread (COMP 5.51/6.24, LIMIT 4.65/5.45):
// the SLOW seats lag because their envelopes equilibrate
// lower, and LIMIT FAST sat low. These factors multiply
// the ONE reference seam in Ba6aLimiter::processCore
// (threshV), in the probes' exact association
// (mode factor x seat factor):
//   COMP  mode x0.19: COMP onsets saturate under reference
//     trim (law-toe limited, §D) — 0.19 and 0.25 measure
//     identically at onset (5.35 FAST); the verified
//     combined set ships 0.19.
//   LIMIT mode x1.41 (+3.0 dB): LIMIT FAST 4.65 -> 4.97.
//   LIMIT SLOW seat x0.35 (-9.1 dB, attack-SLOW seat only,
//     on top of the mode factor): LIMIT SLOW 5.71 -> 4.98.
//     The COMP SLOW seat factor is 1.0 — it buys nothing
//     (COMP SLOW floors at 6.13 against its 304 V slow-law
//     toe; full centring is a law re-voice, not a trim).
// Expected file onsets: 5.35 / 6.13 / 4.97 / 4.98.
// The SLOW-attack demand shaping (ba6aDetSlowTau) reads
// the UNFACTORED reference, exactly as in the §D probes.
inline constexpr float kBa6aRefCompModeFactor      = 0.19f;
inline constexpr float kBa6aRefLimitModeFactor     = 1.41f;
inline constexpr float kBa6aRefLimitSlowSeatFactor = 0.35f;
inline constexpr float kBa6aGrCapDb     = 46.0f;
// Drive law (design §3.1): the box runs hot by design (its
// ~54 dB of gain normalised so nominal programme lives near
// the limiting verge, as a programme limiter at a transmitter
// does); the knob — the INPUT attenuator — trims around that
// working point.
// v1.1.1 (Nathan's ear verdict on 1.1.0: the input "does not
// dig into the compression nearly enough") steepened the
// taper to -3 + 9k + 45k^2 (top +51 dB; programme mean GR at
// max 31.5 dB) — see docs/build-report-v40.md.
// v1.1.5 (Nathan's ear verdict on 1.1.4: "still digging in
// way too hard... at five it should just be barely kissing
// the compressor; right now that's happening at zero"):
// the DEPTH was right but the CENTRE was wrong — the whole
// onset sat ~half a knob too low. The law is re-centred as
// a logistic (S) taper, -5 dB at knob 0 to the SAME +51 dB
// at knob 10 (the top is bit-exact the 1.1.1 depth brief):
// the bottom half stays in the colour region, knob 5 is the
// kissing point, and the working range spreads across the
// top half. Measured (1 kHz tones, mean settled GR): at
// knob 0.5, -20 dBFS -> 0.1 dB and -14 dBFS -> 1.8 dB (was
// 5.8 / 9.3); at knob 0, -20 -> 0.0 and -10 -> 2.2 (colour
// and hot peaks only); programme mean GR 2.6 / 3.3 / 31.2 dB
// at knob 0 / 0.5 / 1. Full table: docs/build-report-v46.md.
// STAGING-FIX CANDIDATE (2026-10-22, TEST BUILD — Nathan's
// staging redesign directive; diagnosis: western-compressor-
// release-1.4.1/DIAGNOSIS-compression-action.md, F6): the
// v1.1.5 logistic bunched 45 of its 56 dB into the last 3.5
// knob steps (per-step ratio 27:1 at the half-step grain) —
// the knob felt empty through its middle and violent at the
// top. The taper is RE-SPREAD with the endpoints kept
// EXACTLY (-5 dB at knob 0, +51 dB at knob 10): the logistic
// flattens (steepness 9.5 -> 5.0) and re-centres (0.82 ->
// 0.62), giving per-full-step increments of
//   +1.76 +2.71 +3.99 +5.54 +7.09 +8.17 +8.38 +7.60 +6.18 +4.59
// (max/min 4.8:1). The staging map is re-voiced BY DESIGN:
// drive at knob 5 is now +16.1 dB (was -2.0) — the INPUT-5
// home position is re-anchored at the bus instead (the
// peak-referenced make-up + staging anchor, see the bus
// constants below), not by withholding drive. The vari-mu
// law, ballistics and GR generation downstream of this law
// are untouched: at MATCHED DRIVE the loop's work is the
// shipped loop's work.
inline float ba6aDriveDb(float knob01) {
    const float k = clampf(knob01, 0.0f, 1.0f);
    auto sig = [](float x) { return 1.0f / (1.0f + std::exp(-5.0f * (x - 0.62f))); };
    const float g = (sig(k) - sig(0.0f)) / (sig(1.0f) - sig(0.0f));
    return -5.0f + 56.0f * g;
}
// Detector sense: the 6H6 is fed from its own winding/tap,
// scaled over the output-terminal level (per-mode sense,
// §COMP/LIMIT below: 2.2 COMP / 1.4 LIMIT, as shipped).
// PHASE 3: the sense is carried in VOLTS at the tap, so it
// keeps growing across the whole drive range — the shipped
// normalised sense was bounded by the cascade's ~0.93
// ceiling, which starved the loop exactly where the
// hardware's control voltage is strongest (DESIGN.md §1).
//
// The vari-mu law (PHASE 3 form; DESIGN.md §4), from the
// same tube physics as the shipped law:
//  (a) the 6H6's sense characteristic: square-law at small
//      envelopes, approaching linear at large ones —
//      shape(env) = env^2/(env + toe), carried in volts.
//  (b) the 6SK7's remote-cutoff law: the dB-per-volt slope
//      is small at the start of an excursion and STIFFENS
//      through the working range — the growth term
//      growth.over^2/(over + growthKnee). The first dB of
//      GR is governed by `slope` alone, so onset is
//      gradual by construction (CONTRACT C-ONSET), and
//      static ratios RISE with depth by mechanism, as the
//      shipped ladder does (CONTRACT §4).
// GR = slope.(shape(env) - shape(ref))
//    + growth.(shape(env) - shape(ref))^2
//             / ((shape(env) - shape(ref)) + growthKnee),
// zero below the reference, capped at kBa6aGrCapDb.
inline float ba6aSenseShape(float envV, float toeV) {
    return envV * envV / (envV + toeV);
}
inline float ba6aVariMuDb(float envSensed, float slope, float toeV,
                          float refSensed, float growth, float growthKnee) {
    const float over = ba6aSenseShape(envSensed, toeV)
                     - ba6aSenseShape(refSensed, toeV);
    if (over <= 0.0f) return 0.0f;
    const float gr = slope * over
                   + growth * over * over / (over + growthKnee);
    return gr > kBa6aGrCapDb ? kBa6aGrCapDb : gr;
}
// Two-breakpoint variant (SLOW law only, Phase 3b): a
// second, wider growth stage keeps the law's slope rising
// into the deep parks. Still bounded: the total slope
// tends to slope + growth + growth2 (G4 discipline).
inline float ba6aVariMuDb2(float envSensed, float slope, float toeV,
                           float refSensed, float growth, float growthKnee,
                           float growth2, float growthKnee2) {
    const float over = ba6aSenseShape(envSensed, toeV)
                     - ba6aSenseShape(refSensed, toeV);
    if (over <= 0.0f) return 0.0f;
    const float gr = slope * over
                   + growth * over * over / (over + growthKnee)
                   + growth2 * over * over / (over + growthKnee2);
    return gr > kBa6aGrCapDb ? kBa6aGrCapDb : gr;
}
// ─────────────────────────────────────────────────────────────────────
// COMP / LIMIT ratio modes (v53, Nathan): the LIMIT cluster's
// IN/OUT bypass was, in his words, "rendered somewhat
// pointless — nobody bypasses the compressor on a
// compressor" — so the switch is now a RATIO mode switch and
// the section is always engaged. Both modes are the same
// vari-mu law family re-voiced on the gain-computer side;
// the audio path and the ballistics constants are untouched
// by the mode. PHASE 3: the voicings are the volts-domain
// law's (slope = dB per shaped volt, toe = the 6H6 corner
// in volts, sense = the detector tap ratio, growth = the
// remote-cutoff slope growth). Calibration targets are the
// contract's static ladders (CONTRACT §4: COMP 1.40 ->
// 3.04, LIMIT 2.62 -> 5.01, rising with depth) with onset
// re-anchored per CONTRACT §2. The mode change slews the
// law parameters over ~5 ms so toggling never clicks; the
// auto makeup rides GR, so it follows the mode by itself.
// Session note: the APVTS 'limit' bool is repurposed
// (true = COMP / false = LIMIT); a pre-1.1.9 session saved
// with the switch OUT (false) now loads as LIMIT-active
// instead of bypassed, and IN (true) loads as COMP.
// DEFAULT = LIMIT.
inline constexpr float kBa6aCompSlope  = 0.75f;  // dB per shaped volt
inline constexpr float kBa6aCompToe    = 90.0f;  // 6H6 toe corner, volts
inline constexpr float kBa6aCompSense  = 2.2f;   // detector tap ratio
inline constexpr float kBa6aCompGrowth = 0.30f;  // remote-cutoff slope growth
inline constexpr float kBa6aCompGrowthKnee = 8.0f;
inline constexpr float kBa6aLimitSlope = 4.2f;
inline constexpr float kBa6aLimitToe   = 5.0f;
inline constexpr float kBa6aLimitSense = 1.40f;
inline constexpr float kBa6aLimitGrowth = 0.55f;
inline constexpr float kBa6aLimitGrowthKnee = 5.0f;
// ── DUAL-SENSE DETECTOR (Phase 3b; DESIGN S8 gap 1,
// lever (b), Nathan's decision 2026-10-24) ──
// A second sense path beside the peak follower: a slow
// follower of the same 6H6 tap (raw volts; programme-time
// constants -- quick to fall, slow to charge) feeding its
// own voicing of the same law family, with its own
// reference scale (the 6H6's second diode section, in the
// hardware fiction). The demand is a sustainedness blend
// of the two laws: w follows rho = slow/fast envelope
// ratio -- ~0.68-0.73 on sustained tones (w = 1: the slow
// law governs statics, CONTRACT S4 ladders), dipping on
// transients (the fast law -- the approved programme
// voicing -- keeps programme peaks). Fitted on the
// statics + programme benches; see REPORT-TEST6-FINAL.
inline constexpr float kBa6aSlowSenseUpTauSec = 0.40f;
inline constexpr float kBa6aSlowSenseDnTauSec = 0.35f;
inline constexpr float kBa6aCompSlowSlope  = 0.1425f;
inline constexpr float kBa6aCompSlowToe    = 304.15f;
inline constexpr float kBa6aCompSlowSense  = 2.770f;
inline constexpr float kBa6aCompSlowGrowth = 1.555f;
inline constexpr float kBa6aCompSlowGrowthKnee = 18.77f;
inline constexpr float kBa6aCompSlowRef    = 0.633f;
inline constexpr float kBa6aLimitSlowSlope = 0.4019f;
inline constexpr float kBa6aLimitSlowToe   = 33.96f;
inline constexpr float kBa6aLimitSlowSense = 1.5782f;
inline constexpr float kBa6aLimitSlowGrowth = 1.1989f;
inline constexpr float kBa6aLimitSlowGrowthKnee = 14.58f;
inline constexpr float kBa6aLimitSlowRef   = 0.8753f;
// Second, wider growth breakpoint of the SLOW law
// (Phase 3b): lets the slow law's slope keep rising into
// the deep parks, where the tap's own sensitivity (V per
// dB) falls. The total slope stays bounded
// (slope + growth + growth2) — the G4 asymptotic
// discipline holds.
inline constexpr float kBa6aCompSlowGrowth2 = 0.0f;
inline constexpr float kBa6aCompSlowGrowthKnee2 = 40.0f;
inline constexpr float kBa6aLimitSlowGrowth2 = 0.5339f;
inline constexpr float kBa6aLimitSlowGrowthKnee2 = 76.39f;
inline constexpr float kBa6aSusRho0 = 0.40f;
inline constexpr float kBa6aSusRho1 = 0.63f;
inline constexpr float kBa6aSusUpTauSec = 0.90f;
inline constexpr float kBa6aSusDnTauSec = 0.03f;
inline constexpr float kBa6aLawSlewTauSec = 0.005f; // mode-change slew (~5 ms)
// ─────────────────────────────────────────────────────────────────────
// Oversampling (v53): the "breaking glass" fix — Nathan's
// ear report on 1.1.8: pushing INPUT hard produced a hard,
// unpleasant glassiness at the limits. Diagnosis
// (docs/harshness-diagnosis.md): at HF the glass is ALIASING
// — the nonlinear cascade runs at base rate, so at 5 kHz
// 9.0% of the fundamental's energy at full drive is folded
// inharmonic energy (vs 0.15% in a 384 kHz reference render).
// The whole limiter core now runs at 2x the host rate
// behind a polyphase windowed-sinc resampler pair
// (Blackman-Harris prototype, 32 taps per phase, cutoff at
// the host Nyquist), which cuts the 5 kHz folded energy to
// 1.8% (the residue is decimation transition leakage of
// genuine ultrasonic harmonics, and the two-tone test lands
// within 0.5 pt of the 384 kHz truth). 4x measured barely
// better (1.6%) at double the cost again and was rejected.
// What oversampling cannot remove — the cascade's TRUE
// high-order harmonics and intermodulation at extreme
// depth (997 Hz H7+ ≈ 4.0%, two-tone off-grid ≈ 45% at
// 36 dB GR) — is documented in the diagnosis as the model's
// limit character, not a digital defect. Colour is
// untouched: THD at the colour point is identical to
// 1.1.8 (0.156%), and the statics (GR, RMS, peaks) match
// the base-rate build to measurement precision.
// Cost: full-chain CPU 4.8% -> 10.7% of realtime on the
// bench VM (G1 pin re-based with cause, docs/
// build-report-v53.md). Latency: the resampler pair adds
// exactly 32 host samples at any rate, reported truthfully
// through Chain::latencySamples(). One housekeeping item:
// decimation ringing can overshoot the makeup ceiling's
// 1.600 bound on flat-topped peaks (Gibbs), so the shipped
// instant-ceiling law is re-applied once at the output
// rate — it touches only samples above 1.600 (the ceiling
// was exonerated as a harshness source in the diagnosis:
// removing it changed no harsh metric).
inline constexpr int   kBa6aOsRatio       = 2;
inline constexpr int   kBa6aOsTapsPerPhase = 32; // prototype = 32*2+1 core-rate taps
// Ballistics (v45 re-voice; research: docs/ballistics-refs.md).
// The four toggle seats are Nathan's chosen reference voicings
// — the BA-6A panel has no attack/release controls of its own
// (provenance: docs/build-report-v45.md):
//   FAST attack  = 1176 attack knob at ~11 o'clock
//                  (≈ 190 µs class, geometric-taper reading).
//   FAST release = 1176 release knob at ~4:30–5 o'clock
//                  (≈ 50 ms class, at the fast end of travel).
//   SLOW attack  = between the LA-3A (≤ 1.5 ms) and the
//                  LA-2A (10 ms); T4-shaped: the charge rate
//                  follows the demand (see ba6aDetSlowTau).
//   SLOW release = the same opto seat: both manuals print the
//                  same release line (50 % ≈ 60 ms, complete
//                  0.5–5 s); the T4 two-stage shape + memory.
// v1.1.5 (Nathan's ear verdict on 1.1.4: both attacks still
// clamp too hard): BOTH attack times DOUBLED on top of the
// v45 seats, releases untouched. The attack-side constants
// are scaled by one factor per seat (FAST x2.8, SLOW x2.65)
// — the factors are calibrated, not nominal: the GR
// trajectory lives in the dB domain over the toe law, so it
// does not time-scale linearly with tau once the settled
// depth moves (the v1.1.5 drive law moved the C-test
// operating point from 25.6 to ~17 dB settled). Measured
// GR t90 after: FAST 0.188 -> 0.375 ms, SLOW full step
// 3.90 -> 7.83 ms; the SLOW small step lands at 9.33 ms
// (was 3.06) — more than double, because the T4 dim-panel
// shape answers a small demand at the new, quieter staging
// with a lazier charge. See docs/build-report-v46.md.
inline constexpr float kBa6aDetFastTauSec  = 0.000168f; // detector env charge, FAST (v45: 60 µs x2.8, v46)
inline constexpr float kBa6aDetDecayTauSec = 0.030f;   // detector env decay
// SLOW detector charge is SHAPED, not a single tau — the T4
// behaviours the manuals document: the EL panel burns brighter
// on a bigger demand so the cell charges faster toward it
// (grab), the rate tapers as the cell nears the demanded state
// (creep), and a small demand (dim panel) slows the whole
// response. tau = lerp(grab, creep, approach)
//                 x (1 + dim x (1 - demand)).
inline constexpr float kBa6aDetSlowGrabTauSec  = 0.002981f; // v45: 0.9 ms x2.65 (v46) x1.25 (v53)
inline constexpr float kBa6aDetSlowCreepTauSec = 0.009938f; // v45: 3.0 ms x2.65 (v46) x1.25 (v53)
inline constexpr float kBa6aDetSlowDim         = 2.6f;   // dim-panel slowdown factor
// v53 (Nathan's ballistics confirmation): the SLOW attack
// seat is the classic LA-2A (attack ~10 ms). The v46 seat
// measured t90 7.85 ms on a full step / 9.33 ms on a small
// step — ~20% fast against the restated reference — so the
// three SLOW attack constants take one common x1.25 (the
// v46 method: shapes and ratios preserved), landing the
// full step at 9.7 ms and the small step at 11.4 ms,
// bracketing the nominal 10 ms. The FAST attack seat is
// NOT touched: Nathan ruled it stays exactly as shipped
// (t90 0.375 ms). FAST release and SLOW release measured
// on their references (0.057 s; 0.070 s / 2.24 s) and are
// likewise untouched. The demand below is computed with
// the SHIPPED law (the no-arg ba6aVariMuDb) in both ratio
// modes, so the detector dynamics are mode-independent.
inline float ba6aDetSlowTau(float peakSensedComp, float envSensedComp) {
    // PHASE 3: the demand is read from the NEW law at its
    // fixed COMP voicing (a property of the panel physics,
    // mode-independent exactly as shipped), in COMP-sensed
    // volts against the one threshold reference.
    const float ref = kBa6aThreshVolts * kBa6aCompSense;
    const float demand = clampf(ba6aVariMuDb(peakSensedComp, kBa6aCompSlope,
                                             kBa6aCompToe, ref,
                                             kBa6aCompGrowth, kBa6aCompGrowthKnee) / 25.0f,
                                0.0f, 1.0f);
    const float approach = peakSensedComp > 1.0e-6f
        ? clampf(envSensedComp / peakSensedComp, 0.0f, 1.0f) : 1.0f;
    const float base = kBa6aDetSlowGrabTauSec
        + (kBa6aDetSlowCreepTauSec - kBa6aDetSlowGrabTauSec) * approach;
    return base * (1.0f + kBa6aDetSlowDim * (1.0f - demand));
}
inline constexpr float kBa6aAttFastTauSec  = 0.000252f; // control attack pole, FAST (v45: 90 µs x2.8, v46)
inline constexpr float kBa6aAttSlowTauSec  = 0.005963f;  // control attack pole, SLOW (v45: 1.8 ms x2.65, v46) x1.25 (v53)
inline constexpr float kBa6aRelSingleTauSec = 0.014f;  // FAST release (1176 seat)
inline constexpr float kBa6aRelDualFastTauSec = 0.022f;
inline constexpr float kBa6aRelDualSlowTauSec = 0.900f; // unstressed tail pole
inline constexpr float kBa6aRelDualFastShare  = 0.60f;
// T4 memory: a stress state integrates the GR actually being
// done (charges over ~1.2 s under reduction, forgets over
// ~8 s in quiet) and stretches the Dual tail pole
// monotonically, bounded: at full stress the pole is
// x(1 + stretch), keeping even the longest tail inside the
// manuals' printed 0.5–5 s complete-release envelope.
inline constexpr float kBa6aMemChargeTauSec = 1.2f;
inline constexpr float kBa6aMemDecayTauSec  = 8.0f;
inline constexpr float kBa6aMemStretch      = 1.6f;
inline constexpr float kBa6aMemRefDb        = 25.0f;   // GR depth counted as full stress
// PER-SEAT BALLISTICS SCALES (AB2 test build, 2026-10-19 —
// NOT a release; Nathan's ear-trial timings). History: v62
// shipped a 1X/2X panel selector multiplying every
// ballistics tau by one slewed scale; v64 / 1.3.1 baked
// the settled 2X value in as the single constant
// kBa6aTimeScale = 2.0f, with the FAST attack chain taking
// a calibrated x3.0 (kBa6aAttFastCalib — the bench step's
// t90 is affine in scale with a fixed ~0.33/0.27 ms
// oversampler-edge floor, so a plain x2 read only
// 1.43x/1.62x). This build REPLACES that one whole-loop
// scale with one calibrated scale per seat, so each seat
// can be re-timed independently to Nathan's AB2 targets
// on the standard step bench (LIMIT = the calibrated
// mode; the constants stay mode-shared — COMP reads
// wherever the shared taus land through its law):
//   attack FAST  t90            = 3.0 ms  (1.3.1: 1.125)
//   attack SLOW  t90 full-step  = 15.0 ms (1.3.1: 15.06 —
//     re-based by Nathan's refinement: 15 ms sits between
//     the LA-2A's 10 ms and the Sta-Level Double's 25 ms,
//     the centre of the classic slow-attack cluster)
//   release FAST (SINGLE) 90%   = 1.0 s   (1.3.1: 0.0935)
//   release SLOW (DUAL) 90%     = 3.0 s fresh (1.3.1: 4.663)
// The frozen seat constants themselves are never edited —
// the calibration lives entirely in these multipliers,
// exactly as in v62/v64. Sites: kBa6aAttFastScale feeds
// the detector FAST charge tau and the FAST control pole;
// kBa6aAttSlowScale feeds the SLOW shaped detector tau
// (scaled as a whole — the grab/creep/dim proportions are
// preserved) and the SLOW control pole; kBa6aRelSingleScale
// feeds the SINGLE tau; kBa6aRelDualScale feeds both DUAL
// taus (the tail pole keeps its T4 stretch product on top)
// and the T4 memory charge/decay taus, so the memory
// behaviour scales with the release it shapes. The
// detector decay keeps its 1.3.1 effective scale (2.0) as
// shared release-side infrastructure: it bounded the old
// 0.09 s FAST release but bounds neither new target.
// NOT scaled, by design (unchanged from v62/v64): the B+
// sag charge/recover, the make-up average and slew, the
// knob and law slews, the sidechain filter and every
// audio-path constant. Ratio laws, statics, colour, drive,
// MOD section and meter are untouched by this build.
inline constexpr float kBa6aAttFastScale   = 10.5f;  // AB2 calibrated: bench FAST t90 3.000 ms LIMIT (COMP 4.438 — mode offset, see report)
inline constexpr float kBa6aAttSlowScale   = 1.996f; // AB2 calibrated: bench SLOW full-step t90 14.958 ms LIMIT (COMP 21.021); small step 24.000 / 24.542
inline constexpr float kBa6aRelSingleScale = 30.2f;  // AB2 calibrated: bench SINGLE 90% recovery 0.9986 s LIMIT / 1.0092 s COMP; 50% 0.3176 / 0.3282
inline constexpr float kBa6aRelDualScale   = 1.29f;  // AB2 calibrated: bench DUAL 90% recovery 3.144 s LIMIT / 2.625 s COMP; 50% 0.1136 / 0.1043; T4 holds 2.998 s vs 4.220 s (contrast 1.41)
inline constexpr float kBa6aDetDecayScale  = 2.0f;  // held at the 1.3.1 effective value
// Auto make-up (v1.0.3 law carried per Nathan; design §3.2).
inline constexpr float kBa6aMakeupAvgChargeTauSec = 0.080f;
inline constexpr float kBa6aMakeupAvgDecayTauSec  = 0.300f;
inline constexpr float kBa6aMakeupFactor = 0.92f;
inline constexpr float kBa6aMakeupCrankDb = 7.0f;   // x knob^2
inline constexpr float kBa6aMakeupCapDb  = 40.0f;
// Make-up safety ceiling (design §3.2): the make-up may never
// push the pre-make-up peak past the unit's output headroom
// (~+4 dBFS, where the 6V6 stage's own bend lives). This is
// v1.0.3's peak guard re-expressed for a series feedback
// limiter: the automated OUTPUT control riding against the
// ceiling an operator would respect.
inline constexpr float kBa6aMakeupPeakCeil = 1.6f;

// ─────────────────────────────────────────────────────────────────────
// STAGING-FIX CANDIDATE (2026-10-22, TEST BUILD) — the FULL2
// BUS output stage, re-voiced in two linked moves (diagnosis
// remedies 1+2; the module above, its law and its 1.600
// discipline are untouched — this is the bus's own stage,
// applied in Chain::process):
//  (1) PEAK-REFERENCED MAKE-UP. The shipped bus law capped
//      make-up only against the 1.600 bound, so through the
//      working travel it handed the GR straight back as
//      level (diagnosis F1/F2: output peaks rose +1.53 dB
//      per dB of drive; give-back +56 dB at INPUT 10). The
//      candidate caps the same law (0.92.avgGR + 7k^2, plus
//      the staging anchor) against a WORKING ceiling — the
//      park level — read off a slow feed-forward tracker of
//      the bus's own pre-make-up peak. Past the working
//      point the cap falls 1 dB per dB of arriving peak:
//      output peaks park, and further INPUT buys density
//      (RMS), not peak level. The tracker is STAGING, not
//      dynamics (fast charge, ~3 s decay — quasi-static
//      over programme phrases), so the make-up does not
//      pump at hit rate; per-hit overshoot is the soft
//      knee's job. The make-up floor is NEGATIVE (-30 dB):
//      at extreme drive the law attenuates to hold the
//      park instead of surrendering to the ceiling.
//  (2) SOFT CEILING. The bus's per-sample instant clamp at
//      1.600 is replaced by a LINKED soft knee whose onset
//      is the park level itself: below it the pair's gain
//      is exactly 1; above it the pair's peak bends along
//      a tanh toward the knee asymptote (park + span =
//      0.981, -0.17 dBFS) — overshoot arrives as progressive,
//      stage-like squash, never a flat top, and the stereo
//      pair's balance is preserved (one gain, taken from
//      the pair's peak). The absolute 1.600 bound is never
//      reached: the knee's asymptote sits below it.
// The staging anchor is the PARK LEVEL itself, with the
// law's constant trim as its fine adjustment: in the
// capped regime (which includes the home position) the
// output RMS sits at park - (the bus's own crest), so the
// park is chosen to land INPUT 5, noon mixes, on sane
// -14 dBFS programme (vocal/dense: bus crest ~8-9 dB) at
// near-unity output RMS: park -5.5 dBFS -> out ~-14 dBFS.
// A corollary of the knee asymptote (0.981) sitting just
// under 0 dBFS: the candidate's output cannot cross
// 0 dBFS at any setting. The trim's measured residual is
// recorded in the candidate report.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kBusParkCeilLin      = 0.530884f; // -5.5 dBFS: the working (park) ceiling — also the staging anchor (see note)
inline constexpr float kBusKneeSpan         = 0.45f;     // soft-knee span; asymptote = park + span = 0.981 (-0.17 dBFS)
inline constexpr float kBusRideChargeTauSec = 0.004f;    // riding peak tracker: charge (staging reference)
inline constexpr float kBusRideDecayTauSec  = 3.0f;      // riding peak tracker: decay (quasi-static)
inline constexpr float kBusMakeupFloorDb    = -30.0f;    // make-up may attenuate to hold the park
inline constexpr float kStagingAnchorDb     = 0.0f;      // staging anchor trim (measured; see candidate report)
// ─────────────────────────────────────────────────────────────────────
// TEST8 (Nathan, 2026-10-25): the bus AUTO MAKE-UP LAW IS
// REMOVED — "act like a normal compressor" (level recovery
// is the OUTPUT pots' job, as on the hardware). The
// diagnosis (DIAGNOSIS-test7-insertion-gain.md §C) proved
// the GR traces and the calibrated onsets are untouched
// by the removal (the detector is inside the module,
// pre-make-up), but on this engine the law's net action
// at working settings was ATTENUATION (-5.7 dB at INPUT
// 5) against a raw drive staging that runs ~+15 dB hot
// at knob 5 — so the law is replaced by a FIXED post-bus
// staging trim, placed where the law sat (after the bus
// sum, BEFORE the soft-ceiling knee, so the ceiling
// still guards the top): §C's pre-compensation estimate
// was -15 dB; the MEASURED optimum on Nathan's vocal
// through the finished TEST8 (seat compensation in,
// knee disengaged at the trimmed level) is -15.60 dB
// for the default seat (COMP SLOW insertion 0.00 dB;
// COMP FAST -0.33 dB), so -15.6 is the shipped value
// (REPORT-TEST8). The soft knee itself stays — a
// ceiling, not make-up. Chain::busMakeupDb() now reports
// this fixed trim (the bus's net gain in dB), so the
// telemetry name survives with honest contents.
// (The module-internal make-up computation in
// Ba6aLimiter is untouched: in module mode it is never
// applied, and the direct-limiter surface keeps it.)
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kBusStagingTrimDb  = -15.6f;      // TEST8 fixed post-bus staging trim (measured)
inline constexpr float kBusStagingTrimLin = 0.16595869f; // 10^(-15.6/20)

class Ba6aLimiter {
public:
    void prepare(double sampleRate);
    void reset();

    void setKnob(float knob01) { knobTarget_ = clampf(knob01, 0.0f, 1.0f); }
    float knob() const { return knobS_; }
    void setAttackFast(bool fast) { attackFast_ = fast; }
    bool attackFast() const { return attackFast_; }
    void setReleaseFast(bool fast) { releaseFast_ = fast; }
    bool releaseFast() const { return releaseFast_; }

    // Timing scales (v64 / 1.3.1; per-seat in the AB2
    // test build): there is deliberately NO setter — the
    // v62 selector is gone and the ballistics are the
    // unit's only ballistics, baked as the per-seat scale
    // constants (see their note). Nothing to glide,
    // nothing to snap: the scales apply from the first
    // sample of the first block.

    // Ratio mode (v53): COMP (true) / LIMIT (false, the
    // default). Only the gain-computer law parameters have
    // targets per mode; the currents slew to them at the
    // core rate (~5 ms) inside processCore.
    void setCompMode(bool comp)
    {
        compMode_ = comp;
        lawSlopeT_ = comp ? kBa6aCompSlope : kBa6aLimitSlope;
        lawToeT_   = comp ? kBa6aCompToe   : kBa6aLimitToe;
        lawSenseT_ = comp ? kBa6aCompSense : kBa6aLimitSense;
        // PHASE 3: the growth pair slews with the rest of
        // the law (same ~5 ms glide, same no-click rule).
        lawGrowthT_ = comp ? kBa6aCompGrowth : kBa6aLimitGrowth;
        lawGrowthKneeT_ = comp ? kBa6aCompGrowthKnee : kBa6aLimitGrowthKnee;
        lawSlowSlopeT_ = comp ? kBa6aCompSlowSlope : kBa6aLimitSlowSlope;
        lawSlowToeT_   = comp ? kBa6aCompSlowToe   : kBa6aLimitSlowToe;
        lawSlowSenseT_ = comp ? kBa6aCompSlowSense : kBa6aLimitSlowSense;
        lawSlowGrowthT_ = comp ? kBa6aCompSlowGrowth : kBa6aLimitSlowGrowth;
        lawSlowGrowthKneeT_ = comp ? kBa6aCompSlowGrowthKnee : kBa6aLimitSlowGrowthKnee;
        lawSlowGrowth2T_ = comp ? kBa6aCompSlowGrowth2 : kBa6aLimitSlowGrowth2;
        lawSlowGrowthKnee2T_ = comp ? kBa6aCompSlowGrowthKnee2 : kBa6aLimitSlowGrowthKnee2;
        lawSlowRefT_   = comp ? kBa6aCompSlowRef   : kBa6aLimitSlowRef;
    }
    bool compMode() const { return compMode_; }

    // Sidechain low-cut: REMOVED in the AB3 test build
    // (gui-v60 round, Nathan) — the strip's SIDECHAIN pot
    // is repurposed as COMP MIX, so the detector-feed HPF
    // feature, its setter and its states are deleted
    // outright (the timesTwo precedent). The detector
    // hears the raw post-iron sense exactly as it did at
    // the feature's 10 Hz bottom stop, where the filter
    // was out of the path — so the shipped 1.3.1 audio
    // and every ballistics pin hold bit-exact.

    // FULL2 (bus architecture, Nathan): false = MODULE
    // mode — process() returns the raw module output (post
    // output transformer, PRE make-up, no instant ceiling,
    // no output-rate ceiling guard): the signal the MOD
    // send and the COMP bus channel are fed from. The
    // make-up LAW still runs inside (avg-GR tracker, peak
    // tracker, cap, slew — appliedMakeupDb() stays live)
    // so the direct-limiter behaviour at the default
    // (true) is bit-identical to every earlier build; the
    // Chain sets false in prepare() and re-applies the
    // law at the output bus, where the peak the cap reads
    // is the summed bus's own. GR, ballistics, statics and
    // the detector (which reads pre-make-up lastPre) are
    // untouched in either mode — the diagnosis verified
    // the GR trace is unchanged with make-up forced off.
    void setApplyMakeup(bool apply) { applyMakeup_ = apply; }
    bool applyMakeup() const { return applyMakeup_; }

    void process(float inL, float inR, float& outL, float& outR);

    // Host-rate delay of the oversampling resampler pair
    // (v53): (prototype length - 1) / ratio = 32 samples at
    // any host rate. Reported through Chain::latencySamples.
    int osDelaySamples() const
    {
        return (kBa6aOsTapsPerPhase * kBa6aOsRatio) / kBa6aOsRatio;
    }

    float gainReductionDb() const { return grDb_; }
    float appliedMakeupDb() const { return makeupDb_; }
    float driveDb() const { return ba6aDriveDb(knobS_); }

    // PHASE 3 calibration seams (tests/audits; no panel
    // control): the detector reference trim — multiplies
    // kBa6aThreshVolts by the dB value — and the detector
    // state readouts the margins/authority audits use.
    void setThreshTrimDb(float db) { threshTrimDb_ = db; }
    float threshTrimDb() const { return threshTrimDb_; }
    float detEnvVolts() const { return detEnv_; } // sensed volts
    float lastPreVolts() const
    {
        const float a = lastPreL_ < 0.0f ? -lastPreL_ : lastPreL_;
        const float b = lastPreR_ < 0.0f ? -lastPreR_ : lastPreR_;
        return (a > b ? a : b) * kVoltsPerFs;
    }

private:
    void processCore(float inL, float inR, float& outL, float& outR);
    float processChannel(float x, int ch, float grLin, float driveLin);

    // Oversampling state (v53): the core runs at fs_ =
    // hostFs_ x kBa6aOsRatio. Polyphase FIR histories: the
    // input ring holds the last 64 host samples per channel;
    // the high-rate ring holds the last prototype-length
    // core outputs. Coefficients are built in prepare().
    double hostFs_ = 48000.0;
    static constexpr int kOsTaps = kBa6aOsTapsPerPhase * kBa6aOsRatio + 1;
    static constexpr int kOsHist = 64;
    float osCoefUp_[kOsTaps] = {};
    float osCoefDn_[kOsTaps] = {};
    float osInL_[kOsHist] = {}, osInR_[kOsHist] = {};
    int   osInPos_ = 0;
    float osHighL_[kOsTaps + kBa6aOsRatio + 8] = {};
    float osHighR_[kOsTaps + kBa6aOsRatio + 8] = {};
    int   osHighPos_ = 0;

    // Ratio-law state (v53): currents slew toward the
    // mode targets at the core rate.
    bool  compMode_ = false; // default LIMIT
    float lawSlope_ = kBa6aLimitSlope, lawSlopeT_ = kBa6aLimitSlope;
    float lawToe_   = kBa6aLimitToe,   lawToeT_   = kBa6aLimitToe;
    float lawSense_ = kBa6aLimitSense, lawSenseT_ = kBa6aLimitSense;
    float lawGrowth_ = kBa6aLimitGrowth, lawGrowthT_ = kBa6aLimitGrowth;
    float lawGrowthKnee_ = kBa6aLimitGrowthKnee, lawGrowthKneeT_ = kBa6aLimitGrowthKnee;
    float lawSlowSlope_ = kBa6aLimitSlowSlope, lawSlowSlopeT_ = kBa6aLimitSlowSlope;
    float lawSlowToe_   = kBa6aLimitSlowToe,   lawSlowToeT_   = kBa6aLimitSlowToe;
    float lawSlowSense_ = kBa6aLimitSlowSense, lawSlowSenseT_ = kBa6aLimitSlowSense;
    float lawSlowGrowth_ = kBa6aLimitSlowGrowth, lawSlowGrowthT_ = kBa6aLimitSlowGrowth;
    float lawSlowGrowthKnee_ = kBa6aLimitSlowGrowthKnee, lawSlowGrowthKneeT_ = kBa6aLimitSlowGrowthKnee;
    float lawSlowGrowth2_ = kBa6aLimitSlowGrowth2, lawSlowGrowth2T_ = kBa6aLimitSlowGrowth2;
    float lawSlowGrowthKnee2_ = kBa6aLimitSlowGrowthKnee2, lawSlowGrowthKnee2T_ = kBa6aLimitSlowGrowthKnee2;
    float lawSlowRef_ = kBa6aLimitSlowRef, lawSlowRefT_ = kBa6aLimitSlowRef;
    float susW_ = 0.0f;               // DUAL-SENSE: sustainedness weight 0..1
    float lawCoef_  = 0.0f;

    double fs_ = 48000.0;
    float knobTarget_ = 0.0f;
    float knobS_ = 0.0f;
    bool attackFast_ = true;    // default: stock (research §A.6)
    bool releaseFast_ = false;  // default: DUAL (research §A.6)

    IronStage ironInL_{IronStage::Voicing::input},      ironInR_{IronStage::Voicing::input};
    IronStage ironMidL_{IronStage::Voicing::interstage}, ironMidR_{IronStage::Voicing::interstage};
    IronStage ironOutL_{IronStage::Voicing::output},     ironOutR_{IronStage::Voicing::output};

    // Response shaping states (per channel): LF droop pole
    // (low-pass state for the high-pass) and HF droop pole.
    float hpLpL_ = 0.0f, hpLpR_ = 0.0f;
    float lpL_ = 0.0f, lpR_ = 0.0f;

    float sag_ = 0.0f;                // B+ sag state (linked), 0..1 — v1.1.2 (study §2.3a)
    bool grSelFast_ = true; float grSelBlend_ = 1.0f; float grSelFrom_ = 0.0f; // PHASE 3: release-select blend state
    float detEnv_ = 0.0f;             // PHASE 3: detector envelope (SENSED VOLTS at the tap)
    float detSlowV_ = 0.0f;           // DUAL-SENSE: slow sustained sense (RAW volts at the tap)
    float peakHoldV_ = 0.0f;          // DUAL-SENSE: ~30 ms tap max-hold (gap-dump trigger)
    float stress_ = 0.0f;             // v45: T4 memory state (dB of sustained GR)
    float lastPreL_ = 0.0f, lastPreR_ = 0.0f; // post-iron, pre-makeup (detector source)
    float prePeak_ = 0.0f;            // slow-decay peak of the pre-makeup output (make-up ceiling)
    float grSingle_ = 0.0f;           // SINGLE-network state (dB)
    float grDual1_ = 0.0f, grDual2_ = 0.0f;   // DUAL-network states (dB shares)
    float grDb_ = 0.0f;
    float avgGrDb_ = 0.0f;
    float makeupDb_ = 0.0f;
    bool applyMakeup_ = true;   // FULL2: false = module mode (see setter)
    float threshTrimDb_ = 0.0f; // PHASE 3: reference trim (see setThreshTrimDb)
};

// ─────────────────────────────────────────────────────────────────────
// AdtChain — the MOD insert: Abbey Road Reel ADT in its widest
// documented configuration (research §B; design §4):
//   SRC  = the source itself, delayed by the 20 ms base offset
//          (the non-Live arrangement: decks may swing ahead of
//          and behind the source). Reported latency = base.
//   DECK 1 (offset 20 ms; FULL2: + kAdtDeck1StaggerMs —
//          see its note): sine LFO ±6 ms @ 0.30 Hz, panned
//          hard RIGHT.
//   DECK 2 (offset 25 ms): smoothed-random LFO ±8 ms, panned
//          hard LEFT.
//   Each deck is its own "machine": independent wow (0.45 /
//   0.62 Hz, ±0.22 ms) and flutter (5.8 / 7.3 Hz, ±0.035 ms),
//   motor-inertia smoothing (~180 ms) on its varispeed, a
//   gentle tape saturation and tape bandwidth (~35 Hz–
//   8.5 kHz). Deck feed = the source mono sum (L+R)/2, the
//   stereo component's documented feed. Mono input widens.
//   The transport runs continuously (lines fed, motors
//   turning) even while off, so engaging crossfades (30 ms,
//   subtractive form — bit-exact at both extremes) into a
//   live machine.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kAdtBaseMs      = 20.0f;
inline constexpr float kAdtDeck2BaseMs = 25.0f;
// FULL2-ONLY VOICING (Nathan, tempered steer): a small
// static stagger on deck 1 (the RIGHT deck) so the two
// sides are offset at all times — "not extreme, just
// barely enough to not have phase cancellation... just
// enough for a wide stereo feel", the width coming from
// the hard panning + decorrelated modulation (sine vs
// smoothed-random), not from a large L/R time split.
// +1.0 ms (the ~1 ms class he targeted); the separation
// statistics that arbitrate it are measured in the FULL2
// report (min instantaneous |dR-dL|, coincidence dwell,
// trajectory cross-correlations over 12 s). This tree
// only — 1.3.1 and the FULL candidate keep 20.0.
inline constexpr float kAdtDeck1StaggerMs = 1.0f;
inline constexpr float kAdtLfo1Ms      = 6.0f;
inline constexpr float kAdtLfo1Hz      = 0.30f;
inline constexpr float kAdtLfo2Ms      = 8.0f;
inline constexpr float kAdtWowMs       = 0.22f;
inline constexpr float kAdtFlutterMs   = 0.035f;
inline constexpr float kAdtMotorTauSec = 0.180f;
// Deck return level. 1.1.0 shipped 0.82; v1.1.1 (Nathan's
// ear verdict: the ADT "should be mixed in more like 60%...
// drop the mix between the compressed and the ADT signal by
// about a third") trims the two deck returns by a third —
// x(2/3) — against the untouched SRC/direct path. Timing,
// LFOs and deck machinery are unchanged. v1.1.2 (his
// verdict on 1.1.1: the MOD "could come down another
// ~10%") trims a further x0.90 -> 0.492, which is 60% of
// the 1.1.0 return level exactly. v46 round (Nathan, on
// 1.1.6: the ADT mix goes 15% LESS again) trims a further
// x0.85 -> 0.4182, which is 51% of the 1.1.0 return
// level exactly. Timing, LFOs and deck machinery are
// unchanged, as in every trim.
inline constexpr float kAdtDeckGain    = 0.82f * (2.0f / 3.0f) * 0.90f * 0.85f;
inline constexpr float kAdtFadeSec     = 0.030f;
inline constexpr float kAdtMinDelayMs  = 3.0f;

class AdtChain {
public:
    void prepare(double sampleRate);
    void reset();

    void setEnabled(bool on);
    bool enabled() const { return target_; }
    bool active() const { return state_ != State::off; }
    // Truthful latency: the base offset when on, 0 when off.
    int latencySamples() const { return latency_; }

    void process(float inL, float inR, float& outL, float& outR);

    // Introspection (tests)
    float deckDelayMs(int deck) const { return deck == 2 ? lastDelay2Ms_ : lastDelay1Ms_; }
    float srcDelayMs() const { return kAdtBaseMs; }
    float fadeGain() const { return fadeGain_; }
    // TEST8: the last process() call's wet components,
    // PRE-fade (the exact addends of wetL/wetR): the SRC
    // (the unmodulated delayed source) and the deck term
    // (kAdtDeckGain x the cross-panned deck). The Chain's
    // MOD MIX law decomposes the machine's output into
    // unmodulated/modulated parts from these. Valid after
    // any call that computed the wet sum (not while the
    // machine is parked off — the Chain's gate covers it).
    float lastWetSrcL() const { return lastWetSrcL_; }
    float lastWetSrcR() const { return lastWetSrcR_; }
    float lastWetDeckL() const { return lastWetDeckL_; }
    float lastWetDeckR() const { return lastWetDeckR_; }

private:
    enum class State { off, fadingIn, on, fadingOut };
    float readDelay(int channel, float delaySamples) const;
    float deckTargetMs(int deck);
    float onePoleLp(float& state, float x, float tauSec) const {
        state += (x - state) * tauCoef(tauSec, fs_);
        return state;
    }

    double fs_ = 48000.0;
    bool target_ = false;
    State state_ = State::off;
    float fadeGain_ = 0.0f;
    int latency_ = 0;

    std::vector<float> ringL_, ringR_;
    int ringSize_ = 0;
    int ringMask_ = 0;
    int writePos_ = 0;

    // Deck transports (motor-smoothed delay states, ms).
    float delay1Ms_ = kAdtBaseMs, delay2Ms_ = kAdtDeck2BaseMs;
    float lastDelay1Ms_ = kAdtBaseMs, lastDelay2Ms_ = kAdtDeck2BaseMs;
    float lfo1Phase_ = 0.0f;
    float wow1Phase_ = 0.0f, wow2Phase_ = 0.25f;
    float flt1Phase_ = 0.0f, flt2Phase_ = 0.4f;
    float rand2_ = 0.0f, rand2Target_ = 0.0f;
    int rand2Hold_ = 0;
    uint32_t rngState_ = 0x1f123bb5u;
    // Per-deck tape bandwidth states (mono deck signals).
    float hp1_ = 0.0f, lp1_ = 0.0f, hp2_ = 0.0f, lp2_ = 0.0f;
    // TEST8: last wet-sum components (see the getters).
    float lastWetSrcL_ = 0.0f, lastWetSrcR_ = 0.0f;
    float lastWetDeckL_ = 0.0f, lastWetDeckR_ = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────
// Phaser90 — the MOD insert's second flavour (v55, Nathan):
// an MXR Phase 90 emulation in its SCRIPT-era (1974) voicing,
// selected by the A/P mini toggle on the panel (A = the Reel
// ADT above, the default; P = this phaser). Nathan's target
// sounds are the 'cosmic country' phaser records — George
// Harrison, Waylon Jennings, Link Wray: a slow, vocal sweep,
// NOT a jet flange. Design note: docs/mod-phaser-design.md.
//
// Circuit basis (documented): four JFET phase-shift stages
// (47 nF caps, the JFET a voltage-controlled resistor in
// parallel with 24 kΩ), ganged under one LFO (~0.1–10 Hz on
// the pedal's SPEED knob); the phased path sums 50/50 with
// the dry at the output mixer, giving TWO notches. The
// script original has NO feedback resistor — the 24 kΩ R28
// regeneration path (and its harder, more pronounced sweep)
// is a block-logo addition, so this model has no feedback
// loop at all. Each stage is modelled as the textbook
// one-pole allpass the stage reduces to, with the break
// frequency placed exactly by the bilinear (tan) mapping;
// the four breaks are ganged to one coefficient, one LFO —
// the pedal is MONO, so both channels share the same sweep
// state and the stereo image cannot wander.
//
// Voicing choices (reconstructions, labelled in the design
// note): the SPEED knob is fixed at 0.30 Hz; the break
// sweeps one decade, 600 Hz–6 kHz, in the log domain (a
// sine LFO), putting the first notch's travel at
// ~250 Hz–2.5 kHz and the second's at ~1.45–14.5 kHz —
// the few-hundred-Hz to few-kHz notch travel published
// analyses describe. Like the ADT beside it, the machine
// runs continuously (LFO turning, stages tracking the input)
// and engaging crossfades over 30 ms in the same
// subtractive form — bit-exact dry while off, bit-exact the
// settled flavour while on. The phaser contributes NO bulk
// delay: with P selected the chain latency falls back to
// the limiter's oversampling figure alone.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kPhaserRateHz     = 0.30f;   // fixed SPEED (reconstruction)
inline constexpr float kPhaserBreakLoHz  = 600.0f;  // stage break, LFO bottom
inline constexpr float kPhaserBreakHiHz  = 6000.0f; // stage break, LFO top
inline constexpr float kPhaserFadeSec    = 0.030f;  // engage fade (ADT's span)

class Phaser90 {
public:
    void prepare(double sampleRate);
    void reset();

    void setEnabled(bool on);
    bool enabled() const { return target_; }
    bool active() const { return state_ != State::off; }
    // No bulk delay in either state — the flavour's latency
    // contribution is zero by construction.
    int latencySamples() const { return 0; }

    void process(float inL, float inR, float& outL, float& outR);

    // TEST8: BOTH arms from ONE pass (the state advances
    // exactly once, as with process()/processEffect()):
    // the classic arm (as process()) and the effect arm
    // (as processEffect()). The Chain's MOD MIX law needs
    // the machine's dry input, its classic output and its
    // bare cascade in the same sample.
    void processBoth(float inL, float inR, float& outL, float& outR,
                     float& effL, float& effR);

    // AB3 (gui-v60 round): the EFFECT-ONLY arm — the bare
    // allpass cascade, with the same engage fade applied
    // toward the cascade instead of toward the classic
    // 50/50 mix (exact input at fade 0, exact cascade at
    // fade 1). In AB3 this was the chain's MOD MIX wet
    // arm; in FULL2 the bus takes the machine's own
    // output (process(), the classic 50/50 pedal) as the
    // MOD channel, per Nathan's bus spec — the insert's
    // own output IS the channel — so processEffect() is
    // retained as the cascade's test surface (the J/L
    // suites pin the machine's two arms directly).
    // process() above is unchanged: the machine's classic
    // self-contained output.
    void processEffect(float inL, float inR, float& outL, float& outR);

    // Introspection (tests)
    float rateHz() const { return kPhaserRateHz; }
    float lfoPhase() const { return lfoPhase_; }
    float breakHz() const { return breakHz_; }
    float fadeGain() const { return fadeGain_; }
    // Test seams: hold the LFO at a fixed phase so the
    // (then LTI) filter's static response can be measured.
    void setLfoFrozen(bool frozen) { lfoFrozen_ = frozen; }
    void setLfoPhase(float cycles) { lfoPhase_ = cycles - std::floor(cycles); }

private:
    enum class State { off, fadingIn, on, fadingOut };
    // One pass of the machine, both output arms (AB3):
    // process() takes the classic arm, processEffect()
    // the cascade arm; the state advances exactly once
    // per call, identically for both.
    void processImpl(float inL, float inR, float& outL, float& outR,
                     float& effL, float& effR);
    float stage(float x, int ch, int st, float c) {
        float& x1 = sx_[ch][st];
        float& y1 = sy_[ch][st];
        const float y = x1 - c * x + c * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    double fs_ = 48000.0;
    bool target_ = false;
    State state_ = State::off;
    float fadeGain_ = 0.0f;
    float lfoPhase_ = 0.0f;
    bool lfoFrozen_ = false;
    float breakHz_ = kPhaserBreakLoHz;
    float sx_[2][4] = {};
    float sy_[2][4] = {};
};

// ─────────────────────────────────────────────────────────────────────
// Chain — the plugin's complete signal path (v4): input ->
// BA-6A limiter (bypassable via the 'limit' toggle) -> ADT
// insert (the 'mod' toggle). Same guarantees as every engine
// before it:
//  * limit OUT + mod OUT: output is BIT-EXACT dry input.
//  * limit toggling crossfades over 45 ms; mod over 30 ms —
//    click-free mid-tone switching; at the fade extremes the
//    selected path is used exactly.
//  * The magic-eye readout follows the limit fade:
//    gainReductionDb() is 0 whenever limit is out.
//  * Latency: 0 unless mod is in; then the ADT base offset.
// v55: the MOD insert has two flavours (A = ADT, P = the
// Phase 90 above). Both machines run continuously; a 30 ms
// flavour blend crossfades between their outputs, hard-
// selecting at the extremes so a settled flavour is its own
// output bit-for-bit. Latency follows the SELECTED flavour:
// the ADT base offset with A, the oversampling figure alone
// with P.
// AB3 (gui-v60 test round): the path was restructured
// into TWO aligned blend stages around the inserts.
// FULL2 (bus architecture, Nathan — superseding AB3's
// cascade): the path is a two-channel MIXER on the
// PA-200 pattern. INPUT drives the compressor module AND
// the sends. The module (make-up DISABLED inside it —
// Ba6aLimiter module mode) feeds two parallel channels:
//   CH1 COMP: the module output, time-aligned at the bus
//     by the engaged insert's extra latency (the v44
//     offset crossfade retargeted onto this channel).
//   CH2 MOD: the insert's own output — the ADT's natural
//     output / the Phase 90's classic output — the insert
//     being fed POST-COMPRESSOR from the module output,
//     at every pot position. An engagement gate (the
//     inserts' own 30 ms span) silences the channel while
//     MOD is out: CH2 contributes nothing.
// COMP MIX and MOD MIX are the channels' GAINS (not
// crossfades — no dry arm anywhere), and the AUTO MAKE-UP
// happens AT THE BUS: the shipped law (0.92.avgGR +
// 7k^2 crank, peak-aware ceiling, programme peaks
// bounded at 1.600) computed from the module's GR and
// applied to the summed bus, its peak tracker reading
// the bus's own pre-make-up peak. The GR meter still
// reads the module's GR. The reported latency is
// unchanged (32 / 992-A / 32-P) and does not depend on
// either pot. Diagnosis: docs/../DIAGNOSIS (staging
// folder) — at AB3's compMix=0 the insert feed was the
// raw input (INPUT-invariant, bit-proven); here the send
// carries the module output unconditionally.
// STAGING-FIX CANDIDATE (2026-10-22, TEST BUILD): the
// bus law's demand side is unchanged, but its bound is
// re-voiced — the cap is peak-referenced to a -2 dBFS
// park ceiling via a slow riding tracker, the make-up
// floor is negative, a constant staging anchor re-
// anchors INPUT 5, and the instant 1.600 clamp is
// replaced by a linked soft knee (asymptote 0.981).
// See the bus constants block (kBusParkCeilLin) for the
// full note.
// TEST8 (Nathan, 2026-10-25) — three bus changes on top
// of the FULL2 architecture:
//  (1) NO AUTO MAKE-UP: the bus law above is GONE (see
//      kBusStagingTrimDb). A fixed staging trim
//      (-15.6 dB, measured) sits where the law sat,
//      before the soft knee.
//  (2) COMP MIX IS A TRUE BLEND (Nathan's commission):
//      the bus gains a DRY ARM — the chain's input,
//      delayed to the comp arm's exact latency (the
//      limiter's 32-sample oversampling delay + the
//      comp channel's bus offset, crossfades included)
//      and gained by the limiter's own drive law, so at
//      GR ~ 0 the pot crossfades between two level-
//      matched copies of the same signal (colour vs
//      clean) instead of fading the section out:
//        bus = compMix.comp + (1-compMix).dry + MOD arm.
//      COMP MIX 0 = pure dry passthrough (at the comp
//      path's latency — the reported latency is
//      unchanged); COMP MIX 100 = the comp path exactly
//      as before. Sessions recalling COMP MIX < 100 now
//      contain dry signal at that share BY DESIGN.
//  (3) MOD MIX IS A TRUE BLEND OF ITS SECTION (Nathan:
//      "the modulation mix needs to follow the same
//      behavior as the compressor mix"): each flavour's
//      channel contribution is formed by a per-flavour
//      law (see Chain::process) whose internal blend
//      completes at the pot's noon and whose top half is
//      the channel gain — MOD MIX 0 contributes exactly
//      nothing (the bus is bit-exact the MOD-out bus),
//      MOD MIX 100 is bit-exact TEST7's bus (the ADT's
//      additive doubling and the pedal's classic output
//      untouched), and between the endpoints the
//      modulated proportion rises as a blend rather
//      than the whole channel fading as one gain.
// ─────────────────────────────────────────────────────────────────────

class Chain {
public:
    void prepare(double sampleRate);
    void reset();

    void setKnob(float knob01) { limiter_.setKnob(knob01); }
    void setAttackFast(bool fast) { limiter_.setAttackFast(fast); }
    bool attackFast() const { return limiter_.attackFast(); }
    void setReleaseFast(bool fast) { limiter_.setReleaseFast(fast); }
    bool releaseFast() const { return limiter_.releaseFast(); }

    // Timing scales (v64 / 1.3.1; per-seat in the AB2
    // test build): no selector survives at this level
    // either — the limiter's ballistics are baked as the
    // per-seat scale constants. No latency consequence;
    // the ADT/phaser inserts are untouched.

    // Ratio mode (v53): the section is always engaged —
    // the old limit in/out bypass is gone by design (see
    // the COMP/LIMIT note at the law constants). COMP =
    // true, LIMIT = false.
    void setCompMode(bool comp) { limiter_.setCompMode(comp); }
    bool compMode() const { return limiter_.compMode(); }
    // Sidechain low-cut: REMOVED in the AB3 test build
    // with the limiter's (see Ba6aLimiter) — the pot is
    // COMP MIX now (below); no pass-through survives.
    void setModIn(bool in) { adt_.setEnabled(in); phaser_.setEnabled(in); }
    bool modIn() const { return adt_.enabled(); }

    // MOD flavour (v55): false = A (Reel ADT, the default),
    // true = P (Phase 90). The blend slews over 30 ms in
    // process(); the reported latency follows the selected
    // flavour immediately, as it does for the MOD toggle.
    void setModPhaser(bool phaser) { modPhaser_ = phaser; }
    bool modPhaser() const { return modPhaser_; }

    // ── The two strip pots (AB3 repurposing; FULL2
    // semantics, Nathan; TEST8 blend semantics): COMP MIX
    // (the ex-SIDECHAIN pot) crossfades the COMP channel
    // against the bus's dry arm, and MOD MIX (the ex-MIX
    // pot) drives the per-flavour blend law of the MOD
    // channel — see the class note above (TEST8 items
    // (2) and (3)).
    // Both targets slew per sample on the retired whole-
    // chain mix's law (one-pole 0.002, snap 2e-5) and snap
    // to their targets in reset(). With MOD out the MOD
    // channel is gated to silence, so the MOD MIX pot is
    // inert. Chain-level defaults are 1.0/1.0 (both
    // channels at full gain — the DSP suite's classic
    // pins address the chain directly); the PROCESSOR's
    // parameters default to 0.5/0.5 (12 o'clock, Nathan)
    // and are pushed into the chain on prepare and on
    // every block.
    void setCompMix(float m) { compMixTarget_ = clampf(m, 0.0f, 1.0f); }
    float compMix() const { return compMixTarget_; }
    void setModMix(float m) { modMixTarget_ = clampf(m, 0.0f, 1.0f); }
    float modMix() const { return modMixTarget_; }

    // FULL2 introspection (tests): the bus's net staging
    // gain in dB — through TEST7 this was the make-up
    // law's live value; TEST8 removed the law, so it now
    // reports the fixed staging trim (kBusStagingTrimDb)
    // — and the insert feed — the module output sample
    // the inserts were last fed (the send tap itself,
    // pre-insert).
    float busMakeupDb() const { return busMakeupDb_; }
    float insertFeedL() const { return lastFeedL_; }
    float insertFeedR() const { return lastFeedR_; }

    int latencySamples() const
    {
        // v53: the limiter's oversampling delay is always in
        // the path (the section is always engaged); the ADT
        // base delay adds when MOD is in. v55: only in the A
        // flavour — the phaser adds no bulk delay.
        return limiter_.osDelaySamples()
             + (adt_.enabled() && !modPhaser_
                    ? adt_.latencySamples() : 0);
    }
    float gainReductionDb() const
    {
        return limiter_.gainReductionDb();
    }
    const Ba6aLimiter& limiter() const { return limiter_; }
    const AdtChain& adt() const { return adt_; }
    const Phaser90& phaser() const { return phaser_; }

    void process(float inL, float inR, float& outL, float& outR);

private:
    // FULL2: the COMP channel's bus offset — the part of
    // the reported latency the inserts themselves add
    // (960 for MOD-in A, 0 otherwise). The comp channel
    // is delayed by it so both channels arrive at the bus
    // together; latencySamples() reports the same figure.
    int modExtraTarget() const
    {
        return (adt_.enabled() && !modPhaser_) ? adt_.latencySamples() : 0;
    }
    float readCompRing(const std::vector<float>& ring, int delay) const
    {
        return ring[(size_t)((compWrite_ - delay) & compMask_)];
    }
    // TEST8: the dry arm's ring — written in lockstep
    // with the comp ring (same write index, same mask),
    // read at the limiter's oversampling delay PLUS the
    // comp channel's current bus offset, so a dry sample
    // and the comp sample it blends against are the same
    // input sample.
    float readDryRing(const std::vector<float>& ring, int delay) const
    {
        return ring[(size_t)((compWrite_ - delay) & compMask_)];
    }

    double fs_ = 48000.0;
    Ba6aLimiter limiter_;
    AdtChain adt_;
    Phaser90 phaser_;
    bool modPhaser_ = false;   // v55: false = A (ADT), true = P (phaser)
    float modBlend_ = 0.0f;    // v55: flavour blend, 0 = A .. 1 = P
    bool prepared_ = false;    // v1.4.1: prepare() has built the chain
    double preparedFs_ = 0.0;  // v1.4.1: the rate it was built at
    // FULL2 bus state (see the setters above).
    float compMixTarget_ = 1.0f, compMixCur_ = 1.0f;
    float modMixTarget_ = 1.0f, modMixCur_ = 1.0f;
    // The MOD channel's engagement gate: 1 while MOD is
    // in (settled), 0 while out; slews linearly over the
    // inserts' own 30 ms engage span on MOD edges, so the
    // channel fades with its machine and is exactly 0
    // settled-out (CH2 contributes nothing, per spec).
    float modGate_ = 0.0f;
    // The COMP channel's alignment ring: the module
    // output, read at the current bus offset. Sized in
    // prepare() for the ADT's 20 ms plus margin (power
    // of two).
    std::vector<float> compRingL_, compRingR_;
    int compMask_ = 0;
    int compWrite_ = 0;
    // The v44 offset crossfade, retargeted onto the COMP
    // channel: when the offset target changes (MOD
    // toggle, A/P flavour edge) the comp channel's reads
    // crossfade old -> new over 30 ms — the inserts' own
    // engagement span.
    int modDelayA_ = 0, modDelayB_ = 0;
    float modTapFade_ = 1.0f;   // 1 = settled on modDelayB_
    // TEST8: the dry arm's rings — the chain input,
    // written every sample beside the comp ring (see
    // readDryRing). Sized with the comp rings in
    // prepare(): the deepest read is the oversampling
    // delay + the ADT offset, inside the same sizing.
    std::vector<float> dryRingL_, dryRingR_;
    // The bus staging state: through TEST7 this held the
    // make-up law's trackers and slewed gain; TEST8
    // removed the law — the only survivor is the
    // telemetry readout, now the fixed staging trim.
    float busMakeupDb_ = 0.0f;
    // Insert-feed introspection (the send tap).
    float lastFeedL_ = 0.0f, lastFeedR_ = 0.0f;
};

} // namespace osc72
