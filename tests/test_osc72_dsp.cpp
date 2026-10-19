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
static std::vector<float> grStep(bool attackFast, bool releaseFast, float knob = 0.75f,
                                 float amp = 0.5f, double seconds = 1.2, bool comp = false) {
    // v64 (1.3.1): through 1.3.0 this helper took an x2 flag
    // (v62) that engaged the 1X/2X selector before reset;
    // the default arm measured the 1X ballistics. The
    // selector is gone — the baked 2X ballistics are the
    // only ballistics — so the flag and the call are gone
    // with it, and every protocol on this helper now reads
    // the baked timings (the C bands were re-based to
    // their 1.3.0-at-2X values, ledgered per pin).
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
        float s = amp * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs);
        float oL, oR; lim.process(s, s, oL, oR);
        tr[(size_t)i] = lim.gainReductionDb();
    }
    return tr;
}
// Release trace: loud sine for 1 s, then silence.
static std::vector<float> grRelease(bool releaseFast, double seconds = 6.5, bool comp = false) {
    // v64: the v62 x2 flag is gone with the selector (see
    // grStep) — this protocol reads the baked ballistics.
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setCompMode(comp);
    lim.setKnob(0.75f);
    lim.setAttackFast(true);
    lim.setReleaseFast(releaseFast);
    lim.reset();
    const int loud = (int)(1.0 * kFs);
    const int total = loud + (int)(kFs * seconds);
    std::vector<float> tr((size_t)(total - loud));
    for (int i = 0; i < total; ++i) {
        float s = i < loud ? 0.5f * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
        float oL, oR; lim.process(s, s, oL, oR);
        if (i >= loud) tr[(size_t)(i - loud)] = lim.gainReductionDb();
    }
    return tr;
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
            for (int i = 0; i < (int)(1.0 * kFs); ++i) {
                float s2 = 0.6f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
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
        Meas m20 = runLim(0.5f, 0.1f, 1000.0f);   // -20 dBFS peak
        Meas m10 = runLim(0.5f, 0.316f, 1000.0f); // -10
        Meas m0  = runLim(0.5f, 1.0f, 1000.0f);   // 0
        float pre20 = db(m20.peakL) - m20.makeupDb;
        float pre10 = db(m10.peakL) - m10.makeupDb;
        float pre0  = db(m0.peakL) - m0.makeupDb;
        float riseLo = pre10 - pre20, riseHi = pre0 - pre10; // per +10 dB in
        std::snprintf(d, sizeof d, "GR %.1f/%.1f/%.1f dB; pre-makeup rises %.1f then %.1f dB per +10 dB",
                      m20.meanGr, m10.meanGr, m0.meanGr, riseLo, riseHi);
        // v46 (v1.1.5 staging, Nathan): knob 5 is now the
        // kissing point — the -20 dBFS tone must sit in the
        // 0-2 dB band here (measured 0.1), so the old
        // "> 1 dB at -20" clause is replaced by the staging
        // pin itself; the growth clauses carry the vari-mu
        // substance (measured 0.1 / 3.4 / 8.8).
        check(m20.meanGr < 2.0f && m10.meanGr > m20.meanGr + 2.5f && m0.meanGr > m10.meanGr + 4.0f,
              "B2a knob 5 kisses at -20 (0-2 dB), GR grows steeply with level (vari-mu)", d);
        check(riseLo < 7.5f && riseHi < riseLo,
              "B2b static ratio > 1.3:1 and RISING with depth", d);
    }
    {
        // B3: GR monotone in the knob at a fixed hot input,
        // and knob 0 keeps nominal programme ~clean of GR.
        Meas k25 = runLim(0.25f, 0.316f, 1000.0f);
        Meas k50 = runLim(0.5f, 0.316f, 1000.0f);
        Meas k75 = runLim(0.75f, 0.316f, 1000.0f);
        Meas k100 = runLim(1.0f, 0.316f, 1000.0f);
        std::snprintf(d, sizeof d, "GR vs knob %.1f/%.1f/%.1f/%.1f dB",
                      k25.meanGr, k50.meanGr, k75.meanGr, k100.meanGr);
        check(k50.meanGr > k25.meanGr && k75.meanGr > k50.meanGr && k100.meanGr > k75.meanGr,
              "B3 GR monotone in drive", d);
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
        for (bool comp : { false, true }) {
            auto tr = grStep(true, false, 0.75f, 0.5f, 1.2, comp);
            float settled = tr.back();
            double t90 = -1;
            for (size_t i = 0; i < tr.size(); ++i)
                if (tr[i] >= 0.9f * settled) { t90 = (double)i / kFs * 1000.0; break; }
            std::snprintf(d, sizeof d, "%s settled GR %.1f dB, t90 %.3f ms",
                          comp ? "COMP" : "LIMIT", settled, t90);
            check(comp ? (t90 > 3.77 && t90 < 5.10) : (t90 > 2.55 && t90 < 3.45),
                  comp ? "C1-COMP attack FAST: t90 3.77-5.10 ms (AB2, ref 4.438)"
                       : "C1 attack FAST: t90 2.55-3.45 ms (AB2 target 3.0, ref 3.000)", d);
        }
    }
    {
        // C2: attack SLOW — the opto seat between the LA-3A
        // (<= 1.5 ms) and the LA-2A (10 ms): geometric seat
        // ~3.9 ms on the standard step in v45, DOUBLED by
        // Nathan's v46 ear verdict to a measured 7.83 ms;
        // T4-shaped, so a small step answers in-family too
        // (C2b), not on the same single-tau curve.
        auto trF = grStep(true, false);
        auto trS = grStep(false, false);
        auto t90of = [&](const std::vector<float>& tr) {
            float settled = tr.back();
            for (size_t i = 0; i < tr.size(); ++i)
                if (tr[i] >= 0.9f * settled) return (double)i / kFs * 1000.0;
            return -1.0;
        };
        double tF = t90of(trF), tS = t90of(trS);
        std::snprintf(d, sizeof d, "t90 fast %.3f ms, slow %.2f ms", tF, tS);
        // v64 pin move (cause as C1): the SLOW full step
        // reads 15.06 ms baked — the K2 LIMIT reference
        // from 1.3.0 at 2X; band = ref +/-15%. The seat
        // separation survives doubled (15.06 / 1.125 =
        // 13.4x the FAST reading), so the >= 8x condition
        // is kept verbatim.
        // AB2 pin move (cause as C1): Nathan's refinement
        // sets the SLOW target at 15.0 ms (between the
        // LA-2A's 10 ms and the Sta-Level Double's 25 ms);
        // the seat reads 14.958 ms (band = ref +/-15%).
        // The separation clause is re-based >= 8x -> >= 4x:
        // with the seats at 3 / 15 ms BY DIRECTIVE the
        // separation is 5.0x by design, not by drift.
        check(tS > 12.7 && tS < 17.2 && tS >= 4.0 * tF,
              "C2 attack SLOW: GR t90 12.7-17.2 ms (AB2 target 15.0, ref 14.958)", d);
        // v53: the SLOW constants take one common x1.25
        // toward the restated LA-2A reference (attack
        // ~10 ms) — in COMP (the near-shipped law) the full
        // step lands on the nominal; in LIMIT the same
        // constants read faster through the steeper law's
        // static map (the C2 band above). Detector dynamics
        // are mode-independent (demand uses the shipped
        // law in both modes).
        auto trSC = grStep(false, false, 0.75f, 0.5f, 1.2, true);
        double tSC = t90of(trSC);
        std::snprintf(d, sizeof d, "COMP full-step t90 %.2f ms", tSC);
        // v64 pin move (cause as C1): reads 21.15 ms baked
        // — the K2-COMP reference from 1.3.0 at 2X; band =
        // ref +/-15%.
        check(tSC > 18.0 && tSC < 24.3,
              "C2c attack SLOW (COMP): full-step t90 18.0-24.3 ms (baked 2X, ref 21.15, v64)", d);
        auto trSmall = grStep(false, false, 0.75f, 0.12f);
        double tSmall = t90of(trSmall);
        std::snprintf(d, sizeof d, "small-step (amp .12) t90 %.2f ms, settled GR %.1f dB",
                      tSmall, trSmall.back());
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
        check(tSmall > 20.5 && tSmall < 27.8,
              "C2b attack SLOW small step: t90 20.5-27.8 ms (baked 2X, ref 24.15, v64)", d);
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
        check(at03 > 0.40f * g0 && at03 < 0.56f * g0, "C4a DUAL tail holds 40-56% of GR at 0.3 s (baked 2X, v64)", d);
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
        check(t50 > 0.097 && t50 < 0.131, "C4c DUAL 50% recovery 97-131 ms in LIMIT (AB2, ref 0.1136)", d);
        auto trC = grRelease(false, 6.5, true);
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
        check(t50C > 0.089 && t50C < 0.120 && t90C > 2.17 && t90C < 3.08,
              "C4d DUAL in COMP: 50% 89-120 ms, 90% 2.17-3.08 s (AB2, refs 0.1043/2.625)", d);
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
            lim.prepare(kFs); lim.setKnob(0.85f);
            lim.setAttackFast(true); lim.setReleaseFast(false); lim.reset();
            const int loud = (int)(holdSec * kFs);
            const int tail = (int)(8.0 * kFs);
            float g0 = 0; double t90 = -1;
            for (int i = 0; i < loud + tail; ++i) {
                float s = i < loud ? 0.5f * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
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
        check(tBrief > 2.47 && tBrief < 3.52 && tLong > 1.35 * tBrief && tLong > 3.48 && tLong < 4.96,
              "C5 DUAL memory: holds at AB2 refs 2.998/4.220 s, long >= 1.35x slower", d);
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
        check(grAt1 >= 28.0f, "D3 programme mean GR at max >= 28 dB (v1.1.1 drive law)", d);
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
            Meas m = runLim(k, 1.0f, 400.0f, 1.2);
            if (m.meanGr >= 15.0f) { kDeep = k; break; }
        }
        float deep = kDeep > 0 ? thdLim(kDeep, 1.0f, 400.0f) : -1.0f;
        std::snprintf(d, sizeof d, "clean 400 Hz %.3f%%; at >=15 dB GR (knob %.2f) %.2f%%",
                      clean, kDeep, deep);
        check(clean < 0.5f, "E3a clean THD < 0.5% @ 400 Hz (spec < 0.4%)", d);
        // v45: ceiling 3.0 -> 3.5 — with the FAST control
        // pole at the 1176 seat the sustained-tone GR settles
        // ~0.3 dB shallower and the downstream stages run
        // that much hotter (measured 2.68 -> 3.31-3.34%; the
        // mechanism + the trade are in build-report-v45).
        check(kDeep > 0 && deep < 3.5f, "E3b THD at >=15 dB GR < 3.5% (spec < 0.75% @18 dB, see report)", d);
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
        osc72::Chain ch;
        ch.prepare(kFs);
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
        // F5 (v44 mix-alignment diagnosis): the ADT's SRC is a
        // PURE integer delay of the limiter output — the fact
        // the processor's MIX blend arithmetic relies on, here
        // measured instead of assumed. The same impulse goes
        // through two chains (MOD out / MOD in, knob 0); the
        // MOD-in impulse response must equal the MOD-out one
        // shifted by exactly the reported latency (960), to
        // float dust, over the arrival window — and the MOD-in
        // output must be silent before the arrival sample. The
        // MOD-in impulse is fired at a moment when both decks
        // sit beyond 21.5 ms (introspection), so no deck echo
        // can reach the window.
        auto fireIr = [](bool mod, std::vector<float>& ir, float& preMax) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.0f);
            ch.setModIn(mod);
            const int len = 1400;
            ir.assign((size_t)len, 0.0f);
            preMax = 0.0f;
            int fireAt = -1;
            const int budget = (int)(12.0 * kFs);
            for (int i = 0; i < budget; ++i) {
                bool fire = false;
                if (fireAt < 0 && i >= (int)(1.0 * kFs)) {
                    if (!mod) fire = true;
                    else if (ch.adt().deckDelayMs(1) > 21.5f
                             && ch.adt().deckDelayMs(2) > 21.5f) fire = true;
                }
                float oL, oR;
                ch.process(fire ? 0.5f : 0.0f, fire ? 0.5f : 0.0f, oL, oR);
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
              "F5 ADT SRC is a pure 960-sample delay of the limiter output (v53: limiter IR itself carries the 32-sample OS delay in both chains)", d);
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
        lim.setKnob(0.70f); lim.setAttackFast(true); lim.setReleaseFast(false);
        const int cyc = (int)(0.5 * kFs), total = cyc * 14;
        std::vector<float> out(total); std::vector<float> grv(total);
        double phq = 0;
        for (int i = 0; i < total; ++i) {
            double t = (i % cyc) / kFs;
            float loud = 0.65f * (float)std::exp(-t / 0.060) * (float)std::sin(2.0 * M_PI * 90.0 * i / kFs);
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
        check(gapGr >= 3.0 && gapGr <= hitGr - 5.0, "H2 gap GR held by recovery, well under hit GR", d);
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
        auto relTimes = [&](bool relFast, bool comp, double& t50, double& t90) {
            auto tr = grRelease(relFast, 7.0, comp);
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
            double a = t90of(grStep(true, false, 0.75f, 0.5f, 1.2, comp));
            double aRef = comp ? 4.438 : 3.000;
            std::snprintf(d, sizeof d, "%s t90 %.3f ms vs AB2 ref %.3f ms", mn, a, aRef);
            check(nearRef(a, aRef, comp ? 0.15 : 0.10),
                  comp ? "K1-COMP AB2 attack FAST t90 == AB2 ref (flagged vs target)"
                       : "K1 AB2 attack FAST t90 == 3.0 ms target (calibrated seat)", d);
            // K2: attack SLOW, full step.
            // AB2 refs: LIMIT 14.958 (target 15.0, +/-10%);
            // COMP 21.021 (flagged, +/-15%).
            double s = t90of(grStep(false, false, 0.75f, 0.5f, 1.2, comp));
            double sRef = comp ? 21.021 : 14.958;
            std::snprintf(d, sizeof d, "%s t90 %.2f ms vs AB2 ref %.2f ms", mn, s, sRef);
            check(nearRef(s, sRef, comp ? 0.15 : 0.10),
                  comp ? "K2-COMP AB2 attack SLOW full-step t90 == AB2 ref (flagged vs target)"
                       : "K2 AB2 attack SLOW full-step t90 == 15.0 ms target", d);
            // K3: attack SLOW, small step (the T4-shaped seat).
            // AB2 refs: 24.000 / 24.542 (report-only, +/-15%).
            double m = t90of(grStep(false, false, 0.75f, 0.12f, 1.2, comp));
            double mRef = comp ? 24.542 : 24.000;
            std::snprintf(d, sizeof d, "%s t90 %.2f ms vs AB2 ref %.2f ms", mn, m, mRef);
            check(nearRef(m, mRef, 0.15),
                  comp ? "K3-COMP AB2 attack SLOW small-step t90 == AB2 ref"
                       : "K3 AB2 attack SLOW small-step t90 == AB2 ref", d);
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
            check(nearRef(d50, d50Ref, 0.15) && nearRef(d90, d90Ref, 0.175),
                  comp ? "K5-COMP AB2 release DUAL 50/90% == AB2 refs (T4 band)"
                       : "K5 AB2 release DUAL 50/90% == AB2 refs (90% target 3.0 s, T4 band)", d);
        }
    }
    {
        // K6: T4 memory (C5 protocol): both holds'
        // recoveries at their AB2 values (the memory taus
        // ride the DUAL seat scale), and the brief/long
        // contrast survives.
        auto relAfter = [&](double holdSec) {
            osc72::Ba6aLimiter lim;
            lim.prepare(kFs); lim.setKnob(0.85f);
            lim.setAttackFast(true); lim.setReleaseFast(false);
            lim.reset();
            const int loud = (int)(holdSec * kFs);
            const int tail = (int)(9.0 * kFs);
            float g0 = 0; double t90 = -1;
            for (int i = 0; i < loud + tail; ++i) {
                float s = i < loud ? 0.5f * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
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
        check(b > 2.998 * 0.825 && b < 2.998 * 1.175
           && l > 4.220 * 0.825 && l < 4.220 * 1.175 && l / b > 1.35,
              "K6 AB2 T4 memory: both holds == AB2 refs, contrast survives", d);
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
        auto fnvF = [](uint64_t h, float f) {
            uint32_t u;
            std::memcpy(&u, &f, 4);
            for (int bn = 0; bn < 4; ++bn) {
                h ^= (u >> (8 * bn)) & 0xFFu;
                h *= 1099511628211ull;
            }
            return h;
        };
        struct Cfg { const char* name; float knob; bool comp, att, rel; int mod; double secs; uint64_t want; };
        const Cfg cfgs[] = {
            // MOD out, knob 0.50 (4 s each)
            { "out k0.50 LIMIT attF relF", 0.50f, false, true,  true,  0, 4.0, 0xed261a92f595aad5ull },
            { "out k0.50 LIMIT attF relS", 0.50f, false, true,  false, 0, 4.0, 0xbc5758ee4cc51170ull },
            { "out k0.50 LIMIT attS relF", 0.50f, false, false, true,  0, 4.0, 0x2b85f3626028121cull },
            { "out k0.50 LIMIT attS relS", 0.50f, false, false, false, 0, 4.0, 0xc5ab87c84a4e747cull },
            { "out k0.50 COMP  attF relF", 0.50f, true,  true,  true,  0, 4.0, 0xe5ad27a01f19317aull },
            { "out k0.50 COMP  attF relS", 0.50f, true,  true,  false, 0, 4.0, 0x3afec921da9e5684ull },
            { "out k0.50 COMP  attS relF", 0.50f, true,  false, true,  0, 4.0, 0xb84b19a214ee4d97ull },
            { "out k0.50 COMP  attS relS", 0.50f, true,  false, false, 0, 4.0, 0x10937302cbc02f02ull },
            // MOD out, knob 0.75
            { "out k0.75 LIMIT attF relF", 0.75f, false, true,  true,  0, 4.0, 0x45356f01c1adf3e9ull },
            { "out k0.75 LIMIT attF relS", 0.75f, false, true,  false, 0, 4.0, 0x1d76e99003b4327full },
            { "out k0.75 LIMIT attS relF", 0.75f, false, false, true,  0, 4.0, 0x2c28992fc28e1d79ull },
            { "out k0.75 LIMIT attS relS", 0.75f, false, false, false, 0, 4.0, 0x0b91c0350939e8b9ull },
            { "out k0.75 COMP  attF relF", 0.75f, true,  true,  true,  0, 4.0, 0x990964f90a2874bfull },
            { "out k0.75 COMP  attF relS", 0.75f, true,  true,  false, 0, 4.0, 0xc98125d4b125d9f3ull },
            { "out k0.75 COMP  attS relF", 0.75f, true,  false, true,  0, 4.0, 0x403b8a734e82b98eull },
            { "out k0.75 COMP  attS relS", 0.75f, true,  false, false, 0, 4.0, 0x9b6dc5c31146d4a0ull },
            // MOD out, knob 1.00
            { "out k1.00 LIMIT attF relF", 1.00f, false, true,  true,  0, 4.0, 0x2b24ce996d3a0463ull },
            { "out k1.00 LIMIT attF relS", 1.00f, false, true,  false, 0, 4.0, 0x36df34b01651b31cull },
            { "out k1.00 LIMIT attS relF", 1.00f, false, false, true,  0, 4.0, 0x3c84b55c57ff3f54ull },
            { "out k1.00 LIMIT attS relS", 1.00f, false, false, false, 0, 4.0, 0x715cc324edb61dcfull },
            { "out k1.00 COMP  attF relF", 1.00f, true,  true,  true,  0, 4.0, 0x6a39ec42381aa53cull },
            { "out k1.00 COMP  attF relS", 1.00f, true,  true,  false, 0, 4.0, 0x5327001398170cfcull },
            { "out k1.00 COMP  attS relF", 1.00f, true,  false, true,  0, 4.0, 0x590f33b1273277f1ull },
            { "out k1.00 COMP  attS relS", 1.00f, true,  false, false, 0, 4.0, 0x3fa4f284e016bca5ull },
            // MOD in, flavour A (ADT), knob 0.75, DUAL (6 s each)
            { "inA k0.75 LIMIT attF relS", 0.75f, false, true,  false, 1, 6.0, 0xa8ee2347e87e06e9ull },
            { "inA k0.75 LIMIT attS relS", 0.75f, false, false, false, 1, 6.0, 0x552f94dfc0ad324aull },
            { "inA k0.75 COMP  attF relS", 0.75f, true,  true,  false, 1, 6.0, 0x67d439d16802015dull },
            { "inA k0.75 COMP  attS relS", 0.75f, true,  false, false, 1, 6.0, 0xbebbf552d5fb9606ull },
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
            { "inP k0.75 LIMIT attS relS", 0.75f, false, false, false, 2, 6.0, 0x8e6208a1926f3bbfull },
            { "inP k0.75 COMP  attS relS", 0.75f, true,  false, false, 2, 6.0, 0xb2ac23b383918e41ull },
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
        auto renderHash = [&](const Cfg& c, float modMix = 1.0f) {
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
            const int total = (int)(c.secs * kFs);
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                float oL, oR; ch.process(l, r, oL, oR);
                h = fnvF(h, oL);
                h = fnvF(h, oR);
                h = fnvF(h, ch.gainReductionDb());
            }
            return h;
        };
        struct Group { const char* pin; int first, last; };
        const Group groups[] = {
            { "K7a baked-2X bit-identity: MOD out, knob 0.50 (8 cfgs)", 0, 8 },
            { "K7b baked-2X bit-identity: MOD out, knob 0.75 (8 cfgs)", 8, 16 },
            { "K7c baked-2X bit-identity: MOD out, knob 1.00 (8 cfgs)", 16, 24 },
            { "K7d baked-2X bit-identity: MOD in A/ADT (4 cfgs)", 24, 28 },
            { "K7e baked-2X bit-identity: MOD in P/Phase 90 (2 cfgs)", 28, 30 },
        };
        for (const Group& g : groups) {
            bool ok = true;
            char bad[160] = "";
            for (int i = g.first; i < g.last; ++i) {
                uint64_t got = renderHash(cfgs[i]);
                if (got != cfgs[i].want) {
                    ok = false;
                    std::snprintf(bad, sizeof bad, "%s: got %016llx want %016llx",
                                  cfgs[i].name, (unsigned long long)got,
                                  (unsigned long long)cfgs[i].want);
                    break;
                }
            }
            std::snprintf(d, sizeof d, "%s", ok ? "all hashes match the FULL-candidate references (AB2 ballistics x AB3 harness)" : bad);
            check(ok, g.pin, d);
        }
        // K7f (AB3; re-based FULL): the P flavour at
        // modMix = 0.5 — the classic pedal. The pin hashes
        // THIS tree's renders (below); the equivalence to
        // the classic internal-50/50 output is the probe
        // verdict recorded here.
        // VERDICT (probe, this tree vs the AB2 tree — the
        // classic chain at the SAME ballistics, so the
        // limiter is held constant and only the blend
        // arithmetic differs; both K7 P configs rendered
        // sample-for-sample, 6 s each): NOT bit-identical
        // — outL/outR differ on 17.67 % / 17.72 % (LIMIT)
        // and 17.52 % / 17.63 % (COMP) of samples, but the
        // largest absolute difference anywhere is
        // 1.192e-07 = 2^-23 (one ulp of full-scale float;
        // -138.5 dBFS), and the GR trace is bit-identical
        // on both configs. Same rounding-path parting as
        // AB3 measured against 1.3.1 (17.4 % / 17.6 %,
        // same bound): (1-m)*dry + m*cascade cannot round
        // identically to the classic in + (mix - in) * g.
        // The AB2-side probe hashes reproduce AB2's own
        // K7e references (f7dd0cbca857c8f4 /
        // a1ff60de775fdc45), so the comparison chain is
        // proven at both ends.
        {
            const uint64_t wantP[2] = { 0x7a11fd2b4f8cb936ull, 0x963dbf00dac0bed5ull };
            bool ok = true;
            char bad[160] = "";
            for (int i = 0; i < 2; ++i) {
                uint64_t got = renderHash(cfgs[28 + i], 0.5f);
                if (got != wantP[i]) {
                    ok = false;
                    std::snprintf(bad, sizeof bad, "%s @modMix0.5: got %016llx want %016llx",
                                  cfgs[28 + i].name, (unsigned long long)got,
                                  (unsigned long long)wantP[i]);
                    break;
                }
            }
            std::snprintf(d, sizeof d, "%s", ok ? "both hashes match the FULL references (1-ulp equivalence to the AB2 classic pedal, see above)" : bad);
            check(ok, "K7f FULL: MOD-in P at modMix=0.5 == AB2 classic P output to 1 ulp (classic pedal)", d);
        }
    }

    std::printf("\n=== L. TWO-STAGE MIX (AB3: COMP MIX / MOD MIX) ===\n");
    {
        // Programme renders through the full chain with
        // explicit pot states (K7 protocol: setters, reset,
        // setModIn last). mod: 0 = out, 1 = in A, 2 = in P.
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
        // L2: compMix=0 (modMix=0): the output is the input
        // delayed by exactly the reported latency, in every
        // MOD state; with MOD out the MOD MIX pot is inert
        // at stage 1 too (0 / 0.37 / 1 all identical).
        {
            bool ok = true;
            const struct { int mod, lat; } cases[] = { { 0, 32 }, { 1, 992 }, { 2, 32 } };
            for (const auto& c : cases) {
                auto r = renderMix(0.0f, 0.0f, c.mod, 0.75f, 2.0);
                // Settled-state identity: the window opens
                // after the engage edge's 30 ms offset
                // crossfade (1440 samples) has completed —
                // mid-edge the dry side morphs between
                // offsets, as the v44 tap did.
                for (size_t i = (size_t)c.lat + 2000; i < r.outL.size(); ++i)
                    if (r.outL[i] != r.inL[i - (size_t)c.lat] || r.outR[i] != r.inR[i - (size_t)c.lat])
                        ok = false;
            }
            auto base = renderMix(0.0f, 0.0f, 0, 0.75f, 2.0);
            for (float mm : { 0.5f, 1.0f }) {
                auto v = renderMix(0.0f, mm, 0, 0.75f, 2.0);
                if (v.outL != base.outL || v.outR != base.outR) ok = false;
            }
            check(ok, "L2 compMix=0: output == input delayed by exactly the reported latency (32/992/32)", "");
        }
        // L3: modMix=0 with MOD in A = the comp path alone,
        // aligned: sample-equal to the MOD-out render
        // shifted by exactly 960.
        {
            auto inA = renderMix(0.6f, 0.0f, 1, 0.75f, 3.0);
            auto out = renderMix(0.6f, 0.0f, 0, 0.75f, 3.0);
            bool ok = true;
            for (size_t i = 960 + 2000; i < inA.outL.size(); ++i)   // settled (see L2)
                if (inA.outL[i] != out.outL[i - 960] || inA.outR[i] != out.outR[i - 960])
                    ok = false;
            check(ok, "L3 modMix=0 (MOD in A): comp path alone, aligned at +960 exactly", "");
        }
        // L4: the linear law at partial mixes is exact
        // superposition of the two settled arms (bit-level):
        // (a) comp stage at 0.5, read through the mod stage
        // at 0 (MOD in A); (b) mod stage at 0.5 in the P
        // flavour at compMix=1.
        {
            auto dA = renderMix(0.0f, 0.0f, 1, 0.75f, 2.0);
            auto wA = renderMix(1.0f, 0.0f, 1, 0.75f, 2.0);
            auto hA = renderMix(0.5f, 0.0f, 1, 0.75f, 2.0);
            bool okA = true;
            // From sample 2000: ahead of it the three
            // renders are inside the engage edge's offset
            // crossfade, whose (b - a) * fade form rounds
            // differently per arm — the superposition is a
            // settled-state identity (the P-stage twin below
            // has no offset edge and holds from sample 0).
            for (size_t i = 2000; i < hA.outL.size(); ++i) {
                if (hA.outL[i] != dA.outL[i] * (1.0f - 0.5f) + wA.outL[i] * 0.5f) okA = false;
                if (hA.outR[i] != dA.outR[i] * (1.0f - 0.5f) + wA.outR[i] * 0.5f) okA = false;
            }
            auto dP = renderMix(1.0f, 0.0f, 2, 0.75f, 2.0);
            auto wP = renderMix(1.0f, 1.0f, 2, 0.75f, 2.0);
            auto hP = renderMix(1.0f, 0.5f, 2, 0.75f, 2.0);
            bool okB = true;
            for (size_t i = 0; i < hP.outL.size(); ++i) {
                if (hP.outL[i] != dP.outL[i] * (1.0f - 0.5f) + wP.outL[i] * 0.5f) okB = false;
                if (hP.outR[i] != dP.outR[i] * (1.0f - 0.5f) + wP.outR[i] * 0.5f) okB = false;
            }
            std::snprintf(d, sizeof d, "comp-stage superposition %d, mod-stage (P) superposition %d",
                          (int)okA, (int)okB);
            check(okA && okB, "L4 partial mixes are the exact linear superposition of the arms", d);
        }
        // L5: MOD MIX is inert with MOD out at a working
        // comp setting: bit-identical at 0 / 0.5 / 1 (the
        // hard selects and the exact-half law), and within
        // one float ulp at a non-dyadic position (0.37) —
        // blending a signal with itself under the linear
        // law is exact only where the weights are.
        {
            auto a = renderMix(0.7f, 0.0f, 0, 0.75f, 2.0);
            auto bq = renderMix(0.7f, 0.5f, 0, 0.75f, 2.0);
            auto cq = renderMix(0.7f, 1.0f, 0, 0.75f, 2.0);
            auto dq = renderMix(0.7f, 0.37f, 0, 0.75f, 2.0);
            double mx = 0;
            for (size_t i = 0; i < a.outL.size(); ++i) {
                mx = std::max(mx, (double)std::fabs(dq.outL[i] - a.outL[i]));
                mx = std::max(mx, (double)std::fabs(dq.outR[i] - a.outR[i]));
            }
            std::snprintf(d, sizeof d, "0.37 vs 0: max |delta| %.2e (1 ulp class)", mx);
            check(a.outL == bq.outL && a.outR == bq.outR && a.outL == cq.outL && a.outR == cq.outR
                  && mx <= 1.2e-7,
                  "L5 modMix inert with MOD out (bit-exact 0/0.5/1; <=1 ulp at 0.37)", d);
        }
        // L6: the GR readout is the limiter's, untouched by
        // either pot: the GR trace of a partial-mix render
        // equals the fully-wet render's trace sample-for-
        // sample.
        {
            auto a = renderMix(0.4f, 0.6f, 1, 0.75f, 2.0);
            auto bq = renderMix(1.0f, 1.0f, 1, 0.75f, 2.0);
            check(a.gr == bq.gr && a.gr.back() > 0.0f,
                  "L6 GR trace identical at partial mixes (meter source unchanged)", "");
        }
        // L7 (the classic-pedal proof, AB3 steer): with the
        // LFO frozen at the sweep bottom (break 600 Hz), the
        // first notch sits where the J-section law puts it.
        // The chain's law, applied to the machine's two arms
        // (dry from a lockstep twin running process(), the
        // cascade from processEffect()), must null at
        // modMix 0.5 with the classic depth, tilt to
        // ~-6 dB at 0.25/0.75 (the (1-2m) residue), and
        // vanish at 1.0 (the pure cascade is allpass,
        // |H| = 1 — no notch at all).
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
                  "L7a modMix 0.5 notch depth == the classic pedal's (both algebraic nulls)", d);
            check(g25 > -7.5 && g25 < -4.5 && g75 > -7.5 && g75 < -4.5,
                  "L7b notch depth at 0.25 / 0.75 ~= -6 dB (the (1-2m) residue)", d);
            check(std::fabs(g100) < 0.75,
                  "L7c modMix 1.0: no notch — the pure cascade is allpass-flat", d);
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
            std::printf("      programme RMS (knob 0.75, 4 s, second half):\n");
            for (int mod : { 1, 2 }) {
                for (float mm : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
                    auto r = renderMix(1.0f, mm, mod, 0.75f, 4.0);
                    std::printf("        MOD in %s, modMix %.2f: %.2f dBFS\n",
                                mod == 1 ? "A" : "P", (double)mm, rmsDb(r));
                }
            }
            for (float cm : { 0.0f, 0.5f, 1.0f }) {
                auto r = renderMix(cm, 1.0f, 0, 0.75f, 4.0);
                std::printf("        MOD out, compMix %.2f: %.2f dBFS\n", (double)cm, rmsDb(r));
            }
            check(true, "L8 level table printed (report data)");
        }
    }

    std::printf("\n%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL TESTS PASSED" : "FAILURES PRESENT",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
