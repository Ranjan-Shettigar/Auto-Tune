#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/VocalTuneLookAndFeel.h"
#include "GUI/PitchDisplayComponent.h"
#include "GUI/InteractiveKeyboardComponent.h"

namespace VocalTune
{

class VocalTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::AudioProcessorValueTreeState::Listener,
                                      public juce::Slider::Listener,
                                      private juce::Timer
{
public:
    explicit VocalTuneAudioProcessorEditor(VocalTuneAudioProcessor&);
    ~VocalTuneAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void sliderValueChanged(juce::Slider* slider) override;

private:
    VocalTuneAudioProcessor& processor;
    VocalTuneLookAndFeel lookAndFeel;

    // Header Components
    juce::TextButton lowLatencyButton;
    juce::Label latencyStatusLabel;
    juce::TooltipWindow tooltipWindow { this, 500 };
    std::atomic<bool> parameterDisplayDirty { false };
    bool latencyDisplayInitialised = false;
    bool displayedPrepared = false, displayedRequested = false, displayedActive = false;
    double displayedLatencyMs = 0.0;

    // Visualizer
    PitchDisplayComponent pitchDisplay;

    // Card 1: Tuning Controls
    juce::Label correctionTitle;
    juce::Slider correctionSlider;
    juce::Label correctionValueLabel;

    juce::Label retuneSpeedTitle;
    juce::Slider retuneSpeedSlider;
    juce::Label retuneSpeedValueLabel;

    juce::Label flexTuneTitle;
    juce::Slider flexTuneSlider;
    juce::Label flexTuneValueLabel;

    // Card 2: Expression & Harmony
    juce::Label vibratoTitle;
    juce::Slider vibratoSlider;
    juce::Label vibratoValueLabel;

    juce::Label formantTitle;
    juce::Slider formantSlider;
    juce::Label formantValueLabel;

    juce::Label harmonizeTitle;
    juce::Slider harmonizeSlider;
    juce::Label harmonizeValueLabel;

    juce::Label harmonyIntervalTitle;
    juce::ComboBox harmonyIntervalCombo;

    // Card 3: Scale, Detection & Tuning
    juce::Label vocalRangeTitle;
    juce::ComboBox vocalRangeCombo;

    juce::Label trackingSensTitle;
    juce::ComboBox trackingSensCombo;

    juce::Label rootNoteLabel;
    juce::ComboBox rootNoteCombo;

    juce::Label scaleLabel;
    juce::TextButton followButton;

    InteractiveKeyboardComponent keyboard;
    juce::Label keyboardTitle;
    juce::Rectangle<int> controlsPanel, scalePanel;

    juce::Label tuningLabel;
    juce::Slider tuningSlider; // Styled as clean 440 Hz box

    juce::ComboBox scaleTypeCombo;

    // APVTS Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> correctionAttach;
    std::unique_ptr<SliderAttachment> retuneSpeedAttach;
    std::unique_ptr<SliderAttachment> flexTuneAttach;

    std::unique_ptr<SliderAttachment> vibratoAttach;
    std::unique_ptr<SliderAttachment> formantAttach;
    std::unique_ptr<SliderAttachment> harmonizeAttach;
    std::unique_ptr<ComboBoxAttachment> harmonyIntervalAttach;

    std::unique_ptr<ComboBoxAttachment> vocalRangeAttach;
    std::unique_ptr<ComboBoxAttachment> trackingSensAttach;
    std::unique_ptr<ComboBoxAttachment> rootNoteAttach;
    std::unique_ptr<ComboBoxAttachment> scaleTypeAttach;
    std::unique_ptr<SliderAttachment> tuningAttach;

    std::unique_ptr<ButtonAttachment> lowLatencyAttach;
    std::unique_ptr<ButtonAttachment> followAttach;

    void updateKeyboardFromScale();
    void updateValueLabels();
    void updateLatencyStatus();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalTuneAudioProcessorEditor)
};

} // namespace VocalTune
