#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace VocalTune
{

class VocalTuneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VocalTuneLookAndFeel();
    ~VocalTuneLookAndFeel() override = default;

    // Custom Rotary Slider
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Custom ComboBox
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    // Custom PopupMenu
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    // Custom Buttons (Pill buttons like Low Latency and Follow)
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // Custom Label font
    juce::Font getLabelFont(juce::Label& label) override;

    // Colors
    static const juce::Colour bgDark;
    static const juce::Colour cardBg;
    static const juce::Colour cardBorder;
    static const juce::Colour accentBlue;
    static const juce::Colour accentAmber;
    static const juce::Colour textMuted;
    static const juce::Colour textLight;
    static const juce::Colour arcNeon;
};

} // namespace VocalTune

