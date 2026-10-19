#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Osc72Dsp.h"

// Western Compressor — engine v4 (2026-10-15): a BA-6A
// limiting-amplifier emulation (LIMIT) plus an Abbey Road
// Reel ADT widest-configuration insert (MOD) behind the
// v32 face; see docs/engine-v4-design.md. Internal project
// name: OSC-72.
// Parameters: ALL THE WAY (0..10), the 'limit' in/out toggle
// (bool, default IN — the compressor bypass), the 'mod' toggle
// (the "warble" parameter: bool, default OUT — a Lo-Fi Loop
// Junky-style insert, post-limiter) and, since v12, 'outLevelL' /
// 'outLevelR' (-12..+12 dB, default 0 dB) — final per-channel
// output trims applied in processBlock after the chain. Since
// v19, a 'power' toggle (bool, default ON) sits on the strip:
// power off bypasses the whole unit in processBlock (dry
// passthrough through a click-free fade, latency reported 0)
// and every panel light goes out.
// AB3 test build (gui-v60 round): the strip's two top
// pots are repurposed — the ex-SIDECHAIN pot is COMP MIX
// ('compMix', parallel compression) and the ex-MIX pot is
// MOD MIX ('modMix', the MOD section's wet amount), both
// float 0..1 defaulting to 0.5 (12 o'clock, Nathan's
// call). The v38 'mix' parameter and the v41 'scCutoff'
// parameter are deleted outright (no stubs; legacy state
// values are orphaned and ignored — see the layout
// comments in PluginProcessor.cpp for the session
// consequences). BOTH blends live inside osc72::Chain as
// two latency-aligned linear-crossfade stages around the
// inserts; the processor's v38/v44 dry-tap machinery is
// deleted with them. Signal order: input -> osc72::Chain
// (comp stage -> inserts -> mod stage) -> O/P LEVEL trims
// -> the v19 POWER fade (settled off, the whole unit is
// out of the path and the bypass is bit-exact at any pot
// setting). Reported latency is the chain's (32 / 992-A /
// 32-P), independent of both pots.
// All limiter/mod routing lives in osc72::Chain, so the tested
// code is the shipped code.
class Osc72Processor : public juce::AudioProcessor
{
public:
    Osc72Processor();
    ~Osc72Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Western Compressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Live gain reduction (dB, >= 0) for the editor's magic-eye
    // display. Written on the audio thread, read on the message
    // thread; relaxed atomics are sufficient for a display value.
    float currentGainReductionDb() const
    {
        const float overrideDb = grDisplayOverrideDb_.load(std::memory_order_relaxed);
        if (overrideDb >= 0.0f) return overrideDb;
        return displayedGrDb_.load(std::memory_order_relaxed);
    }

    // Display-only override for headless screenshots / QA: forces the
    // eye to a fixed GR state without touching the audio path.
    void setGainReductionDisplayOverride(float grDb)
    {
        grDisplayOverrideDb_.store(grDb, std::memory_order_relaxed);
    }

private:
    osc72::Chain chain_;
    bool compMode_ = false; // v53: 'limit' param repurposed (true = COMP); default LIMIT
    bool warbleOn_ = false;
    bool modPhaser_ = false; // v55: MOD flavour — false = A (ADT, default), true = P (Phase 90)
    bool attackFast_ = true;    // v4: stock detector (default FAST)
    bool releaseFast_ = false;  // v4: DUAL recovery (default SLOW)
    bool powerOn_ = true;      // v19 strip POWER toggle (default ON)
    float powerMix_ = 1.0f;    // v19 bypass fade position, 1 = unit on
    float outGainL_ = 1.0f;   // smoothed O/P LEVEL gains (v12)
    float outGainR_ = 1.0f;
    // AB3: no mix state survives at this level — both pot
    // values are read from APVTS and pushed into the chain
    // per block; the slews, taps and offset crossfade are
    // the chain's (see Osc72Dsp.h, Chain).
    std::atomic<float> displayedGrDb_ { 0.0f };
    std::atomic<float> grDisplayOverrideDb_ { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Osc72Processor)
};
