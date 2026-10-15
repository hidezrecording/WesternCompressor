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
    // Engine v4 (Nathan, 2026-10-15): the two new toggles
    // flanking the magic eye. 'attackFast' = the BA-6A detector
    // speed (FAST = stock, the default; SLOW = the era mod).
    // 'releaseFast' = the recovery network (FAST = SINGLE;
    // SLOW = DUAL, the default — the BA-6A's signature
    // programme setting). Research: docs/engine-v4-research.md
    // §A.6; design: docs/engine-v4-design.md §2.3/§6.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "attackFast", 1 }, "Attack Fast", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "releaseFast", 1 }, "Release Fast", false));
    // Sidechain low-cut (Nathan, 2026-10-15, v41): the strip's
    // SIDECHAIN pot — a high-pass in the BA-6A's DETECTOR feed
    // only (audio path untouched), 10–200 Hz on a log taper.
    // Default 10 Hz = the bottom stop, where the filter is out
    // of the path and the shipped sound/ballistics hold
    // exactly. Shown in Hz ("10 Hz".."200 Hz").
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "scCutoff", 1 }, "Sidechain Cutoff",
        juce::NormalisableRange<float>(10.0f, 200.0f, 0.01f, 0.4077f), 10.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(juce::roundToInt(v)) + " Hz"; },
        [](const juce::String& t) { return juce::jlimit(10.0f, 200.0f, t.getFloatValue()); }));
    // Output LINK (Nathan, 2026-10-15, v36): the strip's LINK
    // toggle under the OUTPUT caption. Default LINKED (link is
    // the normal state). EDITOR-SIDE control linkage only:
    // with link on, dragging either O/P LEVEL pot sets both
    // pots (and both params) to the dragged value; the audio
    // path is unchanged — processBlock still applies the two
    // output gains independently, exactly as it reads them.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "outLink", 1 }, "Output Link", true));
    // MIX (Nathan, 2026-10-15, v38): a wet/dry pot on the
    // strip, centred under the POWER toggle — full left DRY
    // (the raw input), full right WET (the full BA-6A
    // chain). Default 1.0 (fully wet), so the pre-feature
    // sound is the default sound and every existing pin
    // holds. The value strings read as a percentage without
    // the sign ("0".."100"), matching the pot's panel
    // numerals. The blend itself lives in processBlock:
    // the dry tap is delayed by the chain's current
    // latency so the two sides never comb.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)); },
        [](const juce::String& t) { return juce::jlimit(0.0f, 1.0f, t.getFloatValue() / 100.0f); }));
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
    attackFast_ = apvts.getRawParameterValue("attackFast")->load() > 0.5f;
    chain_.setAttackFast(attackFast_);
    releaseFast_ = apvts.getRawParameterValue("releaseFast")->load() > 0.5f;
    chain_.setReleaseFast(releaseFast_);
    scCutoffHz_ = apvts.getRawParameterValue("scCutoff")->load();
    chain_.setScCutoff(scCutoffHz_);
    outGainL_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelL")->load());
    outGainR_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelR")->load());
    powerOn_ = apvts.getRawParameterValue("power")->load() > 0.5f;
    powerMix_ = powerOn_ ? 1.0f : 0.0f;
    // v38 MIX: start settled at the saved blend and clear
    // the dry tap's delay lines.
    mixCur_ = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    dryLineL_.assign(kDryLineSize, 0.0f);
    dryLineR_.assign(kDryLineSize, 0.0f);
    dryWrite_ = 0;
    // v44: the dry tap starts settled on the current target.
    dryDelayA_ = dryDelayB_ = powerOn_ ? chain_.latencySamples() : 0;
    dryTapFade_ = 1.0f;
    // Truthful latency: 0 unless mod is in; then the ADT
    // insert's base delay (20 ms). v19: 0 while POWER is off — the chain
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
    // v4 ballistics toggles: no latency consequence; the
    // engine's networks run in parallel and the selection is
    // click-free by construction (design §2.2).
    const bool wantAttackFast = apvts.getRawParameterValue("attackFast")->load() > 0.5f;
    if (wantAttackFast != attackFast_)
    {
        attackFast_ = wantAttackFast;
        chain_.setAttackFast(attackFast_);
    }
    const bool wantReleaseFast = apvts.getRawParameterValue("releaseFast")->load() > 0.5f;
    if (wantReleaseFast != releaseFast_)
    {
        releaseFast_ = wantReleaseFast;
        chain_.setReleaseFast(releaseFast_);
    }
    // v41 sidechain low-cut: no latency consequence; the DSP
    // filter's states track continuously, so moves never step
    // the detector envelope.
    const float wantSc = apvts.getRawParameterValue("scCutoff")->load();
    if (wantSc != scCutoffHz_)
    {
        scCutoffHz_ = wantSc;
        chain_.setScCutoff(scCutoffHz_);
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

    // MIX (v38): the blend target for this block, and the dry
    // tap's read offset — the chain's CURRENT latency, taken
    // after the toggle edges above have settled it, so dry
    // and wet are time-aligned at every mix position (MOD
    // in: 960 samples; out: 0). v44: at the toggle instant
    // itself the offset now crossfades over 30 ms instead of
    // stepping (the dryDelayA_/B_ machinery below). The
    // delay lines are sized defensively here too
    // (prepareToPlay is the real init).
    const float blendTarget = juce::jlimit(
        0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    if ((int) dryLineL_.size() != kDryLineSize)
    {
        dryLineL_.assign(kDryLineSize, 0.0f);
        dryLineR_.assign(kDryLineSize, 0.0f);
        dryWrite_ = 0;
    }
    // v44: on a latency-target change (the MOD or POWER edges
    // above), start the dry tap's 30 ms offset crossfade from
    // the old offset to the new one — the same span the ADT
    // itself takes to engage, so the blend's two sides morph
    // together.
    const int dryTarget = powerOn_ ? chain_.latencySamples() : 0;
    if (dryTarget != dryDelayB_)
    {
        dryDelayA_ = dryDelayB_;
        dryDelayB_ = dryTarget;
        dryTapFade_ = 0.0f;
    }

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
    const float tapStep = 1.0f / (0.030f * (float) sr);   // v44: dry-tap offset crossfade rate
    const float mixTarget = powerOn_ ? 1.0f : 0.0f;
    for (int i = 0; i < n; ++i)
    {
        outGainL_ += (targetGainL - outGainL_) * 0.0015f;
        outGainR_ += (targetGainR - outGainR_) * 0.0015f;
        mixCur_ += (blendTarget - mixCur_) * 0.002f;
        if (std::abs(blendTarget - mixCur_) < 2.0e-5f) mixCur_ = blendTarget;
        powerMix_ += (mixTarget - powerMix_) * fadeCoef;
        if (powerMix_ < 0.0001f) powerMix_ = mixTarget == 0.0f ? 0.0f : powerMix_;
        const float inL = outL[i];
        const float inR = outR ? outR[i] : inL;
        // v38 MIX dry tap: the input is written to the delay
        // lines EVERY sample — unit off included — so the
        // latency-aligned dry signal is always ready for
        // whatever the pot (or the POWER fade) does next.
        dryLineL_[(size_t) dryWrite_] = inL;
        dryLineR_[(size_t) dryWrite_] = inR;
        // Read at the target offset; during a latency
        // transition, crossfade from the old offset's read
        // over 30 ms (v44 — see the header note).
        const int readB = (dryWrite_ - dryDelayB_) & (kDryLineSize - 1);
        float dryL = dryLineL_[(size_t) readB];
        float dryR = dryLineR_[(size_t) readB];
        if (dryTapFade_ < 1.0f)
        {
            const int readA = (dryWrite_ - dryDelayA_) & (kDryLineSize - 1);
            const float aL = dryLineL_[(size_t) readA];
            const float aR = dryLineR_[(size_t) readA];
            dryL = aL + (dryL - aL) * dryTapFade_;
            dryR = aR + (dryR - aR) * dryTapFade_;
            dryTapFade_ = std::min(1.0f, dryTapFade_ + tapStep);
        }
        dryWrite_ = (dryWrite_ + 1) & (kDryLineSize - 1);
        if (powerMix_ == 0.0f)
            continue;   // unit off: input passes dry, untouched
        float oL, oR;
        chain_.process(inL, inR, oL, oR);
        oL *= outGainL_;
        oR *= outGainR_;
        // v38 MIX blend: the latency-aligned dry tap against
        // the full chain + trims path (the wet side). At
        // mix == 1 the wet passes untouched — the
        // pre-feature output, bit-for-bit; at mix == 0 the
        // output is the dry tap alone (the input delayed by
        // exactly the chain's current latency).
        if (mixCur_ < 1.0f)
        {
            if (mixCur_ <= 0.0f)
            {
                oL = dryL;
                oR = dryR;
            }
            else
            {
                oL = dryL + (oL - dryL) * mixCur_;
                oR = dryR + (oR - dryR) * mixCur_;
            }
        }
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
