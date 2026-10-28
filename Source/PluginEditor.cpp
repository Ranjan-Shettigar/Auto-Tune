#include "PluginEditor.h"

namespace VocalTune
{
VocalTuneAudioProcessorEditor::VocalTuneAudioProcessorEditor(VocalTuneAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), pitchDisplay(p.getEngine())
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    auto& apvts = processor.getAPVTS();
    auto label = [this](juce::Label& l, const char* name)
    {
        l.setText(name, juce::dontSendNotification);
        l.setFont(juce::FontOptions(12.0f));
        l.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textMuted);
        addAndMakeVisible(l);
    };
    auto rotary = [&](juce::Slider& slider, juce::Label& title, juce::Label& value,
                      const char* name, const juce::ParameterID& id,
                      std::unique_ptr<SliderAttachment>& attachment)
    {
        label(title, name);
        title.setJustificationType(juce::Justification::centred);
        label(value, "");
        value.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
        value.setJustificationType(juce::Justification::centred);
        value.setColour(juce::Label::textColourId, VocalTuneLookAndFeel::textLight);
        slider.setName(name);
        slider.setComponentID(id.getParamID());
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
        slider.setDoubleClickReturnValue(true, apvts.getParameter(id.getParamID())->convertFrom0to1(
            apvts.getParameter(id.getParamID())->getDefaultValue()));
        slider.addListener(this);
        addAndMakeVisible(slider);
        attachment = std::make_unique<SliderAttachment>(apvts, id.getParamID(), slider);
    };
    rotary(correctionSlider, correctionTitle, correctionValueLabel, "Correction", ParamIDs::correction, correctionAttach);
    rotary(retuneSpeedSlider, retuneSpeedTitle, retuneSpeedValueLabel, "Retune", ParamIDs::retuneSpeed, retuneSpeedAttach);
    rotary(flexTuneSlider, flexTuneTitle, flexTuneValueLabel, "Flex", ParamIDs::flexTune, flexTuneAttach);
    rotary(vibratoSlider, vibratoTitle, vibratoValueLabel, "Vibrato", ParamIDs::vibrato, vibratoAttach);
    rotary(formantSlider, formantTitle, formantValueLabel, "Formant", ParamIDs::formantShift, formantAttach);
    rotary(harmonizeSlider, harmonizeTitle, harmonizeValueLabel, "Harmony", ParamIDs::harmonize, harmonizeAttach);
    correctionSlider.setTooltip("Pitch correction amount. Double-click a knob to restore its default.");
    retuneSpeedSlider.setTooltip("Time to move toward the target note, in milliseconds.");
    flexTuneSlider.setTooltip("Preserve expressive pitch movement farther from the target note.");
    vibratoSlider.setTooltip("Add gentle pitch modulation.");
    formantSlider.setTooltip("Adjust vocal resonance colour in semitones.");
    harmonizeSlider.setTooltip("Blend a second voice at the selected harmony interval.");

    auto combo = [&](juce::ComboBox& box, juce::Label& title, const char* name,
                     const auto& choices, const juce::ParameterID& id,
                     std::unique_ptr<ComboBoxAttachment>& attachment)
    {
        label(title, name);
        box.setName(name);
        int index = 1;
        for (const auto* choice : choices) box.addItem(choice, index++);
        addAndMakeVisible(box);
        attachment = std::make_unique<ComboBoxAttachment>(apvts, id.getParamID(), box);
    };
    combo(rootNoteCombo, rootNoteLabel, "Key", NoteNames, ParamIDs::rootNote, rootNoteAttach);
    combo(scaleTypeCombo, scaleLabel, "Scale", ScaleNames, ParamIDs::scaleType, scaleTypeAttach);
    combo(vocalRangeCombo, vocalRangeTitle, "Input range", VocalRangeNames, ParamIDs::vocalRange, vocalRangeAttach);
    combo(trackingSensCombo, trackingSensTitle, "Tracking", TrackingSensNames, ParamIDs::trackingSens, trackingSensAttach);
    combo(harmonyIntervalCombo, harmonyIntervalTitle, "Harmony interval", HarmonyIntervalNames, ParamIDs::harmonyInterval, harmonyIntervalAttach);
    for (auto* button : { &lowLatencyButton, &followButton })
    {
        button->setClickingTogglesState(true);
        addAndMakeVisible(button);
    }
    lowLatencyButton.setButtonText("Low Latency");
    lowLatencyButton.setName("Low Latency");
    followButton.setButtonText("Follow scale");
    followButton.setTooltip("Constrain correction to the selected key and scale.");
    lowLatencyAttach = std::make_unique<ButtonAttachment>(apvts, ParamIDs::lowLatency.getParamID(), lowLatencyButton);
    followAttach = std::make_unique<ButtonAttachment>(apvts, ParamIDs::followScale.getParamID(), followButton);
    label(latencyStatusLabel, "");
    latencyStatusLabel.setJustificationType(juce::Justification::centredRight);
    label(tuningLabel, "Reference");
    tuningSlider.setName("Tuning reference");
    tuningSlider.setSliderStyle(juce::Slider::LinearBar);
    tuningSlider.setTextValueSuffix(" Hz");
    tuningSlider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
    tuningSlider.setColour(juce::Slider::textBoxTextColourId, VocalTuneLookAndFeel::textLight);
    tuningSlider.setColour(juce::Slider::textBoxOutlineColourId, VocalTuneLookAndFeel::cardBorder);
    addAndMakeVisible(tuningSlider);
    tuningAttach = std::make_unique<SliderAttachment>(apvts, ParamIDs::tuningRef.getParamID(), tuningSlider);
    label(keyboardTitle, "Allowed notes");
    addAndMakeVisible(keyboard);
    addAndMakeVisible(pitchDisplay);
    keyboard.setName("Allowed notes; click to create a custom scale");
    keyboard.onMaskChanged = [this](uint16_t mask)
    {
        auto& state = processor.getAPVTS();
        auto setParameter = [&](const juce::ParameterID& id, float value)
        {
            if (auto* param = state.getParameter(id.getParamID()))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(value));
                param->endChangeGesture();
            }
        };
        setParameter(ParamIDs::customMask, static_cast<float>(mask));
        setParameter(ParamIDs::scaleType, static_cast<float>(ScaleType::Custom));
    };
    for (const auto& id : { ParamIDs::rootNote, ParamIDs::scaleType, ParamIDs::followScale, ParamIDs::customMask })
        apvts.addParameterListener(id.getParamID(), this);
    updateKeyboardFromScale();
    updateValueLabels();
    updateLatencyStatus();
    const int preferredWidth = processor.editorWidth.load();
    const int preferredHeight = processor.editorHeight.load();
    setResizable(true, true);
    setResizeLimits(660, 440, 1200, 800);
    setSize(preferredWidth, preferredHeight);
    startTimerHz(20);
}

