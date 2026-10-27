#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Common/Parameters.h"
#include "DSP/AutoTuneEngine.h"

namespace VocalTune
{

class VocalTuneAudioProcessor : public juce::AudioProcessor
{
public:
    VocalTuneAudioProcessor();
    ~VocalTuneAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const AutoTuneEngine& getEngine() const { return engine; }
    AutoTuneEngine& getEngine() { return engine; }

    bool isAudioPrepared() const { return audioPrepared.load(std::memory_order_acquire); }
    bool isLowLatencyActive() const { return activeLowLatency.load(std::memory_order_relaxed); }
    bool isLowLatencyRequested() const
    {
        return lowLatencyParam == nullptr || lowLatencyParam->load() > 0.5f;
    }
    double getActiveLatencyMilliseconds() const { return activeLatencyMilliseconds.load(std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState apvts;
    AutoTuneEngine engine;

    std::atomic<float>* correctionParam   = nullptr;
    std::atomic<float>* vibratoParam      = nullptr;
    std::atomic<float>* formantShiftParam = nullptr;
    std::atomic<float>* rootNoteParam     = nullptr;
    std::atomic<float>* scaleTypeParam    = nullptr;
    std::atomic<float>* tuningRefParam    = nullptr;
    std::atomic<float>* lowLatencyParam   = nullptr;
    std::atomic<float>* followScaleParam  = nullptr;
    std::atomic<float>* customMaskParam   = nullptr;

    std::atomic<bool> audioPrepared { false };
    std::atomic<bool> activeLowLatency { true };
    std::atomic<double> activeLatencyMilliseconds { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalTuneAudioProcessor)
};

} // namespace VocalTune

