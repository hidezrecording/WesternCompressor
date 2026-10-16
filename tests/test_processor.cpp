// tests/test_processor.cpp — processor-level tests (ENGINE v4).
// Drives Osc72Processor::processBlock — the FULL shipped product
// path: APVTS parameter -> processor -> osc72::Chain (BA-6A +
// Reel ADT) — instead of the DSP classes alone. Built as the
// OscProcessorTest executable (see juce/CMakeLists.txt); run
// headless under xvfb.
//
// Why this exists (kept from v3): the DSP suite proves the DSP
// classes; this proves the plug-in wrapper delivers them —
// the v3 lesson was a MOD that measured fine in the DSP suite
// and was inaudible in the product. v4 pins:
//   P1/P3  MOD difference through the processor (wide/typical)
//   P2     MONO material WIDENS through the processor (side
//          energy appears — the retired mid/side law gave
//          exactly zero; ADT is a doubling effect)
//   P4     reported latency follows MOD (32 / 992 @ 48 kHz — v53: +32 oversampling)
//   P5     POWER off = bit-exact bypass once the fade settles
//   P6     the new ATTACK/RELEASE params: defaults, and their
//          ballistics audible end-to-end through processBlock
//   P7     the v38 MIX pot: default fully wet, mix=1 bit-for-
//          bit the pre-feature output, mix=0 the input (at
//          mix=0 with MOD in: the input delayed by exactly
//          the 960-sample chain latency), mix=0.5 the exact
//          mean of aligned dry and wet, POWER off bypassing
//          regardless of mix
//   P9     MIX ALIGNMENT at impulse level (v44 diagnosis):
//          P7's pins took the nominal latency as their own
//          reference, so a wet path arriving a sample early
//          or late would have passed them. P9 measures the
//          paths themselves: wet/dry cross-correlation peaks
//          at lag 0, the blended impulse peak sits exactly at
//          the expected sample at exactly the mean-of-peaks
//          height (a split peak cannot), and during a MOD
//          toggle the dry tap crossfades its offset with the
//          chain's own engagement law instead of stepping
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int failures = 0;
static int checks = 0;
static void check(bool ok, const char* name, const char* detail = "") {
    ++checks;
    std::printf("%s %-58s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) ++failures;
}

static constexpr double kFs = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

// Deterministic per-material sample generator. Modes:
// 0 = mono, 1 = wide/decorrelated, 2 = typical (mid-dominant).
struct Material {
    int mode;
    unsigned stL = 4242u, stR = 987654u;
    float noise(unsigned& st) {
        st = st * 1664525u + 1013904223u;
        return (float)((st >> 9) & 0x7FFF) / 16384.0f - 1.0f;
    }
    void sample(int i, float& L, float& R) {
        const double t = i / kFs;
        if (mode == 0) {
            float s = 0.55f * (float)std::sin(2.0 * kPi * 220.0 * t)
                    + 0.25f * (float)std::sin(2.0 * kPi * 1244.0 * t)
                    + 0.10f * noise(stL);
            L = R = s;
        } else if (mode == 1) {
            L = 0.45f * (float)std::sin(2.0 * kPi * 220.0 * t)
              + 0.28f * (float)std::sin(2.0 * kPi * 1244.0 * t)
              + 0.16f * noise(stL);
            R = 0.45f * (float)std::sin(2.0 * kPi * 331.0 * t)
              + 0.28f * (float)std::sin(2.0 * kPi * 987.0 * t)
              + 0.16f * noise(stR);
        } else {
            const float m = 0.55f * (float)std::sin(2.0 * kPi * 220.0 * t)
                          + 0.25f * (float)std::sin(2.0 * kPi * 440.0 * t)
                          + 0.10f * noise(stL);
            const float sd = 0.16f * (float)std::sin(2.0 * kPi * 1244.0 * t)
                           + 0.07f * noise(stR);
            L = m + sd; R = m - sd;
        }
    }
};

struct DiffMeas { double diffDb; double outRms; double sideRatio; bool bitExact; };

// Run two identical processors (mod off / on) in lockstep over
// the same material; measure over the settled window. The
// MOD-on path is DELAYED by the 20 ms ADT base offset, so the
// sample difference is large by design — the pins below use
// difference level (audibility) and side energy (width).
static DiffMeas runPair(int mode, float knob, bool limitIn = true) {
    Osc72Processor pOff, pOn;
    for (auto* p : { &pOff, &pOn }) {
        p->prepareToPlay(kFs, 512);
        p->apvts.getParameter("allTheWay")->setValueNotifyingHost(knob / 10.0f);
        p->apvts.getParameter("limit")->setValueNotifyingHost(limitIn ? 1.0f : 0.0f);
        p->apvts.getParameter("power")->setValueNotifyingHost(1.0f);
    }
    pOn.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);

    Material mat{ mode };
    const int block = 512;
    const int total = (int)(4.0 * kFs);
    const int settle = (int)(1.5 * kFs);
    juce::AudioBuffer<float> bOff(2, block), bOn(2, block);
    juce::MidiBuffer midi;
    double dSq = 0, oSq = 0, sOnSq = 0; long n = 0;
    bool exact = true;
    for (int base = 0; base < total; base += block) {
        for (int j = 0; j < block; ++j) {
            float L, R; mat.sample(base + j, L, R);
            bOff.setSample(0, j, L); bOff.setSample(1, j, R);
            bOn.setSample(0, j, L);  bOn.setSample(1, j, R);
        }
        pOff.processBlock(bOff, midi);
        pOn.processBlock(bOn, midi);
        if (base >= settle) {
            for (int j = 0; j < block; ++j) {
                const float aL = bOff.getSample(0, j), aR = bOff.getSample(1, j);
                const float cL = bOn.getSample(0, j),  cR = bOn.getSample(1, j);
                if (aL != cL || aR != cR) exact = false;
                const double dL = (double)cL - aL, dR = (double)cR - aR;
                dSq += dL * dL + dR * dR;
                oSq += (double)aL * aL + (double)aR * aR;
                const double sC = ((double)cL - cR) * 0.5;
                sOnSq += sC * sC;
                ++n;
            }
        }
    }
    DiffMeas m;
    const double diffRms = std::sqrt(dSq / (2.0 * n));
    m.outRms = std::sqrt(oSq / (2.0 * n));
    m.diffDb = 20.0 * std::log10(diffRms / m.outRms);
    const double sideOn = std::sqrt(sOnSq / n);
    m.sideRatio = sideOn / m.outRms;
    m.bitExact = exact;
    return m;
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    char b[192];

    // ── P1. MOD through the processor: wide material ─────────────
    for (float knob : { 0.0f, 5.0f }) {
        DiffMeas m = runPair(1, knob);
        std::snprintf(b, sizeof b, "knob %.0f: diff=%.2f dB rel out, out RMS=%.3f",
                      knob, m.diffDb, m.outRms);
        check(m.diffDb > -8.0, "P1 proc MOD: WIDE material — unmistakable difference", b);
    }

    // ── P2. MOD through the processor: MONO widens (ADT doubling;
    // the retired mid/side law gave exactly 0 side energy) ─────
    {
        DiffMeas m = runPair(0, 5.0f);
        std::snprintf(b, sizeof b, "mono: bit-exact=%d, diff=%.2f dB, side/out RMS=%.3f (v3 design: 0.000)",
                      (int)m.bitExact, m.diffDb, m.sideRatio);
        // v1.1.1 pin move (ADT deck trim x2/3, Nathan):
        // mono side/out measured 0.446 -> 0.238; threshold
        // relaxed 0.25 -> 0.20. See build-report-v40.md.
        check(!m.bitExact && m.diffDb > -8.0 && m.sideRatio > 0.20,
              "P2 proc MOD: MONO material — widens (side energy appears)", b);
    }

    // ── P3. MOD through the processor: typical stereo material ────
    for (float knob : { 0.0f, 5.0f }) {
        DiffMeas m = runPair(2, knob);
        std::snprintf(b, sizeof b, "knob %.0f: diff=%.2f dB rel out, side/out=%.3f",
                      knob, m.diffDb, m.sideRatio);
        check(m.diffDb > -8.0, "P3 proc MOD: TYPICAL stereo — unmistakable difference", b);
    }

    // ── P4. Reported latency follows MOD (ADT base = 20 ms) ──────
    {
        Osc72Processor p;
        p.prepareToPlay(kFs, 512);
        const int latOff = p.getLatencySamples();
        p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        p.processBlock(buf, midi); // edge is consumed in processBlock
        const int latOn = p.getLatencySamples();
        std::snprintf(b, sizeof b, "latency off=%d on=%d", latOff, latOn);
        check(latOff == 32 && latOn == 992, "P4 proc: latency 32 off / 992 on (OS + 20 ms ADT base, v53)", b);
    }

    // ── P5. POWER off = bit-exact bypass (settled) ────────────────
    {
        Osc72Processor p;
        p.prepareToPlay(kFs, 512);
        p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.7f);
        p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
        p.apvts.getParameter("power")->setValueNotifyingHost(0.0f);
        Material mat{ 2 };
        const int block = 512;
        bool exact = true;
        juce::MidiBuffer midi;
        for (int base = 0; base < (int)(2.0 * kFs); base += block) {
            juce::AudioBuffer<float> buf(2, block);
            std::vector<float> inL(block), inR(block);
            for (int j = 0; j < block; ++j) {
                mat.sample(base + j, inL[(size_t)j], inR[(size_t)j]);
                buf.setSample(0, j, inL[(size_t)j]);
                buf.setSample(1, j, inR[(size_t)j]);
            }
            p.processBlock(buf, midi);
            if (base >= (int)(1.0 * kFs)) // fade long settled
                for (int j = 0; j < block; ++j)
                    if (buf.getSample(0, j) != inL[(size_t)j] || buf.getSample(1, j) != inR[(size_t)j])
                        exact = false;
        }
        std::snprintf(b, sizeof b, "settled power-off output == input exactly: %d", (int)exact);
        check(exact, "P5 proc: POWER off is bit-exact bypass", b);
    }

    // ── P6. ATTACK / RELEASE params: defaults + ballistics ───────
    {
        Osc72Processor p;
        p.prepareToPlay(kFs, 512);
        auto* pa = p.apvts.getParameter("attackFast");
        auto* pr = p.apvts.getParameter("releaseFast");
        std::snprintf(b, sizeof b, "attackFast default=%.0f (want 1), releaseFast default=%.0f (want 0)",
                      pa ? (double)pa->getValue() : -1.0, pr ? (double)pr->getValue() : -1.0);
        check(pa && pr && pa->getValue() > 0.5f && pr->getValue() < 0.5f,
              "P6a v4 params exist, defaults FAST attack / SLOW (dual) release", b);

        // End-to-end attack: a burst's first-10 ms peak passes
        // HIGHER with SLOW attack than with FAST.
        auto burstPeak = [&](bool attackFast) {
            Osc72Processor q;
            q.prepareToPlay(kFs, 512);
            q.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.75f);
            q.apvts.getParameter("attackFast")->setValueNotifyingHost(attackFast ? 1.0f : 0.0f);
            juce::MidiBuffer midi;
            const int block = 512;
            // 0.5 s of quiet, then burst; measure peak in the
            // first 10 ms after onset.
            float peak = 0;
            const int onset = (int)(0.5 * kFs);
            for (int base = 0; base < (int)(0.56 * kFs); base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    int i = base + j;
                    float s = i >= onset
                        ? 0.7f * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                q.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) {
                    int i = base + j;
                    if (i >= onset && i < onset + (int)(0.01 * kFs))
                        peak = std::max(peak, std::fabs(buf.getSample(0, j)));
                }
            }
            return peak;
        };
        float pkFast = burstPeak(true), pkSlow = burstPeak(false);
        std::snprintf(b, sizeof b, "burst first-10ms peak: FAST %.3f vs SLOW %.3f", pkFast, pkSlow);
        // v1.1.1 pin move (drive law): at this depth both
        // bursts ride the make-up peak ceiling, compressing
        // the FAST/SLOW peak margin (measured x1.133 under
        // the 1.1.0 law -> x1.024 now: 1.361 vs 1.393). The
        // direction — SLOW passes more transient — is what
        // the pin asserts. See build-report-v40.md.
        check(pkSlow > pkFast * 1.015f, "P6b proc: SLOW attack lets the transient through", b);

        // End-to-end release: after a loud passage ends, the
        // GR readout (the display feed) falls fast on SINGLE
        // and holds on DUAL.
        auto grAfter = [&](bool releaseFast, double atSec) {
            Osc72Processor q;
            q.prepareToPlay(kFs, 512);
            q.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.75f);
            q.apvts.getParameter("releaseFast")->setValueNotifyingHost(releaseFast ? 1.0f : 0.0f);
            juce::MidiBuffer midi;
            const int block = 512;
            float peakGr = 0, atGr = 0;
            const int loudEnd = (int)(1.0 * kFs);
            const int total = loudEnd + (int)(1.2 * kFs);
            for (int base = 0; base < total; base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    int i = base + j;
                    float s = i < loudEnd
                        ? 0.6f * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                q.processBlock(buf, midi);
                float gr = q.currentGainReductionDb();
                if (base + block <= loudEnd) peakGr = std::max(peakGr, gr);
                if (base + block >= loudEnd + (int)(atSec * kFs) && atGr == 0.0f) atGr = gr;
            }
            return std::pair<float, float>(peakGr, atGr);
        };
        auto sgl = grAfter(true, 0.35);
        auto dul = grAfter(false, 0.30);
        std::snprintf(b, sizeof b, "GR after stop: SINGLE %.1f of %.1f dB @+0.35 s, DUAL %.1f of %.1f dB @+0.30 s",
                      sgl.second, sgl.first, dul.second, dul.first);
        // v45: the DUAL clause 0.35 -> 0.28 — the re-voiced
        // opto front (50% recovery at ~60 ms, per the
        // manuals) leaves only the tail share by +0.30 s:
        // raw trace 37%, readout (block-quantised sampling)
        // 33.7%. SINGLE's clause is unchanged and now passes
        // at 0.0 — the 1176-seat Single is fully home by
        // +0.35 s. See build-report-v45.md.
        check(sgl.first > 5.0f && sgl.second < 0.45f * sgl.first && dul.second > 0.28f * dul.first,
              "P6c proc: SINGLE releases fast, DUAL holds (readout)", b);
    }

    // ── P7. MIX pot (v38) ─────────────────────────────────────
    {
        // One processor over typical material, full histories.
        // mix < 0 = leave the param untouched (the default).
        auto runMix = [](float mix, bool warble, double seconds, float power = 1.0f) {
            struct Hist { std::vector<float> inL, inR, outL, outR; };
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.5f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(warble ? 1.0f : 0.0f);
            p.apvts.getParameter("power")->setValueNotifyingHost(power);
            if (mix >= 0.0f)
                p.apvts.getParameter("mix")->setValueNotifyingHost(mix);
            Material mat{ 2 };
            const int block = 512;
            // Whole blocks only — a part-block tail would sit
            // in the histories as unprocessed zeros.
            const int total = ((int)(seconds * kFs) / block) * block;
            Hist r;
            r.inL.assign(total, 0); r.inR.assign(total, 0);
            r.outL.assign(total, 0); r.outR.assign(total, 0);
            juce::MidiBuffer midi;
            for (int base = 0; base + block <= total; base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    float L, R; mat.sample(base + j, L, R);
                    r.inL[(size_t)(base + j)] = L; r.inR[(size_t)(base + j)] = R;
                    buf.setSample(0, j, L); buf.setSample(1, j, R);
                }
                p.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) {
                    r.outL[(size_t)(base + j)] = buf.getSample(0, j);
                    r.outR[(size_t)(base + j)] = buf.getSample(1, j);
                }
            }
            return r;
        };
        const int settle = (int)(0.5 * kFs);

        // P7a: param exists; default 1.0 (fully wet).
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            auto* pm = p.apvts.getParameter("mix");
            std::snprintf(b, sizeof b, "mix exists=%d default=%.3f (want 1.000)",
                          (int)(pm != nullptr), pm ? (double)pm->getValue() : -1.0);
            check(pm && std::fabs(pm->getValue() - 1.0f) < 1e-6,
                  "P7a MIX param exists, default 1.0 (fully wet)", b);
        }
        // P7b: mix=1 (explicit or untouched) == the default
        // instance, sample-for-sample — the pre-feature sound.
        for (bool warble : { false, true }) {
            auto a = runMix(-1.0f, warble, 2.0);
            auto c = runMix(1.0f, warble, 2.0);
            bool exact = a.outL == c.outL && a.outR == c.outR;
            std::snprintf(b, sizeof b, "warble=%d: explicit mix=1 identical to untouched default: %d",
                          (int)warble, (int)exact);
            check(exact, warble ? "P7b MIX=1 bit-for-bit (MOD in)" : "P7b MIX=1 bit-for-bit (MOD out)", b);
        }
        // P7c: mix=0, MOD out: output == input delayed by
        // the reported latency (v53: 32 samples of
        // oversampling — the dry tap aligns to it, exactly
        // as it aligned to 960 with MOD in before).
        {
            auto d = runMix(0.0f, false, 1.5);
            bool exact = true;
            for (int i = settle; i < (int)d.outL.size(); ++i)
                if (d.outL[(size_t)i] != d.inL[(size_t)(i - 32)] || d.outR[(size_t)i] != d.inR[(size_t)(i - 32)])
                    exact = false;
            std::snprintf(b, sizeof b, "settled mix=0 output == input delayed 32 exactly: %d", (int)exact);
            check(exact, "P7c MIX=0 returns the input delayed by the latency (MOD out, v53)", b);
        }
        // P7d: mix=0, MOD in: output == input delayed by the
        // chain's 992-sample latency, exactly (v53).
        {
            auto d = runMix(0.0f, true, 1.5);
            bool exact = true;
            for (int i = settle; i < (int)d.outL.size(); ++i)
                if (d.outL[(size_t)i] != d.inL[(size_t)(i - 992)] || d.outR[(size_t)i] != d.inR[(size_t)(i - 992)])
                    exact = false;
            std::snprintf(b, sizeof b, "settled mix=0 (MOD in) == input delayed 992 exactly: %d", (int)exact);
            check(exact, "P7d MIX=0 returns the input delayed by the chain latency (MOD in)", b);
        }
        // P7e/P7f: mix=0.5 == the mean of the aligned dry and
        // the wet (a parallel instance at mix=1 — the chain
        // evolves identically, the blend is post-chain).
        for (bool warble : { false, true }) {
            auto w = runMix(1.0f, warble, 2.0);
            auto h = runMix(0.5f, warble, 2.0);
            const int lat = warble ? 992 : 32;
            double maxErr = 0;
            for (int i = settle; i < (int)w.outL.size(); ++i) {
                const double mL = 0.5 * ((double)w.inL[(size_t)(i - lat)] + w.outL[(size_t)i]);
                const double mR = 0.5 * ((double)w.inR[(size_t)(i - lat)] + w.outR[(size_t)i]);
                maxErr = std::max(maxErr, std::fabs(mL - h.outL[(size_t)i]));
                maxErr = std::max(maxErr, std::fabs(mR - h.outR[(size_t)i]));
            }
            std::snprintf(b, sizeof b, "warble=%d: max |mix0.5 - (dry+wet)/2| = %.2e", (int)warble, maxErr);
            check(maxErr < 2e-6, warble ? "P7f MIX=0.5 is the mean of aligned dry and wet (MOD in)"
                                        : "P7e MIX=0.5 is the mean of dry and wet (MOD out)", b);
        }
        // P7g: POWER off bypasses bit-exact whatever the mix.
        {
            auto d = runMix(0.3f, true, 2.0, 0.0f);
            bool exact = true;
            for (int i = (int)(1.0 * kFs); i < (int)d.outL.size(); ++i)
                if (d.outL[(size_t)i] != d.inL[(size_t)i] || d.outR[(size_t)i] != d.inR[(size_t)i])
                    exact = false;
            std::snprintf(b, sizeof b, "mix=0.3 + power off: settled output == input exactly: %d", (int)exact);
            check(exact, "P7g POWER off is bit-exact bypass regardless of mix", b);
        }
    }

    // ── P8. SIDECHAIN low-cut (v41) ───────────────────────────
    {
        // Through the whole APVTS -> processor -> Chain path:
        // the param exists (default 10 Hz, Hz strings), and a
        // 40 Hz tone's displayed GR falls materially when the
        // cutoff sweeps to 200 Hz while a 1 kHz tone's holds.
        auto grSteady = [&](float freq, float scNorm) {
            Osc72Processor q;
            q.prepareToPlay(kFs, 512);
            q.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.75f);
            if (scNorm >= 0.0f)
                q.apvts.getParameter("scCutoff")->setValueNotifyingHost(scNorm);
            juce::MidiBuffer midi;
            const int block = 512;
            double acc = 0; int cnt = 0;
            const int total = (int)(2.5 * kFs);
            for (int base = 0; base < total; base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    int i = base + j;
                    float s = 0.5f * (float)std::sin(2.0 * kPi * freq * i / kFs);
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                q.processBlock(buf, midi);
                if (base >= (int)(1.5 * kFs)) { acc += q.currentGainReductionDb(); ++cnt; }
            }
            return (float)(acc / cnt);
        };
        char b[256];
        {
            Osc72Processor q;
            auto* p = q.apvts.getParameter("scCutoff");
            bool ok = p != nullptr
                   && std::fabs(p->getValue() - p->convertTo0to1(10.0f)) < 1.0e-6
                   && p->getCurrentValueAsText().contains("Hz");
            std::snprintf(b, sizeof b, "exists=%d defaultNorm=%.4f text='%s'",
                          (int)(p != nullptr), p ? p->getValue() : -1.0f,
                          p ? p->getCurrentValueAsText().toRawUTF8() : "");
            check(ok, "P8a scCutoff param exists, default 10 Hz, Hz string", b);
            auto hi = p->getCurrentValueAsText(); (void)hi;
            p->setValueNotifyingHost(1.0f);
            std::snprintf(b, sizeof b, "at max: '%s'", p->getCurrentValueAsText().toRawUTF8());
            check(p->getCurrentValueAsText().contains("200"), "P8b scCutoff max reads 200 Hz", b);
        }
        float lfLo = grSteady(40.0f, -1.0f), lfHi = grSteady(40.0f, 1.0f);
        std::snprintf(b, sizeof b, "40 Hz GR: default %.2f dB, sc200 %.2f dB", lfLo, lfHi);
        check(lfLo - lfHi >= 4.0f, "P8c proc: 40 Hz GR falls at sc=200", b);
        float mfLo = grSteady(1000.0f, -1.0f), mfHi = grSteady(1000.0f, 1.0f);
        std::snprintf(b, sizeof b, "1 kHz GR: default %.2f dB, sc200 %.2f dB", mfLo, mfHi);
        check(std::fabs(mfHi - mfLo) <= 0.5f, "P8d proc: 1 kHz GR essentially unaffected", b);
    }

    // ── P9. MIX alignment at impulse level (v44) ─────────────
    {
        // One impulse at low drive ( knob 0: minimal GR), full
        // history. Params are set before processing starts, so
        // by the impulse (0.5 s in) every fade has settled.
        auto runImp = [](float mix, bool warble, int impAt, int total) {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(warble ? 1.0f : 0.0f);
            p.apvts.getParameter("mix")->setValueNotifyingHost(mix);
            juce::MidiBuffer midi;
            std::vector<float> out((size_t)total, 0.0f);
            const int block = 512;
            for (int base = 0; base + block <= total; base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    const float s = (base + j == impAt) ? 0.5f : 0.0f;
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                p.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) out[(size_t)(base + j)] = buf.getSample(0, j);
            }
            return out;
        };
        auto peakAt = [](const std::vector<float>& y, int centre, int halfWin, int& at) {
            float best = -1.0f; at = centre;
            for (int i = centre - halfWin; i <= centre + halfWin; ++i)
                if (std::fabs(y[(size_t)i]) > best) { best = std::fabs(y[(size_t)i]); at = i; }
            return best;
        };
        // Normalised cross-correlation peak lag of two impulse
        // responses over a window, lags in [lagMin, lagMax].
        auto xcorrLag = [](const std::vector<float>& a, const std::vector<float>& c,
                           int w0, int wLen, int lagMin, int lagMax) {
            int bestLag = 0; double best = -1e30;
            for (int lag = lagMin; lag <= lagMax; ++lag) {
                double s = 0.0;
                for (int i = 0; i < wLen; ++i)
                    s += (double)a[(size_t)(w0 + i)] * c[(size_t)(w0 + i + lag)];
                if (s > best) { best = s; bestLag = lag; }
            }
            return bestLag;
        };
        const int total = (int)(1.5 * kFs), impAt = (int)(0.5 * kFs);
        char b[256];
        for (bool warble : { false, true }) {
            const int lat = warble ? 992 : 32;
            auto dry = runImp(0.0f, warble, impAt, total);
            auto wet = runImp(1.0f, warble, impAt, total);
            auto half = runImp(0.5f, warble, impAt, total);
            int atDry = 0, atWet = 0, atHalf = 0;
            const float pkDry = peakAt(dry, impAt + lat, 400, atDry);
            const float pkWet = peakAt(wet, impAt + lat, 400, atWet);
            const float pkHalf = peakAt(half, impAt + lat, 400, atHalf);
            std::snprintf(b, sizeof b, "warble=%d: peaks dry@%+d wet@%+d half@%+d (want all %+d)",
                          (int)warble, atDry - impAt, atWet - impAt, atHalf - impAt, lat);
            check(atDry == impAt + lat && atWet == impAt + lat && atHalf == impAt + lat,
                  warble ? "P9a blended impulse peak exactly at +992 (MOD in, v53)"
                         : "P9a blended impulse peak exactly at +32 (MOD out, v53)", b);
            // Coincidence: the blended peak must reach the mean
            // of the two path peaks. Any path offset splits the
            // peak and the height collapses toward the larger
            // single-path share (~0.25 here vs the 0.40 mean).
            const float meanPk = 0.5f * (pkDry + pkWet);
            std::snprintf(b, sizeof b, "warble=%d: half peak %.4f vs mean-of-peaks %.4f",
                          (int)warble, pkHalf, meanPk);
            check(std::fabs(pkHalf - meanPk) < 0.03f * meanPk,
                  warble ? "P9b blend peak coincides (no split) (MOD in)"
                         : "P9b blend peak coincides (no split) (MOD out)", b);
            // Wet vs dry path cross-correlation: lag must be 0
            // about the nominal arrival.
            const int lag = xcorrLag(dry, wet, impAt + lat - 200, 600, -16, 16);
            std::snprintf(b, sizeof b, "warble=%d: xcorr(dry, wet) peak lag %+d (want 0)", (int)warble, lag);
            check(lag == 0, warble ? "P9c wet/dry cross-correlation offset 0 (MOD in)"
                                   : "P9c wet/dry cross-correlation offset 0 (MOD out)", b);
            // First sample over half the wet peak: the arrival
            // itself, not a deck pre-echo (deck peaks are ~1/4
            // of the SRC peak and cannot cross this threshold).
            int first = -1;
            for (int i = impAt; i < impAt + lat + 400 && first < 0; ++i)
                if (std::fabs(wet[(size_t)i]) > 0.5f * pkWet) first = i;
            std::snprintf(b, sizeof b, "warble=%d: first >50%% sample at %+d (want %+d)",
                          (int)warble, first - impAt, lat);
            // v53: the >50% crossing sits at lat-1 — the
            // linear-phase resampler's centre-1 tap already
            // exceeds half the peak (the peak itself is at
            // lat exactly, P9a). Pre-v53 the ceiling-shaped
            // response crossed at the latency sample itself.
            check(first == impAt + lat - 1, warble ? "P9d wet arrival at the latency (MOD in, v53)"
                                               : "P9d wet arrival at the latency (MOD out, v53)", b);
        }
        // P9e: MOD toggle mid-stream at mix 0.5 — the dry tap
        // must follow the chain's own 30 ms engagement law.
        // An impulse fired 5 ms into the fade has to appear at
        // BOTH offsets with crossfade weights (like the wet
        // path's own dry component does), not sit entirely at
        // the new offset while the chain is still morphing:
        // without the tap crossfade the +0 copy measures only
        // the wet path's fading share (~0.13 of the settled
        // 0.40 peak); with it, ~0.33.
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("mix")->setValueNotifyingHost(0.5f);
            juce::MidiBuffer midi;
            const int tot2 = 96000, flipBlock = 48128, midImp = flipBlock + 240;
            std::vector<float> out((size_t)tot2, 0.0f);
            const int block = 512;
            for (int base = 0; base + block <= tot2; base += block) {
                if (base == flipBlock)
                    p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    const int idx = base + j;
                    const float s = (idx == midImp) ? 0.5f : 0.0f;
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                p.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) out[(size_t)(base + j)] = buf.getSample(0, j);
            }
            int at0 = 0, at960 = 0;
            const float pk0 = peakAt(out, midImp + 32, 48, at0);
            const float pk960 = peakAt(out, midImp + 992, 96, at960);
            std::snprintf(b, sizeof b, "mid-fade impulse: +32 peak %.3f @%+d, +992 peak %.3f @%+d",
                          pk0, at0 - midImp, pk960, at960 - midImp);
            check(pk0 > 0.22f && at0 == midImp + 32 && at960 == midImp + 992,
                  "P9e dry tap crossfades its offset on MOD toggle (no step)", b);
        }
    }

    std::printf("%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL PROCESSOR TESTS PASSED" : "PROCESSOR TEST FAILURES",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
