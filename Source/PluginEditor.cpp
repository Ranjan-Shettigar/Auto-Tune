#include "PluginEditor.h"

namespace VocalTune
{

VocalTuneAudioProcessorEditor::VocalTuneAudioProcessorEditor(VocalTuneAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), pitchDisplay(p.getEngine())
{
    setLookAndFeel(&lookAndFeel);

    // 1. Header: Low Latency Button
    lowLatencyButton.setButtonText("Low Latency");
    lowLatencyButton.setClickingTogglesState(true);
    addAndMakeVisible(lowLatencyButton);
    latencyStatusLabel.setJustificationType(juce::Justification::centredRight);
    latencyStatusLabel.setFont(juce::FontOptions(12.0f));
    latencyStatusLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    addAndMakeVisible(latencyStatusLabel);

    // 2. Visualizer
    addAndMakeVisible(pitchDisplay);

    // 3. Card 1: Correction
    correctionTitle.setText("Correction", juce::dontSendNotification);
    correctionTitle.setJustificationType(juce::Justification::centred);
    correctionTitle.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    correctionTitle.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    addAndMakeVisible(correctionTitle);

    correctionSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    correctionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    correctionSlider.setComponentID("correction");
    correctionSlider.addListener(this);
    addAndMakeVisible(correctionSlider);

    correctionValueLabel.setJustificationType(juce::Justification::centred);
    correctionValueLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textLight);
    correctionValueLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    addAndMakeVisible(correctionValueLabel);

    // 4. Card 2: Vibrato & Formant Shift
    vibratoTitle.setText("Vibrato", juce::dontSendNotification);
    vibratoTitle.setJustificationType(juce::Justification::centred);
    vibratoTitle.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    vibratoTitle.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(vibratoTitle);

    vibratoSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    vibratoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    vibratoSlider.setComponentID("vibrato");
    vibratoSlider.addListener(this);
    addAndMakeVisible(vibratoSlider);