VocalTuneAudioProcessorEditor::~VocalTuneAudioProcessorEditor()
{
    stopTimer();
    for (const auto& id : { ParamIDs::rootNote, ParamIDs::scaleType, ParamIDs::followScale, ParamIDs::customMask })
        processor.getAPVTS().removeParameterListener(id.getParamID(), this);
    setLookAndFeel(nullptr);
}

void VocalTuneAudioProcessorEditor::parameterChanged(const juce::String&, float)
{
    parameterDisplayDirty.store(true, std::memory_order_relaxed);
}

void VocalTuneAudioProcessorEditor::timerCallback()
{
    if (!isShowing()) return;
    if (parameterDisplayDirty.exchange(false, std::memory_order_relaxed)) updateKeyboardFromScale();
    updateLatencyStatus();
}

void VocalTuneAudioProcessorEditor::updateLatencyStatus()
{
    const bool prepared = processor.isAudioPrepared();
    const bool requested = processor.isLowLatencyRequested();
    const bool active = processor.isLowLatencyActive();
    const double latencyMs = processor.getActiveLatencyMilliseconds();
    if (latencyDisplayInitialised && prepared == displayedPrepared
        && requested == displayedRequested && active == displayedActive
        && latencyMs == displayedLatencyMs)
        return;
    latencyDisplayInitialised = true;
    displayedPrepared = prepared;
    displayedRequested = requested;
    displayedActive = active;
    displayedLatencyMs = latencyMs;
    const bool pending = prepared && requested != active;
    const juce::String latency(latencyMs, 1);
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

void VocalTuneAudioProcessorEditor::sliderValueChanged(juce::Slider*)
{
    updateValueLabels();
}

void VocalTuneAudioProcessorEditor::updateValueLabels()
{
    auto value = [](juce::Label& label, double number, int decimals, const char* unit)
    {
        label.setText(juce::String(number, decimals) + unit, juce::dontSendNotification);
    };
    value(correctionValueLabel, correctionSlider.getValue(), 0, " %");
    value(retuneSpeedValueLabel, retuneSpeedSlider.getValue(), 1, " ms");
    value(flexTuneValueLabel, flexTuneSlider.getValue(), 0, " %");
    value(vibratoValueLabel, vibratoSlider.getValue(), 0, " %");
    value(formantValueLabel, formantSlider.getValue(), 1, " st");
    value(harmonizeValueLabel, harmonizeSlider.getValue(), 0, " %");
}

void VocalTuneAudioProcessorEditor::updateKeyboardFromScale()
{
    auto& state = processor.getAPVTS();
    const auto scale = static_cast<ScaleType>(static_cast<int>(state.getRawParameterValue(ParamIDs::scaleType.getParamID())->load()));
    const int root = static_cast<int>(state.getRawParameterValue(ParamIDs::rootNote.getParamID())->load());
    const bool follow = state.getRawParameterValue(ParamIDs::followScale.getParamID())->load() > 0.5f;
    keyboard.setActiveMask(scale == ScaleType::Custom
        ? static_cast<uint16_t>(state.getRawParameterValue(ParamIDs::customMask.getParamID())->load())
        : follow ? computeActiveNotesMask(root, scale) : 0x0fff);
}

void VocalTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(VocalTuneLookAndFeel::bgDark);
    g.setColour(VocalTuneLookAndFeel::accentBlue);
    for (int i = 0; i < 5; ++i)
    {
        const float height = 8.0f + 5.0f * static_cast<float>(2 - std::abs(i - 2));
        g.fillRoundedRectangle(18.0f + i * 5.0f, 26.0f - height * 0.5f, 2.5f, height, 1.2f);
    }
    g.setFont(juce::FontOptions(19.0f).withStyle("Bold"));
    g.setColour(VocalTuneLookAndFeel::textLight);
    g.drawText("vocal tune", 52, 8, 150, 32, juce::Justification::centredLeft);
    for (const auto& panel : { controlsPanel, scalePanel })
    {
        g.setColour(VocalTuneLookAndFeel::cardBg);
        g.fillRoundedRectangle(panel.toFloat(), 10.0f);
        g.setColour(VocalTuneLookAndFeel::cardBorder);
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 10.0f, 1.0f);
    }
    g.setColour(VocalTuneLookAndFeel::textMuted);
    g.setFont(juce::FontOptions(10.5f));
    g.drawText("VOCAL TUNE  /  PITCH & EXPRESSION", 18, getHeight() - 20, 330, 16, juce::Justification::centredLeft);
}

