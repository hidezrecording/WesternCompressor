#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Osc72Dsp.h"

// Hotdog — one-knob stereo parallel FET limiter
// (reconstruction of a documented 1971-72 Record Plant session
// trick; see dsp/Osc72Dsp.h). Internal project name: OSC-72.
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

    const juce::String getName() const override { return "Hotdog"; }
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
    bool limitIn_ = true;
    bool warbleOn_ = false;
    bool powerOn_ = true;      // v19 strip POWER toggle (default ON)
    float powerMix_ = 1.0f;    // v19 bypass fade position, 1 = unit on
    float outGainL_ = 1.0f;   // smoothed O/P LEVEL gains (v12)
    float outGainR_ = 1.0f;
    std::atomic<float> displayedGrDb_ { 0.0f };
    std::atomic<float> grDisplayOverrideDb_ { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Osc72Processor)
};