    vibratoValueLabel.setJustificationType(juce::Justification::centred);
    vibratoValueLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textLight);
    vibratoValueLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    addAndMakeVisible(vibratoValueLabel);

    formantTitle.setText("Formant Shift", juce::dontSendNotification);
    formantTitle.setJustificationType(juce::Justification::centred);
    formantTitle.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    formantTitle.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(formantTitle);

    formantSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    formantSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    formantSlider.setComponentID("formant");
    formantSlider.addListener(this);
    addAndMakeVisible(formantSlider);

    formantValueLabel.setJustificationType(juce::Justification::centred);
    formantValueLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textLight);
    formantValueLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    addAndMakeVisible(formantValueLabel);

    // 5. Card 3: Root Note, Scale, Keyboard, Tuning
    rootNoteLabel.setText("Root Note", juce::dontSendNotification);
    rootNoteLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    rootNoteLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(rootNoteLabel);

    for (int i = 0; i < 12; ++i)
        rootNoteCombo.addItem(NoteNames[static_cast<size_t>(i)], i + 1);
    addAndMakeVisible(rootNoteCombo);

    scaleLabel.setText("Scale", juce::dontSendNotification);
    scaleLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    scaleLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(scaleLabel);

    followButton.setButtonText("Follow");
    followButton.setClickingTogglesState(true);
    addAndMakeVisible(followButton);

    addAndMakeVisible(keyboard);
    keyboard.onMaskChanged = [this](uint16_t newMask)
    {
        if (auto* p = processor.getAPVTS().getParameter(ParamIDs::customMask.getParamID()))
        {
            p->setValueNotifyingHost(static_cast<float>(newMask) / 4095.0f);
        }
    };

    tuningLabel.setText("Tuning", juce::dontSendNotification);
    tuningLabel.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
    tuningLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(tuningLabel);

    tuningSlider.setSliderStyle(juce::Slider::LinearBar);
    tuningSlider.setTextValueSuffix(" Hz");
    tuningSlider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(32, 36, 44));
    tuningSlider.setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(40, 46, 56));
    tuningSlider.setColour(juce::Slider::textBoxTextColourId, VocalTuneLookAndFeel::textLight);
    tuningSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(55, 62, 75));
    addAndMakeVisible(tuningSlider);

    for (size_t i = 0; i < static_cast<size_t>(ScaleType::NumScaleTypes); ++i)
        scaleTypeCombo.addItem(ScaleNames[i], static_cast<int>(i) + 1);
    addAndMakeVisible(scaleTypeCombo);

    // APVTS Attachments
    auto& apvts = processor.getAPVTS();
    correctionAttach = std::make_unique<SliderAttachment>(apvts, ParamIDs::correction.getParamID(), correctionSlider);
    vibratoAttach    = std::make_unique<SliderAttachment>(apvts, ParamIDs::vibrato.getParamID(), vibratoSlider);
    formantAttach    = std::make_unique<SliderAttachment>(apvts, ParamIDs::formantShift.getParamID(), formantSlider);
    tuningAttach     = std::make_unique<SliderAttachment>(apvts, ParamIDs::tuningRef.getParamID(), tuningSlider);

    rootNoteAttach   = std::make_unique<ComboBoxAttachment>(apvts, ParamIDs::rootNote.getParamID(), rootNoteCombo);
    scaleTypeAttach  = std::make_unique<ComboBoxAttachment>(apvts, ParamIDs::scaleType.getParamID(), scaleTypeCombo);

    lowLatencyAttach = std::make_unique<ButtonAttachment>(apvts, ParamIDs::lowLatency.getParamID(), lowLatencyButton);
    followAttach     = std::make_unique<ButtonAttachment>(apvts, ParamIDs::followScale.getParamID(), followButton);

    apvts.addParameterListener(ParamIDs::rootNote.getParamID(), this);
    apvts.addParameterListener(ParamIDs::scaleType.getParamID(), this);
    apvts.addParameterListener(ParamIDs::followScale.getParamID(), this);
    apvts.addParameterListener(ParamIDs::customMask.getParamID(), this);

    updateKeyboardFromScale();
    updateValueLabels();
    updateLatencyStatus();
    startTimerHz(20);

    setSize(820, 560);
}

VocalTuneAudioProcessorEditor::~VocalTuneAudioProcessorEditor()
{
    stopTimer();
    auto& apvts = processor.getAPVTS();
    apvts.removeParameterListener(ParamIDs::rootNote.getParamID(), this);
    apvts.removeParameterListener(ParamIDs::scaleType.getParamID(), this);
    apvts.removeParameterListener(ParamIDs::followScale.getParamID(), this);
    apvts.removeParameterListener(ParamIDs::customMask.getParamID(), this);

    setLookAndFeel(nullptr);
}

void VocalTuneAudioProcessorEditor::parameterChanged(const juce::String& /*paramID*/, float /*newValue*/)
{
    // This callback may run on the audio thread. Post no allocated messages
    // and leave all component access to the message-thread timer.
    parameterDisplayDirty.store(true, std::memory_order_relaxed);
}

void VocalTuneAudioProcessorEditor::timerCallback()
{
    if (parameterDisplayDirty.exchange(false, std::memory_order_relaxed))
    {
        updateKeyboardFromScale();
        updateValueLabels();
    }
    updateLatencyStatus();
}

