#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace VocalTune
{

VocalTuneAudioProcessor::VocalTuneAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    correctionParam   = apvts.getRawParameterValue(ParamIDs::correction.getParamID());
    vibratoParam      = apvts.getRawParameterValue(ParamIDs::vibrato.getParamID());
    formantShiftParam = apvts.getRawParameterValue(ParamIDs::formantShift.getParamID());
    rootNoteParam     = apvts.getRawParameterValue(ParamIDs::rootNote.getParamID());
    scaleTypeParam    = apvts.getRawParameterValue(ParamIDs::scaleType.getParamID());
    tuningRefParam    = apvts.getRawParameterValue(ParamIDs::tuningRef.getParamID());
    lowLatencyParam   = apvts.getRawParameterValue(ParamIDs::lowLatency.getParamID());
    followScaleParam  = apvts.getRawParameterValue(ParamIDs::followScale.getParamID());
    customMaskParam   = apvts.getRawParameterValue(ParamIDs::customMask.getParamID());
}

VocalTuneAudioProcessor::~VocalTuneAudioProcessor()
{
}

const juce::String VocalTuneAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VocalTuneAudioProcessor::acceptsMidi() const
{
    return true;
}

bool VocalTuneAudioProcessor::producesMidi() const
{
    return false;
}

bool VocalTuneAudioProcessor::isMidiEffect() const
{
    return false;
}

double VocalTuneAudioProcessor::getTailLengthSeconds() const
{
    // Keep the compensated delay and spectral overlap tail when the host
    // renders the end of a region or an offline bounce.
    return 0.25;
}

int VocalTuneAudioProcessor::getNumPrograms()
{
    return 1;
}

int VocalTuneAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VocalTuneAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String VocalTuneAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void VocalTuneAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void VocalTuneAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    audioPrepared.store(false, std::memory_order_release);
    const int channels = getTotalNumOutputChannels();
    const bool lowLat = isLowLatencyRequested();
    // Configure latency only while the host has suspended audio processing.
    // A parameter change is pending until the next prepareToPlay call.
    engine.setLowLatencyMode(lowLat);
    engine.prepare(sampleRate, samplesPerBlock, channels);
    setLatencySamples(engine.getLatencySamples());
    activeLowLatency.store(lowLat, std::memory_order_relaxed);
    const double preparedSampleRate = sampleRate > 8000.0 ? sampleRate : 48000.0;
    activeLatencyMilliseconds.store(1000.0 * engine.getLatencySamples() / preparedSampleRate,
                                    std::memory_order_relaxed);
    audioPrepared.store(true, std::memory_order_release);
}

void VocalTuneAudioProcessor::releaseResources()
{
    audioPrepared.store(false, std::memory_order_release);
    engine.reset();
}

bool VocalTuneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    return true;
}

void VocalTuneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    float corr    = (correctionParam   != nullptr) ? correctionParam->load()   : 80.0f;
    float vib     = (vibratoParam      != nullptr) ? vibratoParam->load()      : 0.0f;
    float formant = (formantShiftParam != nullptr) ? formantShiftParam->load() : 0.0f;
    int root      = (rootNoteParam     != nullptr) ? static_cast<int>(rootNoteParam->load()) : 5;
    int sTypeInt  = (scaleTypeParam    != nullptr) ? static_cast<int>(scaleTypeParam->load()) : 0;
    float tuning  = (tuningRefParam    != nullptr) ? tuningRefParam->load()    : 440.0f;
    bool follow   = (followScaleParam  != nullptr) ? (followScaleParam->load() > 0.5f) : true;
    uint16_t mask = (customMaskParam   != nullptr) ? static_cast<uint16_t>(customMaskParam->load()) : 0x0FFF;

    auto scaleType = static_cast<ScaleType>(juce::jlimit(0, static_cast<int>(ScaleType::NumScaleTypes) - 1, sTypeInt));

    engine.process(buffer, corr, vib, formant, root, scaleType, tuning, follow, mask);
}

bool VocalTuneAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VocalTuneAudioProcessor::createEditor()
{
    return new VocalTuneAudioProcessorEditor(*this);
}

void VocalTuneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VocalTuneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace VocalTune

// JUCE Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalTune::VocalTuneAudioProcessor();
}

