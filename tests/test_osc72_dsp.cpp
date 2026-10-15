// tests/test_osc72_dsp.cpp — framework-free DSP tests for OSC-72.
// Compile: g++ -std=c++17 -O2 -o build/test_osc72 tests/test_osc72_dsp.cpp dsp/Osc72Dsp.cpp
#include "../dsp/Osc72Dsp.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>

static int failures = 0;
static int checks = 0;
static void check(bool ok, const char* name, const char* detail = "") {
    ++checks;
    std::printf("%s %-46s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) ++failures;
}

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

// Run a stereo sine for `seconds`, discard the first half as settle,
// return RMS/peak per channel and mean GR over the measured half.
struct Meas { float rmsL, rmsR, peakL, peakR, meanGr; };
static Meas runStereo(float knob, float ampL, float ampR, float freq,
                      double seconds = 2.0) {
    osc72::StereoLimiter lim;
    lim.prepare(kFs);
    lim.setKnob(knob);
    const int total = (int)(kFs * seconds);
    double sL = 0, sR = 0, sGr = 0; int cnt = 0;
    float pL = 0, pR = 0;
    for (int i = 0; i < total; ++i) {
        float inL = ampL * (float)std::sin(2.0 * kPi * freq * i / kFs);
        float inR = ampR * (float)std::sin(2.0 * kPi * freq * i / kFs);
        float oL, oR;
        lim.process(inL, inR, oL, oR);
        if (i >= total / 2) {
            sL += (double)oL * oL; sR += (double)oR * oR;
            sGr += lim.gainReductionDb(); ++cnt;
            if (std::fabs(oL) > pL) pL = std::fabs(oL);
            if (std::fabs(oR) > pR) pR = std::fabs(oR);
        }
    }
    Meas m;
    m.rmsL = (float)std::sqrt(sL / cnt); m.rmsR = (float)std::sqrt(sR / cnt);
    m.peakL = pL; m.peakR = pR; m.meanGr = (float)(sGr / cnt);
    return m;
}

// Burst + decaying-tail signal for the transient-designer tests
// (v9): a 120 ms noise burst (the "hit") followed by an exponential
// reverb-like tail (tau 0.35 s). Deterministic LCG noise, mono fed
// to both channels. Returns output tail energy, burst peak, the
// deepest TD cut while the burst is sounding (attack preservation)
// and the mean TD cut across the tail window.
struct TdMeas { double tailEnergy; float burstPeak; float maxAttackCutDb; float meanTailCutDb; };
static TdMeas runBurstTail(float knob) {
    osc72::StereoLimiter lim;
    lim.prepare(kFs);
    lim.setKnob(knob);
    lim.reset(); // snap knob (setKnob + reset settles instantly)
    unsigned st = 22222u;
    auto noise = [&]() {
        st = st * 1664525u + 1013904223u;
        return (float)((st >> 9) & 0x7FFF) / 16384.0f - 1.0f;
    };
    const int total = (int)(1.6 * kFs);
    const int burstEnd = (int)(0.12 * kFs);
    double tailSum = 0; int tailN = 0;
    double cutSum = 0; int cutN = 0;
    float peak = 0, maxCut = 0;
    for (int i = 0; i < total; ++i) {
        const double t = i / kFs;
        float s;
        if (i < burstEnd) {
            float edge = 1.0f; // 5 ms raised-cosine edges on the burst
            if (i < 240) edge = 0.5f - 0.5f * (float)std::cos(kPi * i / 240.0);
            if (i > burstEnd - 240)
                edge = 0.5f - 0.5f * (float)std::cos(kPi * (burstEnd - i) / 240.0);
            s = 0.85f * noise() * edge;
        } else {
            s = 0.55f * noise() * (float)std::exp(-(t - 0.12) / 0.35);
        }
        float oL, oR;
        lim.process(s, s, oL, oR);
        if (t < 0.12) {
            if (std::fabs(oL) > peak) peak = std::fabs(oL);
            if (lim.tdCutDb() > maxCut) maxCut = lim.tdCutDb();
        }
        if (t >= 0.55 && t < 1.55) {
            tailSum += (double)oL * oL; ++tailN;
            cutSum += lim.tdCutDb(); ++cutN;
        }
    }
    TdMeas m;
    m.tailEnergy = tailSum / tailN;
    m.burstPeak = peak;
    m.maxAttackCutDb = maxCut;
    m.meanTailCutDb = (float)(cutSum / cutN);
    return m;
}

// ── v2 ballistics helpers ───────────────────────────────────────
// Step response of the GR itself: settle in silence, step to the
// standard loud sine (settled GR = mean of the last 200 ms), then
// drop to silence. t63/t90 in ms on the way in (attack) and out
// (release). Release is measured on a full drop: that is the
// pumping-relevant case (a hit ends, the gain must come back).
struct StepMeas { float settledGr; float atk63, atk90, rel63, rel90; };
static StepMeas runStep(float knob) {
    osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(knob); lim.reset();
    StepMeas r{}; r.atk63 = r.atk90 = r.rel63 = r.rel90 = -1.0f;
    std::vector<float> gr;
    auto run = [&](int n, float amp, bool collect) {
        for (int i = 0; i < n; ++i) {
            float in = amp * (float)std::sin(2.0 * kPi * 200.0 * i / kFs);
            float a, c; lim.process(in, in, a, c);
            if (collect) gr.push_back(lim.gainReductionDb());
        }
    };
    run((int)(0.3 * kFs), 0.0f, false);
    const int nLoud = (int)(1.5 * kFs);
    run(nLoud, 0.8f, true);
    double s = 0; int cnt = 0;
    for (int i = nLoud - (int)(0.2 * kFs); i < nLoud; ++i) { s += gr[i]; ++cnt; }
    r.settledGr = (float)(s / cnt);
    for (int i = 0; i < nLoud; ++i) {
        if (r.atk63 < 0 && gr[i] >= 0.632f * r.settledGr) r.atk63 = (float)(i / kFs * 1000.0);
        if (r.atk90 < 0 && gr[i] >= 0.90f * r.settledGr)  r.atk90 = (float)(i / kFs * 1000.0);
    }
    const int base = (int)gr.size();
    run((int)(1.5 * kFs), 0.0f, true);
    for (int i = base; i < (int)gr.size(); ++i) {
        const float frac = gr[i] / r.settledGr;
        if (r.rel63 < 0 && frac <= 0.368f) r.rel63 = (float)((i - base) / kFs * 1000.0);
        if (r.rel90 < 0 && frac <= 0.10f)  r.rel90 = (float)((i - base) / kFs * 1000.0);
    }
    return r;
}

// Pumping proof: 120 ms noise bursts every 500 ms. Per burst
// (skipping the cold first one): GR depth at the burst's end,
// residual GR at the gap's end, recovery = depth - residual.
// Also the largest output peak across a burst's whole period.
struct PumpMeas { float depth, residual, recovery, onsetPeak; };
static PumpMeas runPump(float knob) {
    osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(knob); lim.reset();
    unsigned st = 777u;
    auto noise = [&]() {
        st = st * 1664525u + 1013904223u;
        return (float)((st >> 9) & 0x7FFF) / 16384.0f - 1.0f;
    };
    const int period = (int)(0.5 * kFs), burst = (int)(0.12 * kFs), nBursts = 8;
    const int win = (int)(0.02 * kFs);
    std::vector<float> grH, outH;
    for (int i = 0; i < period * nBursts; ++i) {
        const int ph = i % period;
        float in = 0.0f;
        if (ph < burst) {
            float edge = 1.0f;
            if (ph < 240) edge = 0.5f - 0.5f * (float)std::cos(kPi * ph / 240.0);
            if (ph > burst - 240) edge = 0.5f - 0.5f * (float)std::cos(kPi * (burst - ph) / 240.0);
            in = 0.85f * noise() * edge;
        }
        float a, c; lim.process(in, in, a, c);
        grH.push_back(lim.gainReductionDb());
        outH.push_back(a);
    }
    PumpMeas p{}; p.onsetPeak = 0;
    double dSum = 0, rSum = 0; int used = 0;
    for (int bn = 1; bn < nBursts; ++bn) {
        const int b0 = bn * period;
        double d = 0, r = 0;
        for (int j = burst - win; j < burst; ++j) d += grH[b0 + j];
        for (int j = period - win; j < period; ++j) r += grH[b0 + j];
        dSum += d / win; rSum += r / win; ++used;
        for (int j = 0; j < period; ++j)
            if (std::fabs(outH[b0 + j]) > p.onsetPeak) p.onsetPeak = std::fabs(outH[b0 + j]);
    }
    p.depth = (float)(dSum / used);
    p.residual = (float)(rSum / used);
    p.recovery = p.depth - p.residual;
    return p;
}

// Overshoot probe: pre-charge the limiter with 1 s of loud sine
// (GR and makeup fully settled), drop to silence for `gapMs`,
// then land a single full-scale hit and return its peak over
// 100 ms. This is the slow-attack danger case: fast release has
// dropped the GR while the slow makeup is still tens of dB up.
static float runOvershoot(float knob, double gapMs, bool precharge) {
    osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(knob); lim.reset();
    auto feed = [&](double seconds, float amp) {
        const int n = (int)(seconds * kFs);
        for (int i = 0; i < n; ++i) {
            float in = amp * (float)std::sin(2.0 * kPi * 200.0 * i / kFs);
            float a, c; lim.process(in, in, a, c);
        }
    };
    if (precharge) feed(1.0, 0.9f);
    feed(gapMs / 1000.0, 0.0f);
    float pk = 0;
    const int n = (int)(0.1 * kFs);
    for (int i = 0; i < n; ++i) {
        float in = 0.99f * (float)std::sin(2.0 * kPi * 200.0 * i / kFs);
        float a, c; lim.process(in, in, a, c);
        if (std::fabs(a) > pk) pk = std::fabs(a);
    }
    return pk;
}

// ── Chain helpers ─────────────────────────────────────────────────
// The plugin's actual routing now lives in osc72::Chain (limiter
// with 'limit' in/out -> warble insert with 'mod' in/out); tests
// drive it directly, so what's measured here is the shipped code.

// Goertzel single-bin magnitude (relative) of x[0..n) at freq.
static double goertzel(const std::vector<float>& x, double freq, double fs) {
    const double w = 2.0 * kPi * freq / fs;
    const double cw = std::cos(w), coeff = 2.0 * cw;
    double s0 = 0, s1 = 0, s2 = 0;
    for (float v : x) { s0 = (double)v + coeff * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2) / (double)x.size();
}

int main() {
    char b[128];

    // ── 1. Unity / transparency at knob 0 ──────────────────────────
    {
        Meas m = runStereo(0.0f, 0.5f, 0.5f, 1000.0f);
        const float inRms = 0.5f / (float)std::sqrt(2.0);
        std::snprintf(b, sizeof b, "rms=%.5f (in %.5f) peak=%.5f", m.rmsL, inRms, m.peakL);
        check(std::fabs(m.rmsL - inRms) < 0.001f, "knob0 unity RMS", b);
        check(std::fabs(m.peakL - 0.5f) < 0.001f, "knob0 unity peak", b);
        check(m.meanGr < 0.01f, "knob0 no gain reduction", b);
    }

    // ── 2. Monotonic GR increase with knob ─────────────────────────
    {
        float prev = -1.0f; bool mono = true; float grs[4];
        const float ks[4] = { 0.3f, 0.55f, 0.8f, 1.0f };
        for (int i = 0; i < 4; ++i) {
            Meas m = runStereo(ks[i], 0.8f, 0.8f, 200.0f);
            grs[i] = m.meanGr;
            if (!(m.meanGr > prev)) mono = false;
            prev = m.meanGr;
        }
        std::snprintf(b, sizeof b, "GR %.2f / %.2f / %.2f / %.2f dB",
                      grs[0], grs[1], grs[2], grs[3]);
        check(mono && grs[3] > 10.0f, "GR monotonic with knob, violent at 10", b);
    }

    // ── 3. Peak control under heavy drive ───────────────────────────
    {
        Meas m = runStereo(1.0f, 0.99f, 0.99f, 200.0f);
        std::snprintf(b, sizeof b, "peak out=%.3f (in 0.99), meanGR=%.1f dB", m.peakL, m.meanGr);
        // v3 re-pin (was < 0.85): with the auto makeup CRANKING
        // (Nathan's ask), loud steady material at max sits ON the
        // overshoot ceiling by design — the wet path runs at the
        // saturator's clean ceiling (sat(1.0) = 0.909) instead of
        // under the old v1/v2 makeup regime. The pin is now the
        // ceiling itself: never above ~0.95, still clearly hot.
        check(m.peakL < 0.95f, "knob10 peak controlled at the ceiling, no blowout", b);
        check(m.peakL > 0.2f, "knob10 still hot (makeup working)", b);
    }

    // ── 4. No NaN / denormal blowups; silence tail decays ──────────
    {
        osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(1.0f);
        bool finite = true;
        float oL = 0, oR = 0;
        for (int i = 0; i < 48000; ++i) { // 1 s of slammed full-scale square-ish drive
            float s = (i % 97 < 48) ? 1.0f : -1.0f;
            lim.process(s, -s, oL, oR);
            if (!std::isfinite(oL) || !std::isfinite(oR)) finite = false;
        }
        double tailMax = 0.0;
        for (int i = 0; i < 96000; ++i) { // 2 s silence
            lim.process(0.0f, 0.0f, oL, oR);
            if (!std::isfinite(oL) || !std::isfinite(oR)) finite = false;
            if (i > 48000 && std::fabs(oL) > tailMax) tailMax = std::fabs(oL);
        }
        std::snprintf(b, sizeof b, "tail after 1s silence=%.2e", tailMax);
        check(finite, "all outputs finite under slam + silence", b);
        check(tailMax < 1.0e-6, "silence tail decays to zero", b);
    }

    // ── 5. Stereo-linked GR: same gain on both channels ─────────────
    {
        // Loud L, quiet R (-40 dB), same frequency. With linked GR the
        // quiet channel must receive the SAME gain as the loud one, so
        // out/in RMS ratio matches across channels.
        Meas m = runStereo(0.85f, 0.9f, 0.009f, 300.0f);
        const float ratioL = m.rmsL / (0.9f / (float)std::sqrt(2.0));
        const float ratioR = m.rmsR / (0.009f / (float)std::sqrt(2.0));
        std::snprintf(b, sizeof b, "gain L=%.4f R=%.4f (diff %.2f%%)",
                      ratioL, ratioR, 100.0 * std::fabs(ratioL - ratioR) / ratioL);
        // The GR itself is a single linked state (one gainLin is
        // applied to both channels — see StereoLimiter::process);
        // what differs per channel is the per-channel transformer
        // colour around it (the loud channel's core is driven
        // harder, so its harmonic RMS share differs). That spread
        // was 0.40% at v1 levels, 2.19% at v9 depth, ~5% at v2 —
        // and grows again in v3 because the cranked makeup runs
        // the wet path ~8 dB hotter into the same colour stages
        // (measured 8.4-11.0% across the v3 tuning rounds). The
        // pin allows the colour spread while still catching any
        // real linkage break (which would read as tens of %).
        check(std::fabs(ratioL - ratioR) / ratioL < 0.12f, "linked GR: equal gain L/R", b);
    }

    // ── 6. Mix law (v9): the knob IS the mix ────────────────────────
    // Wet weight 0 / 0.5 / 1.0 at knob 0 / 0.5 (CENTRE — the
    // Raspberries endpoint moved here from 0.7) / 1.0. Supersedes
    // the pre-v9 law (0.5 @ 0.7, 0.75 @ 1.0) per Nathan, 2026-10-14.
    {
        auto wAt = [](float k) {
            osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(k);
            float a, c; // run a second so the knob smoothing settles
            for (int i = 0; i < 48000; ++i) lim.process(0.1f, 0.1f, a, c);
            return lim.wetWeight();
        };
        float w0 = wAt(0.0f), w5 = wAt(0.5f), w10 = wAt(1.0f);
        std::snprintf(b, sizeof b, "wet w: 0->%.3f  0.5->%.3f  1.0->%.3f", w0, w5, w10);
        check(w0 == 0.0f, "mix v9: wet weight 0 at knob 0", b);
        check(std::fabs(w5 - 0.5f) < 0.01f, "mix v9: 50/50 at CENTRE (0.5)", b);
        check(std::fabs(w10 - 1.0f) < 0.01f, "mix v9: 100% wet at knob 1", b);
    }

    // ── 7. CPU sanity: 10 s of stereo audio ─────────────────────────
    {
        osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(0.8f);
        const int n = 480000;
        float acc = 0.0f;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n; ++i) {
            float s = 0.7f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
            float a, c; lim.process(s, s * 0.8f, a, c); acc += a + c;
        }
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::snprintf(b, sizeof b, "%.1f ms for 10 s audio (%.2f%% of realtime), acc=%.2f",
                      ms, ms / 100.0, acc);
        check(ms < 1000.0, "CPU: <10% realtime for stereo core", b);
    }

    // ── 8. Warble OFF: chain routing is exactly limiter -> output ───
    // transformer (v9). Pre-v9 this checked bit-equality against the
    // limiter alone; the Lundahl OUTPUT transformer now legitimately
    // sits after the limiter in the engaged path (Nathan, 2026-10-14),
    // so the check composes the expected path explicitly and demands
    // bit-equality with THAT — any extra colour, gain or routing
    // drift in the chain still fails. Mod off must add nothing.
    {
        osc72::StereoLimiter limA;
        osc72::Chain chain;
        osc72::LundahlStage xoL(osc72::LundahlStage::Voicing::output);
        osc72::LundahlStage xoR(osc72::LundahlStage::Voicing::output);
        limA.prepare(kFs); chain.prepare(kFs);
        xoL.prepare(kFs); xoR.prepare(kFs);
        limA.setKnob(0.8f); chain.setKnob(0.8f);
        // Chain: limit IN (default), mod never enabled. At knob 0.8
        // the output-transformer depth is 1 (full colour).
        bool identical = true;
        for (int i = 0; i < 96000; ++i) {
            float s = 0.7f * (float)std::sin(2.0 * kPi * 330.0 * i / kFs)
                    + 0.2f * (float)std::sin(2.0 * kPi * 1244.0 * i / kFs);
            float aL, aR, oL, oR;
            limA.process(s, s * 0.6f, aL, aR);
            chain.process(s, s * 0.6f, oL, oR);
            // Depth follows the (identically evolving) limiter's
            // colour depth — 1.0 once the knob ride settles at 0.8.
            const float xd = limA.xfmrDepth();
            const float eL = xoL.process(aL, xd);
            const float eR = xoR.process(aR, xd);
            if (oL != eL || oR != eR) identical = false; // exact float equality
        }
        std::snprintf(b, sizeof b, "active=%d latency=%d",
                      (int)chain.warble().active(), chain.warble().latencySamples());
        check(identical && !chain.warble().active(),
              "warble OFF: chain == limiter -> output transformer, bit-exact", b);
        check(chain.warble().latencySamples() == 576, "warble latency figure = 12 ms @48k", b);

        // And at knob 0 the whole engaged chain (limit in, mod out)
        // is bit-transparent end to end: wet weight 0, TD gain 1,
        // both transformer depths 0 (v9 knob-0 guarantee).
        osc72::Chain chain0;
        chain0.prepare(kFs); chain0.setKnob(0.0f);
        bool exact0 = true;
        for (int i = 0; i < 48000; ++i) {
            float s = 0.7f * (float)std::sin(2.0 * kPi * 330.0 * i / kFs);
            float oL, oR;
            chain0.process(s, s * 0.6f, oL, oR);
            if (oL != s || oR != s * 0.6f) exact0 = false;
        }
        check(exact0, "v9: knob 0 chain (limit in, mod out) bit-exact dry", "");
    }

    // ── 9. Warble ON: pitch is modulated, mean pitch preserved ──────
    // v10: the warble lives on the SIDE path, so the character
    // checks feed pure side (antiphase L = s, R = −s → mid ≡ 0) and
    // measure the warbled side at outL (outR is its mirror).
    {
        osc72::WarbleChain wb; wb.prepare(kFs); wb.setEnabled(true);
        const float freq = 440.0f;
        double prevCross = -1.0; double minP = 1e9, maxP = 0, sumP = 0; int nP = 0;
        float prev = 0.0f;
        double sumSq = 0; int nAll = 0;
        for (int i = 0; i < (int)(4.0 * kFs); ++i) {
            float s = 0.5f * (float)std::sin(2.0 * kPi * freq * i / kFs);
            float oL, oR; wb.process(s, -s, oL, oR);
            if (i > (int)(0.5 * kFs)) { // after fade + line fill
                if (prev <= 0.0f && oL > 0.0f) {
                    double cross = i - 1 + (0.0 - prev) / (oL - prev);
                    if (prevCross > 0.0) {
                        double period = cross - prevCross;
                        if (period > 40 && period < 200) { // ~240..1200 Hz sanity band
                            if (period < minP) minP = period;
                            if (period > maxP) maxP = period;
                            sumP += period; ++nP;
                        }
                    }
                    prevCross = cross;
                }
                sumSq += (double)oL * oL; ++nAll;
            }
            prev = oL;
        }
        const double meanFreq = kFs / (sumP / nP);
        const double devPct = (maxP - minP) / (sumP / nP) * 100.0;
        std::snprintf(b, sizeof b, "mean f=%.1f Hz, period swing=%.1f%% over %d cycles",
                      meanFreq, devPct, nP);
        // v2: depth multiplied several-fold on Nathan's verdict
        // ("almost inaudible" at v1's 5.2% p-p). Pin the new
        // region: measured 19.2% p-p.
        check(devPct > 10.0, "warble ON: pitch measurably modulated", b);
        check(std::fabs(meanFreq - freq) / freq < 0.01, "warble ON: mean pitch preserved", b);
        check(sumSq / nAll > 0.05, "warble ON: signal present at healthy level", b);
    }

    // ── 10. Warble ON: bandwidth limited to ~2.6 kHz ────────────────
    // v10: measured on the side path (antiphase feed), as group 9.
    {
        auto binLevel = [](float freq, bool on) {
            osc72::WarbleChain wb; wb.prepare(kFs);
            if (on) wb.setEnabled(true);
            std::vector<float> out;
            for (int i = 0; i < (int)(1.5 * kFs); ++i) {
                float s = 0.5f * (float)std::sin(2.0 * kPi * freq * i / kFs);
                float oL, oR; wb.process(s, -s, oL, oR);
                if (i >= (int)(0.5 * kFs)) out.push_back(oL);
            }
            return goertzel(out, freq, kFs);
        };
        const double lo = binLevel(500.0f, true);
        const double hi = binLevel(5000.0f, true);
        const double hiOff = binLevel(5000.0f, false);
        const double attDb = 20.0 * std::log10(hi / lo);
        const double offDb = 20.0 * std::log10(hiOff / lo);
        std::snprintf(b, sizeof b, "5 kHz vs 500 Hz: on=%.1f dB, off=%.1f dB", attDb, offDb);
        check(attDb < -18.0, "warble ON: 5 kHz component crushed (>18 dB down)", b);
        check(offDb > -1.0, "warble OFF: 5 kHz passes ~unity", b);
    }

    // ── 11. Warble safety: slam, silence, delay bounds ──────────────
    {
        osc72::WarbleChain wb; wb.prepare(kFs); wb.setEnabled(true);
        bool finite = true; float dMin = 1e9f, dMax = 0.0f;
        float oL = 0, oR = 0;
        for (int i = 0; i < 48000; ++i) { // full-scale square slam
            float s = (i % 97 < 48) ? 1.0f : -1.0f;
            wb.process(s, -s, oL, oR);
            if (!std::isfinite(oL) || !std::isfinite(oR)) finite = false;
            if (wb.currentDelayMs() < dMin) dMin = wb.currentDelayMs();
            if (wb.currentDelayMs() > dMax) dMax = wb.currentDelayMs();
        }
        double tail = 0.0;
        for (int i = 0; i < 5 * 48000; ++i) { // 5 s: bounds over all mod phases
            wb.process(0.0f, 0.0f, oL, oR);
            if (!std::isfinite(oL) || !std::isfinite(oR)) finite = false;
            if (wb.currentDelayMs() < dMin) dMin = wb.currentDelayMs();
            if (wb.currentDelayMs() > dMax) dMax = wb.currentDelayMs();
            if (i > 4 * 48000 && std::fabs(oL) > tail) tail = std::fabs(oL);
        }
        std::snprintf(b, sizeof b, "delay range %.2f..%.2f ms, tail=%.2e", dMin, dMax, tail);
        check(finite, "warble: finite under slam + long silence", b);
        // v2: bounds pinned to the named safety constants (the
        // floor dropped 4.0 -> 2.0 ms with the deeper modulation).
        check(dMin >= osc72::kWarbleMinDelayMs && dMax <= osc72::kWarbleMaxDelayMs,
              "warble: delay always in safe band", b);
        check(tail < 1.0e-5, "warble: silence tail drains", b);
    }

    // ── 12. Warble toggling: no clicks (crossfade works) ────────────
    // v10: fed antiphase (pure side) so the warble path is genuinely
    // live across the toggles — a mono feed would pass bit-exactly
    // regardless of the fade and prove nothing about clicks.
    {
        osc72::WarbleChain wb; wb.prepare(kFs);
        float maxStep = 0.0f; float prevOut = 0.0f;
        auto feed = [&](int n, float freq, float amp) {
            for (int i = 0; i < n; ++i) {
                float s = amp * (float)std::sin(2.0 * kPi * freq * i / kFs);
                float oL, oR; wb.process(s, -s, oL, oR);
                float st = std::fabs(oL - prevOut); if (st > maxStep) maxStep = st;
                prevOut = oL;
            }
        };
        feed(24000, 440.0f, 0.3f);   // settle off
        wb.setEnabled(true);
        feed(24000, 440.0f, 0.3f);   // engage mid-tone
        wb.setEnabled(false);
        feed(24000, 440.0f, 0.3f);   // disengage mid-tone
        std::snprintf(b, sizeof b, "max sample step across toggles=%.4f", maxStep);
        check(maxStep < 0.05f, "warble: engage/disengage click-free", b);
    }

    // ── 13. Warble is post-limiter: GR history untouched ────────────
    {
        auto meanGr = [](bool warbleOn) {
            osc72::Chain chain;
            chain.prepare(kFs); chain.setKnob(0.85f);
            if (warbleOn) chain.setModIn(true);
            double s = 0; int n = 0;
            for (int i = 0; i < 96000; ++i) {
                float in = 0.9f * (float)std::sin(2.0 * kPi * 220.0 * i / kFs);
                float oL, oR; chain.process(in, in, oL, oR);
                if (i >= 48000) { s += chain.limiter().gainReductionDb(); ++n; }
            }
            return (float)(s / n);
        };
        const float gOff = meanGr(false), gOn = meanGr(true);
        std::snprintf(b, sizeof b, "mean GR off=%.4f on=%.4f dB", gOff, gOn);
        check(std::fabs(gOn - gOff) < 1.0e-4f, "warble ON: limiter GR identical (post-insert)", b);
    }

    // ── 14. Limit OUT (+ mod OUT): bit-exact dry, GR reads 0 ───────
    {
        osc72::Chain chain;
        chain.prepare(kFs); chain.setKnob(0.8f);
        chain.setLimitIn(false); // mod stays out
        bool identical = true;
        float oL = 0, oR = 0;
        for (int i = 0; i < 96000; ++i) { // 2 s loud stereo material
            float sL = 0.9f * (float)std::sin(2.0 * kPi * 220.0 * i / kFs);
            float sR = 0.9f * (float)std::sin(2.0 * kPi * 331.0 * i / kFs);
            chain.process(sL, sR, oL, oR);
            // Exact from the fade's end (30 ms); check the tail half.
            if (i >= 4800 && (oL != sL || oR != sR)) identical = false;
        }
        // The limiter itself must be hard at work underneath (so the
        // bypass is meaningful) while the eye readout stays at 0.
        std::snprintf(b, sizeof b, "limiter GR=%.2f dB, eye GR=%.4f dB, latency=%d",
                      chain.limiter().gainReductionDb(), chain.gainReductionDb(),
                      chain.latencySamples());
        check(identical, "limit OUT + mod OUT: bit-exact dry input", b);
        check(chain.limiter().gainReductionDb() > 5.0f
                  && chain.gainReductionDb() == 0.0f,
              "limit OUT: eye GR = 0 while limiter still computes GR", b);
        check(chain.latencySamples() == 0, "limit OUT + mod OUT: zero latency", b);

        // Toggling limit mid-tone must be click-free (45 ms fade).
        // v3 measurement note: the raw max-step metric this check
        // used to pin (< 0.05 across the whole feed) conflates the
        // PROCESSED WAVEFORM's own slope with toggle clicks — at
        // knob 0.85 with v3's cranked makeup the steady limited
        // tone's own max sample step is ~0.070 with NO toggling at
        // all (measured directly, settle phase). The honest check
        // is relative: the steps inside the fade windows may not
        // exceed the no-toggle baseline by more than a small
        // margin (a click is a step ABOVE the waveform's slope).
        osc72::Chain chain2;
        chain2.prepare(kFs); chain2.setKnob(0.85f);
        float prevOut = 0.0f;
        auto feed2 = [&](int n, float& mx) {
            for (int i = 0; i < n; ++i) {
                float s = 0.5f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
                float a, c; chain2.process(s, s, a, c);
                float st = std::fabs(a - prevOut); if (st > mx) mx = st;
                prevOut = a;
            }
        };
        float stepSettle = 0.0f, stepFade = 0.0f;
        feed2(24000, stepSettle);  // settle, limit in (baseline)
        chain2.setLimitIn(false);
        feed2(24000, stepFade);    // fade out mid-tone
        chain2.setLimitIn(true);
        feed2(24000, stepFade);    // fade back in mid-tone
        std::snprintf(b, sizeof b, "max step: settle(baseline)=%.4f fade windows=%.4f",
                      stepSettle, stepFade);
        check(stepFade < stepSettle + 0.02f, "limit: engage/disengage click-free", b);
    }

    // ── 15. Mod: hard L/R warble (v10 mid/side; v3 mid tap) ────────
    // v10 (Nathan, 2026-10-14): the insert warbles the SIDE only;
    // the MID passed dry — and on the shipped 1.0.2 Nathan heard
    // NO effect whatsoever, because on mono/centre-anchored
    // material there was literally nothing to hear (mono through
    // MOD was bit-exact BY DESIGN). v3 supersedes the "mid passes
    // dry" guarantee: the mid now rides the SAME transport as a
    // 0.4-depth blend (see the topology note in Osc72Dsp.h), so
    // mono and centre content warble audibly, while the side
    // keeps the full-depth hard L/R swing. The trade-off is
    // documented in docs/audio-v3-diagnosis.md, Issue 1.
    {
        // (a) Mono now warbles, end to end, through the full
        // Chain. Both chains settle identically (mod out, 1 s —
        // asserted bit-exact over the last half-second before the
        // flip, so the control is proven); one then flips mod in.
        // After the engage fade settles, the in-vs-out DIFFERENCE
        // on a mono feed must be a substantial fraction of the
        // signal itself (v10 measured exactly 0.000 — the bug
        // Nathan heard). The limiter is genuinely engaged at
        // centre throughout, so this is the real product path.
        osc72::Chain cOff, cOn;
        cOff.prepare(kFs); cOn.prepare(kFs);
        cOff.setKnob(0.5f); cOn.setKnob(0.5f); // CENTRE
        bool exactBefore = true;
        double grSum = 0; int grN = 0;
        double diffSq = 0, sigSq = 0;
        for (int i = 0; i < (int)(3.0 * kFs); ++i) {
            if (i == (int)(1.0 * kFs)) cOn.setModIn(true);
            const float s = 0.65f * (float)std::sin(2.0 * kPi * 220.0 * i / kFs)
                          + 0.30f * (float)std::sin(2.0 * kPi * 1244.0 * i / kFs)
                          + 0.20f * (float)std::sin(2.0 * kPi * 330.0 * i / kFs);
            float aL, aR, bL, bR;
            cOff.process(s, s, aL, aR);
            cOn.process(s, s, bL, bR);
            if (i >= (int)(0.5 * kFs) && i < (int)(1.0 * kFs)) {
                if (aL != bL || aR != bR) exactBefore = false;
            }
            if (i >= (int)(1.5 * kFs)) {
                const double d = (double)bL - aL;
                diffSq += d * d; sigSq += (double)aL * aL;
                grSum += cOn.limiter().gainReductionDb(); ++grN;
            }
        }
        const float meanGrMono = (float)(grSum / grN);
        const double diffRatio = std::sqrt(diffSq / sigSq);
        std::snprintf(b, sizeof b, "mono diff/signal RMS=%.3f (v10: 0.000), mean limiter GR=%.2f dB",
                      diffRatio, meanGrMono);
        check(exactBefore, "mod v3: control chains bit-exact before the flip", b);
        check(diffRatio > 0.15, "mod v3: MONO now audibly warbled through the Chain", b);
        check(meanGrMono > 5.0f, "mod v3: limiter genuinely engaged at centre", b);

        // (b) Width, at the insert itself: a stereo feed built as
        // mid + side tones. MOD out passes it bit-exactly (side == in,
        // windowed side correlation identically 1). MOD in must leave
        // the MID RMS untouched while the side's arrival time swings
        // with the transport — measured as the spread (std-dev) of
        // the windowed zero-lag correlation between output side and
        // input side: ~0 for a steady path, large when the side is
        // being hauled left/right in time.
        struct WidthMeas { double midRmsIn, midRmsOut, sideRmsRatio, swingStd, sideCv; };
        auto widthRun = [](bool modIn) {
            osc72::WarbleChain wb; wb.prepare(kFs);
            if (modIn) wb.setEnabled(true);
            std::vector<float> oL, oR, iS;
            for (int i = 0; i < (int)(6.0 * kFs); ++i) {
                const float m  = 0.50f * (float)std::sin(2.0 * kPi * 220.0 * i / kFs);
                const float sd = 0.30f * (float)std::sin(2.0 * kPi * 523.0 * i / kFs)
                               + 0.15f * (float)std::sin(2.0 * kPi * 1244.0 * i / kFs);
                float a, c; wb.process(m + sd, m - sd, a, c);
                if (i >= (int)(2.0 * kFs)) { oL.push_back(a); oR.push_back(c); iS.push_back(sd); }
            }
            WidthMeas w{};
            double qMIn = 0, qMOut = 0, qSIn = 0, qSOut = 0;
            const int n = (int)oL.size();
            for (int i = 0; i < n; ++i) {
                const double midOut = ((double)oL[i] + oR[i]) * 0.5;
                const double sideOut = ((double)oL[i] - oR[i]) * 0.5;
                qMOut += midOut * midOut; qSOut += sideOut * sideOut;
                qSIn += (double)iS[i] * iS[i];
            }
            // Input mid RMS over the same span, recomputed exactly:
            {
                double q = 0; int cnt = 0;
                for (int i = (int)(2.0 * kFs); i < (int)(6.0 * kFs); ++i) {
                    const double m = 0.50 * std::sin(2.0 * kPi * 220.0 * i / kFs);
                    q += m * m; ++cnt;
                }
                qMIn = q / cnt;
            }
            w.midRmsIn = std::sqrt(qMIn);
            w.midRmsOut = std::sqrt(qMOut / n);
            w.sideRmsRatio = std::sqrt(qSOut / n) / std::sqrt(qSIn / n);
            // Windowed side swing: zero-lag corr(outSide, inSide).
            const int W = 2048;
            std::vector<double> corrs, sideWinRms;
            for (int base = 0; base + W <= n; base += W) {
                double mA = 0, mB = 0;
                for (int j = 0; j < W; ++j) {
                    mA += ((double)oL[base + j] - oR[base + j]) * 0.5;
                    mB += iS[base + j];
                }
                mA /= W; mB /= W;
                double cov = 0, vA = 0, vB = 0, eA = 0;
                for (int j = 0; j < W; ++j) {
                    const double a = ((double)oL[base + j] - oR[base + j]) * 0.5;
                    const double bb = iS[base + j];
                    cov += (a - mA) * (bb - mB);
                    vA += (a - mA) * (a - mA);
                    vB += (bb - mB) * (bb - mB);
                    eA += a * a;
                }
                corrs.push_back(cov / std::sqrt(vA * vB));
                sideWinRms.push_back(std::sqrt(eA / W));
            }
            double mc = 0; for (double v : corrs) mc += v; mc /= (double)corrs.size();
            double sc = 0; for (double v : corrs) sc += (v - mc) * (v - mc);
            w.swingStd = std::sqrt(sc / (double)corrs.size());
            double mr = 0; for (double v : sideWinRms) mr += v; mr /= (double)sideWinRms.size();
            double sr = 0; for (double v : sideWinRms) sr += (v - mr) * (v - mr);
            w.sideCv = std::sqrt(sr / (double)sideWinRms.size()) / mr;
            return w;
        };
        const WidthMeas wOff = widthRun(false);
        const WidthMeas wOn = widthRun(true);
        std::snprintf(b, sizeof b, "mid RMS out/in: off=%.4f on=%.4f (in %.4f)",
                      wOff.midRmsOut / wOff.midRmsIn, wOn.midRmsOut / wOn.midRmsIn,
                      wOn.midRmsIn);
        // v3 re-pin (v10 demanded the mid RMS within 0.1% — the
        // dry-mid guarantee, now retired). The mid rides the
        // transport as a 0.6 dry + 0.4 delayed blend; on a
        // sustained tone the dry/delayed pair combs, and the
        // long-run RMS of the blend is sqrt(0.6^2 + 0.4^2) = 0.72
        // of dry by construction (measured 0.747). The pin keeps
        // the mid PRESENT and bounded (never gutted, never
        // boosted) — the centre stays the anchor of the image in
        // level terms while audibly moving in time.
        check(wOn.midRmsOut / wOn.midRmsIn > 0.65 && wOn.midRmsOut / wOn.midRmsIn < 1.05,
              "mod v3: MID rides the transport (comb-bounded, still anchored)", b);
        std::snprintf(b, sizeof b, "side swing std: off=%.4f on=%.4f; side RMS CV on=%.3f",
                      wOff.swingStd, wOn.swingStd, wOn.sideCv);
        // v2 note: this correlation-std metric SATURATES once the
        // delay swing exceeds a few periods of the side content
        // (at 523 Hz, ±10 ms of swing sweeps ±5 correlation cycles
        // per analysis window, so the correlation sits low almost
        // all the time and its std no longer grows with depth —
        // measured 0.49 at v1 depth, 0.26 at v2 depth). It still
        // proves the side is being wrenched in time vs OFF; the
        // depth itself is pinned directly by the delay-span check
        // below and by group 9's pitch swing.
        check(wOn.swingStd > 0.15 && wOn.swingStd > 5.0 * wOff.swingStd,
              "mod v3: side swings hard in time (wide L/R warble)", b);
        std::snprintf(b, sizeof b, "side RMS ratio on=%.3f", wOn.sideRmsRatio);
        check(wOn.sideRmsRatio > 0.7 && wOn.sideRmsRatio < 1.3,
              "mod v3: side survives the transport (level sane)", b);
        // v2: the arrival-time swing itself, read off the running
        // delay over 6 s of pure-side feed. v1 depth spanned
        // 9.46..14.67 ms (5.2 ms); v2 must span several-fold more.
        {
            osc72::WarbleChain wb; wb.prepare(kFs); wb.setEnabled(true);
            float dMin = 1e9f, dMax = 0.0f;
            for (int i = 0; i < (int)(6.0 * kFs); ++i) {
                float s = 0.5f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
                float oL, oR; wb.process(s, -s, oL, oR);
                if (wb.currentDelayMs() < dMin) dMin = wb.currentDelayMs();
                if (wb.currentDelayMs() > dMax) dMax = wb.currentDelayMs();
            }
            std::snprintf(b, sizeof b, "delay span over 6 s: %.2f..%.2f ms (span %.2f ms)",
                          dMin, dMax, dMax - dMin);
            check(dMax - dMin > 12.0f,
                  "warble v2: arrival-time swing several-fold deeper (delay span)", b);
        }
    }

    // ── 16. Squash at CENTRE (v2 — supersedes the v9 anchor) ────────
    // v9 pinned centre to the pre-v9 law's 0.7 operating point
    // (17.69 dB mean GR on the standard signal). Nathan's ear
    // verdict on the shipped 1.0.0 build — the limiter "feels
    // almost flat… barely hear it" — superseded that anchor: the
    // v2 retune drives centre decisively harder (threshold slope
    // 42 -> 55, dynamic-curve divisor 8 -> 6.5). Measured centre
    // is now 27.26 dB. See docs/audio-v2-retune.md.
    {
        Meas m = runStereo(0.5f, 0.8f, 0.8f, 200.0f);
        std::snprintf(b, sizeof b, "mean GR at centre=%.2f dB (v9 anchor was 17.69 dB)", m.meanGr);
        check(m.meanGr > 24.0f && m.meanGr < 31.0f,
              "v2: centre squash decisively working (supersedes v9 anchor)", b);
    }

    // ── 17. Transient designer: sustain dries, attack survives ──────
    {
        TdMeas m0 = runBurstTail(0.0f);
        TdMeas m25 = runBurstTail(0.25f);
        TdMeas m50 = runBurstTail(0.5f);
        TdMeas m100 = runBurstTail(1.0f);
        auto db = [](double e, double ref) { return 10.0 * std::log10(e / ref); };
        const double d25 = db(m25.tailEnergy, m0.tailEnergy);
        const double d50 = db(m50.tailEnergy, m0.tailEnergy);
        const double d100 = db(m100.tailEnergy, m0.tailEnergy);
        std::snprintf(b, sizeof b,
                      "tail energy vs knob 0: 0.25:%+.1f dB  0.5:%+.1f dB  1.0:%+.1f dB",
                      d25, d50, d100);
        // v3 re-pin (v2 pinned "refilled-flat, never bloated",
        // with the TD winning outright at max). What changed is
        // the limiter behind the TD: v3's AUTO MAKEUP cranks —
        // full average-GR compensation PLUS a knob-tied crank —
        // so the dried tail is now deliberately forced back UP,
        // swelling past dry at the top of the travel (measured
        // −0.3 / +1.6 / +6.8 dB at 0.25 / 0.5 / 1.0). That swell
        // IS Nathan's requested behaviour ("as you push, the
        // crushed signal comes UP"), so the pin now reads: low
        // travel stays near-flat, the swell is present and
        // bounded at max, and it grows with the knob. The TD's
        // own cut is unchanged and still pinned below (mean tail
        // cut 2.8 / 6.8 / 16.6 dB, monotonic, 0.00 dB during the
        // burst) — the drying still happens; the makeup then
        // re-presents the dried tail louder, flattened.
        check(d25 < 1.0 && d50 < 2.5 && d100 > 3.0 && d100 < 12.0 && d100 > d50,
              "TD: tail dried by the TD, then cranked back UP by the auto makeup", b);
        check(m25.meanTailCutDb < m50.meanTailCutDb && m50.meanTailCutDb < m100.meanTailCutDb
              && m100.meanTailCutDb > 12.0f,
              "TD: own sustain cut grows monotonically with knob", b);
        std::snprintf(b, sizeof b,
                      "deepest TD cut during burst=%.2f dB, mean tail cut @1.0=%.1f dB, burst peak out @1.0=%.3f (in 0.85)",
                      m100.maxAttackCutDb, m100.meanTailCutDb, m100.burstPeak);
        check(m100.maxAttackCutDb < 4.0f,
              "TD: transient attack passes (no cut while burst sounds)", b);
    }

    // ── 18. Lundahl transformer stages (v9) ─────────────────────────
    // Measured directly on the stage pair (input voicing -> output
    // voicing, depth 1), limiter/TD not involved.
    {
        auto stagePair = [](float freq, float amp, int seconds,
                            double& fundGainDb, double& thdPct) {
            osc72::LundahlStage xi(osc72::LundahlStage::Voicing::input);
            osc72::LundahlStage xo(osc72::LundahlStage::Voicing::output);
            xi.prepare(kFs); xo.prepare(kFs);
            std::vector<float> out;
            const int total = (int)(seconds * kFs);
            for (int i = 0; i < total; ++i) {
                float s = amp * (float)std::sin(2.0 * kPi * freq * i / kFs);
                float y = xo.process(xi.process(s, 1.0f), 1.0f);
                if (i >= total / 2) out.push_back(y);
            }
            const double f1 = goertzel(out, freq, kFs);
            double harm = 0;
            for (int h = 2; h <= 5; ++h) {
                const double mh = goertzel(out, freq * h, kFs);
                harm += mh * mh;
            }
            fundGainDb = 20.0 * std::log10((2.0 * f1) / amp);
            thdPct = std::sqrt(harm) / f1 * 100.0;
        };
        double gN, tN, gD, tD, g1k, t1k, g120, t120;
        stagePair(60.0f, 0.25f, 3, gN, tN);    // nominal
        stagePair(60.0f, 0.9f, 3, gD, tD);     // driven
        stagePair(120.0f, 0.9f, 3, g120, t120);
        stagePair(1000.0f, 0.9f, 3, g1k, t1k); // hi-fi top check
        std::snprintf(b, sizeof b,
                      "60 Hz THD nom=%.2f%% driven=%.2f%%, fund gain nom %+.2f dB driven %+.2f dB",
                      tN, tD, gN, gD);
        check(tN < 1.5, "xfmr fat: subtle at nominal (60 Hz THD < 1.5%)", b);
        check(tD > 2.0 && tD < 7.0, "xfmr fat: thickens when driven (60 Hz THD 2-7%)", b);
        check(tD > 3.0 * tN, "xfmr fat: THD grows strongly with level", b);
        check(std::fabs(gN) < 0.5 && std::fabs(gD) < 0.75,
              "xfmr fat: LF weight without level lurch (fund gain small)", b);
        std::snprintf(b, sizeof b, "120 Hz driven THD=%.2f%%, 1 kHz driven THD=%.3f%% fund %+.2f dB",
                      t120, t1k, g1k);
        check(t1k < 0.5, "xfmr: top stays hi-fi clean (1 kHz THD < 0.5% driven)", b);

        // (b) DRY: after a loud 60 Hz burst stops, residual colour
        // on a quiet probe must collapse within a few ms (no ring,
        // no overhang — the flux state is the only memory, ~5 ms).
        {
            osc72::LundahlStage xi(osc72::LundahlStage::Voicing::input);
            osc72::LundahlStage xo(osc72::LundahlStage::Voicing::output);
            xi.prepare(kFs); xo.prepare(kFs);
            const int burstN = (int)(0.10 * kFs);
            for (int i = 0; i < burstN; ++i) {
                float s = 0.95f * (float)std::sin(2.0 * kPi * 60.0 * i / kFs);
                (void)xo.process(xi.process(s, 1.0f), 1.0f);
            }
            // Silence right after the burst: output must be exactly
            // zero — the stage has no tail of its own.
            float silMax = 0.0f;
            for (int i = 0; i < 480; ++i) {
                float y = xo.process(xi.process(0.0f, 1.0f), 1.0f);
                if (std::fabs(y) > silMax) silMax = std::fabs(y);
            }
            // Quiet 1 kHz probe straight after: measure its THD in
            // successive 5 ms windows (each an integer cycle count).
            double winThd[3] = { 0, 0, 0 };
            int probeIdx = 0;
            for (int w = 0; w < 3; ++w) {
                std::vector<float> win;
                for (int i = 0; i < 240; ++i, ++probeIdx) {
                    float s = 0.15f * (float)std::sin(2.0 * kPi * 1000.0 * probeIdx / kFs);
                    win.push_back(xo.process(xi.process(s, 1.0f), 1.0f));
                }
                const double f1 = goertzel(win, 1000.0, kFs);
                double harm = 0;
                for (int h = 2; h <= 3; ++h) {
                    const double mh = goertzel(win, 1000.0 * h, kFs);
                    harm += mh * mh;
                }
                winThd[w] = std::sqrt(harm) / f1 * 100.0;
            }
            std::snprintf(b, sizeof b,
                          "post-burst probe THD: 0-5ms=%.2f%% 5-10ms=%.2f%% 10-15ms=%.2f%%, silence out=%.1e",
                          winThd[0], winThd[1], winThd[2], silMax);
            check(silMax == 0.0f, "xfmr dry: silence after burst is exactly silent", b);
            check(winThd[2] < 0.5, "xfmr dry: probe colour gone by 10-15 ms", b);
            check(winThd[2] <= winThd[0], "xfmr dry: colour decays monotonically", b);
        }

        // (c) Response sanity: no low-mid resonance/honk — the
        // fundamental gain across 100 Hz..10 kHz stays in a tight,
        // peak-free band referenced to 1 kHz.
        {
            const float freqs[9] = { 100, 200, 400, 600, 800, 1000, 2000, 5000, 10000 };
            double gains[9];
            for (int i = 0; i < 9; ++i) {
                double g, t;
                stagePair(freqs[i], 0.5f, 2, g, t);
                gains[i] = g;
            }
            double mn = 1e9, mx = -1e9;
            for (int i = 0; i < 9; ++i) {
                if (gains[i] < mn) mn = gains[i];
                if (gains[i] > mx) mx = gains[i];
            }
            std::snprintf(b, sizeof b,
                          "fund gain 100Hz..10kHz vs own mean: min %+.2f max %+.2f dB (600 Hz %+.2f, 800 Hz %+.2f)",
                          mn, mx, gains[3], gains[4]);
            check(mx - mn < 1.0, "xfmr response: no resonance peak (band < 1 dB)", b);
        }
    }

    // ── 19. v9 knob-law table (report) ──────────────────────────────
    // Wet % / mean GR on the standard hot signal / TD behaviour, at
    // the five knob points the design note tabulates. The checks
    // pin the law's shape; the numbers print for the report.
    {
        const float ks[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        std::printf("  law table (mix = v9 law; squash = v3 retune): knob | wet%% | meanGR(std) | TD tail cut\n");
        float prevGr = -1.0f;
        bool mono = true;
        for (int i = 0; i < 5; ++i) {
            Meas m = runStereo(ks[i], 0.8f, 0.8f, 200.0f);
            TdMeas tm = runBurstTail(ks[i]);
            std::printf("          %.2f | %3.0f%% | %6.2f dB | mean tail cut %.1f dB\n",
                        ks[i], osc72::wetWeightForKnob(ks[i]) * 100.0f,
                        m.meanGr, tm.meanTailCutDb);
            if (i > 0 && !(m.meanGr >= prevGr)) mono = false;
            prevGr = m.meanGr;
        }
        check(mono, "law table: GR monotonic across the full travel", "");
    }

    // ── 20. Ballistics + PUMPING PROOF (v3 pins) ────────────────────
    // History: v1 measured attack t63 0.7/0.4 ms and release t63
    // 170/471 ms (centre/max) — "feels almost flat". v2 retuned to
    // release t63 68/78 ms. Nathan on 1.0.2 (v2): the release is
    // "way too slow — it needs to be like five times quicker at
    // least to pump", and the pumping on continuous material was
    // "lost". v3 pins: release t63 in the teens at centre and the
    // twenties at max on this step protocol (the step number
    // includes bleeding the detector envelope down through the
    // settled over-range, which is ~2.4x deeper at max — the
    // PROGRAMME release, proven by the burst test below and the
    // loop test in group 21, recovers fully between hits).
    {
        StepMeas sC = runStep(0.5f);
        StepMeas sM = runStep(1.0f);
        std::snprintf(b, sizeof b,
                      "centre: settled %.1f dB, atk t63 %.1f ms, rel t63 %.1f ms t90 %.1f ms",
                      sC.settledGr, sC.atk63, sC.rel63, sC.rel90);
        check(sC.atk63 > 10.0f && sC.atk63 < 35.0f,
              "v3 attack: slow-to-medium at centre (transients punch)", b);
        check(sC.rel63 > 8.0f && sC.rel63 < 25.0f && sC.rel90 < 45.0f,
              "v3 release: ~5x faster than v2 at centre (was 68/133 ms)", b);
        std::snprintf(b, sizeof b,
                      "max: settled %.1f dB, atk t63 %.1f ms, rel t63 %.1f ms t90 %.1f ms",
                      sM.settledGr, sM.atk63, sM.rel63, sM.rel90);
        check(sM.atk63 > 5.0f && sM.atk63 < 20.0f,
              "v3 attack: slow-to-medium at max", b);
        check(sM.rel63 > 12.0f && sM.rel63 < 40.0f && sM.rel90 < 60.0f,
              "v3 release: ~3-4x faster than v2 at max (was 78/111 ms)", b);

        PumpMeas pC = runPump(0.5f);
        PumpMeas pM = runPump(1.0f);
        std::snprintf(b, sizeof b,
                      "centre: depth %.1f dB, residual %.2f dB, recovery %.1f dB (%.0f%%), burst-period peak %.3f",
                      pC.depth, pC.residual, pC.recovery,
                      pC.depth > 0 ? 100.0 * pC.recovery / pC.depth : 0.0, pC.onsetPeak);
        check(pC.depth > 20.0f && pC.recovery > 15.0f && pC.recovery > 0.7f * pC.depth,
              "v3 PUMP: centre hits deep, gain surges back between hits", b);
        std::snprintf(b, sizeof b,
                      "max: depth %.1f dB, residual %.2f dB, recovery %.1f dB (%.0f%%), burst-period peak %.3f",
                      pM.depth, pM.residual, pM.recovery,
                      pM.depth > 0 ? 100.0 * pM.recovery / pM.depth : 0.0, pM.onsetPeak);
        check(pM.depth > 55.0f && pM.recovery > 40.0f && pM.recovery > 0.7f * pM.depth,
              "v3 PUMP: max slams and fully recovers between hits", b);
        check(pC.onsetPeak < 0.95f && pM.onsetPeak < 0.95f,
              "v3 pump: no overshoot disaster inside the pump pattern", b);

        // Slow-attack overshoot guard: pre-charged limiter, gap,
        // full-scale hit (see runOvershoot). Unguarded this peaked
        // at 1.58; the guard (feed-forward ceiling, makeup-aware,
        // pre-TD peak basis) must hold every scenario at/below the
        // saturator's clean ceiling region.
        const float oCold = runOvershoot(1.0f, 0.0, false);
        const float oG100 = runOvershoot(1.0f, 100.0, true);
        const float oG380 = runOvershoot(1.0f, 380.0, true);
        const float oC100 = runOvershoot(0.5f, 100.0, true);
        std::snprintf(b, sizeof b,
                      "hit peaks: cold %.3f, max+100 ms gap %.3f, max+380 ms gap %.3f, centre+100 ms gap %.3f",
                      oCold, oG100, oG380, oC100);
        check(oCold < 0.95f && oG100 < 0.95f && oG380 < 0.95f && oC100 < 0.95f,
              "v3 guard: full-scale hits after gaps stay under the ceiling", b);
    }

    // ── 21. CONTINUOUS-MATERIAL PUMP (v3 — the "lost" test) ─────────
    // The burst test (group 20) has silence between hits; Nathan's
    // "lost" verdict was about CONTINUOUS material, where v2's GR
    // sat on a bass-held floor and the slow makeup levelled what
    // movement remained. This is the diagnosis loop, formalised:
    // 120 BPM — kick on quarters (pitch-dropping thump + 2 ms
    // broadband click), snare on 2 & 4 (noise burst + 190 Hz
    // body), hats on 8ths alternating pan, a CONTINUOUS 55 Hz
    // bass line under everything. 8 s per knob; 10 ms windowed
    // envelopes; steady state (first 2 s dropped). Metrics: GR
    // trajectory range (p95-p5 of per-block GR) and the NET gain
    // trajectory (-GR + makeup, from the limiter's own reports)
    // — range, and the net MEAN (the auto-makeup crank must make
    // it RISE with the knob; v2 measured -1.8 dB at centre
    // FALLING to -7.4 dB at max on this material).
    {
        auto loopRun = [](float knob, float& grRange, float& grMean,
                          float& netRange, float& netMean, float& wetPeak) {
            osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(knob);
            const int beat = (int)(0.5 * kFs);
            std::vector<float> grs, nets;
            float pk = 0.0f;
            long long rng = 12345;
            auto noise = [&]() {
                rng = (rng * 1103515245LL + 12345LL) & 0x7fffffffLL;
                return (float)(rng / (double)0x40000000 - 1.0);
            };
            double phase = 0.0;
            for (int bar = 0; bar < 16; ++bar) {
                for (int b8 = 0; b8 < 8; ++b8) {
                    const int n8 = beat / 2;
                    for (int i = 0; i < n8; ++i) {
                        const long long t = (long long)bar * beat * 2 + (long long)b8 * n8 + i;
                        const double tt = t / kFs;
                        const double inBeat = (t % beat) / kFs;
                        const double in8 = i / kFs;
                        double kick = 0, snare = 0, hat = 0;
                        const double fKick = 120.0 - 70.0 * std::min(1.0, inBeat / 0.09);
                        if (inBeat < 0.25)
                            kick = 0.95 * std::sin(2.0 * kPi * fKick * inBeat)
                                 * std::exp(-inBeat / 0.055);
                        if (inBeat < 0.002) kick += 0.5 * noise(); // onset click
                        if ((b8 % 4) == 2 && in8 < 0.18)
                            snare = (0.55 * noise() * std::exp(-in8 / 0.05)
                                   + 0.45 * std::sin(2.0 * kPi * 190.0 * in8)
                                     * std::exp(-in8 / 0.06));
                        if (in8 < 0.05)
                            hat = 0.30 * noise() * std::exp(-in8 / 0.015);
                        phase += 2.0 * kPi * 55.0 / kFs;
                        const double bass = 0.55 * std::sin(phase);
                        const double mono = kick + snare + bass;
                        const float inL = (float)(mono + ((b8 % 2) ? hat : hat * 0.35));
                        const float inR = (float)(mono + ((b8 % 2) ? hat * 0.35 : hat));
                        float oL, oR; lim.process(inL, inR, oL, oR);
                        // Peak in STEADY STATE only (t >= 2 s, the
                        // stats window): at t=0 the knob slew is
                        // still near zero and the raw input (kick +
                        // click + bass, peaking ~1.5+) passes at
                        // near-dry weight — that is the input's own
                        // peak, not a ceiling failure.
                        if (t >= (long long)(2.0 * kFs)) {
                            const float aL = std::fabs(oL), aR = std::fabs(oR);
                            if (aL > pk) pk = aL; if (aR > pk) pk = aR;
                        }
                        (void)tt;
                        if (t >= (long long)(2.0 * kFs) && (t % 480) == 0) {
                            grs.push_back(lim.gainReductionDb());
                            nets.push_back(lim.appliedMakeupDb() - lim.gainReductionDb());
                        }
                    }
                }
            }
            auto pct = [](std::vector<float> v, double p, double& mean) {
                std::sort(v.begin(), v.end());
                double s = 0; for (float x : v) s += x; mean = s / (double)v.size();
                auto at = [&](double q) { return (double)v[(size_t)(q * (v.size() - 1))]; };
                return (float)(at(0.95) - at(0.05));
            };
            double m1, m2;
            grRange = pct(grs, 0.95, m1); grMean = (float)m1;
            netRange = pct(nets, 0.95, m2); netMean = (float)m2;
            wetPeak = pk;
        };
        float grR05, grM05, netR05, netM05, pk05;
        float grR10, grM10, netR10, netM10, pk10;
        loopRun(0.5f, grR05, grM05, netR05, netM05, pk05);
        loopRun(1.0f, grR10, grM10, netR10, netM10, pk10);
        std::snprintf(b, sizeof b,
                      "centre: GR mean %.1f range %.1f dB | NET mean %+.1f range %.1f dB | out peak %.2f (v2: GR range 9.0, NET mean -1.8 range 2.3)",
                      grM05, grR05, netM05, netR05, pk05);
        check(grR05 > 10.0f, "v3 LOOP: GR trajectory rides on continuous material (centre)", b);
        check(netR05 > 3.5f, "v3 LOOP: net gain pumps, not auto-levelled flat (centre)", b);
        check(netM05 > -1.0f, "v3 LOOP: auto makeup holds level at centre", b);
        std::snprintf(b, sizeof b,
                      "max: GR mean %.1f range %.1f dB | NET mean %+.1f range %.1f dB | wet peak %.2f (v2: NET mean -7.4)",
                      grM10, grR10, netM10, netR10, pk10);
        check(netM10 > 2.0f, "v3 LOOP: pushing the knob CRANKS the crushed signal up", b);
        check(pk10 < 0.95f, "v3 LOOP: wet path stays under the ceiling at max", b);
    }

    // ── 22. LF DISTORTION AT THE NEW BALLISTICS (v3) ────────────────
    // The price list for a ~5x faster release is LF waveform
    // riding; the studied units balance it, so v3 must show its
    // numbers, not hide them. Sustained 0.8-amp tones through the
    // limiter (worst case for gain riding — real bass lines move
    // and read far cleaner), THD at 60/100 Hz, centre and max.
    // v2 measured 0.57/1.13% (60 Hz, centre/max) with its slow
    // release; v3's balance (presence-stretched detector hang +
    // peak-hold ceiling basis + programme-dependent release)
    // lands at ~2.3% centre / ~6% max — Level-Loc-family grit at
    // max, near-clean at centre. Pins sit above the measured
    // values with headroom for tuning drift, far below the 36%
    // the first fast cut produced.
    {
        auto toneThd = [](float knob, float freq) {
            osc72::StereoLimiter lim; lim.prepare(kFs); lim.setKnob(knob);
            std::vector<float> out;
            const int total = (int)(4.0 * kFs);
            for (int i = 0; i < total; ++i) {
                float s = 0.8f * (float)std::sin(2.0 * kPi * freq * i / kFs);
                float a, c; lim.process(s, s, a, c);
                if (i >= total / 2) out.push_back(a);
            }
            const double f1 = goertzel(out, freq, kFs);
            double harm = 0;
            for (int h = 2; h <= 5; ++h) harm += std::pow(goertzel(out, freq * h, kFs), 2);
            return std::sqrt(harm) / f1 * 100.0;
        };
        const double t60c = toneThd(0.5f, 60.0f), t60m = toneThd(1.0f, 60.0f);
        const double t100c = toneThd(0.5f, 100.0f), t100m = toneThd(1.0f, 100.0f);
        std::snprintf(b, sizeof b, "60 Hz THD centre=%.2f%% max=%.2f%%; 100 Hz centre=%.2f%% max=%.2f%% (v2 60 Hz: 0.57/1.13%%)",
                      t60c, t60m, t100c, t100m);
        check(t60c < 4.0 && t100c < 4.0, "v3 LF: centre stays near-clean at the fast release", b);
        check(t60m < 10.0 && t100m < 10.0, "v3 LF: max-knob LF grit bounded (fast-release balance)", b);
    }

    std::printf("%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL TESTS PASSED" : "FAILURES PRESENT",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
