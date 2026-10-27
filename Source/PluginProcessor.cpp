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
    correctionParam      = apvts.getRawParameterValue(ParamIDs::correction.getParamID());
    retuneSpeedParam     = apvts.getRawParameterValue(ParamIDs::retuneSpeed.getParamID());
    flexTuneParam        = apvts.getRawParameterValue(ParamIDs::flexTune.getParamID());
    vibratoParam         = apvts.getRawParameterValue(ParamIDs::vibrato.getParamID());
    formantShiftParam    = apvts.getRawParameterValue(ParamIDs::formantShift.getParamID());
    harmonizeParam       = apvts.getRawParameterValue(ParamIDs::harmonize.getParamID());
    harmonyIntervalParam = apvts.getRawParameterValue(ParamIDs::harmonyInterval.getParamID());
    rootNoteParam        = apvts.getRawParameterValue(ParamIDs::rootNote.getParamID());
    scaleTypeParam       = apvts.getRawParameterValue(ParamIDs::scaleType.getParamID());
    vocalRangeParam      = apvts.getRawParameterValue(ParamIDs::vocalRange.getParamID());
    trackingSensParam    = apvts.getRawParameterValue(ParamIDs::trackingSens.getParamID());
    tuningRefParam       = apvts.getRawParameterValue(ParamIDs::tuningRef.getParamID());
    lowLatencyParam      = apvts.getRawParameterValue(ParamIDs::lowLatency.getParamID());
    followScaleParam     = apvts.getRawParameterValue(ParamIDs::followScale.getParamID());
    customMaskParam      = apvts.getRawParameterValue(ParamIDs::customMask.getParamID());
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
    return 0.0;
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
    int channels = getTotalNumOutputChannels();
    engine.prepare(sampleRate, samplesPerBlock, channels);

    bool lowLat = (lowLatencyParam != nullptr) ? (lowLatencyParam->load() > 0.5f) : true;
    engine.setLowLatencyMode(lowLat);
    setLatencySamples(engine.getLatencySamples());
    lastLowLatencyState = lowLat;
}

void VocalTuneAudioProcessor::releaseResources()
{
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

    float corr        = (correctionParam      != nullptr) ? correctionParam->load()      : 80.0f;
    float retuneSpd   = (retuneSpeedParam     != nullptr) ? retuneSpeedParam->load()     : 15.0f;
    float flex        = (flexTuneParam        != nullptr) ? flexTuneParam->load()        : 0.0f;
    float vib         = (vibratoParam         != nullptr) ? vibratoParam->load()         : 0.0f;
    float formant     = (formantShiftParam    != nullptr) ? formantShiftParam->load()    : 0.0f;
    float harmMix     = (harmonizeParam       != nullptr) ? harmonizeParam->load()       : 0.0f;
    int   harmIdx     = (harmonyIntervalParam != nullptr) ? static_cast<int>(harmonyIntervalParam->load()) : 0;
    int   root        = (rootNoteParam        != nullptr) ? static_cast<int>(rootNoteParam->load()) : 5;
    int   sTypeInt    = (scaleTypeParam       != nullptr) ? static_cast<int>(scaleTypeParam->load()) : 0;
    int   rangeIdx    = (vocalRangeParam      != nullptr) ? static_cast<int>(vocalRangeParam->load()) : 0;
    int   sensIdx     = (trackingSensParam    != nullptr) ? static_cast<int>(trackingSensParam->load()) : 0;
    float tuning      = (tuningRefParam       != nullptr) ? tuningRefParam->load()       : 440.0f;
    bool  lowLat      = (lowLatencyParam      != nullptr) ? (lowLatencyParam->load() > 0.5f) : true;
    bool  follow      = (followScaleParam     != nullptr) ? (followScaleParam->load() > 0.5f) : true;
    uint16_t mask     = (customMaskParam      != nullptr) ? static_cast<uint16_t>(customMaskParam->load()) : 0x0FFF;

    if (lowLat != lastLowLatencyState)
    {
        engine.setLowLatencyMode(lowLat);
        setLatencySamples(engine.getLatencySamples());
        lastLowLatencyState = lowLat;
    }

    auto scaleType = static_cast<ScaleType>(juce::jlimit(0, static_cast<int>(ScaleType::NumScaleTypes) - 1, sTypeInt));

    engine.process(buffer, corr, retuneSpd, flex, vib, formant, harmMix, harmIdx, root, scaleType, tuning, rangeIdx, sensIdx, follow, mask);
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalTune::VocalTuneAudioProcessor();
}
