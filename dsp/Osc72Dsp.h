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

// ─────────────────────────────────────────────────────────────────────
// IronStage — one transformer in the BA-6A path (input,
// interstage, output voicings). Behavioural flux model: the
// core flux state is a ~30 Hz magnetisation average of the
// signal (real flux is the signal's integral, so colour lives
// at LF and the top stays clean); flux gates a soft odd/even
// waveshape. The model's ONLY memory is that flux state,
// discharging in ~5 ms — colour starts and stops with the
// signal (criterion 3, dryness: no ring, no tail). Voiced FAT
// per criteria 1/2 (density/weight, always-on colour): these
// stages run slightly hotter at LF than a mint unit's <0.4 %
// ticket — the deviation is documented in the design doc §8.
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kIronFluxTauSec = 0.0053f; // ~30 Hz flux average

class IronStage {
public:
    enum class Voicing { input, interstage, output };
    explicit IronStage(Voicing v = Voicing::input) { setVoicing(v); }
    void setVoicing(Voicing v) {
        switch (v) {
            case Voicing::input:      kFlux_ = 0.30f; odd_ = 0.20f; asym_ = 0.012f; knee_ = 0.05f; break;
            case Voicing::interstage: kFlux_ = 0.18f; odd_ = 0.11f; asym_ = 0.007f; knee_ = 0.05f; break;
            case Voicing::output:     kFlux_ = 0.46f; odd_ = 0.30f; asym_ = 0.018f; knee_ = 0.06f; break;
        }
    }
    void prepare(double fs) { fs_ = fs > 0.0 ? fs : 48000.0; }
    void reset() { flux_ = 0.0f; }
    float process(float x) {
        flux_ += (x - flux_) * tauCoef(kIronFluxTauSec, fs_);
        const float aFlux = flux_ < 0.0f ? -flux_ : flux_;
        if (aFlux < 1.0e-6f) return x;
        const float drive = 1.0f + kFlux_ * aFlux;
        float z = clampf(x * drive, -3.0f, 3.0f);
        z += asym_ * z * z;                        // even-order warmth
        const float shaped = z + odd_ * (tanhPade(z) - z);
        const float gate = aFlux / (aFlux + knee_);
        return x + gate * (shaped - x);
    }
    float flux() const { return flux_; }
private:
    double fs_ = 48000.0;
    float flux_ = 0.0f;
    float kFlux_ = 0.30f, odd_ = 0.20f, asym_ = 0.012f, knee_ = 0.05f;
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
inline constexpr float kBa6aCeilingDb   = -16.0f;
inline constexpr float kBa6aDbPerVolt   = 27.0f; // GR slope on the shaped sense (v1.1.2, below)
inline constexpr float kBa6aDetToeEnv   = 0.38f; // 6H6 square-law toe corner (sense-domain volts)
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
inline float ba6aDriveDb(float knob01) {
    const float k = clampf(knob01, 0.0f, 1.0f);
    auto sig = [](float x) { return 1.0f / (1.0f + std::exp(-9.5f * (x - 0.82f))); };
    const float g = (sig(k) - sig(0.0f)) / (sig(1.0f) - sig(0.0f));
    return -5.0f + 56.0f * g;
}
// Detector sense: the 6H6 is fed from its own winding/tap,
// scaled ~+7 dB over the output-terminal level (detector
// feeds are scaled to the diode's needs in this family; the
// verge at the terminals therefore sits at env ~0.072).
inline constexpr float kBa6aDetSense = 2.2f;
// The vari-mu law, from the tube physics (research §A.3;
// deepened by the v1.1.2 circuit study,
// docs/engine-v5-circuit-study.md §1). Two documented
// shapes stack between the output level and the gain:
//  (a) the 6H6's sense characteristic: a diode detector is
//      SQUARE-LAW at small envelopes (detected output
//      proportional to the input VOLTAGE SQUARED) and only
//      approaches linear at large ones — the control volts
//      themselves arrive with a toe. Modelled as the sense
//      shaping env^2/(env + e0), which is ~env^2/e0 below
//      the corner e0 and ~env above it.
//  (b) the 6SK7's remote-cutoff law: variable-pitch grid
//      sections cut off progressively over a long grid
//      base, so the dB-per-volt slope at the START of an
//      excursion is small (few sections near cutoff) and
//      rises through the mid-range. On the shaped sense
//      the composite reads as a slope that climbs from the
//      verge and firms with depth.
// v1.1.1's law was a constant 21 dB/V from the first
// millivolt over the verge — no toe. Nathan's verdict:
// "the ratio up front feels too strong... the Fairchild
// feels gentler, less slammy at the front of the signal"
// (its attack is FASTER; the gentleness is the law's
// shape). The reshaped law (slope 27 on the shaped sense)
// measures, static sweep at knob 0.5: incremental ratio
// 1.35:1 at ~1 dB GR (was 1.47), ~1.7 at 3 dB (was 1.84),
// 4.14:1 at 18 dB (was 3.57) — softer at the front,
// firmer at depth, working depth kept (GR at -10 dBFS in:
// 11.6 dB, was 11.8-12.3). Still no threshold corner and
// still a genuine feedback ceiling; capped at the pair's
// useful ~38 dB of gain range. Study §4 records the
// physics bound honestly: in a feedback law the mid-depth
// ratio at fixed working depth is set by env x slope at
// the equilibrium — softness is redistributed (front down,
// top up), not conjured.
inline float ba6aSenseShape(float envLin) {
    return envLin * envLin / (envLin + kBa6aDetToeEnv);
}
inline float ba6aVariMuDb(float envLin) {
    const float over = ba6aSenseShape(envLin) - ba6aSenseShape(dbToLinear(kBa6aCeilingDb));
    if (over <= 0.0f) return 0.0f;
    const float gr = kBa6aDbPerVolt * over;
    return gr > kBa6aGrCapDb ? kBa6aGrCapDb : gr;
}
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
inline constexpr float kBa6aDetSlowGrabTauSec  = 0.002385f; // v45: 0.9 ms x2.65 (v46)
inline constexpr float kBa6aDetSlowCreepTauSec = 0.00795f;  // v45: 3.0 ms x2.65 (v46)
inline constexpr float kBa6aDetSlowDim         = 2.6f;   // dim-panel slowdown factor
inline float ba6aDetSlowTau(float peak, float env) {
    const float demand = clampf(ba6aVariMuDb(peak) / 25.0f, 0.0f, 1.0f);
    const float approach = peak > 1.0e-6f ? clampf(env / peak, 0.0f, 1.0f) : 1.0f;
    const float base = kBa6aDetSlowGrabTauSec
        + (kBa6aDetSlowCreepTauSec - kBa6aDetSlowGrabTauSec) * approach;
    return base * (1.0f + kBa6aDetSlowDim * (1.0f - demand));
}
inline constexpr float kBa6aAttFastTauSec  = 0.000252f; // control attack pole, FAST (v45: 90 µs x2.8, v46)
inline constexpr float kBa6aAttSlowTauSec  = 0.00477f;  // control attack pole, SLOW (v45: 1.8 ms x2.65, v46)
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