void VocalTuneAudioProcessorEditor::resized()
{
    if (getWidth() < 660 || getHeight() < 440) return;
    processor.editorWidth.store(getWidth());
    processor.editorHeight.store(getHeight());
    lowLatencyButton.setBounds(getWidth() - 138, 12, 120, 28);
    latencyStatusLabel.setBounds(220, 12, getWidth() - 368, 28);
    auto content = getLocalBounds().reduced(16, 0);
    content.removeFromTop(52);
    content.removeFromBottom(24);
    pitchDisplay.setBounds(content.removeFromTop(88 + (getHeight() - 440) / 4));
    content.removeFromTop(10);
    controlsPanel = content.removeFromTop(138 + (getHeight() - 440) / 3);
    content.removeFromTop(10);
    scalePanel = content;

    juce::Slider* knobs[] { &correctionSlider, &retuneSpeedSlider, &flexTuneSlider, &vibratoSlider, &formantSlider, &harmonizeSlider };
    juce::Label* titles[] { &correctionTitle, &retuneSpeedTitle, &flexTuneTitle, &vibratoTitle, &formantTitle, &harmonizeTitle };
    juce::Label* values[] { &correctionValueLabel, &retuneSpeedValueLabel, &flexTuneValueLabel, &vibratoValueLabel, &formantValueLabel, &harmonizeValueLabel };
    const float scale = juce::jlimit(1.0f, 1.5f, juce::jmin(getWidth() / 740.0f, getHeight() / 460.0f));
    const auto px = [scale](int value) { return juce::roundToInt(value * scale); };
    auto row = controlsPanel.reduced(px(8), px(10));
    for (int i = 0; i < 6; ++i)
    {
        const int left = row.getX() + row.getWidth() * i / 6;
        const int right = row.getX() + row.getWidth() * (i + 1) / 6;
        juce::Rectangle<int> cell(left, row.getY(), right - left, row.getHeight());
        titles[i]->setFont(juce::FontOptions(12.0f * scale));
        values[i]->setFont(juce::FontOptions(16.0f * scale).withStyle("Bold"));
        titles[i]->setBounds(cell.removeFromTop(px(18)));
        values[i]->setBounds(cell.removeFromBottom(px(22)));
        const int size = juce::jmin(112, cell.getWidth() - 10, cell.getHeight());
        knobs[i]->setBounds(cell.withSizeKeepingCentre(size, size));
    }

    auto fields = scalePanel.reduced(px(12), px(10));
    auto top = fields.removeFromTop(px(40));
    auto place = [&](juce::Rectangle<int> area, juce::Label& label, juce::Component& control)
    {
        label.setFont(juce::FontOptions(12.0f * scale));
        label.setBounds(area.removeFromTop(px(14)));
        control.setBounds(area);
    };
    place(top.removeFromLeft(px(56)), rootNoteLabel, rootNoteCombo);
    top.removeFromLeft(px(8));
    const int scaleWidth = top.getWidth() - px(130) - px(96) - px(88) - 3 * px(8);
    place(top.removeFromLeft(scaleWidth), scaleLabel, scaleTypeCombo);
    top.removeFromLeft(px(8));
    place(top.removeFromLeft(px(130)), vocalRangeTitle, vocalRangeCombo);
    top.removeFromLeft(px(8));
    place(top.removeFromLeft(px(96)), trackingSensTitle, trackingSensCombo);
    top.removeFromLeft(px(8));
    followButton.setBounds(top.withTrimmedTop(px(14)));
    fields.removeFromTop(px(6));
    const int secondHeight = juce::jmin(px(72), fields.getHeight());
    auto bottom = fields.removeFromTop(secondHeight);
    place(bottom.removeFromRight(px(100)), tuningLabel, tuningSlider);
    bottom.removeFromRight(px(10));
    place(bottom.removeFromRight(px(148)), harmonyIntervalTitle, harmonyIntervalCombo);
    bottom.removeFromRight(px(10));
    place(bottom, keyboardTitle, keyboard);
    repaint();
}
} // namespace VocalTune
