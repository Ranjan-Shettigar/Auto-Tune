#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    int failures = 0;
    const auto check = [&](bool ok, const char* message)
    {
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    VocalTune::VocalTuneAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000, 64);
    processor.prepareToPlay(48000, 64);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    check(editor->getWidth() == 740 && editor->getHeight() == 460, "editor opens at its compact default size");
    check(editor->isResizable(), "editor supports host and corner resizing");
    const juce::File directory(argc > 1 ? juce::String(argv[1])
                                      : juce::File::getCurrentWorkingDirectory().getChildFile("ui-previews").getFullPathName());
    check(directory.createDirectory().wasOk(), "preview directory can be created");
    for (const auto size : { juce::Point<int>(660, 440), { 740, 460 }, { 1000, 640 },
                             { 1200, 800 }, { 1200, 440 }, { 660, 800 } })
    {
        editor->setSize(size.x, size.y);
        int knobs = 0, choices = 0;
        for (auto* child : editor->getChildren())
        {
            if (!child->isVisible() || dynamic_cast<juce::ResizableCornerComponent*>(child) != nullptr) continue;
            check(!child->getBounds().isEmpty() && editor->getLocalBounds().contains(child->getBounds()),
                  "visible controls stay inside the resized editor");
            if (auto* slider = dynamic_cast<juce::Slider*>(child))
                if (slider->getSliderStyle() == juce::Slider::RotaryVerticalDrag)
                {
                    ++knobs;
                    check(slider->getWidth() >= 48 && slider->getHeight() >= 48, "knobs remain usable at minimum size");
                }
            if (dynamic_cast<juce::ComboBox*>(child) != nullptr) ++choices;
            for (auto* other : editor->getChildren())
                if (other != child && other->isVisible()
                    && dynamic_cast<juce::ResizableCornerComponent*>(other) == nullptr)
                    check(!child->getBounds().intersects(other->getBounds()), "controls do not overlap after resizing");
        }
        check(knobs == 6 && choices == 5, "all tuning, expression and range controls are visible");
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
        auto output = directory.getChildFile("editor-" + juce::String(size.x) + "x" + juce::String(size.y) + ".png").createOutputStream();
        check(output != nullptr && juce::PNGImageFormat().writeImageToStream(snapshot, *output), "native editor preview renders");
    }
    // A knob edit must reach the hosted parameter through its attachment.
    for (auto* child : editor->getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
            if (slider->getComponentID() == "retuneSpeed")
                slider->setValue(27.5, juce::sendNotificationSync);
    check(std::abs(processor.getAPVTS().getRawParameterValue("retuneSpeed")->load() - 27.5f) < 0.01f,
          "retune control is connected to its parameter");
    const int latency = processor.getLatencySamples();
    auto* mode = processor.getAPVTS().getParameter("lowLatency");
    mode->setValueNotifyingHost(0.0f);
    juce::AudioBuffer<float> silentBlock(2, 64);
    silentBlock.clear();
    juce::MidiBuffer midi;
    processor.processBlock(silentBlock, midi);
    check(processor.getLatencySamples() == latency && processor.isLowLatencyActive(),
          "pending mode change preserves stream latency until preparation");
    processor.prepareToPlay(48000, 64);
    check(processor.getLatencySamples() == 3872 && !processor.isLowLatencyActive(),
          "mode is applied before host preparation and reports the correct latency");
    editor->setSize(1000, 640);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    editor.reset();
    VocalTune::VocalTuneAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    std::unique_ptr<juce::AudioProcessorEditor> reopened(restored.createEditor());
    check(reopened->getWidth() == 1000 && reopened->getHeight() == 640, "editor size survives state restore");
    reopened.reset();
    processor.releaseResources();
    std::cout << "Editor regression failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
