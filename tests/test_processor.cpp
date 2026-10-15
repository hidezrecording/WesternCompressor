// tests/test_processor.cpp — processor-level tests (v3 diagnosis).
// Drives Osc72Processor::processBlock — the FULL shipped product
// path: APVTS parameter -> processor -> osc72::Chain -> WarbleChain
// — instead of the DSP classes alone. Built as the OscProcessorTest
// executable (see juce/CMakeLists.txt); run headless under xvfb.
//
// Why this exists (v3, Nathan's 1.0.2 verdict "MOD: not hearing
// any effect whatsoever"): the DSP suite proved the side-path
// warble through osc72::Chain, but nothing proved the plug-in
// wrapper delivers it. This test measures the MOD in-vs-out
// DIFFERENCE through processBlock on three material classes:
//   (a) WIDE stereo (strong side content),
//   (b) MONO (side == 0 — under the v10 mid/side design mono got
//       NO warble, bit-exactly, which is precisely what Nathan
//       heard as "no effect whatsoever"; the v3 mid tap retires
//       that: this test now pins mono as audibly warbled),
//   (c) TYPICAL stereo music-like material (dominant correlated
//       mid, modest side ~ -15 dB — v10 measured a -9 dB
//       difference here; v3's mid tap must beat that clearly).
// Difference metric: RMS(out_on - out_off) relative to RMS(out_off)
// over a settled window, plus side-RMS change.
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

// Deterministic per-material sample generator. `side` modes:
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

struct DiffMeas { double diffDb; double outRms; double sideRatioOnOff; bool bitExact; };

// Run two identical processors (warble off / on) in lockstep over
// the same material; measure over the settled window.
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
    double dSq = 0, oSq = 0, sOffSq = 0, sOnSq = 0; long n = 0;
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
                const double sA = ((double)aL - aR) * 0.5, sC = ((double)cL - cR) * 0.5;
                sOffSq += sA * sA; sOnSq += sC * sC;
                ++n;
            }
        }
    }
    DiffMeas m;
    const double diffRms = std::sqrt(dSq / (2.0 * n));
    m.outRms = std::sqrt(oSq / (2.0 * n));
    m.diffDb = 20.0 * std::log10(diffRms / m.outRms);
    m.sideRatioOnOff = std::sqrt(sOnSq / sOffSq);
    m.bitExact = exact;
    return m;
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    char b[192];

    // ── P1. MOD through the processor: wide material ─────────────
    for (float knob : { 0.0f, 5.0f }) {
        DiffMeas m = runPair(1, knob);
        std::snprintf(b, sizeof b, "knob %.0f: diff=%.2f dB rel out, out RMS=%.3f, side RMS on/off=%.3f",
                      knob, m.diffDb, m.outRms, m.sideRatioOnOff);
        check(m.diffDb > -14.0, "P1 proc MOD: WIDE material — big in/out difference", b);
    }

    // ── P2. MOD through the processor: MONO — was bit-exact (v10),
    // now audibly warbled (v3 mid tap; see docs/audio-v3-diagnosis.md)
    {
        DiffMeas m = runPair(0, 5.0f);
        std::snprintf(b, sizeof b, "mono: bit-exact=%d (v10: 1), diff=%.2f dB rel out (v10: -inf)",
                      (int)m.bitExact, m.diffDb);
        check(!m.bitExact && m.diffDb > -16.0,
              "P2 proc MOD: MONO material — now audibly warbled (v3)", b);
    }

    // ── P3. MOD through the processor: typical stereo material ────
    for (float knob : { 0.0f, 5.0f }) {
        DiffMeas m = runPair(2, knob);
        std::snprintf(b, sizeof b, "knob %.0f: diff=%.2f dB rel out, side RMS on/off=%.3f (v10 design: -8.95 dB)",
                      knob, m.diffDb, m.sideRatioOnOff);
        check(m.diffDb > -12.0, "P3 proc MOD: TYPICAL stereo — difference now unmistakable", b);
    }

    // ── P4. Reported latency follows MOD ──────────────────────────
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
        check(latOff == 0 && latOn == 576, "P4 proc: latency 0 off / 576 on", b);
    }

    std::printf("%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL PROCESSOR TESTS PASSED" : "PROCESSOR TEST FAILURES",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
