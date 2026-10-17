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
// Since v38, a 'mix' pot (float 0..1, default 1.0 = fully
// WET) sits on the strip under POWER: processBlock blends
// the raw input with the chain's processed output. Signal
// order: input -> [dry tap: a delay line read at the
// chain's CURRENT latency, so dry and wet are time-aligned
// at every mix position — 960 samples with MOD in, 0 with
// it out] in parallel with [osc72::Chain -> O/P LEVEL
// trims] -> wet/dry blend by 'mix' -> the v19 POWER fade
// (settled off, the whole blend is out of the path and the
// unit is bit-exact bypass whatever the mix). At mix == 1
// the blend is skipped entirely: the pre-feature output,
// bit-for-bit.
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
    bool timesTwo_ = false;     // v62: 1X/2X timing selector (default 1X)
    float scCutoffHz_ = 10.0f;  // v41: detector sidechain low-cut (default = bottom stop, filter out)
    bool powerOn_ = true;      // v19 strip POWER toggle (default ON)
    float powerMix_ = 1.0f;    // v19 bypass fade position, 1 = unit on
    float outGainL_ = 1.0f;   // smoothed O/P LEVEL gains (v12)
    float outGainR_ = 1.0f;
    // v38 MIX: the smoothed wet/dry blend position (1 = fully
    // wet) and the dry tap's delay lines. The lines are
    // written every sample whatever the state, so the
    // latency-aligned dry signal is always ready; the read
    // offset follows the chain's current latency.
    float mixCur_ = 1.0f;
    std::vector<float> dryLineL_, dryLineR_;
    int dryWrite_ = 0;
    static constexpr int kDryLineSize = 16384;   // power of two; >> the 960-sample max chain latency
    // v44 (mix-alignment diagnosis, docs/mix-alignment-
    // diagnosis.md): the dry tap's read offset, crossfaded.
    // dryDelayB_ is the current target (the chain's latency,
    // or 0 while POWER is off); when the target changes (MOD
    // or POWER edge), dryDelayA_ keeps the old offset and
    // dryTapFade_ ramps 0 -> 1 over 30 ms — the ADT's own
    // engagement time — so during the transition the blend's
    // dry side follows the same crossfade law as the wet
    // side's internal dry component, instead of stepping to
    // the new offset instantly while the chain is still
    // morphing (which weighted a mid-fade transient almost
    // entirely at the new offset: a momentary flam at the
    // toggle, measured in the diagnosis battery §5).
    int dryDelayA_ = 0, dryDelayB_ = 0;
    float dryTapFade_ = 1.0f;   // 1 = settled on dryDelayB_
    std::atomic<float> displayedGrDb_ { 0.0f };
    std::atomic<float> grDisplayOverrideDb_ { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Osc72Processor)
};
