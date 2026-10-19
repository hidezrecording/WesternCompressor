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
        // v53 (Nathan): the default opening state is INPUT at
        // 5 — the 1.1.5 staging's "barely kissing" point
        // (a nominal -14 dBFS signal draws ~1.8 dB of GR).
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 5.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1); },
        [](const juce::String& t) { return t.getFloatValue(); }));
    // Warble switch (Nathan, 2026-10-14): Lo-Fi Loop Junky-style
    // garbled-pitch insert behind the faceplate toggle ('mod').
    // Default OUT; when out the insert is bit-transparent.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "warble", 1 }, "Warble", false));
    // MOD flavour (Nathan, 2026-10-17, v55): the A/P mini
    // toggle beside the MOD cluster (GUI follow-on; see
    // docs/mod-phaser-design.md). false = A, the Reel ADT
    // exactly as shipped (the default); true = P, a script-
    // era MXR Phase 90 (the 'cosmic country' sweep). Only
    // meaningful while MOD is in; the flavour edge changes
    // the reported latency (992 <-> 32 @ 48 kHz) and the
    // dry tap follows via the v44 crossfade machinery.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "modPhaser", 1 }, "Mod Phaser", false));
    // Ratio mode (v53, Nathan): the old limit in/out bypass
    // toggle repurposed — true = COMP, false = LIMIT, and the
    // section is always engaged. Default LIMIT (false), the
    // firmer law. Session note: a pre-1.1.9 session saved OUT
    // (false) loads as LIMIT-active rather than bypassed;
    // saved IN (true) loads as COMP.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "limit", 1 }, "Limit", false));
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
    // REMOVED in 1.3.1 (v64): the 'timesTwo' 1X/2X timing
    // selector parameter (v62, shipped in 1.3.0). Nathan's
    // ear verdict retired the 1X speeds entirely — the 2X
    // ballistics are baked into the DSP as the unit's only
    // ballistics (kBa6aTimeScale), so there is nothing left
    // for a parameter to select. SESSION COMPATIBILITY:
    // the parameter is deleted outright rather than kept
    // as a hidden legacy stub. JUCE state restore
    // (setStateInformation -> apvts.replaceState) matches
    // saved values to parameters by ID and simply never
    // looks up an ID that no longer exists — a 1.3.0
    // session's stored 'timesTwo' value is orphaned and
    // ignored, with no error and no effect on the other
    // parameters (pinned, suite P11a). A hidden stub would
    // instead leave a dead, automatable "Times Two"
    // control in every host's parameter list forever.
    // Consequence, intended by the directive: a session
    // saved at 1X under 1.3.0 now plays with the 2X
    // ballistics; a session saved at 2X is bit-identical.
    // REMOVED in the AB3 test build (gui-v60 round): the
    // 'scCutoff' sidechain low-cut parameter (v41) — Nathan
    // repurposed the strip's SIDECHAIN pot as COMP MIX
    // (below) and the detector HPF feature is deleted
    // outright (the timesTwo precedent: no legacy stub;
    // APVTS restore ignores the orphaned value — pinned
    // in the processor suite, P8). SESSION CONSEQUENCE,
    // intended by the directive: a session saved with the
    // sidechain raised above 10 Hz loses the detector
    // HPF — the detector hears the full-range sense, as it
    // always did at the 10 Hz default (where the shipped
    // sound is unchanged, bit-for-bit).
    // COMP MIX (AB3, Nathan): the strip's LEFT top pot —
    // the parallel-compression blend: dry input (latency-
    // aligned inside osc72::Chain) against the limiter's
    // output, linear crossfade. Default 0.5 = 12 o'clock
    // (Nathan's explicit call — the unit now OPENS at a
    // half blend, a deliberate change of default sound vs
    // 1.3.1; 1.0 is the shipped full-compression sound).
    // Value strings are plain percents ("0".."100"), as
    // the retired 'mix' pot's were.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compMix", 1 }, "Compression Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)); },
        [](const juce::String& t) { return juce::jlimit(0.0f, 1.0f, t.getFloatValue() / 100.0f); }));
    // Output LINK (Nathan, 2026-10-15, v36): the strip's LINK
    // toggle under the OUTPUT caption. Default LINKED (link is
    // the normal state). EDITOR-SIDE control linkage only:
    // with link on, dragging either O/P LEVEL pot sets both
    // pots (and both params) to the dragged value; the audio
    // path is unchanged — processBlock still applies the two
    // output gains independently, exactly as it reads them.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "outLink", 1 }, "Output Link", true));
    // REMOVED in the AB3 test build (gui-v60 round): the
    // v38 whole-chain 'mix' parameter — its single dry/wet
    // blend is superseded by the two-stage scheme (COMP
    // MIX above, MOD MIX below), and the processor's dry
    // tap machinery (v38/v44) is deleted with it: both
    // blends now live inside osc72::Chain, aligned by
    // construction. Deleted outright, no stub (the
    // timesTwo precedent; orphan tolerance pinned, P8).
    // SESSION CONSEQUENCE, intended by the directive: a
    // session saved with mix < 1 (some dry blended in)
    // loads with both new pots at their 0.5 defaults —
    // NOT sonically equivalent; the old single blend has
    // no exact two-stage translation, and the saved value
    // is ignored rather than approximated.
    // MOD MIX (AB3, Nathan): the strip's RIGHT top pot —
    // the MOD section's wet amount: the post-comp signal
    // against the engaged insert's wet arm (the ADT's
    // natural output; the Phase 90's bare cascade, so 0.5
    // is the classic pedal), linear crossfade, aligned
    // inside the chain. Inert while MOD is out. Default
    // 0.5 = 12 o'clock (Nathan: both pots open centred).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "modMix", 1 }, "Mod Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
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
    // AB3: the mix targets go in BEFORE prepare() — its
    // reset snaps the chain's slews to the held targets,
    // so a restored session starts settled at its saved
    // pot state from the first sample (no load glide).
    chain_.setCompMix(apvts.getRawParameterValue("compMix")->load());
    chain_.setModMix(apvts.getRawParameterValue("modMix")->load());
    chain_.prepare(sampleRate);
    chain_.setKnob(apvts.getRawParameterValue("allTheWay")->load() / 10.0f);
    compMode_ = apvts.getRawParameterValue("limit")->load() > 0.5f;
    chain_.setCompMode(compMode_);
    warbleOn_ = apvts.getRawParameterValue("warble")->load() > 0.5f;
    chain_.setModIn(warbleOn_);
    modPhaser_ = apvts.getRawParameterValue("modPhaser")->load() > 0.5f;
    chain_.setModPhaser(modPhaser_);
    attackFast_ = apvts.getRawParameterValue("attackFast")->load() > 0.5f;
    chain_.setAttackFast(attackFast_);
    releaseFast_ = apvts.getRawParameterValue("releaseFast")->load() > 0.5f;
    chain_.setReleaseFast(releaseFast_);
    outGainL_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelL")->load());
    outGainR_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outLevelR")->load());
    powerOn_ = apvts.getRawParameterValue("power")->load() > 0.5f;
    powerMix_ = powerOn_ ? 1.0f : 0.0f;
    // Truthful latency: the limiter's oversampling delay
    // (32 samples, v53) always; the ADT insert's base delay
    // (20 ms) adds while mod is in. v19: 0 while POWER is
    // off — the chain is out of the path entirely.
    setLatencySamples(powerOn_ ? chain_.latencySamples() : 0);
}

