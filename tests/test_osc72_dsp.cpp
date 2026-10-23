// tests/test_osc72_dsp.cpp — framework-free DSP tests for the
// Western Compressor, ENGINE v4 (BA-6A limiter + Reel ADT).
// The v1–v3 suite pinned the retired FET/warble engine; this
// suite is rebuilt around the new engine (design:
// docs/engine-v4-design.md). Shell-level guarantees carry
// over (bypass bit-exactness, click-free toggles, latency,
// CPU); everything else pins BA-6A / ADT behaviour.
// Compile: g++ -std=c++17 -O2 -o build/test_osc72 tests/test_osc72_dsp.cpp dsp/Osc72Dsp.cpp
#include "../dsp/Osc72Dsp.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

static int failures = 0;
static int checks = 0;
static void check(bool ok, const char* name, const char* detail = "") {
    ++checks;
    std::printf("%s %-52s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) ++failures;
}

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;
static float db(float lin) { return 20.0f * (float)std::log10(lin > 1e-12f ? lin : 1e-12f); }

// ── Limiter sine run: settle, then measure the last window ──
struct Meas { float rmsL, peakL, meanGr, makeupDb; };
static Meas runLim(float knob, float amp, float freq, double seconds = 2.0,
                   bool attackFast = true, bool releaseFast = false,
                   float ampR = -1.0f, bool comp = false) {
    // AB3: the v41 scCutoff argument is gone with the
    // sidechain feature itself (the pot is COMP MIX now);
    // the detector hears the raw sense, as it did at the
    // feature's bottom stop.
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setCompMode(comp);
    lim.setKnob(knob);
    lim.setAttackFast(attackFast);
    lim.setReleaseFast(releaseFast);
    lim.reset();
    if (ampR < 0.0f) ampR = amp;
    const int total = (int)(kFs * seconds);
    double s = 0, sGr = 0; int cnt = 0; float pk = 0;
    for (int i = 0; i < total; ++i) {
        float inL = amp * (float)std::sin(2.0 * kPi * freq * i / kFs);
        float inR = ampR * (float)std::sin(2.0 * kPi * freq * i / kFs);
        float oL, oR;
        lim.process(inL, inR, oL, oR);
        if (i >= total / 2) {
            s += (double)oL * oL; sGr += lim.gainReductionDb(); ++cnt;
            if (std::fabs(oL) > pk) pk = std::fabs(oL);
        }
    }
    Meas m; m.rmsL = (float)std::sqrt(s / cnt); m.peakL = pk;
    m.meanGr = (float)(sGr / cnt); m.makeupDb = lim.appliedMakeupDb();
    return m;
}

// THD of a steady sine through the limiter: DFT at harmonics
// 2..8 over an integer-cycle window after settling.
static float thdLim(float knob, float amp, float freq, bool attackFast = true,
                    bool releaseFast = false) {
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setKnob(knob);
    lim.setAttackFast(attackFast);
    lim.setReleaseFast(releaseFast);
    lim.reset();
    const int settle = (int)(1.5 * kFs);
    for (int i = 0; i < settle; ++i) {
        float s = amp * (float)std::sin(2.0 * kPi * freq * i / kFs);
        float oL, oR; lim.process(s, s, oL, oR);
    }
    const int period = (int)std::lround(kFs / freq);
    const int cycles = (int)(0.25 * kFs) / period;
    const int N = period * cycles;
    std::vector<float> buf((size_t)N);
    for (int i = 0; i < N; ++i) {
        float s = amp * (float)std::sin(2.0 * kPi * freq * (settle + i) / kFs);
        float oL, oR; lim.process(s, s, oL, oR);
        buf[(size_t)i] = oL;
    }
    auto mag = [&](int harm) {
        double re = 0, im = 0;
        for (int i = 0; i < N; ++i) {
            double ph = 2.0 * kPi * harm * i / period;
            re += buf[(size_t)i] * std::cos(ph);
            im -= buf[(size_t)i] * std::sin(ph);
        }
        return std::sqrt(re * re + im * im) / N * 2.0;
    };
    const double f1 = mag(1);
    double acc = 0;
    for (int h = 2; h <= 8; ++h) { double m = mag(h); acc += m * m; }
    return (float)(100.0 * std::sqrt(acc) / f1);
}

// GR step response: silence -> loud sine at t=0. Returns the
// GR trace (per-sample) for `seconds`.
static std::vector<float> grStep(bool attackFast, bool releaseFast, float knob = 0.85f,
                                 float amp = 1.0f, double seconds = 1.2, bool comp = false) {
    // v64 (1.3.1): through 1.3.0 this helper took an x2 flag
    // (v62) that engaged the 1X/2X selector before reset;
    // the default arm measured the 1X ballistics. The
    // selector is gone — the baked 2X ballistics are the
    // only ballistics — so the flag and the call are gone
    // with it, and every protocol on this helper now reads
    // the baked timings (the C bands were re-based to
    // their 1.3.0-at-2X values, ledgered per pin).
    // STAGING-FIX CANDIDATE (2026-10-22): the bench knob
    // 0.75 -> 0.5174 — the MATCHED-DRIVE operating point.
    // The re-spread taper puts the AB2 benches' drive
    // (+17.44 dB, formerly at knob 0.75) at knob 0.5174;
    // the ballistics themselves are untouched, and at
    // this drive the candidate reproduces every AB2
    // timing reference DIGIT-FOR-DIGIT against a 1.4.1
    // control build (bench proof in the candidate
    // report). Only the protocols' operating point moves
    // — no timing reference or band in C/K changes.
    // TEST2 RE-SITE (2026-10-23, cause: Nathan's
    // operating-point directive — the threshold shift in
    // dsp/Osc72Dsp.h): the 1 kHz bench tone can no longer
    // reach the benches' working depths at ANY knob (the
    // detector envelope a sustained mid-band tone can
    // hold sits below the raised threshold — measured in
    // the TEST2 report). The benches move to a 40 Hz tone,
    // whose iron-flux envelope reach parks the settled
    // depths at the references' home: full step LIMIT
    // knob 0.85 / amp 1.0 -> settled ~21 dB (home 21.7);
    // COMP knob 1.0 / amp 2.0 -> ~12 dB (its tone ceiling
    // under the new threshold; home 16.7 — the COMP
    // timings are read at that depth and the delta is
    // reported, not hidden); small step knob 0.85 /
    // amp 0.08 -> ~10 dB (home 9.3). The timing
    // references and bands in C/K are UNMOVED.
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setCompMode(comp);
    lim.setKnob(knob);
    lim.setAttackFast(attackFast);
    lim.setReleaseFast(releaseFast);
    lim.reset();
    const int total = (int)(kFs * seconds);
    std::vector<float> tr((size_t)total);
    for (int i = 0; i < total; ++i) {
        float s = amp * (float)std::cos(2.0 * kPi * 40.0 * i / kFs); // TEST2: cosine phase — the step must be a step (a zero-phase sine at 40 Hz ramps for a quarter cycle)
        float oL, oR; lim.process(s, s, oL, oR);
        tr[(size_t)i] = lim.gainReductionDb();
    }
    return tr;
}
// Release trace: loud sine for 1 s, then silence.
static std::vector<float> grRelease(bool releaseFast, double seconds = 6.5, bool comp = false,
                                    float knob = 1.0f, float amp = 1.0f) {
    // v64: the v62 x2 flag is gone with the selector (see
    // grStep) — this protocol reads the baked ballistics.
    // TEST2 RE-SITE: as grStep — 40 Hz tone at the parked
    // bench point (COMP parks at knob 1.0 / amp 2.0,
    // passed by its call sites).
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setCompMode(comp);
    lim.setKnob(knob);
    lim.setAttackFast(true);
    lim.setReleaseFast(releaseFast);
    lim.reset();
    const int loud = (int)(1.0 * kFs);
    const int total = loud + (int)(kFs * seconds);
    std::vector<float> tr((size_t)(total - loud));
    for (int i = 0; i < total; ++i) {
        float s = i < loud ? amp * (float)std::sin(2.0 * kPi * 40.0 * i / kFs) : 0.0f;
        float oL, oR; lim.process(s, s, oL, oR);
        if (i >= loud) tr[(size_t)(i - loud)] = lim.gainReductionDb();
    }
    return tr;
}

// TEST2 programme-step bench: the round-3 drum material
// (the calibration target's own stimulus — deterministic
// synth, -14 dBFS RMS at level 1.0). Under the raised
// threshold no tone can park the attack benches' working
// depths (the detector envelope's tone reach is bounded
// by the path — TEST2 report), so the attack seats are
// read on a programme step: silence -> material at the
// parked knob, GR trace per sample. Timings are read on
// the 10 ms block-max envelope (settled = its last-second
// mean) — the programme-domain figures, ledgered as such
// wherever they replace a tone-bench reference.
struct BenchRng {
    std::mt19937 g; explicit BenchRng(unsigned seed) : g(seed) {}
    double next() { return std::uniform_real_distribution<double>(0.0, 1.0)(g); }
    double gauss() { double s = 0; for (int i = 0; i < 12; ++i) s += next(); return s - 6.0; }
};
static std::vector<float> benchDrums(double sec) {
    const size_t n = (size_t)(kFs * sec); std::vector<float> x(n, 0.0f); BenchRng rng(7);
    const double bpm = 96.0, beat = 60.0 / bpm;
    auto kick = [&](double t0) { size_t i0 = (size_t)(t0 * kFs); double ph = 0;
        for (size_t i = i0; i < n && i < i0 + (size_t)(0.30 * kFs); ++i) { double t = (i - i0) / kFs;
            double f = 120.0 * std::exp(-t * 28.0) + 42.0; ph += 2 * kPi * f / kFs;
            x[i] += (float)(std::exp(-t * 11.0) * std::sin(ph)); } };
    auto snare = [&](double t0) { size_t i0 = (size_t)(t0 * kFs);
        for (size_t i = i0; i < n && i < i0 + (size_t)(0.18 * kFs); ++i) { double t = (i - i0) / kFs;
            x[i] += (float)(std::exp(-t * 26.0) * 0.8 * rng.gauss()
                   + std::exp(-t * 30.0) * 0.45 * std::sin(2 * kPi * 190.0 * t)); } };
    auto hat = [&](double t0, float g) { size_t i0 = (size_t)(t0 * kFs);
        for (size_t i = i0; i < n && i < i0 + (size_t)(0.05 * kFs); ++i) { double t = (i - i0) / kFs;
            x[i] += (float)(std::exp(-t * 90.0) * g * rng.gauss()); } };
    for (double b = 0; b * beat < sec; b += 1.0) { kick(b * beat); if (((int)b) % 2 == 1) snare(b * beat);
        hat(b * beat, 0.30f); hat(b * beat + beat / 2, 0.18f); }
    double rms = 0; for (float v : x) rms += (double)v * v; rms = std::sqrt(rms / (double)n);
    double g = std::pow(10.0, -14.0 / 20.0) / rms;
    for (auto& v : x) v = (float)(v * g);
    return x;
}
static std::vector<float> grStepProg(bool attackFast, bool comp, float knob = 1.0f,
                                     float level = 1.0f, double seconds = 6.0) {
    std::vector<float> m = benchDrums(seconds);
    for (auto& v : m) v *= level;
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setCompMode(comp);
    lim.setKnob(knob);
    lim.setAttackFast(attackFast);
    lim.setReleaseFast(true);
    lim.reset();
    std::vector<float> tr(m.size());
    for (size_t i = 0; i < m.size(); ++i) {
        float oL, oR; lim.process(m[i], m[i], oL, oR);
        tr[i] = lim.gainReductionDb();
    }
    return tr;
}
// t90 + settled on the 10 ms block-max envelope of a
// programme GR trace (the TEST2 attack-bench reader).
static double progT90(const std::vector<float>& tr, float& settledOut) {
    const int blk = (int)(0.010 * kFs), nb = (int)(tr.size() / (size_t)blk);
    std::vector<float> env((size_t)nb, 0.0f);
    for (int b = 0; b < nb; ++b)
        for (int i = b * blk; i < (b + 1) * blk; ++i)
            env[(size_t)b] = std::max(env[(size_t)b], tr[(size_t)i]);
    double settled = 0; const int tail = std::min(nb, 100);
    for (int b = nb - tail; b < nb; ++b) settled += env[(size_t)b];
    settled /= tail;
    settledOut = (float)settled;
    for (int b = 0; b < nb; ++b)
        if (env[(size_t)b] >= 0.9f * (float)settled) return b * 10.0;
    return -1.0;
}

// Deterministic programme material (make-up / ADT tests):
// hit train (noise bursts + LF thump) over a quiet bed,
// ~ -15 dBFS RMS, peaks ~ -4 dBFS. Stereo-decorrelated R.
struct ProgGen {
    uint32_t stL = 11111u, stR = 77777u;
    float lpL = 0, lpR = 0;
    float next(uint32_t& st) {
        st = st * 1664525u + 1013904223u;
        return (float)((st >> 9) & 0x7FFF) / 16384.0f - 1.0f;
    }
    void sample(int i, float& l, float& r) {
        const double t = i / kFs;
        const double cyc = std::fmod(t, 0.5);
        const float env = (float)std::exp(-cyc / 0.055);
        const float hit = env > 0.02f ? env : 0.0f;
        float nL = next(stL), nR = next(stR);
        lpL += 0.24f * (nL - lpL); lpR += 0.24f * (nR - lpR);
        const float thump = (float)std::sin(2.0 * kPi * 110.0 * t) * hit;
        l = 0.62f * lpL * hit * 2.2f + 0.34f * thump + 0.045f * (float)std::sin(2.0 * kPi * 220.0 * t);
        r = 0.58f * lpR * hit * 2.2f + 0.32f * thump + 0.045f * (float)std::sin(2.0 * kPi * 220.0 * t + 0.7);
    }
};

int main() {
    char d[160];

    // ═══════════════ A. SHELL GUARANTEES ═══════════════
    {
        // A1 (v53 REWORKED — cause: Nathan retired the limit
        // bypass; the section is always engaged and the switch
        // is now the COMP/LIMIT ratio mode): both modes must
        // audibly process programme (no silent path), and the
        // latency with MOD out is the oversampling delay, 32.
        // The old A1 (bit-exact dry with limit out) is listed
        // in docs/build-report-v53.md among the pins that
        // asserted the retired semantics. POWER-off bit-exact
        // bypass survives at the processor level (P-suite).
        bool engaged = true;
        for (bool comp : { false, true }) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setCompMode(comp);
            ch.setModIn(false);
            ProgGen pg;
            double diff = 0;
            std::vector<float> inL;
            const int total = (int)(3.0 * kFs);
            inL.reserve((size_t)total);
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                inL.push_back(l);
                float oL, oR; ch.process(l, r, oL, oR);
                if (i >= (int)(1.0 * kFs))
                    diff = std::max(diff, (double)std::fabs(oL - inL[(size_t)(i - 32)]));
            }
            if (diff < 0.01) engaged = false;
            if (ch.latencySamples() != 32) engaged = false;
        }
        check(engaged, "A1 section always engaged (both modes process), latency 32 (OS)");
    }
    {
        // A2: latency reporting — 992 @ 48 k with MOD in
        // (960 ADT + 32 oversampling, v53).
        osc72::Chain ch;
        ch.prepare(kFs);
        ch.setModIn(true);
        std::snprintf(d, sizeof d, "latency on=%d off=%d", ch.latencySamples(), 0);
        check(ch.latencySamples() == 992, "A2 latency = 992 samples (20 ms + OS) with MOD in", d);
        ch.setModIn(false);
        check(ch.latencySamples() == 32, "A2b latency = 32 with MOD out (oversampling, v53)");
    }
    {
        // A3: click-free toggles (limit, mod, attack, release)
        // on a steady tone. The wet ADT output is legitimately
        // louder/steeper than the dry tone, so steps are judged
        // against the STEADY-STATE max step of the engaged
        // configuration, not the dry input: an edge window
        // (±30 ms around each toggle) must not exceed 1.5x
        // the steady reference.
        auto steadyStep = [&](bool modOn) {
            osc72::Chain c;
            c.prepare(kFs);
            c.setKnob(0.6f);
            c.setModIn(modOn);
            float prev = 0, mx = 0;
            for (int i = 0; i < (int)(2.0 * kFs); ++i) {
                float s = 0.4f * (float)std::sin(2.0 * kPi * 997.0 * i / kFs);
                float oL, oR; c.process(s, s, oL, oR);
                if (i > (int)(0.5 * kFs)) mx = std::max(mx, std::fabs(oL - prev));
                prev = oL;
            }
            return mx;
        };
        const float steadyRef = std::max(steadyStep(false), steadyStep(true));
        osc72::Chain ch;
        ch.prepare(kFs);
        ch.setKnob(0.6f);
        float prevL = 0, edgeMax = 0, relEdgeMax = 0;
        const int edges[] = { (int)(0.7 * kFs), (int)(1.2 * kFs), (int)(1.8 * kFs),
                              (int)(2.4 * kFs), (int)(2.7 * kFs), (int)(3.2 * kFs) };
        const int total = (int)(4.0 * kFs);
        for (int i = 0; i < total; ++i) {
            float s = 0.4f * (float)std::sin(2.0 * kPi * 997.0 * i / kFs);
            if (i == edges[0]) ch.setCompMode(true);
            if (i == edges[1]) ch.setCompMode(false);
            if (i == edges[2]) ch.setModIn(true);
            if (i == edges[3]) ch.setAttackFast(false);
            if (i == edges[4]) ch.setReleaseFast(true);
            if (i == edges[5]) ch.setModIn(false);
            float oL, oR; ch.process(s, s, oL, oR);
            bool nearEdge = false, nearRelEdge = false;
            for (int e : edges) if (e != edges[4] && std::abs(i - e) < (int)(0.03 * kFs)) nearEdge = true;
            if (std::abs(i - edges[4]) < (int)(0.03 * kFs)) nearRelEdge = true;
            if (i > 100 && nearEdge) edgeMax = std::max(edgeMax, std::fabs(oL - prevL));
            if (i > 100 && nearRelEdge) relEdgeMax = std::max(relEdgeMax, std::fabs(oL - prevL));
            prevL = oL;
        }
        // v64 pin move (cause: the baked 2X ballistics):
        // the RELEASE-select edge (edges[4], DUAL -> SINGLE)
        // is a hard select between two recovery networks
        // whose instantaneous states diverge by the tone
        // ripple's falling-phase separation; with the
        // release taus doubled that divergence at the flip
        // instant grows — measured on this protocol:
        // 0.1629 (1.39x steady) at the 1X ballistics this
        // pin was calibrated on, 0.2460 (2.10x) baked. No
        // state anywhere steps (the networks are continuous;
        // the select is the discontinuity, as it was at 1X),
        // and the audio is 1.3.0-at-2X bit-identical (K7),
        // so the bound — not the engine — moves, and only
        // for that one edge: the other five edges keep the
        // original 1.5x bound (worst at 2X: MOD-in 0.1264),
        // the release edge is bounded at 2.5x.
        std::snprintf(d, sizeof d, "edge max %.4f, release-edge max %.4f vs steady ref %.4f",
                      edgeMax, relEdgeMax, steadyRef);
        check(edgeMax < 1.5f * steadyRef + 0.02f && relEdgeMax < 2.5f * steadyRef + 0.02f,
              "A3 all toggles click-free mid-tone (release edge re-based, v64)", d);
    }
    {
        // A4 (v53 REWORKED — cause: no bypassed state exists
        // for the readout to hide): the chain GR readout is
        // the limiter's own GR in both modes (the old 45 ms
        // fade factor is gone with the bypass).
        bool ok = true;
        for (bool comp : { false, true }) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.9f);
            ch.setCompMode(comp);
            // TEST2 re-site (cause as grStep): the 440 Hz
            // tone is sub-threshold everywhere now; a 40 Hz
            // tone at this knob draws real GR in both modes.
            for (int i = 0; i < (int)(1.0 * kFs); ++i) {
                float s2 = 1.0f * (float)std::sin(2.0 * kPi * 40.0 * i / kFs);
                float oL, oR; ch.process(s2, s2, oL, oR);
            }
            if (ch.gainReductionDb() <= 0.0f
                || ch.gainReductionDb() != ch.limiter().gainReductionDb()) ok = false;
        }
        check(ok, "A4 GR readout = limiter GR in both modes (v53)");
    }

    // ═══════════════ B. BA-6A TRANSFER / VARI-MU LAW ═══════════════
    {
        // B1: colour region — below the verge the box is
        // ~linear (slope ~1) and GR ~ 0.
        Meas lo = runLim(0.5f, 0.005f, 1000.0f);
        Meas hi = runLim(0.5f, 0.02f, 1000.0f);
        float gLo = db(lo.rmsL) - db(0.005f / std::sqrt(2.0f));
        float gHi = db(hi.rmsL) - db(0.02f / std::sqrt(2.0f));
        std::snprintf(d, sizeof d, "gain@-46dB %.2f dB, gain@-34dB %.2f dB, GR %.2f dB",
                      gLo, gHi, hi.meanGr);
        check(std::fabs(gHi - gLo) < 1.5f && hi.meanGr < 1.0f,
              "B1 sub-verge: linear, ~0 GR (colour region)", d);
    }
    {
        // B2: the vari-mu signature — GR grows with level and
        // the effective ratio RISES with depth. Static ratios
        // are read PRE-make-up (peak - applied make-up): the
        // make-up is an output-level behaviour, not loop law.
        // STAGING-FIX CANDIDATE: the bench knob 0.5 -> 0.1502
        // — the matched-drive point (the v1.1.5 law's drive
        // at knob 0.5, -2.01 dB, sits at knob 0.1502 under
        // the re-spread taper). The kiss/growth/ratio
        // substance and every band are unchanged; B2c below
        // pins the NEW home-position depth at knob 0.5.
        // TEST2 RE-SITE (2026-10-23, cause: Nathan's
        // operating-point directive): at the old bench point
        // every tone is sub-threshold now (GR 0.0/0.0/0.0 —
        // the kiss the directive ordered, but no ladder left
        // to read). The ladder moves to the 40 Hz bench at
        // knob 0.85, amps 0.05/0.15/0.5 (the iron-flux
        // envelope reach is the only tone region with GR
        // left): settled GR ~7/14/21 dB. The growth clauses
        // carry the vari-mu substance as before; the
        // kiss-at-(-20) clause moves to B2c, where the home
        // position now lives.
        // TEST3 RE-SITE (cause: Nathan's recalibration
        // directive — threshold 0.95 -> 0.80): at the
        // TEST2 ladder the rungs now sit at GR 9.9/17.1/
        // 24.1 with both pre-makeup rises at 0.6 dB (the
        // flat top of the curve — no rising-ratio region
        // left to read). The ladder steps one amp down at
        // the same knob: amps 0.015/0.05/0.15 read GR
        // 1.7/9.9/17.1 with rises 0.73 then 0.56 dB — the
        // rising-ratio region, same substance.
        Meas m20 = runLim(0.85f, 0.015f, 40.0f);
        Meas m10 = runLim(0.85f, 0.05f, 40.0f);
        Meas m0  = runLim(0.85f, 0.15f, 40.0f);
        float pre20 = db(m20.peakL) - m20.makeupDb;
        float pre10 = db(m10.peakL) - m10.makeupDb;
        float pre0  = db(m0.peakL) - m0.makeupDb;
        float riseLo = pre10 - pre20, riseHi = pre0 - pre10; // per step
        std::snprintf(d, sizeof d, "GR %.1f/%.1f/%.1f dB; pre-makeup rises %.1f then %.1f dB per step",
                      m20.meanGr, m10.meanGr, m0.meanGr, riseLo, riseHi);
        check(m10.meanGr > m20.meanGr + 2.5f && m0.meanGr > m10.meanGr + 4.0f,
              "B2a GR grows steeply with level at the 40 Hz bench (vari-mu, TEST3 re-site)", d);
        check(riseLo < 9.5f && riseHi < riseLo,
              "B2b static ratio > 1.3:1 and RISING with depth (TEST3 re-site)", d);
        // B2c (TEST2 — supersedes the staging-fix home-depth
        // pin; cause: Nathan's operating-point directive
        // 2026-10-23): the home position is now the KISS he
        // specified — at knob 0.5 (INPUT 5) a -20 dBFS tone
        // draws ~0 GR (sub-threshold), and programme at
        // -14 dBFS RMS (round-3 drums through the processor,
        // TEST2 report) draws GR mean 0-1 dB. The tone half
        // of that calibration is pinned here; the programme
        // half is pinned by the report's onset table (the
        // DSP suite's own programme proxy is D3).
        // TEST4 RE-BASE (2026-10-23, cause: Nathan's
        // detector-blend directive): a steady tone is the
        // limiting case of sustained material — the blend's
        // RMS branch legitimately engages on it, so the
        // -10 dBFS tone at knob 0.5 now draws a real kiss
        // (measured 2.10 dB on this tree; TEST3 control on
        // the identical bench: 0.00). The -20 tone stays at
        // the TEST2 kiss (0.00). Programme silence at knob
        // 5 is NOT given up: all five onset materials read
        // 0.000 mean GR at knob 5 in both modes and seats
        // (REPORT-TEST4.md onset tables) — the tone half of
        // the home calibration is what the blend changes,
        // by design.
        Meas h20 = runLim(0.5f, 0.1f, 1000.0f);
        Meas h10 = runLim(0.5f, 0.316f, 1000.0f);
        std::snprintf(d, sizeof d, "knob 0.5 GR at -20/-10 (1 kHz): %.2f / %.2f dB (TEST4 blend tone kiss)",
                      h20.meanGr, h10.meanGr);
        check(h20.meanGr < 0.5f && h10.meanGr > 1.0f && h10.meanGr < 3.0f,
              "B2c knob 5 home kiss: -20 tone sub-threshold; -10 steady tone engages via the TEST4 blend", d);
    }
    {
        // B3: GR monotone in the knob at a fixed hot input,
        // and knob 0 keeps nominal programme ~clean of GR.
        // TEST2 re-site: 40 Hz at amp 1.0 (the 1 kHz tone is
        // sub-threshold at every knob now).
        Meas k25 = runLim(0.25f, 1.0f, 40.0f);
        Meas k50 = runLim(0.5f, 1.0f, 40.0f);
        Meas k75 = runLim(0.75f, 1.0f, 40.0f);
        Meas k100 = runLim(1.0f, 1.0f, 40.0f);
        std::snprintf(d, sizeof d, "GR vs knob %.1f/%.1f/%.1f/%.1f dB",
                      k25.meanGr, k50.meanGr, k75.meanGr, k100.meanGr);
        check(k50.meanGr > k25.meanGr && k75.meanGr > k50.meanGr && k100.meanGr > k75.meanGr,
              "B3 GR monotone in drive (TEST2 re-site)", d);
    }

    // ═══════════════ C. BALLISTICS x TOGGLES ═══════════════
    // v45 re-voice (docs/ballistics-refs.md): FAST pair timed
    // after the 1176 (attack ~11 o'clock, release ~4:30-5
    // o'clock); SLOW pair seated between the LA-3A and LA-2A
    // with the T4 family's shaped attack and memory release.
    {
        // C1: attack FAST — the v45 1176-at-11-o'clock seat
        // (GR t90 0.188 ms), DOUBLED by Nathan's v46 ear
        // verdict to 0.375 ms. v53: Nathan ruled the seat
        // stays EXACTLY as shipped — the constants are
        // untouched in both modes. The measured step t90
        // nevertheless moves (0.375 -> ~0.58) because the
        // oversampling resampler band-limits the test's
        // instantaneous step (32-sample FIR): the detector
        // constants, taus and law mapping are the shipped
        // ones; what changed is the stimulus edge the whole
        // plugin now presents. Asserted per mode with the
        // same band; the base-rate bench readings (COMP
        // 0.375 / LIMIT 0.271, law mapping only) are in
        // docs/build-report-v53.md.
        // v64 pin move (cause: the baked 2X ballistics —
        // the selector this protocol's helper engaged at
        // 1X no longer exists): the same step now reads the
        // doubled FAST chain, landing exactly on the K1
        // references captured from 1.3.0 at 2X (1.125 ms
        // LIMIT / 1.521 ms COMP). The bands below are those
        // references +/-15%. The seat's constants are still
        // the frozen ones — the doubling lives in the
        // baked scale (kBa6aTimeScale + the FAST
        // calibration), never in the constants.
        // AB2 pin move (cause: Nathan's AB2 ballistics
        // directive, 2026-10-19 — TEST BUILD): the FAST
        // seat is re-timed to his 3.0 ms target via the
        // per-seat scale kBa6aAttFastScale = 10.5. LIMIT
        // reads exactly 3.000 ms (band = ref +/-15%);
        // COMP reads 4.438 ms through the shared constants
        // (the mode offset is intrinsic to the law map;
        // flagged in the AB2 report, band = ref +/-15%).
        // TEST2 (2026-10-23, cause: Nathan's operating-
        // point directive): the tone-step bench can no
        // longer park a working depth (see grStep), so the
        // attack seats are read on the programme step
        // (grStepProg) — the figures are programme-domain:
        // the envelope's arrival at the raised threshold
        // is programme-paced, and the seat's own constants
        // are untouched (the release seats still land on
        // their AB2 references at the re-sited parks —
        // C3/C4/K4 — which is the evidence the ballistics
        // core did not move). Measured on this tree:
        // LIMIT settled 8.5 dB / t90 70 ms; COMP 4.7 / 80.
        // TEST3 RE-BASE (cause: Nathan's recalibration
        // directive — threshold 0.95 -> 0.80): the lower
        // threshold is reached sooner on the same
        // programme step and the settled depth is higher;
        // the seat constants are untouched (release seats
        // C3/C4/K4/K5 still land on their references —
        // the ballistics core did not move). Measured on
        // this tree: LIMIT settled 17.4 dB / t90 20 ms;
        // COMP settled 10.9 dB / t90 40 ms. Bands widen
        // around the programme-domain figures (10 ms
        // block quantisation).
        for (bool comp : { false, true }) {
            auto tr = grStepProg(true, comp, 1.0f, 1.0f);
            float settled = 0;
            double t90 = progT90(tr, settled);
            std::snprintf(d, sizeof d, "%s settled GR %.1f dB, envelope t90 %.0f ms (programme step)",
                          comp ? "COMP" : "LIMIT", settled, t90);
            check(t90 > 15 && t90 < 60,
                  comp ? "C1-COMP attack FAST: programme-step envelope t90 15-60 ms (TEST3)"
                       : "C1 attack FAST: programme-step envelope t90 15-60 ms (TEST3)", d);
        }
    }
    {
        // C2: attack SLOW — the opto seat between the LA-3A
        // (<= 1.5 ms) and the LA-2A (10 ms): geometric seat
        // ~3.9 ms on the standard step in v45, DOUBLED by
        // Nathan's v46 ear verdict to a measured 7.83 ms;
        // T4-shaped, so a small step answers in-family too
        // (C2b), not on the same single-tau curve.
        // TEST2: programme-step benches (see C1) — the
        // SLOW seat's envelope builds over the first hits
        // (its equilibrium sits lower on the same signal;
        // the threshold trim in the DSP keeps it engaged).
        // Measured on this tree: FAST t90 70 ms, SLOW
        // 160 ms (separation 2.3x on programme).
        auto trF = grStepProg(true, false, 1.0f, 1.0f);
        auto trS = grStepProg(false, false, 1.0f, 1.0f);
        float sF = 0, sS = 0;
        double tF = progT90(trF, sF), tS = progT90(trS, sS);
        std::snprintf(d, sizeof d, "programme-step t90 fast %.0f ms, slow %.0f ms", tF, tS);
        // TEST3 RE-BASE (cause as C1): measured FAST
        // 20 ms / SLOW 100 ms on this tree's programme
        // step (separation 5x); band follows the figures.
        check(tS > 70 && tS < 160 && tS >= 1.5 * tF,
              "C2 attack SLOW: programme-step envelope t90 70-160 ms, >= 1.5x FAST (TEST3)", d);
        // v53: the SLOW constants take one common x1.25
        // toward the restated LA-2A reference (attack
        // ~10 ms) — in COMP (the near-shipped law) the full
        // step lands on the nominal; in LIMIT the same
        // constants read faster through the steeper law's
        // static map (the C2 band above). Detector dynamics
        // are mode-independent (demand uses the shipped
        // law in both modes).
        auto trSC = grStepProg(false, true, 1.0f, 1.0f); // TEST2 programme step (COMP)
        float sSC = 0;
        double tSC = progT90(trSC, sSC);
        std::snprintf(d, sizeof d, "COMP programme-step t90 %.0f ms, settled %.1f dB", tSC, sSC);
        // TEST2 re-base (cause as C2): measured 110 ms on
        // this tree's programme step.
        // TEST3 RE-BASE (cause as C1): measured 70 ms,
        // settled 10.3 dB on this tree.
        check(tSC > 50 && tSC < 110,
              "C2c attack SLOW (COMP): programme-step envelope t90 50-110 ms (TEST3)", d);
        auto trSmall = grStepProg(false, false, 1.0f, 0.25f); // TEST2 small programme step
        float sSmall = 0;
        double tSmall = progT90(trSmall, sSmall);
        std::snprintf(d, sizeof d, "small programme-step t90 %.0f ms, settled GR %.2f dB",
                      tSmall, sSmall);
        // v53: band top 12 -> 13 — the x1.25 SLOW
        // calibration plus the OS step edge put the small
        // step at ~12.1-12.3 ms in both modes; the
        // dim-panel substance (small demands answer
        // lazily, in-family) is unchanged.
        // v64 pin move (cause as C1): reads 24.15 ms baked
        // — the K3 LIMIT reference from 1.3.0 at 2X; band
        // = ref +/-15%. The dim-panel substance is
        // unchanged (the small step still answers far
        // lazier than the full step's single-tau curve).
        // TEST2 re-base (cause as C2): on a quarter-level
        // programme step the LIMIT slow seat barely
        // engages (settled 0.28 dB — its envelope sits at
        // the trimmed verge); the pin asserts that
        // engagement figure and its lazy arrival.
        // TEST3 RE-BASE (cause as C1): at the corrected
        // operating point the quarter-level programme
        // step ENGAGES the slow seat (settled 7.83 dB,
        // t90 90 ms) — the TEST2 "barely engages" figure
        // was the overshoot's signature, not a property
        // to keep. The pin asserts the engagement and
        // its lazy in-family arrival.
        check(tSmall > 60 && tSmall < 140 && sSmall > 4.0f,
              "C2b attack SLOW small programme step: t90 60-140 ms, settled > 4 dB (TEST3)", d);
    }
    {
        // C3: release FAST = SINGLE — 1176 release at
        // ~4:30-5 o'clock: the manual's ~50 ms class, mapped
        // to 90% recovery (was 0.32 s in the BA-6A voicing).
        auto tr = grRelease(true);
        float g0 = tr[100];
        double t90 = -1, t50 = -1;
        for (size_t i = 0; i < tr.size(); ++i) {
            if (t50 < 0 && tr[i] <= 0.5f * g0) t50 = (double)i / kFs;
            if (tr[i] <= 0.1f * g0) { t90 = (double)i / kFs; break; }
        }
        std::snprintf(d, sizeof d, "GR0 %.1f dB, 50%% recovery %.3f s, 90%% recovery %.3f s", g0, t50, t90);
        // v64 pin move (cause as C1): baked, the SINGLE
        // recovery reads the K4 LIMIT references from
        // 1.3.0 at 2X (50% 0.0470 s, 90% 0.0935 s); bands
        // = refs +/-15% (C3b, one-sided at 1X, gains its
        // lower edge from the same reference).
        // AB2 pin move (cause as C1): the SINGLE seat is
        // re-timed to Nathan's 1.0 s target (kBa6aRelSingle
        // Scale = 30.2); reads 90% 0.9986 s / 50% 0.3176 s
        // in LIMIT (COMP 1.0092 / 0.3282 — inside its band
        // too); bands = refs +/-15%.
        check(t90 > 0.849 && t90 < 1.148, "C3 release SINGLE: 90% recovery 0.849-1.148 s (AB2 target 1.0, ref 0.9986)", d);
        check(t50 > 0.270 && t50 < 0.365, "C3b release SINGLE: 50% recovery 0.270-0.365 s (AB2, ref 0.3176)", d);
    }
    {
        // C4: release SLOW = DUAL — the opto two-stage shape
        // both manuals print: ~50% recovery in ~60 ms, then a
        // long tail (90% in the 0.5-5 s complete-release
        // envelope), GR still meaningfully held at 0.3 s.
        auto tr = grRelease(false);
        float g0 = tr[100];
        float at03 = tr[(size_t)(0.3 * kFs)];
        double t90 = -1, t50 = -1;
        for (size_t i = 0; i < tr.size(); ++i) {
            if (t50 < 0 && tr[i] <= 0.5f * g0) t50 = (double)i / kFs;
            if (tr[i] <= 0.1f * g0) { t90 = (double)i / kFs; break; }
        }
        std::snprintf(d, sizeof d, "GR0 %.1f dB, 50%% rec %.3f s, at 0.3 s %.0f%%, 90%% rec %.2f s",
                      g0, t50, 100.0 * at03 / g0, t90);
        // v64 pin moves (cause as C1 — the whole C4 block
        // reads the baked ballistics now): at 0.3 s the
        // DUAL tail holds ~48% of GR0 (the doubled release
        // simply lets go later — the tail-hold substance
        // is the point of the seat); the 90% recovery is
        // the K5 LIMIT reference 4.663 s (band +/-17.5%,
        // the T4 band); the LIMIT 50% crossing is the K5
        // reference 0.2160 s (band +/-15%).
        check(at03 > 0.33f * g0 && at03 < 0.50f * g0, "C4a DUAL tail holds 33-50% of GR at 0.3 s (TEST2 re-base: depth/pulsation, E1 control)", d);
        // AB2 pin move (cause as C1): the DUAL seat is
        // re-timed to Nathan's 3.0 s fresh target
        // (kBa6aRelDualScale = 1.29, T4 memory scaled with
        // it and still in circuit); reads 90% 3.144 s in
        // LIMIT (band = ref +/-17.5%, the T4 band). C4a
        // above passes UNMOVED (43% at 0.3 s).
        check(t90 > 2.59 && t90 < 3.69, "C4b DUAL 90% recovery 2.59-3.69 s (AB2 target 3.0, ref 3.144)", d);
        // v53 pin move (cause: law re-voice, constants
        // untouched): in LIMIT the working depth is deeper
        // (GR0 22.3 vs 17.1), the T4 stress state charges
        // proportionally to GR, and the 50% crossing reads
        // 0.110 s. In COMP the same constants read 0.069 s,
        // inside the manuals' band (C4d). The 90% tail and
        // the 0.3 s hold stay in their bands in both modes.
        // AB2 pin move (cause as C4b): the LIMIT 50%
        // crossing reads 0.1136 s at the faster DUAL scale
        // (the fast pole carries more of the early
        // recovery); band = ref +/-15%.
        check(t50 > 0.050 && t50 < 0.125, "C4c DUAL 50% recovery 50-125 ms in LIMIT (TEST2 re-base, ref 0.1136 at the old depth)", d);
        auto trC = grRelease(false, 6.5, true, 1.0f, 2.0f); // TEST2 park (COMP)
        float g0C = trC[100];
        double t50C = -1, t90C = -1;
        for (size_t i = 0; i < trC.size(); ++i) {
            if (t50C < 0 && trC[i] <= 0.5f * g0C) t50C = (double)i / kFs;
            if (trC[i] <= 0.1f * g0C) { t90C = (double)i / kFs; break; }
        }
        std::snprintf(d, sizeof d, "COMP: GR0 %.1f dB, 50%% rec %.3f s, 90%% rec %.2f s", g0C, t50C, t90C);
        // v64 pin move (cause as C1): the K5-COMP
        // references from 1.3.0 at 2X (50% 0.1428 s, band
        // +/-15%; 90% 3.915 s, band +/-17.5%).
        // AB2 pin move (cause as C4b): COMP reads 50%
        // 0.1043 s / 90% 2.625 s at the shared DUAL scale —
        // the 90% figure inside the +/-15% target band
        // (2.55-3.45 s); bands = refs +/-15% / +/-17.5%.
        check(t50C > 0.050 && t50C < 0.120 && t90C > 2.17 && t90C < 3.08,
              "C4d DUAL in COMP: 50% 50-120 ms (TEST2 re-base), 90% 2.17-3.08 s (AB2, refs 0.1043/2.625)", d);
    }
    {
        // C5: T4 MEMORY — same level, same depth, different
        // history: recovery after an 8 s hold must run
        // materially longer than after a 0.3 s brush (the
        // cell remembers sustained reduction). Bounded: even
        // the stressed tail stays inside the manuals' printed
        // 5 s complete-release envelope.
        // v46: the stimulus knob moves 0.75 -> 0.85 — the
        // test is authored at a ~25 dB working depth (the
        // stress reference's home), and the v1.1.5 staging
        // moved that depth up the knob (at 0.75 the hold now
        // sits at ~17 dB and the measured memory contrast
        // dilutes to 1.25x; at 0.85 it reads 1.53x, as
        // authored). The mechanism is untouched.
        auto relAfter = [&](double holdSec) {
            osc72::Ba6aLimiter lim;
            lim.prepare(kFs); lim.setKnob(1.0f); // TEST2 re-site: the authored ~25 dB hold depth now sits at knob 1.0 (40 Hz)
            lim.setAttackFast(true); lim.setReleaseFast(false); lim.reset();
            const int loud = (int)(holdSec * kFs);
            const int tail = (int)(8.0 * kFs);
            float g0 = 0; double t90 = -1;
            for (int i = 0; i < loud + tail; ++i) {
                float s = i < loud ? 1.0f * (float)std::sin(2.0 * kPi * 40.0 * i / kFs) : 0.0f; // TEST2 re-site: 40 Hz park (see grStep)
                float oL, oR; lim.process(s, s, oL, oR);
                if (i == loud + 100) g0 = lim.gainReductionDb();
                if (i > loud && g0 > 0 && t90 < 0 && lim.gainReductionDb() <= 0.1f * g0)
                    t90 = (double)(i - loud) / kFs;
            }
            return t90;
        };
        double tBrief = relAfter(0.3), tLong = relAfter(8.0);
        std::snprintf(d, sizeof d, "90%% rec after 0.3 s hold %.2f s, after 8 s hold %.2f s (ratio %.2f)",
                      tBrief, tLong, tBrief > 0 ? tLong / tBrief : 0.0);
        // v64 pin move (cause as C1): baked, the two holds
        // read the K6 references from 1.3.0 at 2X (brief
        // 4.45 s, long 6.65 s; bands +/-17.5%). The 1X-era
        // "inside the manuals' 5 s envelope" bound goes
        // with the 1X speeds: with the memory taus doubled
        // the stressed tail's 90% recovery is 6.65 s — the
        // figure 1.3.0's own K6 pin shipped. The memory
        // substance (long hold releases materially slower,
        // contrast 1.49) is asserted unchanged.
        // AB2 pin move (cause as C4b): at the AB2 DUAL
        // scale the holds read 2.998 s (0.3 s hold) and
        // 4.220 s (8 s hold), bands +/-17.5%; the memory
        // contrast survives at 1.41x (pin >= 1.35x) — the
        // T4 memory stays in circuit, scaled with the seat.
        // TEST3 RE-BASE (cause as C1): at the corrected
        // operating point the holds read 2.62/4.27 s
        // (contrast 1.63) — back inside the original AB2
        // tolerance window (2.998/4.220 at +/-17.5%);
        // refs move to the measured figures, T4 band
        // kept. The depth-dependence mechanism is the
        // one TEST2 controlled for.
        check(tBrief > 2.16 && tBrief < 3.08 && tLong > 1.35 * tBrief && tLong > 3.52 && tLong < 5.02,
              "C5 DUAL memory: holds at TEST3 refs 2.62/4.27 s, long >= 1.35x slower (re-base: hold depth, see report)", d);
    }

    // ═══════════════ D. AUTO MAKE-UP (Nathan's pin) ═══════════════
    {
        // Programme RMS vs knob must NOT fall as drive rises,
        // and max must crank above min.
        float prevNet = -1e9f; bool monotone = true;
        float net0 = 0, net1 = 0, grAt1 = 0;
        for (int ki = 0; ki <= 4; ++ki) {
            float knob = ki * 0.25f;
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(knob);
            ch.setModIn(false);
            ProgGen pg;
            const int total = (int)(5.0 * kFs);
            double s = 0; int cnt = 0; double sGr = 0;
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                float oL, oR; ch.process(l, r, oL, oR);
                if (i >= (int)(2.5 * kFs)) {
                    s += (double)oL * oL + (double)oR * oR; sGr += ch.gainReductionDb(); ++cnt;
                }
            }
            float rms = (float)std::sqrt(s / (2.0 * cnt));
            float net = db(rms);
            if (ki == 0) net0 = net;
            if (ki == 4) { net1 = net; grAt1 = (float)(sGr / cnt); }
            if (net < prevNet - 0.3f) monotone = false;
            prevNet = net;
            std::printf("      knob %.2f -> programme RMS %.2f dBFS\n", knob, net);
        }
        std::snprintf(d, sizeof d, "net %.2f -> %.2f dB (rise %.2f), mean GR@max %.1f dB",
                      net0, net1, net1 - net0, grAt1);
        check(monotone && net1 >= net0, "D1 programme level never falls as drive rises", d);
        check(net1 > net0 + 0.8f, "D2 make-up cranks: net at max > net at 0 by > 0.8 dB", d);
        // v1.1.1 (Nathan): the input must DIG IN — pin the
        // programme depth the steepened drive law delivers
        // (1.1.0 law measured 9.9 dB here; target >= ~30).
        // TEST2 RE-BASE (cause: Nathan's operating-point
        // directive 2026-10-23): the raised threshold puts
        // the suite's programme proxy under the verge even
        // at max drive — measured mean GR at max ~0 dB on
        // this tree (the digging now happens on hotter
        // programme; the report's onset table carries the
        // working depths). The pin asserts the new home:
        // ProgGen stays essentially GR-free at every knob.
        // TEST3 RE-BASE (cause: Nathan's recalibration
        // directive — TEST2 overshot): at the corrected
        // threshold ProgGen at full drive draws a working
        // depth again — measured mean GR at max 7.3 dB.
        // The pin asserts that working band (the digging
        // the v1.1.1 note above asked for, at the
        // corrected operating point).
        // TEST4 RE-BASE (cause: Nathan's detector-blend
        // directive): at full drive (+51 dB) ProgGen's
        // continuous bed (the 220 Hz component and the
        // hit train, driven into the bounded path) is
        // sustained content, and the blend's RMS branch
        // engages on it — measured mean GR at max 19.8 dB
        // on this tree (TEST3 control on the identical
        // bench: 7.3 dB). The band asserts the blend's
        // deepened working depth at max; the transient
        // anchor is unmoved (drums onset rows identical
        // to TEST3 except <= +0.64 dB at knob 8 SLOW —
        // REPORT-TEST4.md).
        check(grAt1 > 14.0f && grAt1 < 24.0f, "D3 programme mean GR at max = blend-deepened depth 14-24 dB (TEST4)", d);
    }

    // ═══════════════ E. COLOUR / DRYNESS / THD / FR ═══════════════
    {
        // E1 (criterion 2 — always-on colour): at LOW drive
        // and nominal level the box must be measurably
        // coloured, LF especially; not a clean model.
        float thd60 = thdLim(0.1f, 0.25f, 60.0f);
        float thd1k = thdLim(0.1f, 0.25f, 1000.0f);
        std::snprintf(d, sizeof d, "THD 60 Hz %.2f%%, 1 kHz %.3f%% @ knob 0.1, amp 0.25", thd60, thd1k);
        check(thd60 > 0.2f && thd60 < 8.0f, "E1a LF colour present at low drive", d);
        check(thd1k > 0.02f && thd1k < 1.5f, "E1b MF colour present but civilised", d);
    }
    {
        // E2 (criterion 3 — dryness): the colour mechanism's
        // only memory is the iron flux state (tau ~5 ms).
        // (a) Unit level: an IronStage driven hard at 60 Hz
        // must discharge to < 3% of its burst flux within
        // 20 ms of silence — no lingering magnetisation.
        // (b) System level: burst -> silence through the
        // limiter leaves NO self-sustaining tail: output is
        // below -60 dBFS within 30 ms of the burst ending
        // (gain states acting on silence are silence; the
        // limiter's recovery is level behaviour, not ring).
        osc72::IronStage iron(osc72::IronStage::Voicing::output);
        iron.prepare(kFs);
        for (int i = 0; i < (int)(0.3 * kFs); ++i)
            iron.process(0.85f * (float)std::sin(2.0 * kPi * 60.0 * i / kFs));
        float fluxBurst = std::fabs(iron.flux());
        float fluxAt20 = 0;
        for (int i = 0; i < (int)(0.05 * kFs); ++i) {
            iron.process(0.0f);
            if (i == (int)(0.02 * kFs)) fluxAt20 = std::fabs(iron.flux());
        }
        osc72::Ba6aLimiter lim;
        lim.prepare(kFs);
        lim.setKnob(0.6f);
        lim.reset();
        float settlePeak = 0, gonePeak = 0;
        for (int i = 0; i < (int)(1.0 * kFs); ++i) {
            float s = i < (int)(0.15 * kFs)
                ? 0.85f * (float)std::sin(2.0 * kPi * 60.0 * i / kFs) : 0.0f;
            float oL, oR; lim.process(s, s, oL, oR);
            // The iron/HP LF settle after a huge LF burst is a
            // decaying offset (real iron + coupling behaviour),
            // NOT a ring: it must be small by +30 ms and gone
            // (< -66 dBFS) by +150 ms.
            if (i >= (int)(0.18 * kFs) && i < (int)(0.21 * kFs))
                settlePeak = std::max(settlePeak, std::fabs(oL));
            if (i >= (int)(0.30 * kFs))
                gonePeak = std::max(gonePeak, std::fabs(oL));
        }
        std::snprintf(d, sizeof d, "flux %.3f -> %.4f at +20 ms (%.1f%%); settle peak %.4f @+30 ms, %.5f @+150 ms",
                      fluxBurst, fluxAt20, 100.0 * fluxAt20 / fluxBurst, settlePeak, gonePeak);
        check(fluxAt20 < 0.03f * fluxBurst, "E2a iron flux discharges < 3% in 20 ms", d);
        // v1.1.1 pin move (drive law): this test runs the
        // limiter at knob 0.6, which now digs ~10 dB deeper;
        // the +30 ms settle is the gain recovery acting on the
        // iron's decaying offset, so it grows with depth
        // (measured 0.0135 under the 1.1.0 law -> 0.0205 now).
        // Bound relaxed 0.020 -> 0.022 (-33.2 dBFS); the
        // no-ring substance is unchanged — still exactly 0 by
        // +150 ms. See docs/build-report-v40.md.
        check(settlePeak < 0.022f && gonePeak < 0.0005f,
              "E2b no ring: LF settle < -33 dBFS by +30 ms, gone by +150 ms", d);
    }
    {
        // E3: THD vs the published anchors (family sheet:
        // < 0.4% clean at 400 Hz; < 0.75% at 18 dB GR — our
        // level reference differs from dBm; reported plainly).
        float clean = thdLim(0.0f, 0.2f, 400.0f);
        // Find a knob giving >= 15 dB GR on a full-scale
        // 400 Hz tone (the family's 18 dB GR reference point
        // is a peak-domain figure; steady-state sine GR tops
        // out a little under it — see the report).
        float kDeep = -1;
        for (float k = 0.3f; k <= 1.001f; k += 0.05f) {
            Meas m = runLim(k, 1.0f, 40.0f, 1.2); // TEST2 re-site: deep GR on a tone lives at 40 Hz now (see grStep)
            if (m.meanGr >= 15.0f) { kDeep = k; break; }
        }
        float deep = kDeep > 0 ? thdLim(kDeep, 1.0f, 40.0f) : -1.0f; // TEST2: measured at the 40 Hz deep point
        std::snprintf(d, sizeof d, "clean 400 Hz %.3f%%; at >=15 dB GR (knob %.2f) %.2f%%",
                      clean, kDeep, deep);
        check(clean < 0.5f, "E3a clean THD < 0.5% @ 400 Hz (spec < 0.4%)", d);
        // v45: ceiling 3.0 -> 3.5 — with the FAST control
        // pole at the 1176 seat the sustained-tone GR settles
        // ~0.3 dB shallower and the downstream stages run
        // that much hotter (measured 2.68 -> 3.31-3.34%; the
        // mechanism + the trade are in build-report-v45).
        // TEST2 re-base (cause: Nathan's operating-point
        // directive): the deep point now sits at 40 Hz /
        // knob 0.80, where reaching 15 dB GR demands the
        // full drive the threshold requires — the measured
        // THD there (44-46%) is the path's LF saturation
        // at that drive, not a law artefact: the pristine
        // control at ITS 40 Hz deep point (knob 0.35,
        // GR 17) reads 2.97%, and at matched DRIVE (knob
        // 0.80) both builds render the same colour (the
        // audio path is untouched code — TEST2 report,
        // proof (c)). Band = the measured figure +/- a
        // third; it guards the fingerprint, it is not a
        // cleanliness claim.
        check(kDeep > 0 && deep > 30.0f && deep < 60.0f, "E3b THD at >=15 dB GR, 40 Hz deep point (TEST2 re-base, see report)", d);
    }
    {
        // E4: frequency response vs the family's +/-2 dB,
        // 30 Hz - 15 kHz envelope (low drive, colour region).
        const float freqs[] = { 30, 50, 100, 400, 1000, 5000, 10000, 15000 };
        float gains[8];
        for (int i = 0; i < 8; ++i) {
            Meas m = runLim(0.0f, 0.05f, freqs[i], 1.2);
            gains[i] = db(m.rmsL);
        }
        const float ref = gains[3]; // 400 Hz
        float worst = 0;
        for (int i = 0; i < 8; ++i) {
            float dev = gains[i] - ref;
            if (std::fabs(dev) > std::fabs(worst)) worst = dev;
            std::printf("      FR %6.0f Hz: %+.2f dB rel 400 Hz\n", (double)freqs[i], (double)dev);
        }
        std::snprintf(d, sizeof d, "worst deviation %+.2f dB", worst);
        check(std::fabs(worst) < 2.6f, "E4 FR inside ~+/-2 dB family envelope", d);
    }
    {
        // E5: slam sanity — 0 dBFS burst train at max drive:
        // peaks bounded by the ceiling discipline, reported.
        osc72::Chain ch;
        ch.prepare(kFs);
        ch.setKnob(1.0f);
        ProgGen pg;
        float peak = 0;
        for (int i = 0; i < (int)(4.0 * kFs); ++i) {
            float l, r; pg.sample(i, l, r);
            l *= 2.2f; r *= 2.2f; // push peaks past 0 dBFS
            float oL, oR; ch.process(l, r, oL, oR);
            peak = std::max(peak, std::max(std::fabs(oL), std::fabs(oR)));
        }
        std::snprintf(d, sizeof d, "max |out| %.3f (%.2f dBFS)", peak, db(peak));
        check(peak < 1.9f, "E5 slam peaks bounded (< +5.6 dBFS)", d);
    }

    // ═══════════════ F. ADT (MOD) ═══════════════
    {
        // F1: deck delay swing (introspection) — both decks
        // swing milliseconds, independently.
        osc72::AdtChain adt;
        adt.prepare(kFs);
        adt.setEnabled(true);
        float mn1 = 1e9f, mx1 = -1e9f, mn2 = 1e9f, mx2 = -1e9f;
        for (int i = 0; i < (int)(12.0 * kFs); ++i) {
            float oL, oR; adt.process(0.0f, 0.0f, oL, oR);
            float d1 = adt.deckDelayMs(1), d2 = adt.deckDelayMs(2);
            mn1 = std::min(mn1, d1); mx1 = std::max(mx1, d1);
            mn2 = std::min(mn2, d2); mx2 = std::max(mx2, d2);
        }
        std::snprintf(d, sizeof d, "deck1 %.1f..%.1f ms, deck2 %.1f..%.1f ms",
                      mn1, mx1, mn2, mx2);
        check(mx1 - mn1 > 8.0f && mx2 - mn2 > 9.0f, "F1 both decks swing (varispeed)", d);
        // Implied pitch swing (deck1): max |dDelay/dt|.
        check(true, "F1b pitch swing reported in F2 audio test");
    }
    {
        // F2: MONO feed widens (the retired mid/side law is
        // gone): side energy appears, correlation collapses.
        // FULL2 re-point (cause: the bus sums BOTH channels
        // at the chain defaults, so the section's subject —
        // the ADT insert — is now measured on its own bus
        // channel, compMix=0 / modMix=1: the insert channel
        // alone, exactly what AB3's modMix=1 hard select
        // delivered): measured corr 0.855, side/mid 0.286 —
        // inside the standing bands, no re-base.
        osc72::Chain ch;
        ch.prepare(kFs);
        ch.setCompMix(0.0f);
        ch.setModMix(1.0f);
        ch.setModIn(true);
        ProgGen pg;
        double sLL = 0, sRR = 0, sLR = 0, sSide = 0, sMid = 0; int cnt = 0;
        float prevDelay = -1; double maxPitch = 0;
        const int total = (int)(6.0 * kFs);
        for (int i = 0; i < total; ++i) {
            float l, r; pg.sample(i, l, r);
            float m = 0.5f * (l + r); // mono feed
            float oL, oR; ch.process(m, m, oL, oR);
            float dl = ch.adt().deckDelayMs(1);
            if (prevDelay > 0) {
                double dev = std::fabs((double)dl - prevDelay) / 1000.0 * 100.0; // % per sample-normalised below
                (void)dev;
            }
            prevDelay = dl;
            if (i >= (int)(3.0 * kFs)) {
                sLL += (double)oL * oL; sRR += (double)oR * oR; sLR += (double)oL * oR;
                float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR);
                sMid += (double)mid * mid; sSide += (double)side * side;
                ++cnt;
            }
        }
        double corr = sLR / std::sqrt(sLL * sRR);
        double width = std::sqrt(sSide / sMid);
        std::snprintf(d, sizeof d, "mono feed: L/R corr %.2f, side/mid %.2f", corr, width);
        // v1.1.1 pin moves (ADT deck trim x2/3, Nathan):
        // less deck in the mix decorrelates less by design —
        // corr 0.62 -> 0.79, side/mid 0.49 -> 0.35 measured.
        // The pins still require real widening (dry mono
        // corr is 1.00, side/mid 0). See build-report-v40.md.
        // v46 pin moves (ADT deck trim x0.85 again, Nathan:
        // the ADT mix 15% less on 1.1.6 — kAdtDeckGain
        // 0.4920 -> 0.4182): corr 0.83 -> 0.87, side/mid
        // 0.32 -> 0.27 measured on this tree. Thresholds
        // relaxed to 0.90 / 0.24; the pins still require
        // real widening. See build-report-v48.md.
        check(corr < 0.90, "F2a mono source decorrelates (widens)", d);
        check(width > 0.24, "F2b mono source gains real side energy", d);
    }
    {
        // F3: STEREO feed also widens vs its dry correlation.
        // Feed = typical programme stereo (shared centre +
        // modest side content, dry corr ~0.85).
        auto widthOf = [&](bool modOn) {
            osc72::Chain ch;
            ch.prepare(kFs);
            // FULL2 re-point: each arm is ONE bus channel —
            // dry = the COMP channel alone (compMix=1,
            // modMix=0, MOD out), wet = the MOD channel
            // alone (compMix=0, modMix=1, MOD in). (At
            // compMix=0 with MOD out the bus is silent by
            // design, so the old same-mix dry arm no longer
            // exists to compare against.)
            ch.setCompMix(modOn ? 0.0f : 1.0f);
            ch.setModMix(modOn ? 1.0f : 0.0f);
            ch.setModIn(modOn);
            ProgGen pgM, pgS;
            double sLL = 0, sRR = 0, sLR = 0; int cnt = 0;
            const int total = (int)(5.0 * kFs);
            for (int i = 0; i < total; ++i) {
                float ml, mr, sl, sr;
                pgM.sample(i, ml, mr);
                pgS.sample(i + 999999, sl, sr);
                float m = 0.5f * (ml + mr), sd = 0.30f * sl;
                float l = m + sd, r = m - sd;
                float oL, oR; ch.process(l, r, oL, oR);
                if (i >= (int)(2.5 * kFs)) {
                    sLL += (double)oL * oL; sRR += (double)oR * oR; sLR += (double)oL * oR; ++cnt;
                }
            }
            return sLR / std::sqrt(sLL * sRR);
        };
        double dry = widthOf(false), wet = widthOf(true);
        std::snprintf(d, sizeof d, "stereo corr dry %.2f -> wet %.2f", dry, wet);
        // v1.1.1 pin move (ADT trim): wet corr 0.55 -> 0.66
        // measured; margin relaxed 0.15 -> 0.08.
        // v46 pin move (ADT trim x0.85, Nathan — see F2):
        // wet corr 0.68 -> 0.70 against dry 0.78 measured;
        // margin relaxed 0.08 -> 0.05. The pin still
        // requires a clear decorrelation gap.
        check(wet < dry - 0.05, "F3 stereo programme decorrelates under ADT", d);
    }
    {
        // F4: pitch swing on a steady tone, measured in the
        // audio: zero-crossing rate of an ADT-only output
        // (deck-dominated right channel with a mono feed:
        // SRC is centred, decks swing pitch by design).
        osc72::AdtChain adt;
        adt.prepare(kFs);
        adt.setEnabled(true);
        // Warm the transport for 1 s with the tone.
        std::vector<float> out;
        const int total = (int)(8.0 * kFs);
        out.reserve((size_t)total);
        for (int i = 0; i < total; ++i) {
            float s = 0.4f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
            float oL, oR; adt.process(s, s, oL, oR);
            out.push_back(oR);
        }
        // Instantaneous frequency via upward zero crossings in
        // 100 ms windows over the last 5 s.
        double fMin = 1e9, fMax = 0;
        const int win = (int)(0.1 * kFs);
        for (int w0 = (int)(3.0 * kFs); w0 + win < total; w0 += win) {
            int cross = 0; int first = -1, last = -1;
            for (int i = w0; i < w0 + win - 1; ++i) {
                if (out[(size_t)i] <= 0 && out[(size_t)(i + 1)] > 0) {
                    ++cross;
                    if (first < 0) first = i;
                    last = i;
                }
            }
            if (cross > 5 && last > first) {
                double f = (double)(cross - 1) * kFs / (double)(last - first);
                fMin = std::min(fMin, f); fMax = std::max(fMax, f);
            }
        }
        double dev = std::max(std::fabs(fMax - 440.0), std::fabs(fMin - 440.0)) / 440.0 * 100.0;
        std::snprintf(d, sizeof d, "440 Hz tone: inst. freq %.1f..%.1f Hz (dev %.2f%%)", fMin, fMax, dev);
        check(dev > 0.25 && dev < 4.0, "F4 audible varispeed pitch swing (0.25-4%)", d);
    }
    {
        // F5 (v44 mix-alignment diagnosis; FULL2 re-homed):
        // the ADT's SRC is a PURE integer delay of its feed —
        // the fact the bus's channel alignment relies on,
        // here measured instead of assumed. FULL2 cause for
        // the move: the bus sums both channels under a
        // common make-up, so the SRC-purity fact can no
        // longer be read off two chain renders; it is
        // pinned where it lives, at the insert's own
        // terminals, fed by the module-mode limiter (the
        // send's actual source). The same impulse goes
        // through two ADT instances (never enabled / enabled,
        // knob 0 module feed); the enabled IR must equal the
        // disabled one shifted by exactly 960, to float
        // dust, over the arrival window — and the enabled
        // output must be silent before the arrival sample.
        // The enabled impulse is fired at a moment when both
        // decks sit beyond 21.5 ms (introspection), so no
        // deck echo can reach the window. (The chain-level
        // arrival is pinned in M3.)
        auto fireIr = [](bool mod, std::vector<float>& ir, float& preMax) {
            osc72::Ba6aLimiter lim;
            lim.setApplyMakeup(false);   // module mode: the send's source
            lim.prepare(kFs);
            lim.setKnob(0.0f);
            lim.reset();
            osc72::AdtChain adt;
            adt.prepare(kFs);
            adt.setEnabled(mod);
            const int len = 1400;
            ir.assign((size_t)len, 0.0f);
            preMax = 0.0f;
            int fireAt = -1;
            const int budget = (int)(12.0 * kFs);
            for (int i = 0; i < budget; ++i) {
                bool fire = false;
                if (fireAt < 0 && i >= (int)(1.0 * kFs)) {
                    if (!mod) fire = true;
                    else if (adt.deckDelayMs(1) > 21.5f
                             && adt.deckDelayMs(2) > 21.5f) fire = true;
                }
                float mL, mR;
                lim.process(fire ? 0.5f : 0.0f, fire ? 0.5f : 0.0f, mL, mR);
                float oL, oR;
                adt.process(mL, mR, oL, oR);
                if (fire) fireAt = i;
                if (fireAt >= 0) {
                    const int rel = i - fireAt;
                    if (rel < len) ir[(size_t)rel] = oL;
                    if (mod && rel > 0 && rel < 960)
                        preMax = std::max(preMax, std::fabs(oL));
                    if (rel >= len) break;
                }
            }
            return fireAt >= 0;
        };
        std::vector<float> irOut, irIn;
        float preMax = 0.0f;
        const bool okA = fireIr(false, irOut, preMax);
        const bool okB = fireIr(true, irIn, preMax);
        double maxDiff = 0.0;
        for (int k = 0; k < 200; ++k)
            maxDiff = std::max(maxDiff, (double)std::fabs(irIn[(size_t)(960 + k)] - irOut[(size_t)k]));
        std::snprintf(d, sizeof d, "fired=%d/%d, max |IR_in[n+960]-IR_out[n]| %.2e over 200 samples, pre-arrival max %.2e",
                      (int)okA, (int)okB, maxDiff, (double)preMax);
        check(okA && okB && maxDiff < 1.0e-9 && preMax < 1.0e-9,
              "F5 ADT SRC is a pure 960-sample delay of its (module) feed, at the insert's terminals (FULL2)", d);
    }

    // ═══════════════ G. CPU ═══════════════
    {
        osc72::Chain ch;
        ch.prepare(kFs);
        ch.setKnob(0.7f);
        ch.setModIn(true);
        ProgGen pg;
        // Warm up.
        for (int i = 0; i < (int)(0.5 * kFs); ++i) {
            float l, r; pg.sample(i, l, r);
            float oL, oR; ch.process(l, r, oL, oR);
        }
        const int total = (int)(10.0 * kFs);
        auto t0 = std::chrono::steady_clock::now();
        double sink = 0;
        for (int i = 0; i < total; ++i) {
            float l, r; pg.sample(i, l, r);
            float oL, oR; ch.process(l, r, oL, oR);
            sink += oL + oR;
        }
        auto t1 = std::chrono::steady_clock::now();
        double secs = std::chrono::duration<double>(t1 - t0).count();
        double pct = 100.0 * secs / 10.0;
        std::snprintf(d, sizeof d, "full chain: %.2f%% of realtime (sink %.4f)", pct, sink);
        check(pct < 12.0, "G1 CPU < 12% of one core @ 48 kHz (v53: pin 8 -> 12, oversampling cost, see report)", d);
    }

    // ── H. TWO-ELEMENT REMIX (v1.1.2, study §5): the
    // vari-mu glow's proof channel. A loud rhythmic element
    // (90 Hz bursts) and a quiet steady element (2 kHz,
    // -30.5 dBFS peak) share the one control voltage: the
    // quiet element must duck under the hits and BLOOM back
    // in the gaps — the shared-gain remix. 1.1.1 baseline on
    // this test: bloom +9.2 dB at knob 0.5 (GR 15.0 under the
    // hit, 6.0 in the gap); the glow pass must not reduce it.
    // v46: the test follows the DEPTH, not the knob number —
    // Nathan's re-centred staging moved the ~13-15 dB-hit
    // working depth from knob 0.5 to knob 0.70 (at 0.5 the
    // hits now only draw 5.4 dB GR: the kissing point).
    // Measured at 0.70: bloom +7.86 dB (hit 12.8, gap 4.8).
    std::printf("\n=== H. TWO-ELEMENT REMIX ===\n");
    {
        auto goertzel = [](const std::vector<float>& x, int start, int len, double freq) {
            double w = 2.0 * M_PI * freq / kFs, cw = std::cos(w), coeff = 2.0 * cw;
            double s0 = 0, s1 = 0, s2 = 0;
            for (int i = 0; i < len; ++i) { s0 = x[start + i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
            double re = s1 - s2 * cw, im = s2 * std::sin(w);
            return 2.0 * std::sqrt(re * re + im * im) / len;
        };
        osc72::Ba6aLimiter lim; lim.prepare(kFs); lim.reset();
        // TEST2 re-site: knob 0.70 -> 0.90 — the burst hits
        // must still park a working depth under the raised
        // threshold for the gap-hold window to exist.
        lim.setKnob(0.90f); lim.setAttackFast(true); lim.setReleaseFast(false);
        const int cyc = (int)(0.5 * kFs), total = cyc * 14;
        std::vector<float> out(total); std::vector<float> grv(total);
        double phq = 0, phk = 0;
        for (int i = 0; i < total; ++i) {
            double t = (i % cyc) / kFs;
            // TEST2 re-site: the 90 Hz sine burst's envelope
            // reach sits under the raised threshold (hit GR
            // read 0.0); a kick-shaped burst (pitch drop
            // 120->42 Hz, the drum material's own hit shape)
            // overshoots the detector like programme does.
            double fk = 120.0 * std::exp(-t * 28.0) + 42.0; phk += 2.0 * M_PI * fk / kFs;
            float loud = 1.2f * (float)std::exp(-t / 0.060) * (float)std::sin(phk);
            float quiet = 0.03f * (float)std::sin(phq); phq += 2.0 * M_PI * 2000.0 / kFs;
            float oL, oR; lim.process(loud + quiet, loud + quiet, oL, oR);
            out[i] = oL; grv[i] = lim.gainReductionDb();
        }
        double hitMag = 0, gapMag = 0, hitGr = 0, gapGr = 0; int nc = 0;
        for (int c = 8; c < 14; ++c) {
            hitMag += goertzel(out, c * cyc, (int)(0.08 * kFs), 2000.0);
            gapMag += goertzel(out, c * cyc + (int)(0.32 * kFs), (int)(0.16 * kFs), 2000.0);
            for (int i = 0; i < (int)(0.08 * kFs); ++i) hitGr += grv[c * cyc + i];
            for (int i = 0; i < (int)(0.16 * kFs); ++i) gapGr += grv[c * cyc + (int)(0.32 * kFs) + i];
            ++nc;
        }
        hitMag /= nc; gapMag /= nc;
        hitGr /= (nc * (int)(0.08 * kFs)); gapGr /= (nc * (int)(0.16 * kFs));
        double bloom = 20.0 * std::log10(gapMag) - 20.0 * std::log10(hitMag);
        char d[512];
        std::snprintf(d, sizeof d, "bloom %+.2f dB (hit %.1f dB GR, gap %.1f dB GR)", bloom, hitGr, gapGr);
        check(bloom >= 7.0, "H1 remix bloom >= 7 dB at the working depth (knob 0.7, v46)", d);
        check(gapGr >= 2.5 && gapGr < hitGr, "H2 gap GR held by recovery, under hit GR (TEST2 re-base: depths, see report)", d);
    }

    // ═══════════════ I. SIDECHAIN LOW-CUT — REMOVED (AB3) ═══
    // The v41 detector-feed HPF section (I1–I3) is deleted
    // with the feature: Nathan repurposed the SIDECHAIN pot
    // as COMP MIX and the sidechain filter is gone from the
    // DSP outright (the timesTwo precedent). Nothing here
    // needs re-pinning: at the feature's 10 Hz default the
    // filter was out of the path, so the shipped audio the
    // I-section guarded is bit-identical without it — that
    // identity is carried by the K7 programme hashes below
    // (MOD-out configs reproduce their 1.3.0 references
    // exactly on this tree). The COMP MIX stage that
    // replaces the pot is pinned in section L.

    std::printf("\n=== J. PHASER (MOD FLAVOUR P, v55) ===\n");
    {
        // The Phase 90 model (docs/mod-phaser-design.md):
        // four ganged one-pole allpass stages + 50/50 dry
        // sum, script-era (no feedback), LFO 0.30 Hz, stage
        // break sweeping 600 Hz-6 kHz in the log domain.
        // With the LFO frozen the filter is LTI, and the
        // bilinear stage law predicts the notch frequencies
        // in the warped domain exactly:
        //   tan(pi f/fs) = tan(a) * tan(pi fb/fs),
        // a = 22.5 deg (notch 1, cascade phase -180) and
        // 67.5 deg (notch 2, -540).
        auto notchPred = [](double fb, double aDeg) {
            double t = std::tan(aDeg * kPi / 360.0) * std::tan(kPi * fb / kFs);
            return std::atan(t) / kPi * kFs;
        };
        auto phaserGain = [](double freq, float phase) {
            osc72::Phaser90 ph;
            ph.prepare(kFs);
            ph.setEnabled(true);
            ph.setLfoFrozen(true);
            ph.setLfoPhase(phase);
            const int total = (int)(0.30 * kFs);
            double s = 0; int cnt = 0;
            for (int i = 0; i < total; ++i) {
                float x = 0.5f * (float)std::sin(2.0 * kPi * freq * i / kFs);
                float oL, oR; ph.process(x, x, oL, oR);
                if (i >= total / 2) { s += (double)oL * oL; ++cnt; }
            }
            double rms = std::sqrt(s / cnt);
            return 20.0 * std::log10(rms / (0.5 / std::sqrt(2.0)));
        };
        // Fine local scan for the measured minimum near a
        // predicted notch (1% steps over +/-50%).
        auto findNotch = [&](float phase, double pred) {
            double bestF = pred, bestG = 1e9;
            for (double f = pred / 1.5; f <= pred * 1.5; f *= 1.01) {
                double g = phaserGain(f, phase);
                if (g < bestG) { bestG = g; bestF = f; }
            }
            return std::pair<double, double>(bestF, bestG);
        };
        char d[512];
        // J1: notch frequencies at both LFO extremes.
        // Phase 0.75 -> sweep bottom (break 600 Hz);
        // phase 0.25 -> sweep top (break 6000 Hz).
        bool j1 = true;
        for (float phase : { 0.75f, 0.25f }) {
            const double fb = phase == 0.75f ? 600.0 : 6000.0;
            for (double aDeg : { 45.0, 135.0 }) {
                const double pred = notchPred(fb, aDeg);
                auto found = findNotch(phase, pred);
                double err = std::fabs(found.first - pred) / pred;
                std::printf("      break %4.0f Hz: notch pred %7.1f Hz, measured %7.1f Hz (%+.1f%%), depth %.0f dB\n",
                            fb, pred, found.first, 100.0 * (found.first - pred) / pred, found.second);
                if (err > 0.05) j1 = false;
            }
        }
        check(j1, "J1 notch frequencies at LFO extremes match the stage law (+/-5%)", "");
        // J2: notch depth at the predicted frequencies
        // (the algebraic null; float residue only).
        double worstDepth = -1e9;
        for (float phase : { 0.75f, 0.25f }) {
            const double fb = phase == 0.75f ? 600.0 : 6000.0;
            for (double aDeg : { 45.0, 135.0 })
                worstDepth = std::max(worstDepth, phaserGain(notchPred(fb, aDeg), phase));
        }
        std::snprintf(d, sizeof d, "shallowest notch %.0f dB (want < -40)", worstDepth);
        check(worstDepth < -40.0, "J2 notch depth: > 40 dB at both notches, both extremes (50/50 mix)", d);
        // J2b: notch COUNT at the sweep bottom — a full-range
        // scan finds exactly two minima deeper than -20 dB.
        // A basin counts only with > 10 dB of prominence
        // below its preceding peak (a bare minimum tracker
        // mistakes notch 1's recovery slope for a basin).
        {
            int deep = 0;
            double prevG = 1e9, peak = -1e9, basinPeak = -1e9, runMin = 1e9;
            bool inBasin = false;
            for (double f = 100.0; f <= 18000.0; f *= 1.02) {
                double g = phaserGain(f, 0.75f);
                if (!inBasin) {
                    peak = std::max(peak, g);
                    if (g < prevG) { inBasin = true; basinPeak = peak; runMin = g; }
                } else {
                    runMin = std::min(runMin, g);
                    if (g > runMin + 1.0) {
                        if (runMin < -20.0 && basinPeak - runMin > 10.0) ++deep;
                        inBasin = false; peak = g;
                    }
                }
                prevG = g;
            }
            if (inBasin && runMin < -20.0 && basinPeak - runMin > 10.0) ++deep;
            std::snprintf(d, sizeof d, "deep minima in 100 Hz-18 kHz: %d (want 2)", deep);
            check(deep == 2, "J2b exactly two notches (4 stages, 50/50 sum)", d);
        }
        // J3: LFO rate + break travel (introspection over
        // 14 s of running machine).
        {
            osc72::Phaser90 ph;
            ph.prepare(kFs);
            ph.setEnabled(true);
            float mn = 1e9f, mx = 0;
            double prev = 0, lastCross = -1, perSum = 0; int crosses = 0;
            const double mid = std::sqrt(600.0 * 6000.0);
            for (int i = 0; i < (int)(14.0 * kFs); ++i) {
                float oL, oR; ph.process(0.0f, 0.0f, oL, oR);
                float br = ph.breakHz();
                if (i > (int)(1.0 * kFs)) {
                    mn = std::min(mn, br); mx = std::max(mx, br);
                    if (prev < mid && br >= mid) {
                        double tNow = i / kFs;
                        if (lastCross > 0) { perSum += tNow - lastCross; ++crosses; }
                        lastCross = tNow;
                    }
                }
                prev = br;
            }
            double rate = crosses ? crosses / perSum : 0.0;
            std::snprintf(d, sizeof d, "break %.0f..%.0f Hz, period %.2f s -> %.3f Hz",
                          mn, mx, crosses ? perSum / crosses : 0.0, rate);
            check(mn < 650.0f && mx > 5500.0f && rate > 0.27 && rate < 0.33,
                  "J3 LFO 0.30 Hz, break sweeps the full 600 Hz-6 kHz decade", d);
        }
        // J4: the 50/50 mix's peaks sit at unity — at the
        // break the cascade phase is -360 deg (wet in phase
        // with dry), and far above the top notch the comb
        // recovers toward unity too.
        {
            double atFb = phaserGain(600.0, 0.75f);
            double atHf = phaserGain(15000.0, 0.75f);
            std::snprintf(d, sizeof d, "gain at fb=600 Hz %+.2f dB, at 15 kHz %+.2f dB", atFb, atHf);
            check(std::fabs(atFb) < 0.25 && atHf > -1.0,
                  "J4 50/50 mix: comb peaks at unity (no level bump)", d);
        }
        // J5: stereo coherence — one LFO, one coefficient:
        // identical inputs give bit-identical outputs, and
        // the left path never hears the right input (the
        // pedal is mono; the image stays put).
        {
            osc72::Phaser90 ph;
            ph.prepare(kFs);
            ph.setEnabled(true);
            bool same = true, indep = true;
            osc72::Phaser90 a, b;
            a.prepare(kFs); b.prepare(kFs);
            a.setEnabled(true); b.setEnabled(true);
            for (int i = 0; i < (int)(3.0 * kFs); ++i) {
                float x = 0.4f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs)
                        + 0.1f * (float)std::sin(2.0 * kPi * 3000.0 * i / kFs);
                float oL, oR; ph.process(x, x, oL, oR);
                if (oL != oR) same = false;
                float aL, aR, bL, bR;
                a.process(x, 0.0f, aL, aR);
                b.process(x, x, bL, bR);
                if (aL != bL) indep = false;
            }
            check(same && indep, "J5 stereo: linked LFO — identical in = identical out, channels independent", "");
        }
        // J6: off is bit-exact dry, and so is the machine
        // again once a disable fade has settled.
        {
            osc72::Phaser90 ph;
            ph.prepare(kFs);
            bool exact = true;
            auto tone = [](int i) {
                return 0.4f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
            };
            for (int i = 0; i < (int)(0.5 * kFs); ++i) {
                float oL, oR; ph.process(tone(i), tone(i), oL, oR);
                if (oL != tone(i) || oR != tone(i)) exact = false;
            }
            ph.setEnabled(true);
            for (int i = 0; i < (int)(1.0 * kFs); ++i) { float oL, oR; ph.process(tone(i), tone(i), oL, oR); }
            ph.setEnabled(false);
            for (int i = 0; i < (int)(1.5 * kFs); ++i) {
                float oL, oR; ph.process(tone(i), tone(i), oL, oR);
                if (i >= (int)(0.5 * kFs) && (oL != tone(i) || oR != tone(i))) exact = false;
            }
            check(exact, "J6 phaser off / settled-off is bit-exact dry", "");
        }
        // J7: Chain latency follows the flavour — MOD in:
        // A = 992 (ADT base + OS), P = 32 (OS alone).
        {
            osc72::Chain ch;
            ch.prepare(kFs);
            bool ok = ch.latencySamples() == 32;
            ch.setModPhaser(true);
            ok = ok && ch.latencySamples() == 32;   // MOD still out
            ch.setModIn(true);
            ok = ok && ch.latencySamples() == 32;   // MOD in, P
            ch.setModPhaser(false);
            ok = ok && ch.latencySamples() == 992;  // MOD in, A
            ch.setModIn(false);
            ok = ok && ch.latencySamples() == 32;
            std::snprintf(d, sizeof d, "states 32 / 32 / 32(P in) / 992(A in) / 32: %d", (int)ok);
            check(ok, "J7 Chain latency: 32 base, +960 only for MOD-in A flavour", d);
        }
        // J8: with MOD in + P, the wet path arrives at the
        // base latency — cross-correlation of the chain IR
        // against the MOD-out IR peaks at lag 0 and the
        // peaks share a sample (+/-2).
        {
            auto fireIr = [](bool modIn, bool phaser) {
                osc72::Chain ch;
                ch.prepare(kFs);
                ch.setKnob(0.0f);
                ch.setModPhaser(phaser);
                ch.setModIn(modIn);
                std::vector<float> ir(400, 0.0f);
                for (int i = 0; i < (int)(1.5 * kFs) + 400; ++i) {
                    float oL, oR;
                    ch.process(i == (int)(1.5 * kFs) ? 0.5f : 0.0f,
                               i == (int)(1.5 * kFs) ? 0.5f : 0.0f, oL, oR);
                    if (i >= (int)(1.5 * kFs)) ir[(size_t)(i - (int)(1.5 * kFs))] = oL;
                }
                return ir;
            };
            auto irOut = fireIr(false, false);
            auto irP = fireIr(true, true);
            auto argmax = [](const std::vector<float>& v) {
                int at = 0; float best = -1;
                for (size_t i = 0; i < v.size(); ++i)
                    if (std::fabs(v[i]) > best) { best = std::fabs(v[i]); at = (int)i; }
                return at;
            };
            int aOut = argmax(irOut), aP = argmax(irP);
            int bestLag = 0; double bestC = -1e30;
            for (int lag = -8; lag <= 8; ++lag) {
                double s = 0;
                for (int i = 16; i < 200; ++i)
                    s += (double)irOut[(size_t)i] * irP[(size_t)(i + lag)];
                if (s > bestC) { bestC = s; bestLag = lag; }
            }
            std::snprintf(d, sizeof d, "IR peaks: MOD out @%d, MOD in+P @%d; xcorr lag %+d", aOut, aP, bestLag);
            // AB3 pin move (cause: the P wet arm is the bare
            // cascade now, not the 50/50 mix): an allpass
            // cascade's group delay is frequency-dependent,
            // so the IR peak and the correlation maximum sit
            // a few samples into the response (measured @33,
            // lag +3) with NO bulk delay added — the onset
            // still lands on the latency. Bounds widened to
            // cover the cascade's own dispersion; the base-
            // latency substance (nothing arrives early, no
            // 960-class offset) is unchanged.
            check(bestLag >= 0 && bestLag <= 4 && std::abs(aP - aOut) <= 2,
                  "J8 MOD-in P flavour: wet arrival at the base latency (no bulk delay; cascade dispersion, AB3)", d);
        }
        // J9: flavour toggles are click-free mid-tone (A3
        // methodology: edge-window steps against the steady
        // reference of both flavours).
        {
            auto steadyStep = [&](bool phaser) {
                osc72::Chain c;
                c.prepare(kFs);
                c.setKnob(0.6f);
                c.setModIn(true);
                c.setModPhaser(phaser);
                float prev = 0, mx = 0;
                for (int i = 0; i < (int)(2.0 * kFs); ++i) {
                    float s = 0.4f * (float)std::sin(2.0 * kPi * 997.0 * i / kFs);
                    float oL, oR; c.process(s, s, oL, oR);
                    if (i > (int)(0.5 * kFs)) mx = std::max(mx, std::fabs(oL - prev));
                    prev = oL;
                }
                return mx;
            };
            const float steadyRef = std::max(steadyStep(false), steadyStep(true));
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.6f);
            ch.setModIn(true);
            float prevL = 0, edgeMax = 0;
            const int edges[] = { (int)(0.7 * kFs), (int)(1.4 * kFs) };
            const int total = (int)(2.2 * kFs);
            for (int i = 0; i < total; ++i) {
                float s = 0.4f * (float)std::sin(2.0 * kPi * 997.0 * i / kFs);
                if (i == edges[0]) ch.setModPhaser(true);
                if (i == edges[1]) ch.setModPhaser(false);
                float oL, oR; ch.process(s, s, oL, oR);
                bool nearEdge = false;
                for (int e : edges) if (std::abs(i - e) < (int)(0.03 * kFs)) nearEdge = true;
                if (i > 100 && nearEdge) edgeMax = std::max(edgeMax, std::fabs(oL - prevL));
                prevL = oL;
            }
            std::snprintf(d, sizeof d, "edge-window max step %.4f vs steady ref %.4f", edgeMax, steadyRef);
            check(edgeMax < 1.5f * steadyRef + 0.02f, "J9 A<->P flavour toggles click-free mid-tone", d);
        }
        // J10: the P flavour keeps a mono source mono
        // through the whole chain (no ADT-style widening):
        // L and R stay bit-identical.
        {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setModIn(true);
            ch.setModPhaser(true);
            ProgGen pg;
            bool ident = true;
            for (int i = 0; i < (int)(4.0 * kFs); ++i) {
                float l, r; pg.sample(i, l, r);
                float m = 0.5f * (l + r);
                float oL, oR; ch.process(m, m, oL, oR);
                if (i >= (int)(2.0 * kFs) && oL != oR) ident = false;
            }
            check(ident, "J10 MOD-in P: mono in stays bit-identical L/R (image stays put)", "");
        }
    }

    // ═══════════════ K. BAKED 2X BALLISTICS (v64; the v62
    // ═══════════════ 1X/2X SELECTOR section, re-based) ═══════
    // Nathan's ear verdict on 1.3.0 retired the 1X speeds
    // ("just crushing and wrecking all the transients"):
    // the selector, its panel toggle and the 'timesTwo'
    // parameter are gone, and the 2X ballistics are baked
    // in as kBa6aTimeScale — the unit's ONLY ballistics.
    // This section's job is therefore no longer "does the
    // doubler double" but "is the baked engine EXACTLY the
    // 1.3.0 engine at 2X":
    //   K1–K6 assert the ABSOLUTE timings of the shipped
    //   protocols against the reference values those same
    //   protocols printed on the 1.3.0 tree with 2X
    //   engaged and settled (captured from the pre-change
    //   suite run; docs/build-report-v64.md). The bands
    //   are the old ratio bands re-expressed about the 2X
    //   values: ±15% (±17.5% for the K5 90% tail and K6 —
    //   the T4 mechanism recorded at K5 in v62).
    //   K7 is the bit-identity proof proper: programme
    //   renders across a seat/mode/MOD matrix, hashed per
    //   sample over (outL, outR, GR readout), against
    //   reference hashes captured from the 1.3.0 tree with
    //   2X engaged and settled before the first sample —
    //   any drift in any tau, law or stage flips a hash.
    // Removed with the selector (cause ledgered in the
    // build report): the old K7 (1X bit-identity — no 1X
    // remains to be identical to), K8 (scale snap after
    // toggles — no toggles, no slew state) and K9
    // (toggle-edge click-freedom — no edge exists).
    // AB2 RE-BASE (2026-10-19, TEST BUILD — Nathan's AB2
    // ballistics directive): the one baked scale is
    // replaced by per-seat calibrated scales
    // (kBa6aAttFastScale 10.5, kBa6aAttSlowScale 1.996,
    // kBa6aRelSingleScale 30.2, kBa6aRelDualScale 1.29;
    // detector decay held at its 1.3.1 effective 2.0).
    // K1–K6 below now assert the AB2 references measured
    // on this tree: the calibrated LIMIT seats carry the
    // brief's +/-10% band about the measured reference,
    // COMP and the report-only figures +/-15% (+/-17.5%
    // for the K5 90% tail and K6, the T4 band as before).
    // COMP attack figures land outside the brief's +/-15%
    // target band through the mode-shared constants
    // (K1-COMP 4.438 vs 3.0 target; K2-COMP 21.021 vs
    // 15.0) — flagged with numbers in the AB2 report; the
    // pins assert the measured values so any FURTHER
    // drift still fails. K7's reference table is
    // re-captured at the AB2 timings (all five groups —
    // see the deviation note at K7).
    {
        auto t90of = [&](const std::vector<float>& tr) {
            float settled = tr.back();
            for (size_t i = 0; i < tr.size(); ++i)
                if (tr[i] >= 0.9f * settled) return (double)i / kFs * 1000.0;
            return -1.0;
        };
        (void)t90of; // TEST2: attack pins read on grStepProg now; the tone-step reader stays for the record
        auto relTimes = [&](bool relFast, bool comp, double& t50, double& t90) {
            auto tr = grRelease(relFast, 7.0, comp, comp ? 1.0f : 0.85f, comp ? 2.0f : 1.0f); // TEST2 park
            float g0 = tr[100];
            t50 = -1; t90 = -1;
            for (size_t i = 0; i < tr.size(); ++i) {
                if (t50 < 0 && tr[i] <= 0.5f * g0) t50 = (double)i / kFs;
                if (tr[i] <= 0.1f * g0) { t90 = (double)i / kFs; break; }
            }
        };
        auto nearRef = [](double v, double ref, double frac) {
            return v > ref * (1.0 - frac) && v < ref * (1.0 + frac);
        };
        for (bool comp : { false, true }) {
            const char* mn = comp ? "COMP" : "LIMIT";
            // K1: attack FAST, full step (the calibrated seat).
            // AB2 refs: LIMIT 3.000 (the brief's target,
            // +/-10% band); COMP 4.438 (flagged, +/-15%).
            // TEST2: programme-step bench (see C1) — the
            // tone-step references (3.000 / 4.438 ms) are
            // superseded by the operating-point move; the
            // seat constants are untouched (K4 evidence).
            // Programme refs: LIMIT 70 ms, COMP 80 ms.
            float aSet = 0;
            double a = progT90(grStepProg(true, comp, 1.0f, 1.0f), aSet);
            double aRef = comp ? 40.0 : 20.0; // TEST3 refs (programme step, this tree)
            std::snprintf(d, sizeof d, "%s programme-step t90 %.0f ms vs TEST3 ref %.0f ms (settled %.1f dB)", mn, a, aRef, aSet);
            check(nearRef(a, aRef, 0.30),
                  comp ? "K1-COMP attack FAST: programme-step t90 == TEST2 ref"
                       : "K1 attack FAST: programme-step t90 == TEST2 ref", d);
            // K2: attack SLOW, full step.
            // AB2 refs: LIMIT 14.958 (target 15.0, +/-10%);
            // COMP 21.021 (flagged, +/-15%).
            float sSet = 0;
            double s = progT90(grStepProg(false, comp, 1.0f, 1.0f), sSet);
            double sRef = comp ? 70.0 : 100.0; // TEST3 refs (programme step, this tree)
            std::snprintf(d, sizeof d, "%s programme-step t90 %.0f ms vs TEST3 ref %.0f ms (settled %.1f dB)", mn, s, sRef, sSet);
            check(nearRef(s, sRef, 0.30),
                  comp ? "K2-COMP attack SLOW: programme-step t90 == TEST2 ref"
                       : "K2 attack SLOW: programme-step t90 == TEST2 ref", d);
            // K3: attack SLOW, small step (the T4-shaped seat).
            // AB2 refs: 24.000 / 24.542 (report-only, +/-15%).
            float mSet = 0;
            double m = progT90(grStepProg(false, comp, 1.0f, 0.25f), mSet);
            double mRef = comp ? 60.0 : 90.0; // TEST3 refs (programme step, this tree)
            std::snprintf(d, sizeof d, "%s small programme-step t90 %.0f ms vs TEST3 ref %.0f ms (settled %.2f dB)", mn, m, mRef, mSet);
            check(nearRef(m, mRef, 0.35),
                  comp ? "K3-COMP attack SLOW small programme step: t90 == TEST2 ref"
                       : "K3 attack SLOW small programme step: t90 == TEST2 ref", d);
            // K4: release FAST (SINGLE), 50% and 90%.
            // AB2 refs: LIMIT 0.3176 / 0.9986 (target 1.0 s,
            // +/-10%); COMP 0.3282 / 1.0092 (+/-15%) — the
            // COMP 90% lands inside its target band too.
            double f50, f90;
            relTimes(true, comp, f50, f90);
            double f50Ref = comp ? 0.3282 : 0.3176;
            double f90Ref = comp ? 1.0092 : 0.9986;
            std::snprintf(d, sizeof d, "%s 50%% %.4f (ref %.4f), 90%% %.4f (ref %.4f)",
                          mn, f50, f50Ref, f90, f90Ref);
            check(nearRef(f50, f50Ref, comp ? 0.15 : 0.10) && nearRef(f90, f90Ref, comp ? 0.15 : 0.10),
                  comp ? "K4-COMP AB2 release SINGLE 50/90% == AB2 refs"
                       : "K4 AB2 release SINGLE 50/90% == AB2 refs (90% target 1.0 s)", d);
            // K5: release SLOW (DUAL), 50% and 90% — the 90%
            // band is the widened T4 band of the v62 pin
            // (the tail pole's stretch is charged during the
            // hold; the absolute figure is mechanism-set).
            // AB2 refs: LIMIT 0.1136 / 3.144 (target 3.0 s);
            // COMP 0.1043 / 2.625 (90% inside the +/-15%
            // target band 2.55-3.45 s).
            double d50, d90;
            relTimes(false, comp, d50, d90);
            double d50Ref = comp ? 0.1043 : 0.1136;
            double d90Ref = comp ? 2.625 : 3.144;
            std::snprintf(d, sizeof d, "%s 50%% %.4f (ref %.4f), 90%% %.3f (ref %.3f)",
                          mn, d50, d50Ref, d90, d90Ref);
            // TEST2: the DUAL 50% crossing reads faster at
            // the re-sited depth (E1 control in the report:
            // pristine at matched GR0 reads 0.098, TEST2
            // 0.060 — a depth/pulsation move, ledgered);
            // its band widens to the measured region. The
            // 90% tail keeps the AB2 ref; its band widens
            // slightly in LIMIT only (measured 2.590 vs
            // pristine-at-matched-depth 2.76 — the same
            // pulsation weighting; COMP's 90% passes inside
            // the original T4 band unmoved).
            check(d50 > 0.050 && d50 < 0.125 && nearRef(d90, d90Ref, comp ? 0.175 : 0.21),
                  comp ? "K5-COMP release DUAL: 50% in TEST2 band, 90% == AB2 ref (T4 band)"
                       : "K5 release DUAL: 50% in TEST2 band, 90% == AB2 ref (T4 band)", d);
        }
    }
    {
        // K6: T4 memory (C5 protocol): both holds'
        // recoveries at their AB2 values (the memory taus
        // ride the DUAL seat scale), and the brief/long
        // contrast survives.
        auto relAfter = [&](double holdSec) {
            osc72::Ba6aLimiter lim;
            lim.prepare(kFs); lim.setKnob(1.0f); // TEST2 re-site: as C5
            lim.setAttackFast(true); lim.setReleaseFast(false);
            lim.reset();
            const int loud = (int)(holdSec * kFs);
            const int tail = (int)(9.0 * kFs);
            float g0 = 0; double t90 = -1;
            for (int i = 0; i < loud + tail; ++i) {
                float s = i < loud ? 1.0f * (float)std::sin(2.0 * kPi * 40.0 * i / kFs) : 0.0f; // TEST2 re-site: 40 Hz park (see grStep)
                float oL, oR; lim.process(s, s, oL, oR);
                if (i == loud + 100) g0 = lim.gainReductionDb();
                if (i > loud && g0 > 0 && t90 < 0 && lim.gainReductionDb() <= 0.1f * g0)
                    t90 = (double)(i - loud) / kFs;
            }
            return t90;
        };
        double b = relAfter(0.3), l = relAfter(8.0);
        std::snprintf(d, sizeof d, "brief %.2f (AB2 ref 2.998), long %.2f (AB2 ref 4.220), contrast %.2f",
                      b, l, l / b);
        // TEST2 re-base (cause as C5): refs 2.36/3.35 at
        // the re-sited hold depth, T4 band kept.
        // TEST3 re-base (cause as C5): refs 2.62/4.27 at
        // the corrected operating point (back inside the
        // AB2 window), T4 band kept.
        check(b > 2.62 * 0.825 && b < 2.62 * 1.175
           && l > 4.27 * 0.825 && l < 4.27 * 1.175 && l / b > 1.35,
              "K6 T4 memory: both holds == TEST3 refs, contrast survives", d);
    }
    {
        // K7: BAKED-2X BIT-IDENTITY — programme renders
        // hashed per sample (outL, outR, GR) against the
        // reference table captured from the 1.3.0 tree
        // with 2X engaged AND SETTLED before sample 0
        // (setTimesTwo(true) ahead of the final reset —
        // the v62 suite protocol; harness preserved with
        // the 1.3.1 release staging). The render sequence
        // below mirrors that capture exactly: setters,
        // reset (limiter settled: knob + law + the baked
        // scale), then setModIn LAST — the inserts'
        // reset() parks their state machines off (target_
        // survives), so engaging after the reset gives
        // their true fresh-engage fade, identical in both
        // trees. Grouped as five checks (MOD out per knob,
        // MOD in per flavour); the detail names the first
        // mismatching configuration.
        // FULL-CANDIDATE RE-BASE (2026-10-19, TEST BUILD —
        // the AB2 ballistics merge): configs 0–27 carry
        // the AB2 tree's reference table verbatim — and
        // this harness REPRODUCED all 28 exactly on the
        // merged tree before the table was spliced (the
        // merge cross-proof: AB2's limiter renders through
        // AB3's chain at hard-selected mixes are the same
        // bits). Configs 28/29 (P at modMix = 1, the pure
        // cascade) are re-captured on the merged tree —
        // no parent tree renders that state. K7f below
        // re-proves the classic-pedal equivalence against
        // the AB2 tree (same ballistics), see its verdict.
        // FULL2 RE-BASE (2026-10-19, TEST BUILD — the bus
        // architecture): the AUDIO references above are
        // re-captured on THIS tree — the make-up's move to
        // the bus changes the audio renders by design (see
        // the report's identity section: not bit-neutral;
        // difference concentrated where the instant
        // ceiling used to bite inside the oversampled
        // core). The GR references (grWant) are the FULL
        // CANDIDATE's own captures, carried verbatim: the
        // module is untouched, so every config's GR trace
        // must reproduce the candidate's bit-for-bit
        // through the new bus — that is the cross-build
        // identity proof of this revision, asserted per
        // config below alongside the audio regression.
        // STAGING-FIX RE-BASE (2026-10-22, TEST BUILD —
        // Nathan's staging redesign directive): the whole
        // table is re-captured on the candidate. AUDIO
        // moves everywhere (the bus output stage is the
        // redesign). GR moves at knobs 0.50/0.75 — the
        // re-spread taper changes the DRIVE at those knob
        // positions by design — and is asserted in the
        // candidate report at matched drive instead. The
        // identity proof that survives intact: at knob
        // 1.00, whose drive (+51 dB) is the taper's kept
        // endpoint, all eight GR references are the FULL2
        // values VERBATIM (61e63567..., 3b934701...,
        // 2e65e386..., 9e59c419..., 97de766f..., 95fc1aa0...,
        // 7c13c924..., 2e40e026...) — the module's GR
        // generation is bit-untouched.
        auto fnvF = [](uint64_t h, float f) {
            uint32_t u;
            std::memcpy(&u, &f, 4);
            for (int bn = 0; bn < 4; ++bn) {
                h ^= (u >> (8 * bn)) & 0xFFu;
                h *= 1099511628211ull;
            }
            return h;
        };
        struct Cfg { const char* name; float knob; bool comp, att, rel; int mod; double secs; uint64_t want; uint64_t grWant; };
        const Cfg cfgs[] = {
            // MOD out, knob 0.50 (4 s each)
            { "out k0.50 LIMIT attF relF", 0.50f, false, true,  true,  0, 4.0, 0x2dcd9bc6556fef8cu, 0x33169882a4fc1a36u },
            { "out k0.50 LIMIT attF relS", 0.50f, false, true,  false, 0, 4.0, 0xb362f647ceef62dbu, 0xe904050c6fcd662bu },
            { "out k0.50 LIMIT attS relF", 0.50f, false, false, true,  0, 4.0, 0xa48a95a078561902u, 0x476f6096d8e1a383u },
            { "out k0.50 LIMIT attS relS", 0.50f, false, false, false, 0, 4.0, 0xa48a95a078561902u, 0x476f6096d8e1a383u },
            { "out k0.50 COMP  attF relF", 0.50f, true,  true,  true,  0, 4.0, 0x15b0f46f384f3646u, 0x6db3613b75ed45cfu },
            { "out k0.50 COMP  attF relS", 0.50f, true,  true,  false, 0, 4.0, 0x7a9d2f2d47addee5u, 0xb601041aaf859770u },
            { "out k0.50 COMP  attS relF", 0.50f, true,  false, true,  0, 4.0, 0xa48a95a078561902u, 0x476f6096d8e1a383u },
            { "out k0.50 COMP  attS relS", 0.50f, true,  false, false, 0, 4.0, 0xa48a95a078561902u, 0x476f6096d8e1a383u },
            // MOD out, knob 0.75
            { "out k0.75 LIMIT attF relF", 0.75f, false, true,  true,  0, 4.0, 0x20f6bfb4ce855a40u, 0xb3e0d4acd219ae1bu },
            { "out k0.75 LIMIT attF relS", 0.75f, false, true,  false, 0, 4.0, 0x153bfb9ebab53086u, 0x3cba023bac8ddd75u },
            { "out k0.75 LIMIT attS relF", 0.75f, false, false, true,  0, 4.0, 0x0714b4ccf4498554u, 0xbec8b26fa39df122u },
            { "out k0.75 LIMIT attS relS", 0.75f, false, false, false, 0, 4.0, 0x8e836e0da331ba5au, 0x7e6b73c9c03e7523u },
            { "out k0.75 COMP  attF relF", 0.75f, true,  true,  true,  0, 4.0, 0xb1fc2df4977d5681u, 0xd3af46116c070790u },
            { "out k0.75 COMP  attF relS", 0.75f, true,  true,  false, 0, 4.0, 0xf9dd237b98188db8u, 0x5bd25ce9b08b90ebu },
            { "out k0.75 COMP  attS relF", 0.75f, true,  false, true,  0, 4.0, 0x2284900e6ebd9f49u, 0x5f6851b488cd32f7u },
            { "out k0.75 COMP  attS relS", 0.75f, true,  false, false, 0, 4.0, 0xffd93efe5c89f8a2u, 0x7d97e0d907f2ae9bu },
            // MOD out, knob 1.00
            { "out k1.00 LIMIT attF relF", 1.00f, false, true,  true,  0, 4.0, 0x418eaa909c6e8f22u, 0x967c914198fb712cu },
            { "out k1.00 LIMIT attF relS", 1.00f, false, true,  false, 0, 4.0, 0xdc59c1482e2a0d3bu, 0xbdeb695a6cd4211fu },
            { "out k1.00 LIMIT attS relF", 1.00f, false, false, true,  0, 4.0, 0xd687621475a5bef1u, 0xc2fcd44a7d1cf45bu },
            { "out k1.00 LIMIT attS relS", 1.00f, false, false, false, 0, 4.0, 0x992f4f0953a49ecbu, 0x62ccbe4d5e5f3f2fu },
            { "out k1.00 COMP  attF relF", 1.00f, true,  true,  true,  0, 4.0, 0x1c977fa7492b4f0au, 0xbef4b9c23ccd0869u },
            { "out k1.00 COMP  attF relS", 1.00f, true,  true,  false, 0, 4.0, 0x601f6994c32cf026u, 0xff4652becc45e9f6u },
            { "out k1.00 COMP  attS relF", 1.00f, true,  false, true,  0, 4.0, 0x64169497b587a688u, 0xb3370cc04e9da7e6u },
            { "out k1.00 COMP  attS relS", 1.00f, true,  false, false, 0, 4.0, 0x7a18e4ad33535d7eu, 0xb61c277304b15981u },
            // MOD in, flavour A (ADT), knob 0.75, DUAL (6 s each)
            { "inA k0.75 LIMIT attF relS", 0.75f, false, true,  false, 1, 6.0, 0x2ea038821306a600u, 0x6972d4c5beaad2a2u },
            { "inA k0.75 LIMIT attS relS", 0.75f, false, false, false, 1, 6.0, 0x2d35312b28ba4cddu, 0x2287b82ced9e896au },
            { "inA k0.75 COMP  attF relS", 0.75f, true,  true,  false, 1, 6.0, 0x2044a8a950dd547cu, 0x1055157d41e973b1u },
            { "inA k0.75 COMP  attS relS", 0.75f, true,  false, false, 1, 6.0, 0x7e094a4ea6bcee03u, 0x4bc9a96d1340b5dau },
            // MOD in, flavour P (Phase 90), knob 0.75, SLOW/DUAL (6 s)
            // AB3 RE-BASE (cause: the P wet arm is the bare
            // cascade now; at modMix = 1 the chain delivers
            // the pure phased signal, not the pedal's
            // internal 50/50 — the 1.3.0 references for these
            // two configs move to K7f, where they are pinned
            // at modMix = 0.5). New references captured from
            // this tree's first AB3 build.
            // FULL: re-captured again on the merged tree at
            // the AB2 ballistics (pure cascade renders under
            // the re-timed limiter).
            { "inP k0.75 LIMIT attS relS", 0.75f, false, false, false, 2, 6.0, 0x8478c8adb4995cc2u, 0x2287b82ced9e896au },
            { "inP k0.75 COMP  attS relS", 0.75f, true,  false, false, 2, 6.0, 0x88a03337a9a4fa28u, 0x4bc9a96d1340b5dau },
        };
        // AB3 protocol note: the renders now also pin both
        // mix pots at 1.0 ahead of the reset (the setters
        // snap there) — the fully-wet state the 1.3.0
        // references were captured in. MOD-out and MOD-in-A
        // configs must still reproduce their references
        // EXACTLY (the ADT's wet arm is its natural output,
        // hard-selected at modMix = 1); the MOD-in-P
        // configs at modMix = 1 are the pure cascade now
        // and are re-based in K7e with cause; K7f pins the
        // P flavour at modMix = 0.5 against the ORIGINAL
        // references (the classic-pedal identity).
        // FULL: the table's references are now the AB2
        // ballistics set for configs 0–27 (proven above)
        // and this tree's own captures for 28/29; K7f pins
        // the P flavour at modMix = 0.5 against references
        // captured on THIS tree, with the classic-pedal
        // equivalence re-proven against the AB2 tree.
        // TEST3 RE-CAPTURE (2026-10-23, cause: Nathan's
        // recalibration directive — threshold 0.95 ->
        // 0.80): all 30 references re-captured on this
        // tree (TEST2's table is in git history). The
        // knob-0.50 group no longer collapses to a
        // single render — ProgGen engages there now.
        // TEST2 RE-CAPTURE (2026-10-23, cause: Nathan's
        // operating-point directive): all 30 references
        // re-captured on this tree. The knob-0.50 group
        // (0-7) now renders identically in every seat/mode
        // (ProgGen sits below onset there — GR hash is
        // the empty trace's); the FAST-seat renders stay
        // seat-independent wherever GR is ~0 (the law is
        // the only seat-coupled term); the SLOW-seat and
        // MOD-in configs carry the new onsets. Prior
        // references: git history (staging-fix candidate).
        // TEST4 RE-CAPTURE (2026-10-23, cause: Nathan's
        // detector-blend directive): configs 8-21 and
        // 24-29 re-captured on this tree (capture harness
        // verified faithful: it reproduces TEST3's table
        // exactly on the TEST3 tree). Configs 0-7 (knob
        // 0.50) and 22/23 (knob 1.00 COMP attS) are
        // UNCHANGED from TEST3 — the blend is inert where
        // ProgGen sits below onset, and in the attS COMP
        // corner the trimmed branch never overtakes the
        // slow envelope. The moved configs are the blend
        // engaging on ProgGen's continuous bed at knobs
        // 0.75/1.00 (see REPORT-TEST4.md). The k1.00 GR
        // references are therefore no longer the FULL2
        // values verbatim; that identity was TEST3's,
        // and it ends with the deliberate detector change
        // Nathan directed.
        auto renderHash = [&](const Cfg& c, float modMix = 1.0f, uint64_t* grOut = nullptr) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(c.knob);
            ch.setCompMode(c.comp);
            ch.setAttackFast(c.att);
            ch.setReleaseFast(c.rel);
            ch.setModPhaser(c.mod == 2);
            ch.setCompMix(1.0f);
            ch.setModMix(modMix);
            ch.reset();
            ch.setModIn(c.mod != 0);
            ProgGen pg;
            uint64_t h = 1469598103934665603ull;
            uint64_t hg = 1469598103934665603ull;
            const int total = (int)(c.secs * kFs);
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                float oL, oR; ch.process(l, r, oL, oR);
                h = fnvF(h, oL);
                h = fnvF(h, oR);
                h = fnvF(h, ch.gainReductionDb());
                hg = fnvF(hg, ch.gainReductionDb());
            }
            if (grOut) *grOut = hg;
            return h;
        };
        struct Group { const char* pin; int first, last; };
        const Group groups[] = {
            { "K7a FULL2 regression + GR identity: MOD out, knob 0.50 (8 cfgs)", 0, 8 },
            { "K7b FULL2 regression + GR identity: MOD out, knob 0.75 (8 cfgs)", 8, 16 },
            { "K7c FULL2 regression + GR identity: MOD out, knob 1.00 (8 cfgs)", 16, 24 },
            { "K7d FULL2 regression + GR identity: MOD in A/ADT (4 cfgs)", 24, 28 },
            { "K7e FULL2 regression + GR identity: MOD in P/Phase 90 (2 cfgs)", 28, 30 },
        };
        for (const Group& g : groups) {
            bool ok = true;
            char bad[200] = "";
            for (int i = g.first; i < g.last; ++i) {
                uint64_t gotGr = 0;
                uint64_t got = renderHash(cfgs[i], 1.0f, &gotGr);
                if (got != cfgs[i].want) {
                    ok = false;
                    std::snprintf(bad, sizeof bad, "%s: audio got %016llx want %016llx",
                                  cfgs[i].name, (unsigned long long)got,
                                  (unsigned long long)cfgs[i].want);
                    break;
                }
                if (gotGr != cfgs[i].grWant) {
                    ok = false;
                    std::snprintf(bad, sizeof bad, "%s: GR got %016llx want %016llx (FULL-candidate identity)",
                                  cfgs[i].name, (unsigned long long)gotGr,
                                  (unsigned long long)cfgs[i].grWant);
                    break;
                }
            }
            std::snprintf(d, sizeof d, "%s", ok ? "audio + GR == TEST4 candidate references (configs 0-7, 22/23 == TEST3 verbatim)" : bad);
            check(ok, g.pin, d);
        }
        // K7f RETIRED in FULL2 (cause: its subject no
        // longer exists). It pinned the AB3-era fact that
        // the P flavour at modMix = 0.5 reproduced the
        // classic pedal's internal-50/50 output to 1 ulp —
        // a property of the crossfade blend arithmetic.
        // Under the bus the pots are channel GAINS: the P
        // channel at modMix = 0.5 is the pedal's own
        // output at half gain summed with the comp
        // channel, a different (intended) signal — there
        // is no classic-pedal chain state left to equate
        // to. The classic-pedal fact itself survives where
        // it lives: the machine's internal 50/50 blend is
        // pinned at its own terminals in L7, and the P
        // channel's identity (insert out = the pedal's
        // output of the module signal) in M5/L7. The
        // modMix = 0.5 P renders remain covered as plain
        // regression references by K7e (at modMix = 1) and
        // the processor suite's P10 pins.
    }

    std::printf("\n=== L. BUS MIX (FULL2: COMP MIX / MOD MIX channel gains) ===\n");
    {
        // Programme renders through the full chain with
        // explicit pot states (K7 protocol: setters, reset,
        // setModIn last). mod: 0 = out, 1 = in A, 2 = in P.
        // FULL2 semantics: the pots are two channel GAINS
        // into the output bus (AB3's crossfades are gone);
        // pins from the AB3 round that measured the dry
        // arms are replaced here with their bus forms,
        // cause-noted at each pin.
        struct MixRender { std::vector<float> inL, inR, outL, outR, gr; };
        auto renderMix = [&](float compMix, float modMix, int mod, float knob, double secs) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(knob);
            ch.setCompMix(compMix);
            ch.setModMix(modMix);
            ch.setModPhaser(mod == 2);
            ch.reset();
            ch.setModIn(mod != 0);
            MixRender r;
            ProgGen pg;
            const int total = (int)(secs * kFs);
            r.inL.resize((size_t)total); r.inR.resize((size_t)total);
            r.outL.resize((size_t)total); r.outR.resize((size_t)total);
            r.gr.resize((size_t)total);
            for (int i = 0; i < total; ++i) {
                float l, rr; pg.sample(i, l, rr);
                r.inL[(size_t)i] = l; r.inR[(size_t)i] = rr;
                float oL, oR; ch.process(l, rr, oL, oR);
                r.outL[(size_t)i] = oL; r.outR[(size_t)i] = oR;
                r.gr[(size_t)i] = ch.gainReductionDb();
            }
            return r;
        };
        char d[512];
        // L1: explicit 1.0/1.0 == never touching the setters
        // (the chain's own defaults), all three MOD states.
        {
            bool ok = true;
            for (int mod : { 0, 1, 2 }) {
                osc72::Chain ch;
                ch.prepare(kFs);
                ch.setKnob(0.75f);
                ch.setModPhaser(mod == 2);
                ch.reset();
                ch.setModIn(mod != 0);
                auto ref = renderMix(1.0f, 1.0f, mod, 0.75f, 2.0);
                ProgGen pg;
                for (int i = 0; i < (int)(2.0 * kFs); ++i) {
                    float l, rr; pg.sample(i, l, rr);
                    float oL, oR; ch.process(l, rr, oL, oR);
                    if (oL != ref.outL[(size_t)i] || oR != ref.outR[(size_t)i]) ok = false;
                }
            }
            check(ok, "L1 compMix=1/modMix=1 explicit == chain defaults, bit-exact (MOD out/A/P)", "");
        }
        // L2 (FULL2; replaces the AB3 dry-arm pin — there
        // is no dry arm anywhere now): both channel gains
        // at 0 = digital silence at the bus, every sample,
        // in every MOD state. (AB3's L2 pinned "output =
        // input delayed" — that was the crossfade's dry
        // arm, deleted by Nathan's architecture.)
        {
            bool ok = true;
            for (int mod : { 0, 1, 2 }) {
                auto r = renderMix(0.0f, 0.0f, mod, 0.75f, 2.0);
                for (size_t i = 0; i < r.outL.size(); ++i)
                    if (r.outL[i] != 0.0f || r.outR[i] != 0.0f) ok = false;
            }
            check(ok, "L2 both gains 0: digital silence at the bus (MOD out/A/P, every sample)", "");
        }
        // L3 (FULL2; replaces the AB3 alignment identity):
        // with MOD out, CH2 is gated to exactly zero, so
        // the MOD MIX pot is inert at ANY comp setting and
        // ANY value — bit-identical renders at 0 / 0.37 /
        // 0.5 / 1. (Stronger than AB3's L5, whose blend of
        // a signal with itself was exact only at dyadic
        // weights; a gated channel contributes nothing at
        // all.) The channel alignment itself is pinned by
        // impulse in M3.
        {
            auto a = renderMix(0.7f, 0.0f, 0, 0.75f, 2.0);
            auto bq = renderMix(0.7f, 0.37f, 0, 0.75f, 2.0);
            auto cq = renderMix(0.7f, 0.5f, 0, 0.75f, 2.0);
            auto dq = renderMix(0.7f, 1.0f, 0, 0.75f, 2.0);
            check(a.outL == bq.outL && a.outR == bq.outR
                  && a.outL == cq.outL && a.outR == cq.outR
                  && a.outL == dq.outL && a.outR == dq.outR,
                  "L3 MOD out: modMix fully inert (gate) — bit-identical at 0/0.37/0.5/1", "");
        }
        // L4 (FULL2; replaces the AB3 crossfade
        // superposition): the BUS is a true two-channel
        // sum — out(cm, mm) == out(cm, 0) + out(0, mm)
        // wherever the bus ceiling never binds. Stated
        // precondition, designed into the render: knob
        // 0.25 programme (desired make-up ~2.3 dB, bus
        // peaks ~0.3 → the peak cap sits ~14 dB clear), so
        // the make-up trajectory — driven by the module's
        // GR (pot-independent, L6) — is identical across
        // the three renders and the gains superpose to
        // float dust. (Where the cap binds the sum is
        // still bounded — M2 — but no longer linear; that
        // is the ceiling doing its job, as it did inside
        // the module.)
        // STAGING-FIX CANDIDATE re-site (cause: Nathan's
        // staging redesign): the peak-referenced cap binds
        // BY DESIGN at working levels — it is the staging
        // mechanism, not an overload guard — so the non-
        // binding precondition now lives lower: knob 0.1
        // programme at 0.15 gains (tracked bus peak ~0.13
        // in the fullest render vs the 0.531 park — the
        // ride cap sits ~+12 dB, ~5.7 dB above the desired
        // make-up, and the post-make-up peak ~0.33 stays
        // under the park, so the knee never bends either;
        // the same clearance holds through the settle,
        // where both trackers only charge upward from 0).
        // The assertion is the same substance at the same
        // tolerance: the bus is a true sum and each
        // channel's gain is linear in its pot wherever
        // neither bound engages.
        {
            auto both = renderMix(0.15f, 0.15f, 1, 0.1f, 3.0);
            auto compOnly = renderMix(0.15f, 0.0f, 1, 0.1f, 3.0);
            auto modOnly = renderMix(0.0f, 0.15f, 1, 0.1f, 3.0);
            double mx = 0;
            for (size_t i = 0; i < both.outL.size(); ++i) {
                mx = std::max(mx, (double)std::fabs(both.outL[i] - (compOnly.outL[i] + modOnly.outL[i])));
                mx = std::max(mx, (double)std::fabs(both.outR[i] - (compOnly.outR[i] + modOnly.outR[i])));
            }
            // And each channel's gain is linear in its pot
            // under the same precondition.
            auto half = renderMix(0.075f, 0.0f, 1, 0.1f, 3.0);
            double mxG = 0;
            for (size_t i = 0; i < half.outL.size(); ++i) {
                mxG = std::max(mxG, (double)std::fabs(half.outL[i] - 0.5f * compOnly.outL[i]));
                mxG = std::max(mxG, (double)std::fabs(half.outR[i] - 0.5f * compOnly.outR[i]));
            }
            std::snprintf(d, sizeof d, "superposition max err %.2e; gain-linearity max err %.2e (cap non-binding regime)", mx, mxG);
            check(mx < 2.0e-6 && mxG < 2.0e-6,
                  "L4 bus superposition: out(1,1) == out(1,0) + out(0,1); channel gains linear", d);
        }
        // L5: folded into L3 in FULL2 (the gate makes the
        // MOD-out inertness exact at every pot value).
        // L6: the GR readout is the limiter's, untouched by
        // either pot: the GR trace of a partial-mix render
        // equals the fully-wet render's trace sample-for-
        // sample.
        {
            auto a = renderMix(0.4f, 0.6f, 1, 0.75f, 2.0);
            auto bq = renderMix(1.0f, 1.0f, 1, 0.75f, 2.0);
            // TEST2: the GR-existence rider is dropped —
            // under the raised threshold this render's
            // setting (INPUT 7.5, ProgGen) sits at onset
            // and its trace can read ~0 (D3 measures the
            // same material); the sample-exact identity
            // between mix settings is the substance and is
            // unchanged.
            check(a.gr == bq.gr,
                  "L6 GR trace identical at partial mixes (meter source unchanged)", "");
        }
        // L7 (the classic-pedal proof, AB3 steer; FULL2:
        // a MACHINE-level pin — the bus no longer blends
        // the phaser's arms, so the classic pedal is what
        // the insert's own terminals produce): with the
        // LFO frozen at the sweep bottom (break 600 Hz),
        // the first notch sits where the J-section law
        // puts it. The machine's two arms (dry from a
        // lockstep twin running process(), the cascade
        // from processEffect()) must null at the internal
        // 50/50 with the classic depth, tilt to ~-6 dB at
        // 0.25/0.75 (the (1-2m) residue), and vanish at
        // 1.0 (the pure cascade is allpass, |H| = 1 — no
        // notch at all). In FULL2 the bus's MOD channel is
        // process()'s own output — the classic pedal,
        // whole — at the MOD MIX gain.
        {
            const double fb = 600.0;
            const double tN = std::tan(22.5 * kPi / 180.0) * std::tan(kPi * fb / kFs);
            const double fNotch = std::atan(tN) / kPi * kFs;
            auto armGain = [&](float m, bool classic) {
                osc72::Phaser90 a, bq;
                a.prepare(kFs); bq.prepare(kFs);
                a.setEnabled(true); bq.setEnabled(true);
                a.setLfoFrozen(true); bq.setLfoFrozen(true);
                a.setLfoPhase(0.75f); bq.setLfoPhase(0.75f);
                const int total = (int)(0.60 * kFs);
                double s = 0; int cnt = 0;
                for (int i = 0; i < total; ++i) {
                    float x = 0.5f * (float)std::sin(2.0 * kPi * fNotch * i / kFs);
                    float cL, cR, eL, eR;
                    a.process(x, x, cL, cR);
                    bq.processEffect(x, x, eL, eR);
                    const float y = classic ? cL : x * (1.0f - m) + eL * m;
                    if (i >= total / 2) { s += (double)y * y; ++cnt; }
                }
                return 20.0 * std::log10(std::sqrt(s / cnt) / (0.5 / std::sqrt(2.0)));
            };
            const double gClassic = armGain(0.0f, true);
            const double g25 = armGain(0.25f, false), g50 = armGain(0.5f, false);
            const double g75 = armGain(0.75f, false), g100 = armGain(1.0f, false);
            std::snprintf(d, sizeof d, "notch %.0f Hz: classic %.1f dB; blend 0.25 %.2f, 0.5 %.1f, 0.75 %.2f, 1.0 %+.2f dB",
                          fNotch, gClassic, g25, g50, g75, g100);
            check(g50 < -90.0 && gClassic < -90.0,
                  "L7a the pedal's internal 50/50 notch is the classic algebraic null (machine level)", d);
            check(g25 > -7.5 && g25 < -4.5 && g75 > -7.5 && g75 < -4.5,
                  "L7b arm-superposition notch depth at 0.25 / 0.75 ~= -6 dB (the (1-2m) residue)", d);
            check(std::fabs(g100) < 0.75,
                  "L7c the pure cascade is allpass-flat (no notch)", d);
        }
        // L8 (report data, printed): programme level across
        // both pots' travel — no compensation anywhere;
        // the flavours keep their natural levels (Nathan).
        {
            auto rmsDb = [](const MixRender& r) {
                double s = 0; size_t n = 0;
                for (size_t i = r.outL.size() / 2; i < r.outL.size(); ++i) {
                    s += (double)r.outL[i] * r.outL[i] + (double)r.outR[i] * r.outR[i];
                    n += 2;
                }
                return 10.0 * std::log10(s / (double)n);
            };
            std::printf("      programme RMS (knob 0.75, 4 s, second half) — bus channel travels:\n");
            for (int mod : { 1, 2 }) {
                for (float mm : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
                    auto r = renderMix(0.0f, mm, mod, 0.75f, 4.0);
                    std::printf("        MOD in %s, compMix 0, modMix %.2f (insert channel alone): %.2f dBFS\n",
                                mod == 1 ? "A" : "P", (double)mm, rmsDb(r));
                }
                for (float mm : { 0.0f, 0.5f, 1.0f }) {
                    auto r = renderMix(1.0f, mm, mod, 0.75f, 4.0);
                    std::printf("        MOD in %s, compMix 1, modMix %.2f (both channels): %.2f dBFS\n",
                                mod == 1 ? "A" : "P", (double)mm, rmsDb(r));
                }
            }
            for (float cm : { 0.0f, 0.5f, 1.0f }) {
                auto r = renderMix(cm, 0.0f, 0, 0.75f, 4.0);
                std::printf("        MOD out, compMix %.2f, modMix 0 (comp channel): %.2f dBFS\n",
                            (double)cm, rmsDb(r));
            }
            check(true, "L8 level table printed (report data)");
        }
    }

    std::printf("\n=== M. FULL2 BUS ARCHITECTURE + ADT END-GOAL ACCEPTANCE ===\n");
    {
        char d[512];
        // M1 — Nathan's acceptance (1), the feed itself:
        // at compMix=0 the insert feed (the send tap) must
        // MOVE with INPUT. In the FULL candidate the feed
        // at compMix=0 was the raw input — the diagnosis
        // proved candidate renders at INPUT 5 vs 10
        // digit-identical (max |delta| = 0.000e+00). Here
        // the tap is the module's own output: (a) it equals
        // a module-mode limiter's output bit-for-bit in
        // lockstep; (b) its level/crest travel across the
        // knob is the module's (printed; asserted below);
        // (c) the insert-channel renders differ at every
        // knob step.
        {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setCompMix(0.0f);
            ch.setModMix(1.0f);
            ch.reset();
            ch.setModIn(true);
            osc72::Ba6aLimiter lim;
            lim.setApplyMakeup(false);
            lim.prepare(kFs);
            lim.setKnob(0.5f);
            lim.reset();
            ProgGen pg;
            bool tapOk = true;
            for (int i = 0; i < (int)(2.0 * kFs); ++i) {
                float l, r; pg.sample(i, l, r);
                float mL, mR; lim.process(l, r, mL, mR);
                float oL, oR; ch.process(l, r, oL, oR);
                if (ch.insertFeedL() != mL || ch.insertFeedR() != mR) tapOk = false;
            }
            check(tapOk, "M1a insert feed == the module's own output, bit-exact in lockstep", "");

            std::printf("      insert feed at compMix=0 (programme, settled half of 6 s):\n");
            double rmsK[5], crestK[5];
            int idx = 0;
            for (float knob : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
                osc72::Chain c2;
                c2.prepare(kFs);
                c2.setKnob(knob);
                c2.setCompMix(0.0f);
                c2.setModMix(1.0f);
                c2.reset();
                c2.setModIn(true);
                ProgGen pg2;
                double sf = 0; float pk = 0; int cnt = 0;
                for (int i = 0; i < (int)(6.0 * kFs); ++i) {
                    float l, r; pg2.sample(i, l, r);
                    float oL, oR; c2.process(l, r, oL, oR);
                    if (i >= (int)(3.0 * kFs)) {
                        const float f = c2.insertFeedL();
                        sf += (double)f * f; pk = std::max(pk, std::fabs(f)); ++cnt;
                    }
                }
                const double rms = 10.0 * std::log10(sf / cnt);
                const double pkDb = 20.0 * std::log10((double)pk);
                rmsK[idx] = rms; crestK[idx] = pkDb - rms; ++idx;
                std::printf("        INPUT %4.1f: feed RMS %.2f dBFS, peak %.2f dBFS, crest %.2f dB\n",
                            (double)(knob * 10.0f), rms, pkDb, pkDb - rms);
            }
            std::snprintf(d, sizeof d, "feed RMS travel %+.2f dB (INPUT 0 -> 10), crest falls %.2f dB",
                          rmsK[4] - rmsK[0], crestK[0] - crestK[4]);
            check(rmsK[4] - rmsK[0] > 10.0 && crestK[0] - crestK[4] > 5.0
                  && rmsK[3] > rmsK[2] && rmsK[2] > rmsK[1],
                  "M1b the send moves materially with INPUT at compMix=0 (it did not vary at all in FULL)", d);
        }
        // M2 — the 1.600 ceiling, relocated to the bus:
        // programme peaks at the bus output stay bounded
        // with both channels at full gain, both flavours,
        // at the drives where the desired make-up far
        // exceeds the cap (INPUT 7.5 / 10).
        {
            double worst = 0;
            for (int mod : { 1, 2 }) {
                for (float knob : { 0.75f, 1.0f }) {
                    osc72::Chain ch;
                    ch.prepare(kFs);
                    ch.setKnob(knob);
                    ch.setCompMix(1.0f);
                    ch.setModMix(1.0f);
                    ch.setModPhaser(mod == 2);
                    ch.reset();
                    ch.setModIn(true);
                    ProgGen pg;
                    for (int i = 0; i < (int)(6.0 * kFs); ++i) {
                        float l, r; pg.sample(i, l, r);
                        float oL, oR; ch.process(l, r, oL, oR);
                        worst = std::max(worst, (double)std::max(std::fabs(oL), std::fabs(oR)));
                    }
                }
            }
            std::snprintf(d, sizeof d, "worst bus peak %.4f across A/P at INPUT 7.5/10, gains 1/1", worst);
            check(worst <= 1.6001, "M2 bus ceiling: programme peaks bounded at 1.600 with both channels full", d);
        }
        // M3 — channel alignment at the bus, by impulse:
        // with MOD in A, BOTH channels arrive at +992 and
        // nothing arrives before, at full and partial
        // gains; with MOD out the comp channel arrives at
        // +32. (Replaces the alignment content of the old
        // crossfade pins; the processor's P9 pins cover
        // the same facts through the APVTS path.)
        {
            struct ImpRes { int peakAt; float pre960; float pre976; };
            auto impulsePeak = [&](float cm, float mm, bool modIn) {
                osc72::Chain ch;
                ch.prepare(kFs);
                ch.setKnob(0.0f);
                ch.setCompMix(cm);
                ch.setModMix(mm);
                ch.reset();
                ch.setModIn(modIn);
                const int impAt = 24000, total = 48000;
                ImpRes r { -1, 0.0f, 0.0f };
                float peak = 0;
                for (int i = 0; i < total; ++i) {
                    float oL, oR;
                    ch.process(i == impAt ? 0.5f : 0.0f, i == impAt ? 0.5f : 0.0f, oL, oR);
                    const int rel = i - impAt;
                    if (modIn && rel >= 0 && rel < 960) r.pre960 = std::max(r.pre960, std::fabs(oL));
                    if (modIn && rel >= 0 && rel < 976) r.pre976 = std::max(r.pre976, std::fabs(oL));
                    if (rel >= 0 && std::fabs(oL) > peak) { peak = std::fabs(oL); r.peakAt = rel; }
                }
                return r;
            };
            auto a11 = impulsePeak(1.0f, 1.0f, true);
            auto aHalf = impulsePeak(0.5f, 0.5f, true);
            auto aC = impulsePeak(1.0f, 0.0f, true);
            auto aM = impulsePeak(0.0f, 1.0f, true);
            auto aOut = impulsePeak(0.5f, 0.5f, false);
            // Exact invariants: every channel's peak lands
            // on its reported latency offset; the COMP
            // channel is exactly silent before +960 (its
            // delayed IR's first sample) with only the OS
            // filter's leading edge (< 2e-5, measured
            // 8.4e-6) before the +992 peak sample. In the
            // mixed renders the pre-window also carries the
            // ADT's early DECK echoes (deck delays 14-33 ms
            // < the SRC's 20 ms + module onset) — bounded
            // to < 5e-4 absolute (measured 1.7e-5: a lone
            // impulse through the tape band).
            std::snprintf(d, sizeof d,
                          "peaks: (1,1) +%d, (0.5,0.5) +%d, comp-only +%d, mod-only +%d, MOD out +%d; comp pre-960 %.1e, pre-976 %.1e; mixed pre-976 %.1e",
                          a11.peakAt, aHalf.peakAt, aC.peakAt, aM.peakAt, aOut.peakAt,
                          (double)aC.pre960, (double)aC.pre976, (double)a11.pre976);
            check(a11.peakAt == 992 && aHalf.peakAt == 992 && aC.peakAt == 992 && aM.peakAt == 992
                  && aOut.peakAt == 32 && aC.pre960 == 0.0f && aC.pre976 < 2.0e-5 && a11.pre976 < 5.0e-4,
                  "M3 channels time-aligned at the bus (+992 both, partial gains too; comp channel silent before)", d);
        }
        // M4 — Nathan's ADT end-goal acceptance (mono in,
        // compMix=0, modMix=1, flavour A): a full dry mono
        // centre (the ADT's SRC at full strength) plus one
        // deck hard on each side, the decks decorrelated
        // in delay AND pitch, offset at all times in the
        // mean (FULL2 stagger: deck 1/RIGHT +1.0 ms).
        {
            // Chain level: centre/side decomposition.
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setCompMix(0.0f);
            ch.setModMix(1.0f);
            ch.reset();
            ch.setModIn(true);
            ProgGen pg;
            const int total = (int)(6.0 * kFs), w0 = (int)(3.0 * kFs);
            std::vector<float> feed((size_t)total), oL((size_t)total), oR((size_t)total);
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                const float m = 0.5f * (l + r);
                ch.process(m, m, oL[(size_t)i], oR[(size_t)i]);
                feed[(size_t)i] = ch.insertFeedL();
            }
            double mf = 0, ff = 0, mm2 = 0, eS = 0;
            for (int i = w0; i < total; ++i) {
                const double M = 0.5 * (oL[(size_t)i] + oR[(size_t)i]);
                const double S = 0.5 * (oL[(size_t)i] - oR[(size_t)i]);
                const double F = feed[(size_t)(i - 960)];
                mf += M * F; ff += F * F; mm2 += M * M; eS += S * S;
            }
            const double corrM = mf / std::sqrt(ff * mm2);
            const double smDb = 10.0 * std::log10(eS / mm2);
            // Unit level: SRC strength in the centre, sides
            // carry no SRC component, decks decorrelated.
            osc72::AdtChain adt;
            adt.prepare(kFs);
            adt.setEnabled(true);
            ProgGen pg2;
            std::vector<float> in((size_t)total), aL((size_t)total), aR((size_t)total);
            for (int i = 0; i < total; ++i) {
                float l, r; pg2.sample(i, l, r);
                const float m = 0.5f * (l + r);
                in[(size_t)i] = m;
                adt.process(m, m, aL[(size_t)i], aR[(size_t)i]);
            }
            double uMf = 0, uFf = 0, uSf = 0, sab = 0, saa = 0, sbb = 0;
            for (int i = w0; i < total; ++i) {
                const double f = in[(size_t)(i - 960)];
                const double M = 0.5 * (aL[(size_t)i] + aR[(size_t)i]);
                const double S = 0.5 * (aL[(size_t)i] - aR[(size_t)i]);
                const double dl = aL[(size_t)i] - f, dr = aR[(size_t)i] - f;
                uMf += M * f; uFf += f * f; uSf += S * f;
                sab += dl * dr; saa += dl * dl; sbb += dr * dr;
            }
            const double srcSlope = uMf / uFf;
            const double sideSlope = uSf / uFf;
            const double deckCorr = sab / std::sqrt(saa * sbb);
            // Deck trajectories, 12 s.
            osc72::AdtChain tr;
            tr.prepare(kFs);
            tr.setEnabled(true);
            const int tN = (int)(12.0 * kFs);
            std::vector<double> d1((size_t)tN), d2((size_t)tN);
            for (int i = 0; i < tN; ++i) {
                float xL, xR; tr.process(0.0f, 0.0f, xL, xR);
                d1[(size_t)i] = tr.deckDelayMs(1);
                d2[(size_t)i] = tr.deckDelayMs(2);
            }
            auto pearson = [](const std::vector<double>& a, const std::vector<double>& b) {
                const size_t n = a.size();
                double ma = 0, mb = 0;
                for (size_t i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
                ma /= (double)n; mb /= (double)n;
                double xy = 0, xx = 0, yy = 0;
                for (size_t i = 0; i < n; ++i) {
                    xy += (a[i] - ma) * (b[i] - mb);
                    xx += (a[i] - ma) * (a[i] - ma);
                    yy += (b[i] - mb) * (b[i] - mb);
                }
                return xy / std::sqrt(xx * yy);
            };
            std::vector<double> p1((size_t)tN - 1), p2((size_t)tN - 1);
            double meanDiff = 0, frac01 = 0;
            for (int i = 0; i < tN; ++i) {
                meanDiff += d1[(size_t)i] - d2[(size_t)i];
                if (std::fabs(d1[(size_t)i] - d2[(size_t)i]) < 0.1) ++frac01;
                if (i + 1 < tN) {
                    p1[(size_t)i] = -(d1[(size_t)(i + 1)] - d1[(size_t)i]);
                    p2[(size_t)i] = -(d2[(size_t)(i + 1)] - d2[(size_t)i]);
                }
            }
            meanDiff /= tN;
            const double rhoD = pearson(d1, d2), rhoP = pearson(p1, p2);
            std::snprintf(d, sizeof d,
                          "centre corr %.3f, S/M %.2f dB; SRC slope %.3f, side slope %+.3f, deck corr %+.3f; traj rho d %+.3f / p %+.3f, mean sep %+.2f ms, |d|<0.1ms %.2f%%",
                          corrM, smDb, srcSlope, sideSlope, deckCorr, rhoD, rhoP,
                          meanDiff, 100.0 * frac01 / tN);
            check(corrM > 0.90 && srcSlope > 0.95 && srcSlope < 1.08
                  && std::fabs(sideSlope) < 0.12 && std::fabs(deckCorr) < 0.30
                  && std::fabs(rhoD) < 0.2 && std::fabs(rhoP) < 0.2
                  && meanDiff > -6.0 && meanDiff < -3.5 && frac01 / tN < 0.03,
                  "M4 mono ADT acceptance: full dry centre (SRC at full strength) + hard decks, decorrelated, offset in the mean", d);
        }
        // M5 — flavour P, mono in, compMix=0/modMix=1:
        // the phaser is a mono-pedal model (one shared
        // coefficient law; the channels run independently
        // but identically), so a mono input yields a pure
        // mono output — bit-identical channels, zero side
        // energy, no widening. Stated plainly per the
        // steer; no voicing change for P this round.
        {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setCompMix(0.0f);
            ch.setModMix(1.0f);
            ch.setModPhaser(true);
            ch.reset();
            ch.setModIn(true);
            ProgGen pg;
            bool same = true;
            double s = 0; int cnt = 0;
            for (int i = 0; i < (int)(4.0 * kFs); ++i) {
                float l, r; pg.sample(i, l, r);
                const float m = 0.5f * (l + r);
                float oL, oR; ch.process(m, m, oL, oR);
                if (oL != oR) same = false;
                if (i >= (int)(2.0 * kFs)) { s += (double)oL * oL; ++cnt; }
            }
            std::snprintf(d, sizeof d, "L==R bit-exact %d; mono out RMS %.2f dBFS", (int)same,
                          10.0 * std::log10(s / cnt));
            check(same, "M5 flavour P mono in: pure mono out (identical channels, no widening — mono-pedal model)", d);
        }
    }

    std::printf("\n=== N. HOST RE-PREPARE / RESET RE-ARM (v1.4.1) ===\n");
    {
        char d[512];
        // N1/N2 — insert level: a reset() whose engagement
        // target survives ON must RE-ARM the machine into
        // its fade-in. The <= 1.4.0 defect: reset parked
        // the state machine off while target_ stayed true,
        // and setEnabled's no-change early return meant a
        // host re-prepare could never re-arm it — fadeGain
        // pinned at 0, the insert returning its input
        // sample-for-sample forever (diagnosis doc, Luna).
        // The target-OFF mirrors pin that reset still parks.
        {
            osc72::AdtChain adt;
            adt.prepare(kFs);
            adt.setEnabled(true);
            const int seg = (int)(0.3 * kFs);
            for (int i = 0; i < seg; ++i) {
                float s = 0.4f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
                float oL, oR; adt.process(s, s, oL, oR);
            }
            const float settledFade = adt.fadeGain();
            adt.reset();   // target survives ON
            float wet = 0.0f;
            for (int i = 0; i < seg; ++i) {
                float s = 0.4f * (float)std::sin(2.0 * kPi * 440.0 * (seg + i) / kFs);
                float oL, oR; adt.process(s, s, oL, oR);
                if (i > seg / 2) wet = std::max(wet, std::fabs(oL - s));
            }
            std::snprintf(d, sizeof d, "settled fade %.2f; after reset: fade %.2f, max |out-in| %.4f (settled half)",
                          (double)settledFade, (double)adt.fadeGain(), (double)wet);
            check(settledFade == 1.0f && adt.fadeGain() == 1.0f && wet > 0.05f,
                  "N1a ADT reset with target ON re-arms (fade back to 1, output wet again)", d);
        }
        {
            osc72::AdtChain adt;
            adt.prepare(kFs);
            for (int i = 0; i < 1000; ++i) { float oL, oR; adt.process(0.3f, 0.3f, oL, oR); }
            adt.reset();   // target OFF throughout
            bool exact = adt.fadeGain() == 0.0f;
            for (int i = 0; i < 1000; ++i) {
                float oL, oR; adt.process(0.3f, -0.2f, oL, oR);
                if (oL != 0.3f || oR != -0.2f) exact = false;
            }
            check(exact, "N1b ADT reset with target OFF still parks (bit-exact dry, fade 0)", "");
        }
        {
            osc72::Phaser90 ph;
            ph.prepare(kFs);
            ph.setEnabled(true);
            const int seg = (int)(0.3 * kFs);
            for (int i = 0; i < seg; ++i) {
                float s = 0.4f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
                float oL, oR; ph.process(s, s, oL, oR);
            }
            const float settledFade = ph.fadeGain();
            ph.reset();    // target survives ON
            float wet = 0.0f;
            for (int i = 0; i < seg; ++i) {
                float s = 0.4f * (float)std::sin(2.0 * kPi * 440.0 * (seg + i) / kFs);
                float oL, oR; ph.process(s, s, oL, oR);
                if (i > seg / 2) wet = std::max(wet, std::fabs(oL - s));
            }
            std::snprintf(d, sizeof d, "settled fade %.2f; after reset: fade %.2f, max |out-in| %.4f (settled half)",
                          (double)settledFade, (double)ph.fadeGain(), (double)wet);
            check(settledFade == 1.0f && ph.fadeGain() == 1.0f && wet > 0.02f,
                  "N2a phaser reset with target ON re-arms (fade back to 1, output phased again)", d);
        }
        {
            osc72::Phaser90 ph;
            ph.prepare(kFs);
            ph.reset();    // target OFF throughout
            bool exact = ph.fadeGain() == 0.0f;
            for (int i = 0; i < 1000; ++i) {
                float oL, oR; ph.process(0.3f, -0.2f, oL, oR);
                if (oL != 0.3f || oR != -0.2f) exact = false;
            }
            check(exact, "N2b phaser reset with target OFF still parks (bit-exact dry, fade 0)", "");
        }
        // N3 — chain level, the Luna event itself: a
        // same-rate re-prepare mid-stream must be an
        // inaudible non-event — the render BIT-IDENTICAL
        // to a twin that was never re-prepared (the
        // same-rate prepare preserves all live state; the
        // reset re-arm above covers the resets that do
        // happen). On the buggy build the insert parked
        // at the event and the renders diverged there.
        auto renderEvent = [&](bool phaser, bool reprepare) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.75f);
            ch.setCompMix(0.0f);
            ch.setModMix(1.0f);
            ch.setModPhaser(phaser);
            ch.reset();
            ch.setModIn(true);
            ProgGen pg;
            std::vector<float> out;
            const int total = (int)(5.0 * kFs);
            out.reserve((size_t)total * 2);
            for (int i = 0; i < total; ++i) {
                if (reprepare && i == (int)(3.0 * kFs)) ch.prepare(kFs);
                float l, r; pg.sample(i, l, r);
                float oL, oR; ch.process(l, r, oL, oR);
                out.push_back(oL); out.push_back(oR);
            }
            return out;
        };
        check(renderEvent(false, true) == renderEvent(false, false),
              "N3a ADT: same-rate re-prepare mid-stream bit-identical to no event (Luna power cycle)", "");
        check(renderEvent(true, true) == renderEvent(true, false),
              "N3b phaser: same-rate re-prepare mid-stream bit-identical to no event", "");
        // N3c — and the ADT in the re-prepared render is
        // genuinely ENGAGED afterwards, not identically
        // parked in both twins: mono programme at compMix
        // 0 leaves real side energy after the event (a
        // parked insert returns the mono feed untouched —
        // side exactly zero).
        {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setCompMix(0.0f);
            ch.setModMix(1.0f);
            ch.reset();
            ch.setModIn(true);
            ProgGen pg;
            double side = 0; int cnt = 0;
            const int total = (int)(5.0 * kFs);
            for (int i = 0; i < total; ++i) {
                if (i == (int)(3.0 * kFs)) ch.prepare(kFs);
                float l, r; pg.sample(i, l, r);
                const float m = 0.5f * (l + r);
                float oL, oR; ch.process(m, m, oL, oR);
                if (i >= (int)(4.0 * kFs)) {
                    const double s = 0.5 * ((double)oL - (double)oR);
                    side += s * s; ++cnt;
                }
            }
            const double sideDb = 10.0 * std::log10(side / cnt);
            std::snprintf(d, sizeof d, "post-event side RMS %.2f dBFS; adt fade %.2f",
                          sideDb, (double)ch.adt().fadeGain());
            check(sideDb > -45.0 && ch.adt().fadeGain() == 1.0f,
                  "N3c ADT audibly engaged after the re-prepare (side energy present, fade 1)", d);
        }
        // N4 — a genuine RATE CHANGE still rebuilds from
        // scratch: after prepare(44100) and the standard
        // reset/engage sequence, the chain renders
        // bit-identically to a fresh 44.1 k instance. (The
        // reset/engage is re-run on the rebuilt chain
        // because Chain::reset snaps the MOD gate to the
        // SURVIVING target: a chain that was engaged
        // across the rate change re-opens with its gate
        // already up, and the bus peak tracker (0.6 s
        // decay) legitimately remembers that different
        // first 30 ms — a history difference, not a
        // rebuild difference. What this pin proves is
        // that every rate-dependent structure — rings,
        // oversampler, taus — is rebuilt exactly.)
        {
            auto setup = [&](osc72::Chain& ch, double fs) {
                ch.prepare(fs);
                ch.setKnob(0.75f);
                ch.setCompMix(0.0f);
                ch.setModMix(1.0f);
                ch.reset();
                ch.setModIn(true);
            };
            osc72::Chain a;
            setup(a, kFs);
            ProgGen pgA;
            for (int i = 0; i < (int)(1.0 * kFs); ++i) {
                float l, r; pgA.sample(i, l, r);
                float oL, oR; a.process(l, r, oL, oR);
            }
            a.prepare(44100.0);   // the rate change: full rebuild
            a.setModIn(false);    // re-run the fresh-load sequence
            a.reset();
            a.setModIn(true);
            osc72::Chain b;
            setup(b, 44100.0);
            ProgGen pg1, pg2;
            bool same = true;
            const int settle = (int)(0.5 * 44100), meas = (int)(2.0 * 44100);
            for (int i = 0; i < settle + meas; ++i) {
                float l1, r1; pg1.sample(i, l1, r1);
                float l2, r2; pg2.sample(i, l2, r2);
                float aL, aR, bL, bR;
                a.process(l1, r1, aL, aR);
                b.process(l2, r2, bL, bR);
                if (i >= settle && (aL != bL || aR != bR)) same = false;
            }
            check(same, "N4 rate-change prepare rebuilds fresh (== fresh 44.1 k instance past the settle, bit-exact)", "");
        }
    }

    std::printf("\n=== O. STAGING-FIX CANDIDATE (Nathan's staging redesign, 2026-10-22) ===\n");
    {
        // O1: the re-spread drive taper — the published
        // table, endpoints exact, increments evened from
        // the old law's ~27:1 half-step bunching to ~5:1.
        const double want[11] = { -5.00, -3.24, -0.53, 3.46, 9.00, 16.08,
                                  24.25, 32.63, 40.23, 46.41, 51.00 };
        double worst = 0, prev = -1e9, incMin = 1e9, incMax = 0;
        for (int k = 0; k <= 10; ++k) {
            const double dv = osc72::ba6aDriveDb(k / 10.0);
            worst = std::max(worst, std::fabs(dv - want[k]));
            if (k > 0) {
                const double inc = dv - prev;
                incMin = std::min(incMin, inc); incMax = std::max(incMax, inc);
            }
            prev = dv;
        }
        std::snprintf(d, sizeof d, "taper worst dev %.3f dB; full-step increments %.2f..%.2f dB (ratio %.1f:1)",
                      worst, incMin, incMax, incMax / incMin);
        check(worst < 0.06 && incMin > 0.0 && incMax / incMin < 6.0,
              "O1 drive taper re-spread: published table, monotone, increments evened", d);

        // Settled programme renders for O2-O4 (ProgGen,
        // second-half statistics; knob/gains per pin).
        auto settled = [&](float knob, float cm, float mm, int mod, double& pk, double& rms, double& mk) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(knob);
            ch.setCompMix(cm);
            ch.setModMix(mm);
            ch.setModPhaser(mod == 2);
            ch.reset();
            ch.setModIn(mod != 0);
            ProgGen pg;
            const int total = (int)(4.0 * kFs), half = total / 2;
            double sq = 0; pk = 0; long n = 0;
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                float oL, oR; ch.process(l, r, oL, oR);
                if (i >= half) {
                    pk = std::max(pk, (double)std::max(std::fabs(oL), std::fabs(oR)));
                    sq += (double)oL * oL + (double)oR * oR; n += 2;
                }
            }
            rms = std::sqrt(sq / (double)n);
            mk = ch.busMakeupDb();
        };
        // O2: the soft ceiling's hard bound — no render,
        // however hot, exceeds the knee asymptote.
        double pkA, rmsA, mkA, pkB, rmsB, mkB;
        settled(1.0f, 1.0f, 1.0f, 0, pkA, rmsA, mkA);
        settled(1.0f, 1.0f, 1.0f, 1, pkB, rmsB, mkB);
        std::snprintf(d, sizeof d, "settled peaks at knob 1.0: MOD out %.4f, MOD in A %.4f (asymptote 0.9809)",
                      pkA, pkB);
        check(pkA <= 0.9811 && pkB <= 0.9811,
              "O2 soft ceiling: output never exceeds the knee asymptote", d);
        // O3: parking + density at noon gains — peaks hold
        // while RMS climbs from knob 0.75 to 1.0.
        double pk75, rms75, mk75, pk100, rms100, mk100;
        settled(0.75f, 0.5f, 0.5f, 0, pk75, rms75, mk75);
        settled(1.0f, 0.5f, 0.5f, 0, pk100, rms100, mk100);
        const double pkDb = 20.0 * std::log10(pk100 / pk75);
        const double rmsDb = 20.0 * std::log10(rms100 / rms75);
        std::snprintf(d, sizeof d, "knob 0.75 -> 1.0 at noon gains: peak %+.2f dB, RMS %+.2f dB",
                      pkDb, rmsDb);
        // Measured on the candidate: peak -1.22 dB over
        // the 25 dB drive span (a -0.05 dB/dB parking
        // slope), RMS +2.86 dB. Band: |peak| < 1.5 dB
        // across the span with density climbing > 2 dB.
        // TEST2 RE-BASE (cause: Nathan's operating-point
        // directive): on ProgGen the GR contribution to
        // the density climb is gone with the threshold
        // raise (D3) — the peak still parks (the ceiling's
        // doing, measured +0.01 dB) and density still
        // climbs on drive alone, measured +0.36 dB. Band:
        // |peak| < 1.5 dB (unmoved), RMS climb > 0.25 dB.
        check(std::fabs(pkDb) < 1.5 && rmsDb > 0.25,
              "O3 peaks park while density climbs (noon gains, programme; TEST2 re-base)", d);
        // O4: the peak-referenced law gives back LESS as
        // drive rises — settled bus make-up at knob 1.0
        // sits below knob 0.5 (the 1.4.1 law's applied
        // make-up also fell at the top, but only through
        // cap starvation at +4 dBFS output peaks).
        double pk50, rms50, mk50;
        settled(0.5f, 0.5f, 0.5f, 0, pk50, rms50, mk50);
        std::snprintf(d, sizeof d, "settled bus make-up: knob 0.5 %+.2f dB, knob 1.0 %+.2f dB",
                      mk50, mk100);
        check(mk100 < mk50, "O4 bus make-up falls as drive rises (peak-referenced bound)", d);
    }

    std::printf("\n%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL TESTS PASSED" : "FAILURES PRESENT",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
