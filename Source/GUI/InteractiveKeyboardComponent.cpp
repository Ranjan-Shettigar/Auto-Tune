#include "InteractiveKeyboardComponent.h"

namespace VocalTune
{

InteractiveKeyboardComponent::InteractiveKeyboardComponent()
{
    setOpaque(false);
}

void InteractiveKeyboardComponent::setActiveMask(uint16_t mask)
{
    if (activeMask != mask)
    {
        activeMask = mask;
        repaint();
    }
}

void InteractiveKeyboardComponent::resized()
{
}

juce::Rectangle<float> InteractiveKeyboardComponent::getWhiteKeyRect(int whiteIndex) const
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    float keyW = b.getWidth() / 7.0f;
    return juce::Rectangle<float>(b.getX() + static_cast<float>(whiteIndex) * keyW, b.getY(), keyW, b.getHeight());
}

juce::Rectangle<float> InteractiveKeyboardComponent::getBlackKeyRect(int blackIndex) const
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    float keyW = b.getWidth() / 7.0f;
    float blackW = keyW * 0.65f;
    float blackH = b.getHeight() * 0.62f;

    float x = b.getX() + blackKeyDefs[blackIndex].position * keyW - blackW * 0.5f;
    return juce::Rectangle<float>(x, b.getY(), blackW, blackH);
}

int InteractiveKeyboardComponent::getNoteAtPosition(juce::Point<float> pt) const
{
    // Check black keys first (they sit on top)
    for (int i = 0; i < 5; ++i)
    {
        if (getBlackKeyRect(i).contains(pt))
            return blackKeyDefs[i].noteIndex;
    }

    // Check white keys
    for (int i = 0; i < 7; ++i)
    {
        if (getWhiteKeyRect(i).contains(pt))
            return whiteNotes[i];
    }

    return -1;
}

void InteractiveKeyboardComponent::mouseDown(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.position);
    if (note >= 0 && note < 12)
    {
        // Toggle note in mask
        uint16_t newMask = activeMask ^ static_cast<uint16_t>(1 << note);
        if (newMask == 0) // Keep at least one note active
            newMask = static_cast<uint16_t>(1 << note);

        setActiveMask(newMask);

        if (onMaskChanged)
            onMaskChanged(newMask);
    }
}

void InteractiveKeyboardComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    float corner = 6.0f;

    // Background container
    g.setColour(juce::Colour::fromRGB(20, 24, 30));
    g.fillRoundedRectangle(b, corner);

    const juce::Colour activeBlue    = juce::Colour::fromRGB(12, 115, 230);
    const juce::Colour inactiveWhite = juce::Colour::fromRGB(45, 52, 62);
    const juce::Colour inactiveBlack = juce::Colour::fromRGB(24, 28, 35);
    const juce::Colour keyBorder     = juce::Colour::fromRGB(18, 22, 28);

    // 1. Draw 7 White keys
    for (int i = 0; i < 7; ++i)
    {
        auto r = getWhiteKeyRect(i).reduced(0.5f);
        int note = whiteNotes[i];
        bool isActive = (activeMask & (1 << note)) != 0;

        juce::Colour keyCol = isActive ? activeBlue : inactiveWhite;
        g.setColour(keyCol);
        g.fillRoundedRectangle(r, 3.0f);

        // Border between keys
        g.setColour(keyBorder);
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
    }

    // 2. Draw 5 Black keys (layered on top)
    for (int i = 0; i < 5; ++i)
    {
        auto r = getBlackKeyRect(i).reduced(0.5f);
        int note = blackKeyDefs[i].noteIndex;
        bool isActive = (activeMask & (1 << note)) != 0;

        juce::Colour keyCol = isActive ? activeBlue.darker(0.15f) : inactiveBlack;
        g.setColour(keyCol);
        g.fillRoundedRectangle(r, 2.0f);

        g.setColour(keyBorder);
        g.drawRoundedRectangle(r, 2.0f, 1.0f);
    }

    // Outer border
    g.setColour(juce::Colour::fromRGB(35, 42, 52));
    g.drawRoundedRectangle(b, corner, 1.2f);
}

} // namespace VocalTune

