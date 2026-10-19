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
//   P7     the AB3 MIX POTS (compMix / modMix): both exist
//          with 0.5 (12 o'clock) defaults and percent
//          strings; the retired 'mix'/'scCutoff' are absent;
//          defaults == explicit 0.5/0.5 bit-for-bit; both
//          pots at 0 return the input delayed by exactly
//          the reported latency (32 MOD out / 992 MOD in);
//          each stage at 0.5 is the mean of its two aligned
//          arms through the full processor path; POWER off
//          bypasses bit-exact at partial pot settings
//   P8     AB3 legacy state: a 1.3.1 session chunk carrying
//          'scCutoff' and 'mix' values restores cleanly —
//          both params are gone and the orphaned values are
//          ignored (the P11a mechanics for this round's
//          deletions)
//   P10    MOD flavour A/P (v55): the 'modPhaser' param —
//          latency matrix (992 only for MOD-in A), mix=0 /
//          impulse alignment in the P flavour, A<->P mid-
//          stream toggle crossfades, POWER-off bit-exactness
//          and mono-stays-mono with P selected
//   P11    baked 2X ballistics (v64; was the v62 1X/2X
//          selector section): the 'timesTwo' param is
//          REMOVED — its absence and the tolerance of a
//          legacy 1.3.0 state carrying its value are
//          pinned, and the end-to-end attack/release
//          timings are pinned at the 1.3.0-at-2X reference
//          values the selector's 2X arm printed
//   P9     TWO-STAGE ALIGNMENT at impulse level (AB3;
//          the v44 diagnosis discipline carried to the new
//          scheme): P7's pins take the nominal latency as
//          their reference, so a stage arriving a sample
//          early or late would pass them. P9 measures the
//          paths themselves per stage: the comp stage's
//          partial blend peaks exactly at +32 coincident
//          (MOD out), the mod stage's exactly at +992
//          (MOD in A), and during a MOD toggle at a partial
//          modMix the impulse's energy is shared between
//          the offsets at crossfade scale — never a full-
//          weight copy stepped to either (the v44 flam,
//          now guarded at the chain's internal tap)
//   P12    HOST RE-PREPARE (v1.4.1): a same-rate
//          prepareToPlay re-call mid-stream (Luna's top-bar
//          power cycle) is bit-identical to no event, in
//          both MOD flavours — <= 1.4.0 parked the inserts
//          on that event (DSP suite section N has the
//          insert/chain-level pins)
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
static DiffMeas runPair(int mode, float knob, bool limitIn = true, bool phaser = false) {
    Osc72Processor pOff, pOn;
    for (auto* p : { &pOff, &pOn }) {
        p->prepareToPlay(kFs, 512);
        p->apvts.getParameter("allTheWay")->setValueNotifyingHost(knob / 10.0f);
        p->apvts.getParameter("limit")->setValueNotifyingHost(limitIn ? 1.0f : 0.0f);
        p->apvts.getParameter("power")->setValueNotifyingHost(1.0f);
        // AB3: both mix pots pinned at 1.0 (full) — these
        // MOD-difference pins and their thresholds were
        // calibrated at full engagement; the pots' own
        // behaviour (and their 0.5 defaults) is P7's subject.
        p->apvts.getParameter("compMix")->setValueNotifyingHost(1.0f);
        p->apvts.getParameter("modMix")->setValueNotifyingHost(1.0f);
    }
    pOn.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
    if (phaser)
        pOn.apvts.getParameter("modPhaser")->setValueNotifyingHost(1.0f);

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
        // AB2 pin move (cause: Nathan's AB2 ballistics —
        // TEST BUILD): with the seats re-timed to 3 / 15 ms
        // the FAST seat grabs much later inside the 10 ms
        // window, so both bursts ride the make-up ceiling
        // to within 0.2% of each other (measured FAST 1.377
        // vs SLOW 1.380, was x1.024 apart at 1.3.1). The
        // 1.5% margin no longer exists to assert; the pin
        // keeps the direction only.
        check(pkSlow >= pkFast, "P6b proc: SLOW attack lets the transient through", b);

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
        // AB2 pin move (cause: Nathan's AB2 ballistics —
        // TEST BUILD): the seats swap their early/late
        // relationship by design — SINGLE is now the 1.0 s
        // seat (50% recovery at ~0.32 s on the bench), so
        // at +0.35 s it has shed ~54% (readout 10.2 of
        // 22.3 dB = 0.457 of peak; clause 0.45 -> 0.50),
        // while DUAL's fast pole leads early and its 3 s
        // tail still holds 7.7 of 22.3 dB (0.345) at
        // +0.30 s — the DUAL clause passes UNMOVED.
        check(sgl.first > 5.0f && sgl.second < 0.50f * sgl.first && dul.second > 0.28f * dul.first,
              "P6c proc: SINGLE sheds most GR by +0.35 s, DUAL tail holds (readout, AB2)", b);
    }

    // ── P7. THE TWO MIX POTS (FULL2: bus channel gains) ────
    {
        // One processor over typical material, full histories.
        // compMix/modMix < 0 = leave the param untouched
        // (the 0.5 defaults).
        auto runPots = [](float compMix, float modMix, bool warble, bool phaser,
                          double seconds, float power = 1.0f) {
            struct Hist { std::vector<float> inL, inR, outL, outR; };
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.5f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(warble ? 1.0f : 0.0f);
            p.apvts.getParameter("modPhaser")->setValueNotifyingHost(phaser ? 1.0f : 0.0f);
            p.apvts.getParameter("power")->setValueNotifyingHost(power);
            if (compMix >= 0.0f)
                p.apvts.getParameter("compMix")->setValueNotifyingHost(compMix);
            if (modMix >= 0.0f)
                p.apvts.getParameter("modMix")->setValueNotifyingHost(modMix);
            Material mat{ 2 };
            const int block = 512;
            // Whole blocks only — a part-block tail would sit
            // in the histories as unprocessed zeros.
            const int total = ((int)(seconds * kFs) / block) * block;
            Hist r;
            r.inL.assign((size_t)total, 0); r.inR.assign((size_t)total, 0);
            r.outL.assign((size_t)total, 0); r.outR.assign((size_t)total, 0);
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

        // P7a: both params exist with 0.5 defaults (12
        // o'clock, Nathan) and plain percent strings; the
        // retired 'mix' and 'scCutoff' params are absent.
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            auto* pc = p.apvts.getParameter("compMix");
            auto* pm = p.apvts.getParameter("modMix");
            auto* pOldMix = p.apvts.getParameter("mix");
            auto* pOldSc = p.apvts.getParameter("scCutoff");
            std::snprintf(b, sizeof b, "compMix default=%.3f text='%s', modMix default=%.3f; mix %s, scCutoff %s",
                          pc ? (double)pc->getValue() : -1.0,
                          pc ? pc->getCurrentValueAsText().toRawUTF8() : "",
                          pm ? (double)pm->getValue() : -1.0,
                          pOldMix ? "STILL EXISTS" : "absent",
                          pOldSc ? "STILL EXISTS" : "absent");
            check(pc && pm && !pOldMix && !pOldSc
                  && std::fabs(pc->getValue() - 0.5f) < 1e-6
                  && std::fabs(pm->getValue() - 0.5f) < 1e-6
                  && pc->getCurrentValueAsText() == "50"
                  && pm->getCurrentValueAsText() == "50",
                  "P7a AB3 pots exist, default 0.5 ('50'); 'mix'/'scCutoff' removed", b);
        }
        // P7b: untouched defaults == explicit 0.5/0.5,
        // sample-for-sample (the defaults ARE the centred
        // pots; nothing about the opening state is implied
        // by an untouched instance beyond them).
        for (bool warble : { false, true }) {
            auto a = runPots(-1.0f, -1.0f, warble, false, 2.0);
            auto c = runPots(0.5f, 0.5f, warble, false, 2.0);
            bool exact = a.outL == c.outL && a.outR == c.outR;
            std::snprintf(b, sizeof b, "warble=%d: explicit 0.5/0.5 identical to untouched defaults: %d",
                          (int)warble, (int)exact);
            check(exact, warble ? "P7b defaults == explicit 0.5/0.5 bit-for-bit (MOD in)"
                                : "P7b defaults == explicit 0.5/0.5 bit-for-bit (MOD out)", b);
        }
        // P7c (FULL2 form; the AB3 form pinned "output =
        // input delayed" via the crossfade's dry arm —
        // there is no dry arm now): both pots at 0, MOD
        // out: the bus sums two zero-gain channels = exact
        // digital silence once the pot slews have settled.
        {
            auto d = runPots(0.0f, 0.0f, false, false, 1.5);
            bool exact = true;
            for (int i = settle; i < (int)d.outL.size(); ++i)
                if (d.outL[(size_t)i] != 0.0f || d.outR[(size_t)i] != 0.0f)
                    exact = false;
            std::snprintf(b, sizeof b, "settled compMix=0/modMix=0 output == 0.0f exactly: %d", (int)exact);
            check(exact, "P7c both pots 0: digital silence at the bus (MOD out)", b);
        }
        // P7d (FULL2 form): both pots at 0, MOD in (A):
        // silence — the gate cannot resurrect a zero-gain
        // channel either.
        {
            auto d = runPots(0.0f, 0.0f, true, false, 1.5);
            bool exact = true;
            for (int i = settle; i < (int)d.outL.size(); ++i)
                if (d.outL[(size_t)i] != 0.0f || d.outR[(size_t)i] != 0.0f)
                    exact = false;
            std::snprintf(b, sizeof b, "settled compMix=0/modMix=0 (MOD in) == 0.0f exactly: %d", (int)exact);
            check(exact, "P7d both pots 0: digital silence at the bus (MOD in)", b);
        }
        // P7e (FULL2 form): the COMP channel's gain is
        // linear — at 0.5 the bus carries exactly half the
        // comp channel (the compMix=0 render is now the
        // silent bus, so the old mean-of-arms formula
        // reduces to half the full render; it still holds
        // to 0.00e+00 here, MOD out, because the bus
        // make-up trajectory is common and the cap is
        // clear of this material at knob 0.5).
        {
            auto d = runPots(0.0f, 1.0f, false, false, 2.0);
            auto w = runPots(1.0f, 1.0f, false, false, 2.0);
            auto h = runPots(0.5f, 1.0f, false, false, 2.0);
            double maxErr = 0;
            for (int i = settle; i < (int)w.outL.size(); ++i) {
                const double mL = 0.5 * ((double)d.outL[(size_t)i] + w.outL[(size_t)i]);
                const double mR = 0.5 * ((double)d.outR[(size_t)i] + w.outR[(size_t)i]);
                maxErr = std::max(maxErr, std::fabs(mL - h.outL[(size_t)i]));
                maxErr = std::max(maxErr, std::fabs(mR - h.outR[(size_t)i]));
            }
            std::snprintf(b, sizeof b, "max |compMix0.5 - (zero+full)/2| = %.2e", maxErr);
            check(maxErr < 2e-6, "P7e compMix=0.5 is exactly half the comp channel (MOD out, FULL2 gain law)", b);
        }
        // P7f (FULL2 form): the MOD channel's gain sums
        // linearly into the bus — at 0.5 the output is the
        // mean of the comp-only (modMix=0) and both-
        // channels (modMix=1) renders — UP TO the bus
        // ceiling: the summed render's higher bus peaks
        // engage the peak-aware cap where the single-
        // channel renders do not shave the same peaks, so
        // exact superposition breaks at the loudest
        // transients by design (measured 2.89e-03 max on
        // this material at knob 0.5 — a fraction of a dB
        // on peak samples; the DSP suite's L4 pins the
        // exact law in the cap-free regime, and M2 pins
        // the bound). Bound set from the measurement with
        // headroom.
        {
            auto d = runPots(1.0f, 0.0f, true, true, 2.0);
            auto w = runPots(1.0f, 1.0f, true, true, 2.0);
            auto h = runPots(1.0f, 0.5f, true, true, 2.0);
            double maxErr = 0;
            for (int i = settle; i < (int)w.outL.size(); ++i) {
                const double mL = 0.5 * ((double)d.outL[(size_t)i] + w.outL[(size_t)i]);
                const double mR = 0.5 * ((double)d.outR[(size_t)i] + w.outR[(size_t)i]);
                maxErr = std::max(maxErr, std::fabs(mL - h.outL[(size_t)i]));
                maxErr = std::max(maxErr, std::fabs(mR - h.outR[(size_t)i]));
            }
            std::snprintf(b, sizeof b, "max |modMix0.5 - (compOnly+full)/2| = %.2e (MOD in, P; bus-cap residue, see comment)", maxErr);
            check(maxErr < 6e-3, "P7f modMix=0.5 sums the channels (mean form; bus-cap residue bounded, FULL2)", b);
        }
        // P7g: POWER off bypasses bit-exact at PARTIAL pot
        // settings (the brief's explicit case: the bypass
        // must not depend on either pot).
        {
            auto d = runPots(0.3f, 0.7f, true, false, 2.0, 0.0f);
            bool exact = true;
            for (int i = (int)(1.0 * kFs); i < (int)d.outL.size(); ++i)
                if (d.outL[(size_t)i] != d.inL[(size_t)i] || d.outR[(size_t)i] != d.inR[(size_t)i])
                    exact = false;
            std::snprintf(b, sizeof b, "compMix=0.3/modMix=0.7 + power off: settled output == input exactly: %d", (int)exact);
            check(exact, "P7g POWER off is bit-exact bypass at partial pot settings", b);
        }
    }

    // ── P8. LEGACY STATE (AB3): 'scCutoff' + 'mix' orphans ──
    {
        // Both parameters are GONE (Nathan's repurposing:
        // the sidechain feature is deleted and the whole-
        // chain mix is superseded by the two-stage scheme).
        // A 1.3.1 session state carrying their values must
        // restore cleanly — the orphaned entries are
        // ignored, the surviving parameters land exactly
        // (the P11a mechanics: the state is built from this
        // build's own state XML with orphaned PARAM
        // elements injected, byte-for-byte the shape a
        // 1.3.1 chunk presents to setStateInformation).
        Osc72Processor src;
        src.prepareToPlay(kFs, 512);
        src.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.7f);
        src.apvts.getParameter("compMix")->setValueNotifyingHost(0.8f);
        const float srcKnob = src.apvts.getParameter("allTheWay")->getValue();
        const float srcComp = src.apvts.getParameter("compMix")->getValue();
        juce::MemoryBlock mb;
        src.getStateInformation(mb);
        bool injected = false;
        Osc72Processor dst;
        if (auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), (int)mb.getSize())) {
            for (auto kv : { std::pair<const char*, const char*> { "scCutoff", "0.85" },
                             std::pair<const char*, const char*> { "mix", "0.25" } }) {
                auto* orphan = new juce::XmlElement("PARAM");
                orphan->setAttribute("id", kv.first);
                orphan->setAttribute("value", kv.second);
                xml->addChildElement(orphan);
            }
            juce::MemoryBlock mb2;
            juce::AudioProcessor::copyXmlToBinary(*xml, mb2);
            dst.setStateInformation(mb2.getData(), (int)mb2.getSize());
            injected = true;
        }
        auto* pMix = dst.apvts.getParameter("mix");
        auto* pSc = dst.apvts.getParameter("scCutoff");
        const float dstKnob = dst.apvts.getParameter("allTheWay")->getValue();
        const float dstComp = dst.apvts.getParameter("compMix")->getValue();
        std::snprintf(b, sizeof b, "mix %s, scCutoff %s; state injected=%d, knob %.3f -> %.3f, compMix %.3f -> %.3f",
                      pMix ? "STILL EXISTS" : "absent", pSc ? "STILL EXISTS" : "absent",
                      (int)injected, srcKnob, dstKnob, srcComp, dstComp);
        check(pMix == nullptr && pSc == nullptr && injected
              && std::fabs(dstKnob - srcKnob) < 0.001f
              && std::fabs(dstComp - srcComp) < 0.001f,
              "P8 'scCutoff'/'mix' removed; 1.3.1 legacy state restores, orphans ignored", b);
    }

    // ── P9. BUS ALIGNMENT at impulse level (FULL2) ────────
    {
        // One impulse at low drive (knob 0: minimal GR), full
        // history. Params are set before processing starts, so
        // by the impulse (0.5 s in) every fade has settled.
        auto runImp2 = [](float compMix, float modMix, bool warble, bool phaser,
                          int impAt, int total) {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(warble ? 1.0f : 0.0f);
            p.apvts.getParameter("modPhaser")->setValueNotifyingHost(phaser ? 1.0f : 0.0f);
            p.apvts.getParameter("compMix")->setValueNotifyingHost(compMix);
            p.apvts.getParameter("modMix")->setValueNotifyingHost(modMix);
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
        const int total = (int)(1.5 * kFs), impAt = (int)(0.5 * kFs);
        char b[256];
        // P9a/P9b (FULL2 form): the COMP channel at
        // partial gain (MOD out): compMix=0 is exactly
        // silent (no dry arm exists to peak), compMix=1
        // and 0.5 both peak exactly at +32, and the half
        // gain's peak is half the full channel's — a
        // misaligned or stepped gain would split the peak
        // or break the ratio.
        {
            auto dry = runImp2(0.0f, 1.0f, false, false, impAt, total);
            auto wet = runImp2(1.0f, 1.0f, false, false, impAt, total);
            auto half = runImp2(0.5f, 1.0f, false, false, impAt, total);
            int atWet = 0, atHalf = 0;
            const float pkWet = peakAt(wet, impAt + 32, 400, atWet);
            const float pkHalf = peakAt(half, impAt + 32, 400, atHalf);
            float dryMax = 0;
            for (float v : dry) dryMax = std::max(dryMax, std::fabs(v));
            std::snprintf(b, sizeof b, "comp channel: cm0 max %.1e (want 0), peaks wet@%+d half@%+d (want +32)",
                          (double)dryMax, atWet - impAt, atHalf - impAt);
            check(dryMax == 0.0f && atWet == impAt + 32 && atHalf == impAt + 32,
                  "P9a comp channel impulse peak exactly at +32; compMix=0 silent (MOD out, FULL2)", b);
            std::snprintf(b, sizeof b, "half peak %.4f vs half the full peak %.4f", pkHalf, 0.5f * pkWet);
            check(std::fabs(pkHalf - 0.5f * pkWet) < 0.03f * pkWet,
                  "P9b comp channel gain is linear at the impulse peak (no split)", b);
        }
        // P9c/P9d (FULL2 reading): the channels at the
        // bus (MOD in A, compMix=1): the comp channel
        // alone (modMix=0), both channels (1) and comp +
        // half ADT (0.5) all peak exactly at +992,
        // coincident — the alignment the bus is built on,
        // and the half render's peak is the channels'
        // mean (superposition at the peak).
        {
            auto dry = runImp2(1.0f, 0.0f, true, false, impAt, total);
            auto wet = runImp2(1.0f, 1.0f, true, false, impAt, total);
            auto half = runImp2(1.0f, 0.5f, true, false, impAt, total);
            int atDry = 0, atWet = 0, atHalf = 0;
            const float pkDry = peakAt(dry, impAt + 992, 400, atDry);
            const float pkWet = peakAt(wet, impAt + 992, 400, atWet);
            const float pkHalf = peakAt(half, impAt + 992, 400, atHalf);
            std::snprintf(b, sizeof b, "bus channels: peaks compOnly@%+d both@%+d half@%+d (want all +992)",
                          atDry - impAt, atWet - impAt, atHalf - impAt);
            check(atDry == impAt + 992 && atWet == impAt + 992 && atHalf == impAt + 992,
                  "P9c bus channels' impulse peaks coincide exactly at +992 (MOD in, A)", b);
            const float meanPk = 0.5f * (pkDry + pkWet);
            std::snprintf(b, sizeof b, "half peak %.4f vs mean-of-peaks %.4f", pkHalf, meanPk);
            check(std::fabs(pkHalf - meanPk) < 0.03f * meanPk,
                  "P9d half-gain bus peak is the channels' mean (no split)", b);
        }
        // P9e: MOD toggle mid-stream at compMix=1 /
        // modMix=0.5 — the engagement edge in FULL2 form:
        // the comp channel must crossfade its delay
        // offset (32 -> 992) with the chain's 30 ms law
        // while the MOD channel's gate ramps up (1.47 s):
        // an impulse fired 5 ms into the edge has to
        // appear at BOTH offsets at partial weight, never
        // as a full-weight copy stepped to either one.
        // FULL2 readings on this tree: +32 peak 0.167,
        // +992 peak 0.230 — inside the standing bounds,
        // which are kept.
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("compMix")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("modMix")->setValueNotifyingHost(0.5f);
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
            int at32 = 0, at992 = 0;
            const float pk32 = peakAt(out, midImp + 32, 48, at32);
            const float pk992 = peakAt(out, midImp + 992, 96, at992);
            std::snprintf(b, sizeof b, "mid-edge impulse: +32 peak %.3f @%+d, +992 peak %.3f @%+d",
                          pk32, at32 - midImp, pk992, at992 - midImp);
            check(pk32 > 0.05f && pk32 < 0.30f && pk992 > 0.02f && pk992 < 0.30f
                  && at32 == midImp + 32 && at992 == midImp + 992,
                  "P9e engagement edge: comp channel crossfades its offset, gate ramps (no step)", b);
        }
    }

    // ── P10. MOD flavour A/P (v55): the 'modPhaser' param ────
    // A (false, default) = the Reel ADT exactly as shipped;
    // P (true) = the script-era Phase 90. Latency must track
    // the flavour truthfully (992 <-> 32 with MOD in), the
    // mod stage's offset crossfade (AB3; the v44 dry tap's
    // heir, inside the chain) must carry A<->P edges exactly
    // as it carries MOD edges, and the POWER-off / full-pot
    // invariants must hold in the P flavour too.
    {
        char b[256];
        // P10a: param exists, default A (false).
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            auto* pm = p.apvts.getParameter("modPhaser");
            std::snprintf(b, sizeof b, "exists=%d default=%.3f (want 0.000 = A)",
                          (int)(pm != nullptr), pm ? (double)pm->getValue() : -1.0);
            check(pm && pm->getValue() < 0.5f,
                  "P10a modPhaser param exists, default A (ADT)", b);
        }
        // P10b: reported latency across the state matrix,
        // edges consumed in processBlock one at a time.
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            auto edge = [&](const char* id, float v) {
                p.apvts.getParameter(id)->setValueNotifyingHost(v);
                buf.clear();
                p.processBlock(buf, midi);
                return p.getLatencySamples();
            };
            const int s0 = p.getLatencySamples();              // A, MOD out
            const int s1 = edge("warble", 1.0f);               // A, MOD in
            const int s2 = edge("modPhaser", 1.0f);            // P, MOD in
            const int s3 = edge("warble", 0.0f);               // P, MOD out
            const int s4 = edge("modPhaser", 0.0f);            // A, MOD out
            const int s5 = edge("warble", 1.0f);               // A, MOD in again
            std::snprintf(b, sizeof b, "latencies %d / %d / %d / %d / %d / %d (want 32/992/32/32/32/992)",
                          s0, s1, s2, s3, s4, s5);
            check(s0 == 32 && s1 == 992 && s2 == 32 && s3 == 32 && s4 == 32 && s5 == 992,
                  "P10b proc: latency follows flavour — 992 only for MOD-in A", b);
        }
        // P10c (FULL2 form; the AB3 form pinned the dry
        // arm's delayed return): both pots at 0 with MOD
        // in + P — the bus is silent, exactly, like every
        // both-gains-zero state (P7c/P7d in the A flavour
        // and MOD out).
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.5f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("modPhaser")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("compMix")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("modMix")->setValueNotifyingHost(0.0f);
            Material mat{ 2 };
            const int block = 512;
            const int total = ((int)(1.5 * kFs) / block) * block;
            std::vector<float> inL((size_t)total), inR((size_t)total),
                               outL((size_t)total), outR((size_t)total);
            juce::MidiBuffer midi;
            for (int base = 0; base + block <= total; base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    float L, R; mat.sample(base + j, L, R);
                    inL[(size_t)(base + j)] = L; inR[(size_t)(base + j)] = R;
                    buf.setSample(0, j, L); buf.setSample(1, j, R);
                }
                p.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) {
                    outL[(size_t)(base + j)] = buf.getSample(0, j);
                    outR[(size_t)(base + j)] = buf.getSample(1, j);
                }
            }
            bool exact = true;
            for (int i = (int)(0.5 * kFs); i < total; ++i)
                if (outL[(size_t)i] != 0.0f || outR[(size_t)i] != 0.0f)
                    exact = false;
            std::snprintf(b, sizeof b, "settled pots=0 (MOD in, P) == 0.0f exactly: %d", (int)exact);
            check(exact, "P10c both pots 0: digital silence at the bus (MOD in, P flavour)", b);
        }
        // P10d: impulse alignment with MOD in + P (P9
        // methodology at the 32-sample latency): the bus
        // channels — compOnly = modMix 0 (the comp channel
        // alone), both = modMix 1 (comp + the pedal's own
        // output), half = modMix 0.5 — at compMix 1.
        // (AB3 read its arms as dry/cascade/blend; FULL2's
        // "wet" render carries the comp channel too, which
        // is why its peak now sits at +32 outright —
        // readings below.)
        {
            auto runImpP = [](float modMix, int impAt, int total) {
                Osc72Processor p;
                p.prepareToPlay(kFs, 512);
                p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.0f);
                p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
                p.apvts.getParameter("modPhaser")->setValueNotifyingHost(1.0f);
                p.apvts.getParameter("compMix")->setValueNotifyingHost(1.0f);
                p.apvts.getParameter("modMix")->setValueNotifyingHost(modMix);
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
            const int total = (int)(1.5 * kFs), impAt = (int)(0.5 * kFs);
            auto dry = runImpP(0.0f, impAt, total);
            auto wet = runImpP(1.0f, impAt, total);
            auto half = runImpP(0.5f, impAt, total);
            int atDry = 0, atWet = 0;
            const float pkDry = peakAt(dry, impAt + 32, 200, atDry);
            const float pkWet = peakAt(wet, impAt + 32, 200, atWet);
            // Instrumentation (AB3 origin; FULL2 reading):
            // the cascade's own IR is dispersed over tens of
            // samples with its maximum a few samples in and
            // well under the comp channel's IR peak, so
            // peak-coincidence is the wrong instrument for
            // it. What alignment means here: the bus's
            // response ONSET lands on the latency (nothing
            // arrives early, no bulk delay), the comp
            // channel's peak sits exactly at +32, and the
            // channels superpose (P10e). FULL2 readings:
            // compOnly peak @+32, both-channels peak @+32
            // (the comp channel's spike dominates the sum),
            // onset @+30.
            int onset = -1;
            for (int i = impAt; i < impAt + 64 && onset < 0; ++i)
                if (std::fabs(wet[(size_t)i]) > 0.25f * pkWet) onset = i;
            std::snprintf(b, sizeof b, "P flavour: compOnly peak %.3f @%+d; both peak %.3f @%+d, onset @%+d (want +30..+38)",
                          pkDry, atDry - impAt, pkWet, atWet - impAt, onset - impAt);
            // The onset reads +31 (inside the +30 bound):
            // the limiter's linear-phase resampler pre-rings
            // a sample ahead of its +32 peak (the shipped
            // P9d documented its >50% crossing at lat-1) and
            // the cascade answers instantly through its
            // direct term — pre-ringing, not an early
            // arrival: the energy peak and the dry arm both
            // sit at the latency, and P10e/P10f close the
            // question.
            check(atDry == impAt + 32
                  && atWet >= impAt + 32 && atWet <= impAt + 36
                  && onset >= impAt + 30 && onset <= impAt + 38,
                  "P10d P flavour: comp channel peaks at +32, bus onset at the latency (no bulk delay, FULL2)", b);
            // P10e (FULL2 form): the half-gain render is
            // the superposition of the channel renders
            // through the whole processor path — to float
            // association, not sample-exactly: the bus
            // forms g*(comp + 0.5*ph) in one association
            // where the check forms 0.5*(g*comp) +
            // 0.5*(g*(comp+ph)) (the AB3 chain formed the
            // blend in the same association as the check,
            // hence its old sample-exactness). The bus
            // make-up is common across the renders (knob
            // 0, cap far clear), so the residue is pure
            // rounding; bound 2e-6 as in P7e. (dry here =
            // the comp channel alone, modMix=0; wet = both
            // channels, modMix=1.)
            double supErr = 0;
            for (int i = (int)(0.4 * kFs); i < total; ++i)
                supErr = std::max(supErr, (double)std::fabs(
                    half[(size_t)i] - (dry[(size_t)i] * (1.0f - 0.5f) + wet[(size_t)i] * 0.5f)));
            std::snprintf(b, sizeof b, "max |half - (0.5*compOnly + 0.5*both)| = %.2e", supErr);
            check(supErr < 2e-6, "P10e modMix=0.5 superposes the channels to float association (MOD in, P flavour, FULL2)", b);
            int bestLag = 0; double bestC = -1e30;
            for (int lag = -16; lag <= 16; ++lag) {
                double s = 0.0;
                for (int i = 0; i < 600; ++i)
                    s += (double)dry[(size_t)(impAt + 32 - 200 + i)]
                       * wet[(size_t)(impAt + 32 - 200 + i + lag)];
                if (s > bestC) { bestC = s; bestLag = lag; }
            }
            std::snprintf(b, sizeof b, "xcorr(dry, wet) peak lag %+d (want 0..+4, AB3 cascade dispersion)", bestLag);
            // AB3 bound move (cause as P10d/J8): the wet arm
            // is the bare cascade, whose group delay shifts
            // the correlation maximum a few samples late
            // (measured +3) with no bulk delay anywhere.
            check(bestLag >= 0 && bestLag <= 4, "P10f wet/dry cross-correlation offset 0..+4 (MOD in, P flavour; cascade dispersion, AB3)", b);
        }
        // P10g/P10h: flavour toggles MID-STREAM at
        // compMix=1 / modMix=0.5 (the P9e pattern on the
        // A/P edges). An impulse fired 5 ms into the flip
        // must ride the crossfades — never a full-weight
        // copy stepped to either offset. The two directions
        // differ geometrically (the ADT's 960-sample pipe
        // reads out late on A->P); each direction's
        // assertions are spelled out at the checks.
        auto flipTest = [&](bool toPhaser, const char* name) {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
            if (!toPhaser) // P->A: start settled in P
                p.apvts.getParameter("modPhaser")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("compMix")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("modMix")->setValueNotifyingHost(0.5f);
            juce::MidiBuffer midi;
            const int tot = 96000, flipBlock = 48128, midImp = flipBlock + 240;
            std::vector<float> out((size_t)tot, 0.0f);
            const int block = 512;
            for (int base = 0; base + block <= tot; base += block) {
                if (base == flipBlock)
                    p.apvts.getParameter("modPhaser")->setValueNotifyingHost(toPhaser ? 1.0f : 0.0f);
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    const int idx = base + j;
                    const float s = (idx == midImp) ? 0.5f : 0.0f;
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                p.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) out[(size_t)(base + j)] = buf.getSample(0, j);
            }
            auto peakAt = [](const std::vector<float>& y, int centre, int halfWin, int& at) {
                float best = -1.0f; at = centre;
                for (int i = centre - halfWin; i <= centre + halfWin; ++i)
                    if (std::fabs(y[(size_t)i]) > best) { best = std::fabs(y[(size_t)i]); at = i; }
                return best;
            };
            int at32 = 0, at992 = 0;
            const float pk32 = peakAt(out, midImp + 32, 48, at32);
            const float pk992 = peakAt(out, midImp + 992, 96, at992);
            char bb[256];
            std::snprintf(bb, sizeof bb, "mid-flip impulse: +32 peak %.3f @%+d, +992 peak %.3f @%+d",
                          pk32, at32 - midImp, pk992, at992 - midImp);
            if (toPhaser) {
                // A->P geometry (FULL2): the comp channel's
                // delay crossfades 960 -> 0 while the flavour
                // blend swaps the insert's output ADT ->
                // cascade; the impulse's energy is SHARED
                // between the offsets at crossfade weight,
                // neither copy near settled height. FULL2
                // readings on this tree: 0.042 @+32 / 0.041
                // @+992 — inside the standing bounds (kept).
                // (AB3 read 0.015 @+31 / 0.027 @+992 with its
                // dry side at 0.5 weight.)
                check(pk32 > 0.005f && pk992 > 0.01f && pk32 < 0.15f && pk992 < 0.15f
                      && std::abs(at32 - (midImp + 32)) <= 2 && at992 == midImp + 992, name, bb);
            } else {
                // P->A geometry (FULL2): the comp channel's
                // rising late read plus the ADT pipe's
                // emerging SRC carry the +992 share; the +32
                // copy is the comp channel's early read plus
                // the outgoing cascade. FULL2 readings on
                // this tree: 0.179 @+32 / 0.241 @+992 —
                // inside the standing bounds (kept). (AB3
                // read 0.063 @+31 / 0.161 @+992.)
                check(pk32 > 0.03f && pk32 < 0.20f && pk992 > 0.05f && pk992 < 0.30f
                      && std::abs(at32 - (midImp + 32)) <= 2 && at992 == midImp + 992, name, bb);
            }
        };
        flipTest(true,  "P10g channel/flavour crossfades on A->P toggle (no step)");
        flipTest(false, "P10h channel/flavour crossfades on P->A toggle (no step)");
        // P10i: POWER off is bit-exact bypass with MOD in +
        // P selected AND partial pot settings (the P5
        // invariant in the new flavour, at the AB3 pots'
        // working positions).
        {
            Osc72Processor p;
            p.prepareToPlay(kFs, 512);
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.7f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("modPhaser")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("compMix")->setValueNotifyingHost(0.3f);
            p.apvts.getParameter("modMix")->setValueNotifyingHost(0.7f);
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
                if (base >= (int)(1.0 * kFs))
                    for (int j = 0; j < block; ++j)
                        if (buf.getSample(0, j) != inL[(size_t)j] || buf.getSample(1, j) != inR[(size_t)j])
                            exact = false;
            }
            std::snprintf(b, sizeof b, "settled power-off (MOD in, P) == input exactly: %d", (int)exact);
            check(exact, "P10i proc: POWER off is bit-exact bypass (MOD in, P flavour)", b);
        }
        // P10j: the P flavour is audible end-to-end on MONO
        // material — and, unlike the ADT, it does NOT widen:
        // the mono source stays mono (side/out ~ 0).
        {
            DiffMeas m = runPair(0, 5.0f, true, true);
            std::snprintf(b, sizeof b, "mono + P: bit-exact=%d, diff=%.2f dB, side/out=%.4f (ADT: >0.20)",
                          (int)m.bitExact, m.diffDb, m.sideRatio);
            check(!m.bitExact && m.diffDb > -20.0 && m.sideRatio < 0.01,
                  "P10j proc MOD flavour P: mono — phaser audible, no widening", b);
        }
    }

    // ── P11. BAKED 2X BALLISTICS (v64; the v62 1X/2X
    // ── selector section, re-based) ───────────────────────
    // The 'timesTwo' parameter is GONE (Nathan's 1.3.0 ear
    // verdict retired the 1X speeds; the 2X ballistics are
    // baked into the DSP). This section pins what remains:
    // the parameter's absence and the legacy-state
    // tolerance around it (P11a), and the end-to-end
    // timings at their 1.3.0-at-2X reference values through
    // processBlock (P11b/c — the same protocols the v62
    // pins ran against the selector's 2X arm, captured
    // from the pre-change suite run), plus the latency
    // figures the selector never touched (P11d).
    {
        // P11a: 'timesTwo' no longer exists, and a 1.3.0
        // session state carrying its value restores
        // cleanly — the orphaned entry is ignored, the
        // surviving parameters land exactly. (The state is
        // built from this build's own state XML with an
        // orphaned PARAM element injected, i.e. byte-for-
        // byte the shape a 1.3.0 chunk presents to
        // setStateInformation: apvts.replaceState matches
        // values to parameters by ID and never looks up an
        // ID that no longer exists.)
        {
            Osc72Processor src;
            src.prepareToPlay(kFs, 512);
            src.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.7f);
            const float srcKnob = src.apvts.getParameter("allTheWay")->getValue();
            juce::MemoryBlock mb;
            src.getStateInformation(mb);
            bool injected = false;
            Osc72Processor dst;
            if (auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), (int)mb.getSize())) {
                auto* orphan = new juce::XmlElement("PARAM");
                orphan->setAttribute("id", "timesTwo");
                orphan->setAttribute("value", "1.0");
                xml->addChildElement(orphan);
                juce::MemoryBlock mb2;
                juce::AudioProcessor::copyXmlToBinary(*xml, mb2);
                dst.setStateInformation(mb2.getData(), (int)mb2.getSize());
                injected = true;
            }
            auto* pt = dst.apvts.getParameter("timesTwo");
            const float dstKnob = dst.apvts.getParameter("allTheWay")->getValue();
            std::snprintf(b, sizeof b, "param %s; state injected=%d, knob %.3f -> %.3f after legacy restore",
                          pt ? "STILL EXISTS" : "absent", (int)injected, srcKnob, dstKnob);
            check(pt == nullptr && injected && std::fabs(dstKnob - srcKnob) < 0.001f,
                  "P11a 'timesTwo' removed; 1.3.0 legacy state restores, orphan ignored", b);
        }
        // P11b: end-to-end attack — the GR readout's rise
        // time (t90) with the SLOW seat selected, measured
        // on 64-sample blocks (the v62 protocol). Reference:
        // 16.0 ms — the figure the 2X arm printed on the
        // 1.3.0 tree. (A burst-peak method cannot see the
        // ballistics: at this depth the early peaks ride the
        // make-up ceiling identically — the P6b ceiling note
        // — while the GR trajectory itself carries them.)
        auto grRiseT90 = [&]() {
            Osc72Processor q;
            q.prepareToPlay(kFs, 64);
            q.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.75f);
            q.apvts.getParameter("attackFast")->setValueNotifyingHost(0.0f);
            juce::MidiBuffer midi;
            const int block = 64;
            const int onset = (int)(0.3 * kFs);
            const int total = onset + (int)(0.3 * kFs);
            std::vector<float> rd;
            for (int base = 0; base + block <= total; base += block) {
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    int i = base + j;
                    float s = i >= onset
                        ? 0.6f * (float)std::sin(2.0 * kPi * 1000.0 * i / kFs) : 0.0f;
                    buf.setSample(0, j, s); buf.setSample(1, j, s);
                }
                q.processBlock(buf, midi);
                if (base >= onset) rd.push_back(q.currentGainReductionDb());
            }
            const float settled = rd.back();
            for (size_t k = 0; k < rd.size(); ++k)
                if (rd[k] >= 0.9f * settled)
                    return (double)(k + 1) * block / kFs * 1000.0;
            return -1.0;
        };
        double r = grRiseT90();
        std::snprintf(b, sizeof b, "SLOW attack GR rise t90: baked %.1f ms vs 1.3.0-at-2X ref 16.0 ms", r);
        check(std::fabs(r - 16.0) <= (64.0 / kFs * 1000.0),
              "P11b proc: baked SLOW attack rise == 1.3.0-at-2X end-to-end (readout)", b);

        // P11c: end-to-end release — the GR readout's 50%
        // recovery time after a loud passage (DUAL seat),
        // same small-block readout method; the peak depth
        // it falls FROM is part of the pin. References from
        // the 1.3.0 tree's 2X arm: 0.103 s from 22.5 dB.
        // AB2 pin move (cause: Nathan's AB2 ballistics —
        // TEST BUILD): at the faster DUAL scale the same
        // protocol reads 0.079 s from 22.3 dB (the depth
        // moves 0.2 dB with the re-timed charge); the pin
        // re-centres on the AB2 references, tolerance
        // unchanged (one 64-sample block; 0.3 dB).
        auto grFallT50 = [&](float& peakGr) {
            Osc72Processor q;
            q.prepareToPlay(kFs, 64);
            q.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.75f);
            q.apvts.getParameter("releaseFast")->setValueNotifyingHost(0.0f);
            juce::MidiBuffer midi;
            const int block = 64;
            const int loudEnd = (int)(1.0 * kFs);
            const int total = loudEnd + (int)(1.2 * kFs);
            peakGr = 0;
            double t50 = -1;
            for (int base = 0; base + block <= total; base += block) {
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
                else if (t50 < 0 && peakGr > 0 && gr <= 0.5f * peakGr)
                    t50 = (double)(base + block - loudEnd) / kFs;
            }
            return t50;
        };
        float pk = 0;
        double f = grFallT50(pk);
        std::snprintf(b, sizeof b, "DUAL release 50%% recovery: AB2 %.3f s vs ref 0.079 s, peak %.1f dB (ref 22.3)",
                      f, pk);
        check(std::fabs(f - 0.079) <= (64.0 / kFs) && std::fabs(pk - 22.3f) < 0.3f,
              "P11c proc: AB2 DUAL release 50% recovery == AB2 ref from the same depth", b);

        // P11d: the reported latency is untouched by the
        // bake (MOD in A: 992; MOD out: 32).
        {
            auto latOf = [&](bool modIn) {
                Osc72Processor p;
                if (modIn) p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
                p.prepareToPlay(kFs, 512);
                juce::AudioBuffer<float> buf(2, 512);
                juce::MidiBuffer midi;
                buf.clear();
                p.processBlock(buf, midi);
                return p.getLatencySamples();
            };
            const int a = latOf(true), e = latOf(false);
            std::snprintf(b, sizeof b, "latency MOD-in %d, MOD-out %d", a, e);
            check(a == 992 && e == 32,
                  "P11d proc: latency unchanged by the bake (992 / 32)", b);
        }
    }

    // ── P12. HOST RE-PREPARE (v1.4.1 fix) ───────────────────────
    // A host can re-call prepareToPlay on a live instance
    // without reloading it — Luna's top-bar power cycle
    // does exactly this through the JUCE AU wrapper. On
    // <= 1.4.0 the re-call rebuilt the chain under a live
    // MOD engagement and parked both inserts forever
    // (reset parked the state machines off while the
    // engagement target survived, and the no-change
    // setEnabled re-apply never re-armed them). The fix
    // preserves live state across a same-rate re-prepare
    // and re-arms on any genuine target-on reset (DSP
    // suite, section N). Pins: a mid-stream same-rate
    // re-call is BIT-IDENTICAL to a twin that never saw
    // it, in both MOD flavours.
    {
        auto renderWithEvent = [&](bool phaser, bool reprepare) {
            Osc72Processor p;
            p.apvts.getParameter("allTheWay")->setValueNotifyingHost(0.75f);
            p.apvts.getParameter("warble")->setValueNotifyingHost(1.0f);
            p.apvts.getParameter("modPhaser")->setValueNotifyingHost(phaser ? 1.0f : 0.0f);
            p.apvts.getParameter("compMix")->setValueNotifyingHost(0.0f);
            p.apvts.getParameter("modMix")->setValueNotifyingHost(1.0f);
            p.prepareToPlay(kFs, 512);
            Material mat{ 2 };
            juce::MidiBuffer midi;
            const int block = 512;
            std::vector<float> out;
            for (int base = 0; base < (int)(4.0 * kFs); base += block) {
                if (reprepare && base == (int)(2.0 * kFs))
                    p.prepareToPlay(kFs, 512);   // the Luna event
                juce::AudioBuffer<float> buf(2, block);
                for (int j = 0; j < block; ++j) {
                    float l, r; mat.sample(base + j, l, r);
                    buf.setSample(0, j, l);
                    buf.setSample(1, j, r);
                }
                p.processBlock(buf, midi);
                for (int j = 0; j < block; ++j) {
                    out.push_back(buf.getSample(0, j));
                    out.push_back(buf.getSample(1, j));
                }
            }
            return out;
        };
        check(renderWithEvent(false, true) == renderWithEvent(false, false),
              "P12a proc: same-rate prepareToPlay re-call mid-stream bit-identical (ADT in)", "");
        check(renderWithEvent(true, true) == renderWithEvent(true, false),
              "P12b proc: same-rate prepareToPlay re-call mid-stream bit-identical (phaser in)", "");
    }

    std::printf("%s (%d checks, %d failures)\n",
                failures == 0 ? "ALL PROCESSOR TESTS PASSED" : "PROCESSOR TEST FAILURES",
                checks, failures);
    return failures == 0 ? 0 : 1;
}