void Osc72Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    chain_.setKnob(apvts.getRawParameterValue("allTheWay")->load() / 10.0f);
    // AB3: both mix pots ride into the chain every block
    // (the chain slews them per sample on the retired
    // whole-chain mix's law, snap included).
    chain_.setCompMix(apvts.getRawParameterValue("compMix")->load());
    chain_.setModMix(apvts.getRawParameterValue("modMix")->load());

    // Faceplate toggle edges. 'limit' selects the ratio mode
    // (v53: true = COMP / false = LIMIT; the law slews inside
    // the gain computer, no click, no latency consequence).
    // 'mod' engages/disengages the warble insert and keeps
    // the reported latency honest.
    const bool wantComp = apvts.getRawParameterValue("limit")->load() > 0.5f;
    if (wantComp != compMode_)
    {
        compMode_ = wantComp;
        chain_.setCompMode(compMode_);
    }
    const bool wantWarble = apvts.getRawParameterValue("warble")->load() > 0.5f;
    if (wantWarble != warbleOn_)
    {
        warbleOn_ = wantWarble;
        chain_.setModIn(warbleOn_);
        setLatencySamples(powerOn_ ? chain_.latencySamples() : 0);
    }
    // v55 MOD flavour edge (A <-> P): the chain's flavour
    // blend crossfades over 30 ms inside Chain::process, and
    // the reported latency tracks the newly selected flavour
    // at once (992 <-> 32 @ 48 kHz with MOD in) — AB3: the
    // mod stage's offset crossfade inside the chain follows
    // it, exactly as it follows a MOD edge.
    const bool wantPhaser = apvts.getRawParameterValue("modPhaser")->load() > 0.5f;
    if (wantPhaser != modPhaser_)
    {
        modPhaser_ = wantPhaser;
        chain_.setModPhaser(modPhaser_);
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
    // (AB3: the v41 sidechain edge is gone with the
    // feature — the pot it served is COMP MIX now, pushed
    // into the chain at the top of this function.)
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

    // AB3: the v38/v44 processor dry tap is DELETED — both
    // blends (and the mod stage's offset crossfade, the
    // v44 machinery's heir) live inside osc72::Chain now,
    // where the two stages are aligned by construction.
    // What remains here is the shipped order around the
    // chain: chain -> O/P LEVEL trims -> the v19 POWER
    // fade. POWER off is bit-exact bypass at ANY pot
    // setting: settled off, the chain is skipped entirely.

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
    // editor. v19: while POWER is off the eye reads nothing.
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
