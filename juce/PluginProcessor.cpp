#include "PluginProcessor.h"
#include "PluginEditor.h"

Osc72Processor::Osc72Processor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Params", createParameterLayout())
{
}

Osc72Processor::~Osc72Processor() = default;

bool Osc72Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::mono())
        return true;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo())
        return true;
    return false;
}

juce::AudioProcessorValueTreeState::ParameterLayout
Osc72Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "allTheWay", 1 }, "All The Way",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1); },
        [](const juce::String& t) { return t.getFloatValue(); }));
    // Warble switch (Nathan, 2026-10-14): Lo-Fi Loop Junky-style
    // garbled-pitch insert behind the faceplate toggle ('mod').
    // Default OUT; when out the insert is bit-transparent.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "warble", 1 }, "Warble", false));
    // Limit in/out (Nathan, 2026-10-14, v8): the compressor's own
    // bypass toggle on the faceplate. Default IN. With limit out
    // and mod out the plugin is bit-exact dry with zero latency.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "limit", 1 }, "Limit", true));
    // Power (Nathan, 2026-10-14, v19): the strip's POWER toggle,
    // a third switch cluster matching LIMIT/MOD. Default ON.
    // Power off = the whole unit goes off, "like our other
    // plugins": audio fully bypassed in processBlock (dry
    // passthrough through a click-free fade, latency reported
    // 0 even if MOD is in) and every panel light out — POWER
    // jewel, section jewels and the magic eye all dark.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "power", 1 }, "Power", true));
    // L/R output level (Nathan, 2026-10-14, v12): the two small
    // O/P LEVEL knobs at the bottom of the rack panel, after the
    // H2 Audio 002. Real per-channel trims, -12..+12 dB, unity
    // (0 dB) at centre; applied as the final gain stage in
    // processBlock, downstream of osc72::Chain.
    for (const char* id : { "outLevelL", "outLevelR" })
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 },
            id[8] == 'L' ? "Output Level L" : "Output Level R",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f,
            juce::String(), juce::AudioProcessorParameter::genericParameter,
            [](float v, int) { return juce::String(v, 1) + " dB"; },
            [](const juce::String& t) { return t.getFloatValue(); }));
    }
    return { params.begin(), params.end() };
}

void Osc72Processor::prepareToPlay(double sampleRate, int)
{
    chain_.prepare(sampleRate);
    chain_.setKnob(apvts.getRawParameterValue("allTheWay")->load() / 10.0f);
    limitIn_ = apvts.getRawParameterValue("limit")->load() > 0.5f;
    chain_.setLimitIn(limitIn_);
    warbleOn_ = apvts.getRawParameterValue("warble")->load() > 0.5f;
    chain_.setModIn(warbleOn_);
    outGainL_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelL")->load());
    outGainR_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelR")->load());
    powerOn_ = apvts.getRawParameterValue("power")->load() > 0.5f;
    powerMix_ = powerOn_ ? 1.0f : 0.0f;
    // Truthful latency: 0 unless mod is in; then the warble path's
    // base delay (12 ms). v19: 0 while POWER is off — the chain
    // is out of the path entirely.
    setLatencySamples(powerOn_ ? chain_.latencySamples() : 0);
}

void Osc72Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    chain_.setKnob(apvts.getRawParameterValue("allTheWay")->load() / 10.0f);

    // Faceplate toggle edges. 'limit' just re-points the chain's
    // crossfade (no latency consequence). 'mod' engages/disengages
    // the warble insert and keeps the reported latency honest.
    const bool wantLimit = apvts.getRawParameterValue("limit")->load() > 0.5f;
    if (wantLimit != limitIn_)
    {
        limitIn_ = wantLimit;
        chain_.setLimitIn(limitIn_);
    }
    const bool wantWarble = apvts.getRawParameterValue("warble")->load() > 0.5f;
    if (wantWarble != warbleOn_)
    {
        warbleOn_ = wantWarble;
        chain_.setModIn(warbleOn_);
        setLatencySamples(powerOn_ ? chain_.latencySamples() : 0);
    }
    // POWER edge (v19): off pulls the whole unit out of the
    // path — reported latency drops to 0 even if MOD is in,
    // and the per-sample fade below takes the audio dry.
    const bool wantPower = apvts.getRawParameterValue("power")->load() > 0.5f;
    if (wantPower != powerOn_)
    {
        powerOn_ = wantPower;
        setLatencySamples(powerOn_ ? chain_.latencySamples() : 0);
    }

    // O/P LEVEL trims (v12): final per-channel gains after the
    // chain, one-pole smoothed per sample so knob moves never
    // click (~15 ms settle at 48 kHz). At 0 dB both gains are
    // exactly 1.0 and the stage is a no-op multiply.
    const float targetGainL = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelL")->load());
    const float targetGainR = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelR")->load());

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const int n = buffer.getNumSamples();
    // v19 POWER fade: powerMix_ slews toward 1 (on) or 0 (off)
    // with a ~25 ms one-pole — the same click-free treatment
    // the chain's own toggles get. While the fade runs, the
    // output is a dry/wet crossfade of input against the full
    // chain + trims path; settled off (mix == 0), the chain is
    // skipped entirely and the buffer passes bit-exact dry,
    // trims included in the bypass.
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const float fadeCoef = 1.0f - std::exp(-1.0f / (0.025f * (float) sr));
    const float mixTarget = powerOn_ ? 1.0f : 0.0f;
    for (int i = 0; i < n; ++i)
    {
        outGainL_ += (targetGainL - outGainL_) * 0.0015f;
        outGainR_ += (targetGainR - outGainR_) * 0.0015f;
        powerMix_ += (mixTarget - powerMix_) * fadeCoef;
        if (powerMix_ < 0.0001f) powerMix_ = mixTarget == 0.0f ? 0.0f : powerMix_;
        const float inL = outL[i];
        const float inR = outR ? outR[i] : inL;
        if (powerMix_ == 0.0f)
            continue;   // unit off: input passes dry, untouched
        float oL, oR;
        chain_.process(inL, inR, oL, oR);
        oL *= outGainL_;
        oR *= outGainR_;
        if (powerMix_ == 1.0f)
        {
            outL[i] = oL;
            if (outR) outR[i] = oR;
        }
        else
        {
            outL[i] = inL + (oL - inL) * powerMix_;
            if (outR) outR[i] = inR + (oR - inR) * powerMix_;
        }
    }
    // Publish the gain reduction for the magic-eye display in the
    // editor (the chain already reads 0 while limit is out).
    // v19: while POWER is off the eye reads nothing.
    displayedGrDb_.store(powerOn_ ? chain_.gainReductionDb() : 0.0f,
                         std::memory_order_relaxed);
}

juce::AudioProcessorEditor* Osc72Processor::createEditor()
{
    return new Osc72Editor(*this);
}

void Osc72Processor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void Osc72Processor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Osc72Processor();
}