    // Sidechain low-cut (v41, Nathan): a one-pole high-pass
    // in the DETECTOR FEED ONLY — per channel on the post-
    // iron sense signal, ahead of the peak combine. The
    // audio path never sees it; it only decides how much
    // low end the 6H6 detector reacts to. 10 Hz is the
    // bottom stop: there the filter is OUT of the path
    // (the sense passes bit-identical), so the shipped
    // 1.1.2 sound and every ballistics pin hold exactly
    // at the default. The LP states track the sense even
    // while bypassed, so sweeping up from the stop never
    // steps the envelope.
    void setScCutoff(float hz)
    {
        scCutoffHz_ = clampf(hz, 10.0f, 200.0f);
        scCoef_ = tauCoef(1.0f / (6.283185307179586f * scCutoffHz_), fs_);
    }
    float scCutoff() const { return scCutoffHz_; }

    void process(float inL, float inR, float& outL, float& outR);

    float gainReductionDb() const { return grDb_; }
    float appliedMakeupDb() const { return makeupDb_; }
    float driveDb() const { return ba6aDriveDb(knobS_); }

private:
    float processChannel(float x, int ch, float grLin, float driveLin);

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
    float scCutoffHz_ = 10.0f;        // v41: detector sidechain low-cut (Hz)
    float scCoef_ = 0.0f;             // v41: its one-pole LP coefficient
    float scLpL_ = 0.0f, scLpR_ = 0.0f; // v41: per-channel LP states (HP = sense - LP)
    float detEnv_ = 0.0f;             // detector envelope (linear peak)
    float stress_ = 0.0f;             // v45: T4 memory state (dB of sustained GR)
    float lastPreL_ = 0.0f, lastPreR_ = 0.0f; // post-iron, pre-makeup (detector source)
    float prePeak_ = 0.0f;            // slow-decay peak of the pre-makeup output (make-up ceiling)
    float grSingle_ = 0.0f;           // SINGLE-network state (dB)
    float grDual1_ = 0.0f, grDual2_ = 0.0f;   // DUAL-network states (dB shares)
    float grDb_ = 0.0f;
    float avgGrDb_ = 0.0f;
    float makeupDb_ = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────
// AdtChain — the MOD insert: Abbey Road Reel ADT in its widest
// documented configuration (research §B; design §4):
//   SRC  = the source itself, delayed by the 20 ms base offset
//          (the non-Live arrangement: decks may swing ahead of
//          and behind the source). Reported latency = base.
//   DECK 1 (offset 20 ms): sine LFO ±6 ms @ 0.30 Hz, panned
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
// the 1.1.0 return level exactly.
inline constexpr float kAdtDeckGain    = 0.82f * (2.0f / 3.0f) * 0.90f;
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
// ─────────────────────────────────────────────────────────────────────
inline constexpr float kLimitFadeSec = 0.045f;

class Chain {
public:
    void prepare(double sampleRate);
    void reset();

    void setKnob(float knob01) { limiter_.setKnob(knob01); }
    void setAttackFast(bool fast) { limiter_.setAttackFast(fast); }
    bool attackFast() const { return limiter_.attackFast(); }
    void setReleaseFast(bool fast) { limiter_.setReleaseFast(fast); }
    bool releaseFast() const { return limiter_.releaseFast(); }

    void setLimitIn(bool in);
    bool limitIn() const { return limitTarget_; }
    void setScCutoff(float hz) { limiter_.setScCutoff(hz); }
    float scCutoff() const { return limiter_.scCutoff(); }
    void setModIn(bool in) { adt_.setEnabled(in); }
    bool modIn() const { return adt_.enabled(); }

    int latencySamples() const
    {
        return adt_.enabled() ? adt_.latencySamples() : 0;
    }
    float gainReductionDb() const
    {
        return limiter_.gainReductionDb() * limitFade_;
    }
    const Ba6aLimiter& limiter() const { return limiter_; }
    const AdtChain& adt() const { return adt_; }

    void process(float inL, float inR, float& outL, float& outR);

private:
    double fs_ = 48000.0;
    Ba6aLimiter limiter_;
    AdtChain adt_;
    bool limitTarget_ = true;
    float limitFade_ = 1.0f;
};

} // namespace osc72
