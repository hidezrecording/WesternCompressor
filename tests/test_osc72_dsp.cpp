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
        check(m.peakL < 0.85f, "knob10 peak controlled, no flat-top blowout", b);
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
        check(std::fabs(ratioL - ratioR) / ratioL < 0.05f, "linked GR: equal gain L/R", b);
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
        check(devPct > 2.0, "warble ON: pitch measurably modulated", b);
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
        check(dMin >= 4.0f && dMax <= 24.0f, "warble: delay always in safe band", b);
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

        // Toggling limit mid-tone must be click-free (30 ms fade).
        osc72::Chain chain2;
        chain2.prepare(kFs); chain2.setKnob(0.85f);
        float maxStep = 0.0f, prevOut = 0.0f;
        auto feed2 = [&](int n) {
            for (int i = 0; i < n; ++i) {
                float s = 0.5f * (float)std::sin(2.0 * kPi * 440.0 * i / kFs);
                float a, c; chain2.process(s, s, a, c);
                float st = std::fabs(a - prevOut); if (st > maxStep) maxStep = st;
                prevOut = a;
            }
        };
        feed2(24000);              // settle, limit in
        chain2.setLimitIn(false);
        feed2(24000);              // fade out mid-tone
        chain2.setLimitIn(true);
        feed2(24000);              // fade back in mid-tone
        std::snprintf(b, sizeof b, "max sample step across limit toggles=%.4f", maxStep);
        check(maxStep < 0.05f, "limit: engage/disengage click-free", b);
    }

    // ── 15. Mod v10: hard L/R warble, centre stays DRY (mid/side) ──
    // Supersedes the v8 "degraded width" checks above all recognition:
    // Nathan's note (2026-10-14) reverses that brief. The insert now
    // warbles the SIDE component only; the MID passes dry. Width
    // narrowing and the mono-correlation-drop target are GONE — on a
    // mono feed mod in must now be indistinguishable from mod out.
    {
        // (a) Centre stays dry, end to end: a mono feed through the
        // full Chain is BIT-EXACT with mod in vs mod out, limiter
        // genuinely engaged at centre. Both chains settle identically
        // (mod out, 1 s); one then flips mod in — every following
        // sample, including across the 30 ms engage fade, must match.
        osc72::Chain cOff, cOn;
        cOff.prepare(kFs); cOn.prepare(kFs);
        cOff.setKnob(0.5f); cOn.setKnob(0.5f); // CENTRE
        bool exact = true;
        double grSum = 0; int grN = 0;
        for (int i = 0; i < (int)(3.0 * kFs); ++i) {
            if (i == (int)(1.0 * kFs)) cOn.setModIn(true);
            const float s = 0.65f * (float)std::sin(2.0 * kPi * 220.0 * i / kFs)
                          + 0.30f * (float)std::sin(2.0 * kPi * 1244.0 * i / kFs)
                          + 0.20f * (float)std::sin(2.0 * kPi * 330.0 * i / kFs);
            float aL, aR, bL, bR;
            cOff.process(s, s, aL, aR);
            cOn.process(s, s, bL, bR);
            if (i >= (int)(1.0 * kFs)) {
                if (aL != bL || aR != bR) exact = false; // exact equality
                grSum += cOn.limiter().gainReductionDb(); ++grN;
            }
        }
        const float meanGrMono = (float)(grSum / grN);
        std::snprintf(b, sizeof b, "mean limiter GR during check=%.2f dB", meanGrMono);
        check(exact, "mod v10: mono through Chain bit-exact (centre dry)", b);
        check(meanGrMono > 5.0f, "mod v10: limiter genuinely engaged at centre", b);

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
        check(std::fabs(wOn.midRmsOut - wOn.midRmsIn) / wOn.midRmsIn < 0.001,
              "mod v10: MID untouched by the warble (RMS < 0.1% off)", b);
        std::snprintf(b, sizeof b, "side swing std: off=%.4f on=%.4f; side RMS CV on=%.3f",
                      wOff.swingStd, wOn.swingStd, wOn.sideCv);
        check(wOn.swingStd > 0.25 && wOn.swingStd > 5.0 * wOff.swingStd,
              "mod v10: side swings hard in time (wide L/R warble)", b);
        std::snprintf(b, sizeof b, "side RMS ratio on=%.3f", wOn.sideRmsRatio);
        check(wOn.sideRmsRatio > 0.7 && wOn.sideRmsRatio < 1.3,
              "mod v10: side survives the transport (level sane)", b);
    }

    // ── 16. Squash at CENTRE == the old law's 0.7 squash (v9) ───────
    // The v9 threshold law re-anchors the travel so threshold(0.5)
    // is exactly the pre-v9 threshold(0.7). On the standard hot
    // signal the pre-v9 suite measured 23.00 dB mean GR — at knob
    // 0.8 (its test points were 0.3/0.55/0.8/1.0); the same law and
    // detector at 0.7 give ~17.7 dB (GR ≈ over-threshold × dynamic
    // fraction: 19.5 dB over × 0.91). Centre must land on THAT.
    {
        Meas m = runStereo(0.5f, 0.8f, 0.8f, 200.0f);
        std::snprintf(b, sizeof b, "mean GR at centre=%.2f dB (old 0.7 law value ~17.7 dB)", m.meanGr);
        check(m.meanGr > 14.0f && m.meanGr < 21.0f,
              "v9: centre squash == old 0.7 Raspberries squash", b);
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
        check(d25 < 0.0 && d50 < d25 && d100 < d50,
              "TD: tail/sustain energy falls monotonically with knob", b);
        check(d100 < -6.0, "TD: sustain cut substantial at max", b);
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
        std::printf("  v9 law: knob | wet%% | meanGR(std) | TD tail cut (burst+tail)\n");
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
        check(mono, "v9 law: GR monotonic across the full travel", "");
    }

    std::printf("%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL TESTS PASSED" : "FAILURES PRESENT",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
