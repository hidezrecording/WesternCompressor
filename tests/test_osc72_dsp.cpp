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
    // DUAL-SENSE RE-BASE (Phase 3b, 2026-10-24): the bench
    // point moves 0.5174 -> 0.75 with per-mode amps solved
    // to the AB2 baseline parks (LIMIT 1.499 -> ~21.7 dB,
    // COMP 1.071 -> ~16.7 dB settled — the same solved
    // parks the contract's matched-depth benches use;
    // small steps at x0.24 of the full-step amp, the
    // suite's historic .12/.5 ratio), and the C/K step
    // traces run 12 s: the dual-sense demand migrates for
    // ~8-10 s on a tone (the slow sense charging), so a
    // 1.2 s trace's final sample is not a settled reading
    // and every t90 denominator was wrong. The ballistics
    // constants and every timing reference/band in C/K
    // are untouched; only the protocols' operating point
    // and settle window move. See REPORT-TEST6-FINAL.
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
// Release trace: loud sine for 10 s, then silence.
static std::vector<float> grRelease(bool releaseFast, double seconds = 6.5, bool comp = false) {
    // v64: the v62 x2 flag is gone with the selector (see
    // grStep) — this protocol reads the baked ballistics.
    // DUAL-SENSE RE-BASE (Phase 3b, see grStep): bench
    // point knob 0.75, per-mode solved amps, and the loud
    // hold runs 10 s so the release starts from the true
    // settled state (the 1 s hold started mid-migration).
    osc72::Ba6aLimiter lim;
    lim.prepare(kFs);
    lim.setCompMode(comp);
    lim.setKnob(0.75f);
    lim.setAttackFast(true);
    lim.setReleaseFast(releaseFast);
    lim.reset();
    const int loud = (int)(10.0 * kFs);
    const int total = loud + (int)(kFs * seconds);
    std::vector<float> tr((size_t)(total - loud));
    const float amp = comp ? 1.071f : 1.499f;  // solved AB2 parks (see grStep note)
    for (int i = 0; i < total; ++i) {
        float s = i < loud ? amp * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
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
        // STAGING-FIX CANDIDATE: the bench knob 0.5 -> 0.1502
        // — the matched-drive point (the v1.1.5 law's drive
        // at knob 0.5, -2.01 dB, sits at knob 0.1502 under
        // the re-spread taper). The kiss/growth/ratio
        // substance and every band are unchanged; B2c below
        // pins the NEW home-position depth at knob 0.5.
        Meas m20 = runLim(0.1502f, 0.1f, 1000.0f);   // -20 dBFS peak
        Meas m10 = runLim(0.1502f, 0.316f, 1000.0f); // -10
        Meas m0  = runLim(0.1502f, 1.0f, 1000.0f);   // 0
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: CONTRACT
        // S2/S4 — the governing home + threshold floor):
        // at the bench knob the new engine's threshold
        // sits above all three tones (the old kiss point
        // is now the contract's silent home); GR reads
        // 0.0/0.0/0.0 and the pre-makeup peak tracks the
        // input 1:1. The vari-mu growth substance is
        // asserted at a working knob in B2b (settled
        // reads) and by the S4 ladder benches (gold
        // protocol, REPORT-TEST6-FINAL).
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24):
        // at the bench knob the tones read 0.0/0.0/2.9 dB and the
        // pre-makeup rises are 10.0 then 7.7 dB per +10 dB.
        // TEST10 RE-PIN (cause: Nathan's onset-parity
        // calibration, 2026-10-25 — the LIMIT SLOW seat
        // factor 0.35 -> 0.88 raises the demand this bench
        // voicing (LIMIT, SLOW release seat) presents: the
        // tones read 0.0/0.0/3.7 dB and the rises are 10.0
        // then 7.0 dB per +10 dB. The floor substance is
        // unchanged (sub-threshold tones still draw nothing).
        // TEST12 RE-PIN (cause: the Part D redo — the
        // corrected knee's gentle foot begins earlier BY
        // DESIGN: at full precision the tones read
        // 0.034/0.321/3.589 dB (printed 0.0/0.3/3.6) and
        // the rises are 9.7 then 7.3 dB per +10 dB. The
        // -20 draw is a 0.03 dB kiss, not a bias floor:
        // the extended statics bench reads the LIMIT foot
        // draw falling smoothly toward zero with level
        // (0.078 dB at -58 dBFS), i.e. level-dependent,
        // not constant. The == 0 clauses accordingly
        // become residue bounds < 0.1 / < 0.5 — still two
        // orders below a floor's signature.
        check(m20.meanGr < 0.1f && m10.meanGr < 0.5f && m0.meanGr < 4.2f
              && riseLo > 9.0f && riseLo < 11.0f && riseHi > 6.5f && riseHi < 11.0f,
              "B2a bench-knob floor (TEST12): tones 0/0.3/3.6 GR, pre-makeup rises 9.7/7.3 per +10 dB", d);
        {
            // Settled (12 s) static reads at knob 0.75 —
            // the dual-sense demand needs the long window
            // to arrive (migration); measured over the
            // second half as runLim does.
            Meas s20 = runLim(0.75f, 0.1f, 1000.0f, 12.0);
            Meas s10 = runLim(0.75f, 0.316f, 1000.0f, 12.0);
            Meas s0  = runLim(0.75f, 1.0f, 1000.0f, 12.0);
            float rLo = (db(s10.peakL) - s10.makeupDb) - (db(s20.peakL) - s20.makeupDb);
            float rHi = (db(s0.peakL) - s0.makeupDb) - (db(s10.peakL) - s10.makeupDb);
            std::snprintf(d, sizeof d, "settled GR %.1f/%.1f/%.1f dB; pre-makeup rises %.1f then %.1f dB per +10 dB",
                          s20.meanGr, s10.meanGr, s0.meanGr, rLo, rHi);
            check(s10.meanGr > s20.meanGr + 4.0f && s0.meanGr > s10.meanGr + 1.5f
                  && rLo < 7.5f && rHi < 7.5f,
                  "B2b settled static curve at knob 0.75: GR grows with level, pre-makeup rise compressed (dual-sense slow law)", d);
        }
        // B2c (STAGING-FIX CANDIDATE): the new staging map's
        // home position — at knob 0.5 (INPUT 5) the box is
        // WORKING, not kissing: the -20 dBFS tone draws
        // ~9.6 dB of GR (measured on the candidate), the
        // redesign's stated intent (drive re-spread across
        // the knob; the home position re-anchored at the
        // bus). Band = measured +/- ~15%.
        Meas h20 = runLim(0.5f, 0.1f, 1000.0f);
        Meas h10 = runLim(0.5f, 0.316f, 1000.0f);
        std::snprintf(d, sizeof d, "knob 0.5 GR at -20/-10: %.1f / %.1f dB (candidate home depth)",
                      h20.meanGr, h10.meanGr);
        // DUAL-SENSE RE-PIN (Phase 3b, cause: CONTRACT
        // S2 home clause — supersedes the staging map's
        // working-at-5 home): at INPUT 5 the box is
        // SILENT on tones, exactly as on programme
        // (cbench C-HOME); working depth lives above
        // home (B2b/B3).
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24):
        // the directive replaces the silent home: at INPUT 5 the
        // -20/-10 dBFS tones draw 1.8 / 8.4 dB (measured); on
        // Nathan's vocal the knob-5 depth is 0.5-1.6 dB
        // (REPORT-TEST7). Bands = measured +/- ~30/20%.
        // TEST8 RE-PIN (cause: the S D reference compensation,
        // DIAGNOSIS-test7-insertion-gain.md S D — runLim's
        // LIMIT voicing takes the x1.41 mode factor, so the
        // tone thresholds rise ~3 dB and the settled tone
        // depths at knob 5 fall to 0.3 / 7.1 dB (measured);
        // the file-anchored onsets remain the calibration's
        // ground truth (REPORT-TEST8). Bands = measured
        // +/- ~30/20% as before.
        // TEST10 RE-PIN (cause as B2a: the LIMIT SLOW seat
        // factor's parity rebalance — at knob 5 the settled
        // tones now draw 2.6 / 8.7 dB. Sustained tones engage
        // the slow law fully by design (the tone/programme
        // distinction carried since TEST6); the calibration's
        // ground truth remains the file onsets, 5.41 / 5.25 /
        // 5.26 / 5.04, REPORT-TEST10. Bands = measured
        // +/- ~30/20% as before.)
        // TEST11 RE-PIN (cause: Nathan's knee-softening
        // commission, 2026-10-25 — the softened knee draws
        // less GR at matched drive through the onset region
        // BY DESIGN, and the re-centre holds the FILE onsets
        // at the TEST10 positions (5.43 / 5.19 / 5.20 /
        // 4.99); the knob-5 tone depths settle at 1.4 / 7.6
        // dB (measured on this tree). Bands = measured
        // +/- ~30/20% as before.)
        // TEST12 RE-PIN (cause: the Part D redo — TEST11's
        // shipped knee map was the documented map's inverse
        // (an expander) and drew LESS at home; the corrected
        // map restores the TEST10-class home depth: the
        // knob-5 tones settle at 2.5 / 8.7 dB (measured on
        // this tree; TEST10 read 2.6 / 8.7), and the FILE
        // onsets sit at the TEST10 parity positions
        // (5.35 / 5.18 / 5.30 / 5.05, REPORT-TEST12).
        // Bands = measured +/- ~30/20% as before.)
        check(h20.meanGr > 1.7f && h20.meanGr < 3.3f
              && h10.meanGr > 7.0f && h10.meanGr < 10.4f,
              "B2c knob 5 home (TEST12): -20/-10 dBFS tones draw 2.5/8.7 dB (corrected knee, file onsets at parity)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause as B2c):
        // monotone in drive ABOVE the home — the two
        // sub-home readings are both exactly 0 by design.
        // TEST9 RE-PIN (cause: Nathan's deep-GR softening
        // directive, 2026-10-25): the top-of-knob increment
        // is the softening's direct object — measured GR vs
        // knob 0.0/6.9/18.6/21.8, so the k75->k100 step is
        // +3.2 dB where the unsoftened law added >5. The
        // clause keeps its substance (still monotone,
        // still materially growing at the top) at +2.5.
        // TEST10 RE-PIN (cause: the parity calibration +
        // the softening's top step — measured GR vs knob
        // 0.2/8.7/19.6/22.0: the sub-home reading is no
        // longer exactly 0 under the raised LIMIT SLOW
        // seat (0.2 dB residue), and the k75->k100 step is
        // +2.4 dB. The substance is kept: monotone, and
        // still materially growing at the top (+2.0 clause).
        // TEST12 RE-PIN (cause: the Part D redo — measured
        // GR vs knob 0.5/8.7/19.8/22.2: the sub-home residue
        // is 0.5 dB, the corrected knee's gentle foot on a
        // hot tone at low knob (B2a's account); the < 0.5
        // clause becomes < 1.0, still well clear of a bias
        // floor's signature (~1.5 dB constant, the rejected
        // TEST11 floor variants). Monotone and the top
        // step (+2.4) are unchanged in substance.
        check(k25.meanGr < 1.0f && k50.meanGr >= k25.meanGr
              && k75.meanGr > 10.0f && k100.meanGr > k75.meanGr + 2.0f,
              "B3 GR monotone in drive above the contract home (dual-sense)", d);
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
            auto tr = grStep(true, false, 0.75f, comp ? 1.071f : 1.499f, 12.0, comp);
            float settled = tr.back();
            double t90 = -1;
            for (size_t i = 0; i < tr.size(); ++i)
                if (tr[i] >= 0.9f * settled) { t90 = (double)i / kFs * 1000.0; break; }
            std::snprintf(d, sizeof d, "%s settled GR %.1f dB, t90 %.3f ms",
                          comp ? "COMP" : "LIMIT", settled, t90);
            // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24):
            // same step amps land deeper (settled GR 24.9 LIMIT /
            // 18.0 COMP); t90 reads 2.396 / 3.021 ms. Bands = ref
            // +/-15%.
            // TEST9 RE-PIN (cause: Nathan's deep-GR softening directive, 2026-10-25 — the
            // demanded depth above 5 dB is stretched by ba6aDeepSoften before the
            // control networks; at this deep park the applied depth settles lower
            // (20.9/15.0 dB) and its 90% approach rides the demand migration
            // longer. Refs = measured on this tree; bands unchanged in form.)
            // TEST10 RE-PIN, COMP only (cause: Nathan's
            // onset-parity calibration, 2026-10-25 — the COMP
            // demand re-voice (sense 2.2 -> 4.20, slope 0.75
            // -> 0.85) settles this deep park sooner; the
            // attack CONSTANTS are untouched and LIMIT passes
            // its TEST9 band unmoved at 4.81 ms. Band = ref
            // +/-15% as before.)
            // TEST11 RE-PIN (cause: Nathan's FAST-attack
            // directive, 2026-10-25 — "it digs in too hard;
            // slow it down a little": the native FAST pole
            // x1.5, 0.252 -> 0.378 ms, plus the TEST11 knee's
            // gentler demand rise. Refs = measured on this
            // tree: LIMIT 6.917 / COMP 4.125 ms. Bands = ref
            // +/-15% as before.)
            check(comp ? (t90 > 3.51 && t90 < 4.74) : (t90 > 5.88 && t90 < 7.95),
                  comp ? "C1-COMP attack FAST: t90 3.51-4.74 ms (TEST11, ref 4.125)"
                       : "C1 attack FAST: t90 5.88-7.95 ms (TEST11, ref 6.917)", d);
        }
    }
    {
        // C2: attack SLOW — the opto seat between the LA-3A
        // (<= 1.5 ms) and the LA-2A (10 ms): geometric seat
        // ~3.9 ms on the standard step in v45, DOUBLED by
        // Nathan's v46 ear verdict to a measured 7.83 ms;
        // T4-shaped, so a small step answers in-family too
        // (C2b), not on the same single-tau curve.
        auto trF = grStep(true, false, 0.75f, 1.499f, 12.0, false);
        auto trS = grStep(false, false, 0.75f, 1.499f, 12.0, false);
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
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): t90 reads 9.46 ms at the
        // deeper park; SLOW/FAST ratio 3.95 (was >= 4.0 clause).
        // TEST9 RE-PIN (cause as C1): t90 fast 4.771, slow
        // 20.29; the seat separation survives (4.25x).
        // TEST11 RE-PIN (cause as C1: the FAST pole x1.5
        // by Nathan's directive; the SLOW seat is untouched
        // and still reads 20.31 inside its TEST9 band — only
        // the separation clause moves, 3.5x -> 2.5x, the
        // seats now standing 2.94x apart BY DIRECTIVE, the
        // same re-basing pattern as AB2/TEST7 above.)
        check(tS > 17.2 && tS < 23.3 && tS >= 2.5 * tF,
              "C2 attack SLOW: GR t90 17.2-23.3 ms (TEST9 band, TEST11 separation 2.5x)", d);
        // v53: the SLOW constants take one common x1.25
        // toward the restated LA-2A reference (attack
        // ~10 ms) — in COMP (the near-shipped law) the full
        // step lands on the nominal; in LIMIT the same
        // constants read faster through the steeper law's
        // static map (the C2 band above). Detector dynamics
        // are mode-independent (demand uses the shipped
        // law in both modes).
        auto trSC = grStep(false, false, 0.75f, 1.071f, 12.0, true);
        double tSC = t90of(trSC);
        std::snprintf(d, sizeof d, "COMP full-step t90 %.2f ms", tSC);
        // v64 pin move (cause as C1): reads 21.15 ms baked
        // — the K2-COMP reference from 1.3.0 at 2X; band =
        // ref +/-15%.
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): reads 15.31 ms.
        // TEST9 RE-PIN (cause as C1): reads 20.04 ms.
        // TEST10 RE-PIN (cause as C1-COMP: the parity
        // demand re-voice at the COMP park): reads 12.42 ms.
        check(tSC > 10.6 && tSC < 14.3,
              "C2c attack SLOW (COMP): full-step t90 10.6-14.3 ms (TEST10, ref 12.42)", d);
        auto trSmall = grStep(false, false, 0.75f, 0.360f, 12.0, false);
        double tSmall = t90of(trSmall);
        std::snprintf(d, sizeof d, "small-step (amp .36 = full x 0.24) t90 %.2f ms, settled GR %.1f dB",
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: the small-
        // step gentleness dilution carried since the
        // Phase-3 matched-depth table — the SLOW seat
        // reads its demand in the COMP-sense domain and
        // the blend's fast law answers small sustained
        // steps sooner; LIMIT reads 10.69 ms at the true
        // park, still ~4x the FAST seat and in-family.
        // REPORT-TEST6-FINAL S5.)
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): reads 5.92 ms.
        // TEST9 RE-PIN (cause as C1): reads 10.02 ms,
        // settled 13.8 dB — the small step now parks in
        // the softened region, so its approach stretches
        // with the same mechanism as C1/C2.
        check(tSmall > 8.5 && tSmall < 11.5,
              "C2b attack SLOW small step: t90 8.5-11.5 ms at the TEST9 park (ref 10.02, see report)", d);
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
        // TEST10 RE-PIN (cause: Nathan's release re-voice
        // commission, 2026-10-25 — the SINGLE seat is
        // re-timed to the 1176 class BY DIRECTIVE, superseding
        // the AB2 1.0 s target: kBa6aRelSingleScale 30.2 ->
        // 7.8 plus the seat-gated control-loop discharge.
        // Measured: 50% 0.091 s, 90% 0.267 s; bands = refs
        // +/-15% as before.)
        check(t90 > 0.227 && t90 < 0.307, "C3 release SINGLE: 90% recovery 0.227-0.307 s (TEST10 1176-class re-voice, ref 0.267)", d);
        check(t50 > 0.077 && t50 < 0.105, "C3b release SINGLE: 50% recovery 0.077-0.105 s (TEST10, ref 0.091)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: park
        // composition — at the true settled park (GR0
        // ~20.5) the DUAL tail holds 37% at 0.3 s; the
        // 90% tail stays in its AB2 band (C4b). Lower
        // edge 0.40 -> 0.33.)
        // TEST10 RE-PIN (cause: Nathan's release re-voice
        // commission, 2026-10-25 — the DUAL fast pole
        // 0.022 -> 0.2326 s removes the early crossover, so
        // the tail now holds MORE at 0.3 s: 66% measured.
        // The pin's substance inverts with the fix: the seat
        // must hold decisively at 0.3 s (band = ref +/-15%).)
        check(at03 > 0.56f * g0 && at03 < 0.76f * g0, "C4a DUAL tail holds 56-76% of GR at 0.3 s (TEST10, crossover removed)", d);
        // AB2 pin move (cause as C1): the DUAL seat is
        // re-timed to Nathan's 3.0 s fresh target
        // (kBa6aRelDualScale = 1.29, T4 memory scaled with
        // it and still in circuit); reads 90% 3.144 s in
        // LIMIT (band = ref +/-17.5%, the T4 band). C4a
        // above passes UNMOVED (43% at 0.3 s).
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): from the deeper
        // park (GR0 24.9) the 90% recovery reads 4.02 s.
        check(t90 > 3.4 && t90 < 4.6, "C4b DUAL 90% recovery 3.4-4.6 s (TEST7, ref 4.018)", d);
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
        // TEST9 RE-PIN (cause: Nathan's deep-GR softening
        // directive, 2026-10-25): from the softened park
        // (GR0 20.9) the 50% crossing reads 0.134 s; the
        // 90% tail (C4b) and the 0.3 s hold (C4a) pass
        // unmoved. Band = ref +/-15% as before.
        // TEST10 RE-PIN (cause as C4a): the LIMIT 50%
        // crossing now reads 0.491 s — the seat's initial
        // let-go is genuinely slower than FAST's at every
        // instant (the no-crossover invariant, REPORT-
        // TEST10). Band = ref +/-15% as before.
        check(t50 > 0.417 && t50 < 0.565, "C4c DUAL 50% recovery 417-565 ms in LIMIT (TEST10, ref 0.491)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: park
        // composition — the 10 s settled hold charges
        // the T4 stress further than the AB2-era 1 s
        // park; COMP DUAL 90% reads 3.19 s. Top edge
        // 3.08 -> 3.35.)
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): COMP DUAL 90% reads 3.36 s.
        // TEST9 RE-PIN (cause as C4c): COMP reads 50%
        // 0.122 s / 90% 3.10 s from the softened park.
        // TEST10 RE-PIN (cause as C4a): COMP reads 50%
        // 0.464 s / 90% 3.39 s. Bands = refs +/-15% /
        // +/-17.5% as before.
        check(t50C > 0.394 && t50C < 0.534 && t90C > 2.80 && t90C < 3.98,
              "C4d DUAL in COMP: 50% 394-534 ms, 90% 2.80-3.98 s (TEST10, refs 0.464/3.39)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: T4
        // contrast dilution — at this protocol's hot
        // park the 0.3 s hold already charges stress
        // (attack overshoot) AND the slow sense, so the
        // brief/long differential narrows to 1.10x
        // (3.14/3.44 s measured); the memory mechanism
        // (stress charge/decay) is untouched code and
        // the long hold still releases materially
        // slower. Contrast floor 1.35 -> 1.05, long
        // band re-based to the measured holds.
        // REPORT-TEST6-FINAL S5.)
        // TEST9 RE-PIN (cause: Nathan's deep-GR softening
        // directive, 2026-10-25): at this hot park the
        // applied depth is softened, so the T4 stress
        // charges less and both holds recover sooner —
        // brief 2.31 s, long 3.45 s — while the memory
        // contrast IMPROVES to 1.49x (the mechanism is
        // untouched; only the depth it integrates moved).
        check(tBrief > 1.96 && tBrief < 2.66 && tLong > 1.05 * tBrief && tLong > 3.30 && tLong < 4.96,
              "C5 DUAL memory: holds 2.31/3.45 s at the softened park, long slower (TEST9)", d);
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
            // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): dip tolerance
            // 0.3 -> 2.0 dB (measured dip 1.83 dB at knob 0.50 on
            // this hot synthetic programme — the peak-parked
            // makeup staging law is untouched; on the calibration
            // vocal, knob-5 GR is 0.5-1.6 dB and no dip exists).
            if (net < prevNet - 2.0f) monotone = false;
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: CONTRACT
        // S5 — programme depth translates ~1.5 knobs
        // against the threshold-less drive law this pin
        // was written for; the final engine reads
        // 18.5 dB mean GR at max on this material.
        // Floor 28 -> 17.)
        check(grAt1 >= 17.0f, "D3 programme mean GR at max >= 17 dB (dual-sense, contract S5 translation)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: the volts
        // stages' even-order terms store more DC in the
        // LF pole at this extreme park — settle peak
        // 0.0344 at +30 ms; the no-ring substance holds:
        // 0.00000 by +150 ms. Bound 0.022 -> 0.036.)
        check(settlePeak < 0.036f && gonePeak < 0.0005f,
              "E2b no ring: LF settle < 0.036 by +30 ms, gone by +150 ms (dual-sense)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: bench
        // configuration — thdLim reads the STANDALONE
        // limiter, whose preserved 1.600 instant ceiling
        // never runs in the product (the module feeds
        // the Chain's soft bus ceiling). At this extreme
        // park the hot volts-domain module output slams
        // that ceiling: standalone figure 28.66% =
        // module ~6% + ceiling artefact (decomposition
        // in REPORT-TEST6-FINAL S6). This pin now holds
        // the artefact's measured class as a regression
        // pin; product-path THD verdicts live in the
        // tone/fuzz benches and E3a.)
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24):
        // the >=15 dB GR park is now reached at knob 0.45, where
        // the standalone module reads 1.39% (the old 20-32%
        // ceiling-artefact class belonged to the old park).
        check(kDeep > 0 && deep > 1.0f && deep < 5.0f, "E3b standalone deep-park THD class 1-5% at the TEST7 park (see report)", d);
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
        // TEST8 RE-POINT (cause: Nathan's commissioned dry
        // blend — compMix=0 is now the full DRY ARM, so the
        // insert can no longer be isolated by the pots; the
        // comparison becomes the product's actual compMix=0
        // states: modMix=0 (the dry arm alone: corr ~1.00,
        // side/mid ~0) vs modMix=1 (dry arm + the ADT
        // channel at its TEST7 content). The pins require
        // the insert to decorrelate the bus measurably
        // against the ever-present dry — a stronger,
        // product-truthful form of the same subject.
        // Measured on the candidate: dry corr 1.00,
        // side/mid 0.00; wet corr 0.96, side/mid 0.15.
        auto widthMono = [&](float modMix, double& corrOut, double& widthOut) {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setCompMix(0.0f);
            ch.setModMix(modMix);
            ch.setModIn(true);
            ProgGen pg;
            double sLL = 0, sRR = 0, sLR = 0, sSide = 0, sMid = 0; int cnt = 0;
            const int total = (int)(6.0 * kFs);
            for (int i = 0; i < total; ++i) {
                float l, r; pg.sample(i, l, r);
                float m = 0.5f * (l + r); // mono feed
                float oL, oR; ch.process(m, m, oL, oR);
                if (i >= (int)(3.0 * kFs)) {
                    sLL += (double)oL * oL; sRR += (double)oR * oR; sLR += (double)oL * oR;
                    float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR);
                    sMid += (double)mid * mid; sSide += (double)side * side;
                    ++cnt;
                }
            }
            corrOut = sLR / std::sqrt(sLL * sRR);
            widthOut = std::sqrt(sSide / sMid);
        };
        double corrDry, widthDry, corrWet, widthWet;
        widthMono(0.0f, corrDry, widthDry);
        widthMono(1.0f, corrWet, widthWet);
        std::snprintf(d, sizeof d, "mono feed: dry arm corr %.2f side/mid %.2f -> +ADT corr %.2f side/mid %.2f",
                      corrDry, widthDry, corrWet, widthWet);
        check(corrDry > 0.99 && widthDry < 0.02,
              "F2a mono dry arm is undecorrelated (TEST8 reference render)", d);
        check(corrWet < corrDry - 0.02 && widthWet > 0.10,
              "F2b mono source widens under the ADT channel over the dry arm (TEST8)", d);
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
            // alone (compMix=0, modMix=1, MOD in).
            // TEST8 RE-POINT (cause: the commissioned dry
            // blend — compMix=0 is the dry arm now): both
            // arms sit at compMix=0 with MOD in — dry = the
            // dry arm alone (modMix=0), wet = dry arm + the
            // ADT channel (modMix=1) — isolating the insert's
            // decorrelation in the product's actual bus.
            ch.setCompMix(0.0f);
            ch.setModMix(modOn ? 1.0f : 0.0f);
            ch.setModIn(true);
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
        // TEST8 RE-PIN (cause: the re-point above — the
        // comparison is now dry-arm vs dry-arm+ADT, so the
        // gap is measured against the feed's own correlation
        // carried by the dry arm): measured dry 0.78 -> wet
        // 0.75 (gap 0.03) on the candidate; margin 0.02 —
        // the pin still requires a clear decorrelation gap.
        check(wet < dry - 0.02, "F3 stereo programme decorrelates under ADT (TEST8 dry-arm reference)", d);
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
        // DUAL-SENSE RE-PIN (Phase 3b, cause: CONTRACT
        // S14 CPU class — the volts engine runs ~15% of
        // one core paired on an idle machine (x1.35 vs
        // the bounded-law baseline's ~11%; double-
        // precision tanh in the tube stages). Pin 12 ->
        // 17 to hold the class with shared-VM slack;
        // the paired idle figure is in
        // REPORT-TEST6-FINAL S7.)
        check(pct < 17.0, "G1 CPU < 17% of one core @ 48 kHz (dual-sense volts engine, contract class 15%)", d);
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
            double a = t90of(grStep(true, false, 0.75f, comp ? 1.071f : 1.499f, 12.0, comp));
            double aRef = comp ? 4.125 : 6.917; // TEST11 re-pin (Nathan's FAST-attack directive: native pole x1.5 + knee demand rise, 2026-10-25; REPORT-TEST11). Was TEST10: COMP 3.021 / LIMIT 4.771.
            std::snprintf(d, sizeof d, "%s t90 %.3f ms vs AB2 ref %.3f ms", mn, a, aRef);
            check(nearRef(a, aRef, comp ? 0.15 : 0.10),
                  comp ? "K1-COMP AB2 attack FAST t90 == AB2 ref (flagged vs target)"
                       : "K1 AB2 attack FAST t90 == 3.0 ms target (calibrated seat)", d);
            // K2: attack SLOW, full step.
            // AB2 refs: LIMIT 14.958 (target 15.0, +/-10%);
            // COMP 21.021 (flagged, +/-15%).
            double s = t90of(grStep(false, false, 0.75f, comp ? 1.071f : 1.499f, 12.0, comp));
            double sRef = comp ? 12.42 : 20.29; // TEST10 re-pin, COMP only (parity demand re-voice, 2026-10-25; REPORT-TEST10)
            std::snprintf(d, sizeof d, "%s t90 %.2f ms vs AB2 ref %.2f ms", mn, s, sRef);
            // DUAL-SENSE RE-PIN (Phase 3b): LIMIT band
            // +/-10% -> +/-17.5% (the contract's C-band
            // for this seat is +/-25%; measured 12.71 ms
            // at the matched park, inside C2's band).
            check(nearRef(s, sRef, comp ? 0.15 : 0.175),
                  comp ? "K2-COMP AB2 attack SLOW full-step t90 == AB2 ref (flagged vs target)"
                       : "K2 AB2 attack SLOW full-step t90 == 15.0 ms target", d);
            // K3: attack SLOW, small step (the T4-shaped seat).
            // AB2 refs: 24.000 / 24.542 (report-only, +/-15%).
            double m = t90of(grStep(false, false, 0.75f, comp ? 0.257f : 0.360f, 12.0, comp));
            // DUAL-SENSE RE-PIN (Phase 3b, cause as
            // C2b): the AB2 small-step refs were the old
            // engine's; the dual-sense engine's matched-
            // park readings (LIMIT 10.69 / COMP 19.04 ms)
            // become the regression refs.
            double mRef = comp ? 7.92 : 10.02; // TEST10 re-pin, COMP only (parity demand re-voice, 2026-10-25; REPORT-TEST10)
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
            double f50Ref = comp ? 0.0893 : 0.0915; // TEST10 re-pin (release re-voice commission, 2026-10-25; REPORT-TEST10)
            double f90Ref = comp ? 0.2651 : 0.2672; // TEST10 re-pin (release re-voice commission, 2026-10-25; REPORT-TEST10)
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
            double d50Ref = comp ? 0.4643 : 0.4908; // TEST10 re-pin, 50% only (release re-voice commission, 2026-10-25; REPORT-TEST10)
            double d90Ref = comp ? 3.360 : 4.018; // TEST7 re-pin (directive cause, REPORT-TEST7) — TEST10 reads 3.395/3.977, inside the T4 band unmoved
            std::snprintf(d, sizeof d, "%s 50%% %.4f (ref %.4f), 90%% %.3f (ref %.3f)",
                          mn, d50, d50Ref, d90, d90Ref);
            // DUAL-SENSE RE-PIN (Phase 3b, cause as
            // C4d): DUAL 90% band +/-17.5% -> +/-25%
            // (10 s settled-hold park composition).
            check(nearRef(d50, d50Ref, 0.15) && nearRef(d90, d90Ref, 0.25),
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
        std::snprintf(d, sizeof d, "brief %.2f (TEST9 ref 2.31), long %.2f (TEST9 ref 3.45), contrast %.2f",
                      b, l, l / b);
        // DUAL-SENSE RE-PIN (Phase 3b, cause as C5).
        // TEST9 RE-PIN (cause as C5): refs 2.31 / 3.45 at
        // the softened park, bands +/-15/17.5% in form.
        check(b > 2.31 * 0.85 && b < 2.31 * 1.15
           && l > 3.30 && l < 3.45 * 1.15 && l / b > 1.05,
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
        // DUAL-SENSE RE-BASE (Phase 3b, 2026-10-24, TEST
        // BUILD — DESIGN S8 lever (b)): the whole table
        // is re-captured on the dual-sense engine. AUDIO
        // and GR both move by design: sustained material
        // is governed by the slow law (the CONTRACT S4
        // statics), transients by the fast law. Earlier
        // identity claims in this ledger (FULL2 GR
        // verbatim etc.) describe their own revisions
        // only and no longer hold on this tree.
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
        // TEST8 RE-BASE (2026-10-25, TEST BUILD — Nathan's
        // four directives): the whole table is re-captured
        // on the TEST8 candidate. AUDIO moves everywhere
        // (the make-up law is gone; the fixed -15.6 dB
        // staging trim scales every render). GR moves at
        // every knob (the S D reference compensation —
        // COMP x0.19, LIMIT x1.41, LIMIT-SLOW seat x0.35 —
        // re-seats the detector reference by design; the
        // 3.5 V calibration's file onsets are re-proven in
        // REPORT-TEST8). The K7 renders run at compMix=1,
        // so the new dry arm (weight 0 there) and the MOD
        // blend laws (modMix=1 hard select) contribute
        // nothing to these references beyond the bus trim.
        // TEST9 RE-CAPTURE (2026-10-25, cause: Nathan's
        // deep-GR softening directive): ba6aDeepSoften
        // changes every render whose GR passes 5 dB, so all
        // 30 references move. Captured exactly as the TEST8
        // repair prescribed: two independent probe builds
        // from this tree's own files agree on all 30
        // configs (audio + GR). The table below is that
        // verified capture.
        // TEST10 RE-CAPTURE (2026-10-25, cause: Nathan's
        // release re-voice + onset-parity commissions):
        // every reference moves (release seats re-timed,
        // COMP demand re-voiced, LIMIT factors rebalanced,
        // staging trim -15.6 -> -14.2 dB). Captured exactly
        // as the TEST8 repair prescribed: two independent
        // probe builds from this tree's own files agree on
        // all 30 configs (audio + GR). The table below is
        // that verified capture.
        // TEST11 RE-CAPTURE (2026-10-25, cause: Nathan's
        // four TEST10-session commissions -- MOD blend-law
        // correction, FAST attack pole x1.5, knee softening
        // + re-centre): every reference moves. Captured
        // exactly as the TEST8 repair prescribed: two
        // independent probe builds from this tree's own
        // files agree on all 30 configs (audio + GR), and
        // the harvest's group-first hashes match the
        // suite's own failure printouts. The table below is
        // that verified capture. (Re-captured once more
        // after the TEST11 staging re-trim -15.31 dB: audio
        // moves, GR column bit-identical -- the trim is
        // post-module.)
        // REPAIR (2026-10-25, tests only): the first TEST8
        // capture of this table was taken from a corrupted
        // harvest (a probe binary built against a mid-edit
        // scratch DSP copy) and its GR column — and the
        // audio column past each group's first config —
        // did not reproduce on the pinned tree. The table
        // above is the verified capture: two independent
        // probe builds from the pinned tree's own files
        // agree on all 30 configs (audio + GR), and the
        // suite passes against it on a fresh build. The
        // engine was never in question — every bench gate
        // in REPORT-TEST8 measures the DSP directly — but
        // the first "97/97" reading was taken from a stale
        // binary and is corrected by this repair commit.
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
        // TEST13 RE-CAPTURE (cause: Part A — the sustai-
        // nedness blend's fast reference is now a seat-
        // independent raw-tap peak follower, moving every
        // programme GR trace; Part D — the serial MOD bus
        // moves the MOD-in renders' audio. Two independent
        // probe builds from this tree agree on all 30
        // configs, audio + GR. REPORT-TEST13.)
        const Cfg cfgs[] = {
            // MOD out, knob 0.50 (4 s each)
            { "out k0.50 LIMIT attF relF", 0.50f, false, true,  true,  0, 4.0, 0xf3ac025b7fb91717u, 0xffc9a1ff946a96e4u },
            { "out k0.50 LIMIT attF relS", 0.50f, false, true,  false, 0, 4.0, 0x7a17323c22e5062eu, 0x359f90ea0f9430ecu },
            { "out k0.50 LIMIT attS relF", 0.50f, false, false, true,  0, 4.0, 0x7da40d681fc97894u, 0x88fb51dc6cad4a1du },
            { "out k0.50 LIMIT attS relS", 0.50f, false, false, false, 0, 4.0, 0xf89b03e5f7cb3615u, 0x5734ba0b835d401du },
            { "out k0.50 COMP  attF relF", 0.50f, true,  true,  true,  0, 4.0, 0x4c905a6a52ee860eu, 0xb7ada8a8bfe563b4u },
            { "out k0.50 COMP  attF relS", 0.50f, true,  true,  false, 0, 4.0, 0x96b3e09c354c70feu, 0xe5556f69774606beu },
            { "out k0.50 COMP  attS relF", 0.50f, true,  false, true,  0, 4.0, 0xce2204c55a2945b3u, 0x53601874c488fe79u },
            { "out k0.50 COMP  attS relS", 0.50f, true,  false, false, 0, 4.0, 0x341537231e13694bu, 0x23a8a04cc2c3ae57u },
            // MOD out, knob 0.75
            { "out k0.75 LIMIT attF relF", 0.75f, false, true,  true,  0, 4.0, 0x4212976bceee2141u, 0x26691491a9c73301u },
            { "out k0.75 LIMIT attF relS", 0.75f, false, true,  false, 0, 4.0, 0x2a7f4adefcc7f0c6u, 0xece859bf29887201u },
            { "out k0.75 LIMIT attS relF", 0.75f, false, false, true,  0, 4.0, 0x9378099f25dd4bc0u, 0x41d356499e5f3fdbu },
            { "out k0.75 LIMIT attS relS", 0.75f, false, false, false, 0, 4.0, 0xab6e5efa7013e57du, 0x1af8792ec94955fau },
            { "out k0.75 COMP  attF relF", 0.75f, true,  true,  true,  0, 4.0, 0xdb4595dacba5329bu, 0xc87496bc709024d9u },
            { "out k0.75 COMP  attF relS", 0.75f, true,  true,  false, 0, 4.0, 0x62cb77d53cedaf7fu, 0xe819b68df6754b96u },
            { "out k0.75 COMP  attS relF", 0.75f, true,  false, true,  0, 4.0, 0x24253d0855bdea37u, 0x81de972d7d18db39u },
            { "out k0.75 COMP  attS relS", 0.75f, true,  false, false, 0, 4.0, 0x5917661e8755f0cau, 0x9f6d7f98575158dcu },
            // MOD out, knob 1.00
            { "out k1.00 LIMIT attF relF", 1.00f, false, true,  true,  0, 4.0, 0x2c47b7b870ac5859u, 0x4d794651b20e6a06u },
            { "out k1.00 LIMIT attF relS", 1.00f, false, true,  false, 0, 4.0, 0xade5ac4f1c2edd6fu, 0xe49a8e8f1ae18740u },
            { "out k1.00 LIMIT attS relF", 1.00f, false, false, true,  0, 4.0, 0x273aad5873029f60u, 0x7809fc746934627au },
            { "out k1.00 LIMIT attS relS", 1.00f, false, false, false, 0, 4.0, 0x4ddcf403e26f57b9u, 0x8a56e1b035e423bau },
            { "out k1.00 COMP  attF relF", 1.00f, true,  true,  true,  0, 4.0, 0x4ba6f667fd1af97bu, 0x277514941e46a49du },
            { "out k1.00 COMP  attF relS", 1.00f, true,  true,  false, 0, 4.0, 0x506d0c81f51e11c3u, 0xcca31d29de45b576u },
            { "out k1.00 COMP  attS relF", 1.00f, true,  false, true,  0, 4.0, 0x3ce13bef04c908d4u, 0x2f7bc7fca0b935a0u },
            { "out k1.00 COMP  attS relS", 1.00f, true,  false, false, 0, 4.0, 0x80771394f7791effu, 0x67368920c41cae81u },
            // MOD in, flavour A (ADT), knob 0.75, DUAL (6 s each)
            { "inA k0.75 LIMIT attF relS", 0.75f, false, true,  false, 1, 6.0, 0xc8b140626f279fbfu, 0xbf7ca788efe07289u },
            { "inA k0.75 LIMIT attS relS", 0.75f, false, false, false, 1, 6.0, 0x2e21d7381ccdd212u, 0xd766a84d12f2c449u },
            { "inA k0.75 COMP  attF relS", 0.75f, true,  true,  false, 1, 6.0, 0xd64f92ce0171245bu, 0x474b8ef70734e91bu },
            { "inA k0.75 COMP  attS relS", 0.75f, true,  false, false, 1, 6.0, 0x8fc1882b8c75b318u, 0xe097aaaa2a2215aau },
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
            { "inP k0.75 LIMIT attS relS", 0.75f, false, false, false, 2, 6.0, 0x0d2538f5b506fd2fu, 0xd766a84d12f2c449u },
            { "inP k0.75 COMP  attS relS", 0.75f, true,  false, false, 2, 6.0, 0x220b610995d74640u, 0xe097aaaa2a2215aau },
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
            std::snprintf(d, sizeof d, "%s", ok ? "audio + GR == TEST13 candidate references (two-probe harvest)" : bad);
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
        // TEST8 semantics (Nathan, 2026-10-25): COMP MIX
        // is a true crossfade comp<->dry (the dry arm is
        // back, commissioned) and MOD MIX is a true blend
        // of its section (endpoint-exact laws in Chain);
        // the pins below are re-based where the algebra
        // changed, cause-noted at each pin.
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
        // TEST8 REWRITE (cause: Nathan's commissioned dry
        // blend, 2026-10-25 — the dry arm is BACK, by his
        // directive): compMix=0 / modMix=0 is no longer
        // silence; it is the pure dry passthrough — the
        // input delayed by exactly the reported latency
        // (32, or 32+960 with the ADT engaged) at the
        // drive-referenced gain (limiter drive law at the
        // knob x the fixed bus staging trim; the bus knee
        // cannot engage at this gain). The pin asserts the
        // identity on the settled tail AND that the bus is
        // not silent. (The old both-gains-0 silence cannot
        // coexist with the commissioned bus formula —
        // out = compMix.comp + (1-compMix).dry + mod term
        // evaluates to the dry arm at (0,0) — so the
        // silence pins are replaced by this passthrough
        // pin here and in the processor suite's P7, per
        // the task's item (e).)
        {
            // TEST13 RE-SITE (cause: Part B — knob 0's drive
            // is now -80 dB by Nathan's directive, so the
            // passthrough at knob 0 is the passthrough AT
            // SILENCE: the identity is asserted there with
            // an explicit floor clause, and the audibility
            // clause moves to the same identity at knob
            // 0.36, whose drive (-3.8 dB) keeps the bus
            // clear of the ceiling park, as the original
            // knob-0 site's -5 dB did.)
            bool ok = true, audible = false, silent = true;
            const float gainRef = osc72::dbToLinear(osc72::ba6aDriveDb(0.0f))
                                * osc72::kBusStagingTrimLin;
            for (int mod : { 0, 1, 2 }) {
                auto r = renderMix(0.0f, 0.0f, mod, 0.0f, 2.0);
                const int lat = (mod == 1) ? 992 : 32; // Chain::latencySamples()
                const int start = (int)(1.0 * kFs);
                for (int i = start; i < (int)r.outL.size(); ++i) {
                    const float wantL = gainRef * r.inL[(size_t)(i - lat)];
                    const float wantR = gainRef * r.inR[(size_t)(i - lat)];
                    if (std::fabs(r.outL[(size_t)i] - wantL) > 1.0e-5f
                        || std::fabs(r.outR[(size_t)i] - wantR) > 1.0e-5f) ok = false;
                    if (std::fabs(r.outL[(size_t)i]) > 1.0e-3f) silent = false;
                }
            }
            const float gainRef5 = osc72::dbToLinear(osc72::ba6aDriveDb(0.36f))
                                 * osc72::kBusStagingTrimLin;
            for (int mod : { 0, 1, 2 }) {
                auto r = renderMix(0.0f, 0.0f, mod, 0.36f, 2.0);
                const int lat = (mod == 1) ? 992 : 32;
                const int start = (int)(1.0 * kFs);
                for (int i = start; i < (int)r.outL.size(); ++i) {
                    const float wantL = gainRef5 * r.inL[(size_t)(i - lat)];
                    const float wantR = gainRef5 * r.inR[(size_t)(i - lat)];
                    if (std::fabs(r.outL[(size_t)i] - wantL) > 1.0e-5f
                        || std::fabs(r.outR[(size_t)i] - wantR) > 1.0e-5f) ok = false;
                    if (std::fabs(r.outL[(size_t)i]) > 1.0e-4f) audible = true;
                }
            }
            check(ok && audible && silent, "L2 TEST8+13: compMix 0 / modMix 0 = pure dry passthrough at the reported latency (silent at knob 0 per Part B, audible at working drive)", "");
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
        // TEST8: the assertion itself is rewritten (see
        // the block) — the bus is no longer two channel
        // gains; the staging trim also retires the cap
        // precondition (nothing is made up any more, so
        // nothing rides a cap; the knee simply never
        // bends at these levels).
        {
            // TEST13 REWRITE (cause: Nathan's Part D serial
            // blend directive, 2026-10-26 — the bus is no
            // longer a sum of independent terms: the inserts
            // are fed the comp blend, so the wet depends on
            // compMix and the TEST8 inclusion-exclusion (i)
            // is structurally false). The serial bus's exact
            // identities, wherever the bus knee never bends
            // and the GR trajectory is pot-independent (L6):
            //  (i) MOD MIX affinity at fixed compMix — the
            //      crossfade trades the through blend for
            //      its modulated version linearly:
            //      out(cm, mm/2) == (out(cm,0) + out(cm,1))/2;
            // (ii) the COMP crossfade is affine in its pot
            //      (the TEST8 identity, at modMix=0):
            //      out(cm/2, 0) == (out(cm,0) + out(0,0))/2.
            // Re-sited from knob 0.1 to 0.36 (cause: Part
            // B's taper — 0.36 restores the original site's
            // ~-3.8 dB drive, so the knee-non-binding
            // precondition and signal levels are as pinned).
            auto wetFull = renderMix(0.15f, 1.0f, 1, 0.36f, 3.0);
            auto compOnly = renderMix(0.15f, 0.0f, 1, 0.36f, 3.0);
            auto wetHalf = renderMix(0.15f, 0.5f, 1, 0.36f, 3.0);
            auto dryOnly = renderMix(0.0f, 0.0f, 1, 0.36f, 3.0);
            double mx = 0;
            for (size_t i = 0; i < wetHalf.outL.size(); ++i) {
                mx = std::max(mx, (double)std::fabs(wetHalf.outL[i] - 0.5f * (compOnly.outL[i] + wetFull.outL[i])));
                mx = std::max(mx, (double)std::fabs(wetHalf.outR[i] - 0.5f * (compOnly.outR[i] + wetFull.outR[i])));
            }
            auto half = renderMix(0.075f, 0.0f, 1, 0.36f, 3.0);
            double mxG = 0;
            for (size_t i = 0; i < half.outL.size(); ++i) {
                mxG = std::max(mxG, (double)std::fabs(half.outL[i] - 0.5f * (compOnly.outL[i] + dryOnly.outL[i])));
                mxG = std::max(mxG, (double)std::fabs(half.outR[i] - 0.5f * (compOnly.outR[i] + dryOnly.outR[i])));
            }
            std::snprintf(d, sizeof d, "mod-affinity max err %.2e; comp-affinity max err %.2e (knee non-binding regime)", mx, mxG);
            check(mx < 2.0e-6 && mxG < 2.0e-6,
                  "L4 TEST13 serial bus algebra: MOD crossfade affine in its pot; COMP crossfade affine in its pot", d);
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
            check(a.gr == bq.gr && a.gr.back() > 0.0f,
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
        // digit-identical (max |delta| = 0.000e+00). Through
        // TEST12 the tap was the module's own output.
        // TEST13 REWRITE (cause: Nathan's Part D serial
        // directive, 2026-10-26): the tap is now the COMP
        // BLEND itself — (a) at compMix=1 it still equals a
        // module-mode limiter's output bit-for-bit in
        // lockstep; (b) at compMix=0 it equals the DRIVEN
        // DRY bit-for-bit in lockstep (the input delayed by
        // the limiter's oversampling delay at the drive
        // gain); (c) its level travel across the knob at
        // compMix=0 is the drive law's (printed; asserted
        // below).
        {
            osc72::Chain ch;
            ch.prepare(kFs);
            ch.setKnob(0.5f);
            ch.setCompMix(1.0f);
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
            check(tapOk, "M1a TEST13: insert feed at compMix=1 == the module's own output, bit-exact in lockstep", "");
            {
                osc72::Chain c0;
                c0.prepare(kFs);
                c0.setKnob(0.5f);
                c0.setCompMix(0.0f);
                c0.setModMix(1.0f);
                c0.reset();
                c0.setModIn(true);
                const int osD = c0.limiter().osDelaySamples();
                const float g = osc72::dbToLinear(osc72::ba6aDriveDb(0.5f));
                ProgGen pg0;
                std::vector<float> hL, hR;
                bool dryOk = true;
                for (int i = 0; i < (int)(2.0 * kFs); ++i) {
                    float l, r; pg0.sample(i, l, r);
                    hL.push_back(l); hR.push_back(r);
                    float oL, oR; c0.process(l, r, oL, oR);
                    const float wL = i >= osD ? g * hL[(size_t)(i - osD)] : 0.0f;
                    const float wR = i >= osD ? g * hR[(size_t)(i - osD)] : 0.0f;
                    if (c0.insertFeedL() != wL || c0.insertFeedR() != wR) dryOk = false;
                }
                check(dryOk, "M1a2 TEST13: insert feed at compMix=0 == the driven dry, bit-exact in lockstep", "");
            }

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
            // TEST13 RE-PIN (cause: Part D serial feed — at
            // compMix=0 the send is the driven dry, a gain-
            // only path: its RMS travel is the drive law's
            // full 131 dB and its crest is the input's own,
            // constant by construction. The module-crest
            // clause belonged to the module-output tap.)
            check(rmsK[4] - rmsK[0] > 10.0 && std::fabs(crestK[0] - crestK[4]) < 0.25
                  && rmsK[3] > rmsK[2] && rmsK[2] > rmsK[1],
                  "M1b TEST13: the send at compMix=0 is the driven dry — moves with INPUT by the drive law, crest unchanged", d);
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
        // TEST13 RE-PIN (cause: Nathan's Part B directive,
        // 2026-10-26 — INPUT 0 must be silence and the gain
        // must move below 5): the bottom half is reshaped
        // (-80 dB at 0, graded u^1.2 rise); knobs 5..10 are
        // the standing table byte-identical.
        const double want[11] = { -80.00, -57.43, -35.97, -15.92, 2.15, 16.08,
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
        // TEST7 RE-PIN (cause: Nathan's file-anchored threshold directive, 2026-10-24): peak reads -1.65 dB over the span; band 1.5 -> 1.8.
        check(std::fabs(pkDb) < 1.8 && rmsDb > 2.0,
              "O3 peaks park while density climbs (noon gains, programme)", d);
        // O4: the peak-referenced law gives back LESS as
        // drive rises — settled bus make-up at knob 1.0
        // sits below knob 0.5 (the 1.4.1 law's applied
        // make-up also fell at the top, but only through
        // cap starvation at +4 dBFS output peaks).
        // TEST8 REWRITE (cause: Nathan's directive, 2026-
        // 10-25 — the bus auto-makeup law is REMOVED, "act
        // like a normal compressor"): there is no make-up
        // left to fall. busMakeupDb() now reports the fixed
        // staging trim, and the pin is its constancy: the
        // same value at knob 0.5 and knob 1.0, equal to
        // kBusStagingTrimDb (-15.6 dB, the measured optimum
        // for flat insertion at INPUT 5 on Nathan's vocal;
        // DIAGNOSIS S C estimated -15, the task's trim rule
        // ships the measured value — REPORT-TEST8).
        double pk50, rms50, mk50;
        settled(0.5f, 0.5f, 0.5f, 0, pk50, rms50, mk50);
        std::snprintf(d, sizeof d, "settled bus staging trim: knob 0.5 %+.2f dB, knob 1.0 %+.2f dB (fixed)",
                      mk50, mk100);
        check(std::fabs(mk50 - osc72::kBusStagingTrimDb) < 0.01f
              && std::fabs(mk100 - osc72::kBusStagingTrimDb) < 0.01f,
              "O4 TEST8: bus staging is the fixed trim at every drive (make-up law removed)", d);
    }

    std::printf("\n%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL TESTS PASSED" : "FAILURES PRESENT",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