void VocalTuneAudioProcessorEditor::updateLatencyStatus()
{
    const bool prepared = processor.isAudioPrepared();
    const bool requested = processor.isLowLatencyRequested();
    const bool active = processor.isLowLatencyActive();
    const bool pending = prepared && requested != active;
    const juce::String latency(processor.getActiveLatencyMilliseconds(), 1);
    const juce::String buttonText = pending ? "Low Latency *" : "Low Latency";
    if (lowLatencyButton.getButtonText() != buttonText)
        lowLatencyButton.setButtonText(buttonText);

    const juce::String status = !prepared ? "Audio inactive"
        : pending ? "Audio restart required" : "Latency: " + latency + " ms";
    latencyStatusLabel.setText(status, juce::dontSendNotification);
    latencyStatusLabel.setColour(juce::Label::textColourId, pending
        ? juce::Colour::fromRGB(237, 184, 100) : VocalTuneLookAndFeel::textMuted);

    juce::String tooltip;
    if (prepared)
        tooltip = juce::String("Active Low Latency: ") + (active ? "ON" : "OFF")
                  + ". Reported plugin latency: " + latency + " ms.";
    else
        tooltip = "Audio is inactive. The selected mode will apply when the audio engine starts.";
    if (pending)
        tooltip += juce::String("\nRequested Low Latency: ") + (requested ? "ON" : "OFF")
                   + ". Restart the DAW audio device or reload this plugin to apply."
                     " Stopping playback alone may not apply the change.";
    else
        tooltip += "\nChanging this setting takes effect after restarting the DAW audio device or reloading the plugin.";
    lowLatencyButton.setTooltip(tooltip);
    latencyStatusLabel.setTooltip(tooltip);
}

void VocalTuneAudioProcessorEditor::sliderValueChanged(juce::Slider* /*slider*/)
{
    updateValueLabels();
}

void VocalTuneAudioProcessorEditor::updateValueLabels()
{
    correctionValueLabel.setText(juce::String(static_cast<int>(std::round(correctionSlider.getValue()))) + " %", juce::dontSendNotification);
    vibratoValueLabel.setText(juce::String(static_cast<int>(std::round(vibratoSlider.getValue()))) + " %", juce::dontSendNotification);
    
    // Formant shift display: e.g. 0 % or 0.0 st
    float formantVal = static_cast<float>(formantSlider.getValue());
    if (std::abs(formantVal) < 0.05f)
        formantValueLabel.setText("0 %", juce::dontSendNotification);
    else
        formantValueLabel.setText(juce::String(formantVal, 1) + " st", juce::dontSendNotification);
}

void VocalTuneAudioProcessorEditor::updateKeyboardFromScale()
{
    int root = rootNoteCombo.getSelectedItemIndex();
    if (root < 0) root = 5; // F

    int sTypeIdx = scaleTypeCombo.getSelectedItemIndex();
    if (sTypeIdx < 0) sTypeIdx = 0; // Natural Minor

    auto sType = static_cast<ScaleType>(sTypeIdx);
    bool follow = followButton.getToggleState();

    if (sType == ScaleType::Custom)
    {
        if (auto* p = processor.getAPVTS().getRawParameterValue(ParamIDs::customMask.getParamID()))
        {
            keyboard.setActiveMask(static_cast<uint16_t>(p->load()));
        }
    }
    else if (follow)
    {
        uint16_t mask = computeActiveNotesMask(root, sType);
        keyboard.setActiveMask(mask);
    }
}

void VocalTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Outer Dark Background
    g.setColour(VocalTuneLookAndFeel::bgDark);
    g.fillRect(bounds);

    // 2. Top Header Bar
    auto headerArea = bounds.removeFromTop(44.0f);
    g.setColour(juce::Colour::fromRGB(14, 17, 22));
    g.fillRect(headerArea);

    // Stylized logo icon: oval badge with stylized wave / 'F' emblem
    float logoX = 18.0f;
    float logoY = 10.0f;
    float logoW = 24.0f;
    float logoH = 24.0f;
    auto logoRect = juce::Rectangle<float>(logoX, logoY, logoW, logoH);

    g.setColour(juce::Colour::fromRGB(220, 225, 235));
    g.drawRoundedRectangle(logoRect, 5.0f, 1.5f);

    // Stylized emblem inside logo
    juce::Path emblem;
    emblem.startNewSubPath(logoX + 7.0f, logoY + 18.0f);
    emblem.lineTo(logoX + 7.0f, logoY + 6.0f);
    emblem.lineTo(logoX + 17.0f, logoY + 6.0f);
    emblem.startNewSubPath(logoX + 7.0f, logoY + 12.0f);
    emblem.lineTo(logoX + 14.0f, logoY + 12.0f);
    g.strokePath(emblem, juce::PathStrokeType(1.6f));

    // Plugin Title: "Vocal Tune"
    g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    g.setColour(VocalTuneLookAndFeel::textLight);
    g.drawText("Vocal Tune", juce::Rectangle<float>(logoX + logoW + 10.0f, 0.0f, 200.0f, 44.0f), juce::Justification::centredLeft, false);

    // 3. Bottom Cards Area
    // Draw 3 rounded cards
    float cardY = 345.0f;
    float cardH = 200.0f;
    float corner = 10.0f;

    auto drawCard = [&](juce::Rectangle<float> rect)
    {
        // Dark drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(rect.translated(0.0f, 2.0f), corner);

        // Card body
        g.setColour(VocalTuneLookAndFeel::cardBg);
        g.fillRoundedRectangle(rect, corner);

        // Inset highlight rim
        g.setColour(juce::Colour::fromRGB(70, 76, 88));
        g.drawRoundedRectangle(rect, corner, 1.0f);
    };

    // Card 1: Correction
    drawCard(juce::Rectangle<float>(15.0f, cardY, 160.0f, cardH));

    // Card 2: Vibrato & Formant Shift
    drawCard(juce::Rectangle<float>(185.0f, cardY, 245.0f, cardH));

    // Card 3: Scale & Tuning
    drawCard(juce::Rectangle<float>(440.0f, cardY, 365.0f, cardH));
}

void VocalTuneAudioProcessorEditor::resized()
{
    // 1. Header: Low Latency button
    lowLatencyButton.setBounds(getWidth() - 145, 9, 128, 26);
    latencyStatusLabel.setBounds(260, 9, getWidth() - 415, 26);

    // 2. Pitch Display
    pitchDisplay.setBounds(0, 44, getWidth(), 290);

    // 3. Card 1: Correction
    float cardY = 345.0f;
    correctionTitle.setBounds(20, static_cast<int>(cardY) + 12, 150, 20);
    correctionSlider.setBounds(48, static_cast<int>(cardY) + 42, 94, 94);
    correctionValueLabel.setBounds(20, static_cast<int>(cardY) + 150, 150, 22);

    // 4. Card 2: Vibrato & Formant Shift
    // Vibrato (left)
    vibratoTitle.setBounds(195, static_cast<int>(cardY) + 14, 105, 20);
    vibratoSlider.setBounds(216, static_cast<int>(cardY) + 48, 64, 64);
    vibratoValueLabel.setBounds(195, static_cast<int>(cardY) + 124, 105, 20);

    // Formant Shift (right)
    formantTitle.setBounds(310, static_cast<int>(cardY) + 14, 110, 20);
    formantSlider.setBounds(333, static_cast<int>(cardY) + 48, 64, 64);
    formantValueLabel.setBounds(310, static_cast<int>(cardY) + 124, 110, 20);

    // 5. Card 3: Pitch & Scale Controls
    int card3X = 440;
    // Row 1: Root Note & Scale/Follow
    rootNoteLabel.setBounds(card3X + 16, static_cast<int>(cardY) + 14, 75, 20);
    rootNoteCombo.setBounds(card3X + 16, static_cast<int>(cardY) + 38, 75, 25);

    scaleLabel.setBounds(card3X + 130, static_cast<int>(cardY) + 14, 60, 20);
    followButton.setBounds(card3X + 265, static_cast<int>(cardY) + 12, 85, 24);

    // Row 2: Interactive Keyboard
    keyboard.setBounds(card3X + 130, static_cast<int>(cardY) + 42, 220, 64);

    // Row 3: Tuning & Scale Type
    tuningLabel.setBounds(card3X + 16, static_cast<int>(cardY) + 82, 75, 20);
    tuningSlider.setBounds(card3X + 16, static_cast<int>(cardY) + 106, 75, 25);

    scaleTypeCombo.setBounds(card3X + 130, static_cast<int>(cardY) + 120, 220, 26);
}

} // namespace VocalTune

