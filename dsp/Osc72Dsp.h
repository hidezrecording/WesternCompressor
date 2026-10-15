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
inline constexpr float kBa6aDbPerVolt   = 21.0f; // GR slope: dB per unit over-verge envelope
inline constexpr float kBa6aGrCapDb     = 46.0f;
// Drive law (design §3.1): the box runs hot by design (its
// ~54 dB of gain normalised so nominal programme lives near
// the limiting verge, as a programme limiter at a transmitter
// does); the knob — the INPUT attenuator — trims around that
// working point on a quadratic taper.
// v1.1.1 (Nathan's ear verdict on 1.1.0: "the input pot does
// not dig into the compression nearly enough — we need to be
// able to hit the input of the compressor much harder"): the
// taper steepens from -3 + 3k + 12k^2 (top +12 dB; programme
// mean GR at max measured 9.9 dB, tone centre 5.7 dB) to
// -3 + 9k + 45k^2 (top +51 dB). Knob 0 is UNCHANGED (-3 dB:
// hot box, near-unity, GR only on hot peaks). Measured with
// the new law: programme mean GR at max 31.5 dB, tone GR at
// centre (k=0.5, -10 dBFS 1 kHz) 12.3 dB — see
// docs/build-report-v40.md.
inline float ba6aDriveDb(float knob01) {
    const float k = clampf(knob01, 0.0f, 1.0f);
    return -3.0f + 9.0f * k + 45.0f * k * k;
}
// Detector sense: the 6H6 is fed from its own winding/tap,
// scaled ~+7 dB over the output-terminal level (detector
// feeds are scaled to the diode's needs in this family; the
// verge at the terminals therefore sits at env ~0.072).
inline constexpr float kBa6aDetSense = 2.2f;
// The vari-mu law, from the tube physics (research §A.3):
// the 6H6 develops a control VOLTAGE proportional to the
// output level (linear volts at the detector), and the
// 6SK7's gain falls roughly linearly in dB per control volt
// (remote-cutoff gm). So GR in dB is proportional to the
// over-verge envelope in VOLTS — no threshold corner, an
// effective ratio that rises continuously with depth, and a
// genuine feedback ceiling (the deeper the excursion, the
// harder the loop pulls back). Capped at the pair's useful
// ~38 dB of gain range.
inline float ba6aVariMuDb(float envLin) {
    const float over = envLin - dbToLinear(kBa6aCeilingDb);
    if (over <= 0.0f) return 0.0f;
    const float gr = kBa6aDbPerVolt * over;
    return gr > kBa6aGrCapDb ? kBa6aGrCapDb : gr;
}
// Ballistics (design §2.2). Attack FAST = stock detector;
// SLOW = the era mod (flagged). Release FAST = SINGLE network
// (90 % ≈ 0.30 s); SLOW = DUAL network (fast partial + slow
// tail, 90 % ≈ 3 s).
inline constexpr float kBa6aDetFastTauSec  = 0.00015f; // detector env charge
inline constexpr float kBa6aDetSlowTauSec  = 0.005f;
inline constexpr float kBa6aDetDecayTauSec = 0.030f;   // detector env decay
inline constexpr float kBa6aAttFastTauSec  = 0.0005f;  // control attack pole
inline constexpr float kBa6aAttSlowTauSec  = 0.012f;
inline constexpr float kBa6aRelSingleTauSec = 0.130f;
inline constexpr float kBa6aRelDualFastTauSec = 0.055f;
inline constexpr float kBa6aRelDualSlowTauSec = 1.500f;
inline constexpr float kBa6aRelDualFastShare  = 0.55f;
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

    float detEnv_ = 0.0f;             // detector envelope (linear peak)
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
// LFOs and deck machinery are unchanged.
inline constexpr float kAdtDeckGain    = 0.82f * (2.0f / 3.0f);
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
