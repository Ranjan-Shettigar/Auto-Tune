#include "PitchDisplayComponent.h"
#include "VocalTuneLookAndFeel.h"
#include <cmath>

namespace VocalTune
{
PitchDisplayComponent::PitchDisplayComponent(const AutoTuneEngine& eng) : engine(eng)
{
    startTimerHz(30);
}
PitchDisplayComponent::~PitchDisplayComponent() { stopTimer(); }
void PitchDisplayComponent::timerCallback()
{
    if (!isShowing()) return;
    const auto state = engine.getVisualizerState();
    const float next = currentCentDeviation * 0.49f + state.centDeviation * 0.51f;
    if (std::abs(next - currentCentDeviation) < 0.05f && isVoiced == state.isVoiced
        && currentNote == state.noteInOctave && currentOctave == state.octave) return;
    currentCentDeviation = next;
    isVoiced = state.isVoiced;
    currentNote = state.noteInOctave;
    currentOctave = state.octave;
    repaint();
}
void PitchDisplayComponent::resized() {}
void PitchDisplayComponent::paint(juce::Graphics& g)
{
    const float scale = juce::jlimit(1.0f, 1.6f, juce::jmin(getWidth() / 708.0f, getHeight() / 94.0f));
    const float width = getWidth() / scale;
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(scale));
    const auto bounds = juce::Rectangle<float>(0, 0, width, getHeight() / scale).reduced(0.5f);
    g.setColour(VocalTuneLookAndFeel::cardBg);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(VocalTuneLookAndFeel::cardBorder);
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
    const float middle = bounds.getCentreY();
    g.setColour(VocalTuneLookAndFeel::textMuted);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("TARGET NOTE", 18, static_cast<int>(middle) - 28, 112, 14, juce::Justification::centredLeft);
    g.setColour(isVoiced ? VocalTuneLookAndFeel::textLight : VocalTuneLookAndFeel::textMuted);
    g.setFont(juce::FontOptions(32.0f).withStyle("Bold"));
    const auto note = isVoiced ? juce::String(NoteNames[static_cast<size_t>(juce::jlimit(0, 11, currentNote))])
                                 + juce::String(currentOctave) : juce::String("--");
    g.drawText(note, 16, static_cast<int>(middle) - 12, 112, 38, juce::Justification::centredLeft);
    const float left = 140, right = bounds.getRight() - 116;
    const float y = middle + 3;
    const float centre = (left + right) * 0.5f;
    g.setColour(VocalTuneLookAndFeel::textMuted);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("FLAT", static_cast<int>(left), static_cast<int>(y) - 30, 50, 16, juce::Justification::centredLeft);
    g.drawText(isVoiced ? "TRACKING" : "LISTENING", static_cast<int>(centre) - 55, static_cast<int>(y) - 30, 110, 16, juce::Justification::centred);
    g.drawText("SHARP", static_cast<int>(right) - 50, static_cast<int>(y) - 30, 50, 16, juce::Justification::centredRight);
    g.setColour(VocalTuneLookAndFeel::cardBorder);
    g.fillRoundedRectangle(left, y - 3, right - left, 6, 3);
    g.setColour(VocalTuneLookAndFeel::accentBlue.withAlpha(0.17f));
    g.fillRoundedRectangle(centre - 12, y - 11, 24, 22, 4);
    for (int i = 0; i <= 4; ++i)
    {
        const float x = left + (right - left) * i / 4.0f;
        g.setColour(VocalTuneLookAndFeel::textMuted.withAlpha(0.45f));
        g.drawVerticalLine(static_cast<int>(x), y + 7, y + 11);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(juce::String(i * 25 - 50), static_cast<int>(x) - 20, static_cast<int>(y) + 14, 40, 12, juce::Justification::centred);
    }
    if (isVoiced)
    {
        const float x = left + (juce::jlimit(-50.0f, 50.0f, currentCentDeviation) + 50) / 100 * (right - left);
        g.setColour(VocalTuneLookAndFeel::accentBlue);
        g.fillRoundedRectangle(x - 2, y - 10, 4, 20, 2);
    }
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.setColour(isVoiced ? VocalTuneLookAndFeel::accentBlue : VocalTuneLookAndFeel::textMuted);
    const juce::String cents = isVoiced ? (currentCentDeviation > 0 ? "+" : "") + juce::String(currentCentDeviation, 0) : "--";
    g.drawText(cents, static_cast<int>(width) - 105, static_cast<int>(middle) - 22, 86, 30, juce::Justification::centredRight);
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(VocalTuneLookAndFeel::textMuted);
    g.drawText("cents", static_cast<int>(width) - 105, static_cast<int>(middle) + 9, 86, 16, juce::Justification::centredRight);
}
} // namespace VocalTune
