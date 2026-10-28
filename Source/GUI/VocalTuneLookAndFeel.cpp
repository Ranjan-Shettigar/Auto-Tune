#include "VocalTuneLookAndFeel.h"
#include <cmath>

namespace VocalTune
{

const juce::Colour VocalTuneLookAndFeel::bgDark      = juce::Colour::fromRGB(16, 20, 24);
const juce::Colour VocalTuneLookAndFeel::cardBg      = juce::Colour::fromRGB(25, 31, 37);
const juce::Colour VocalTuneLookAndFeel::cardBorder  = juce::Colour::fromRGB(43, 52, 60);
const juce::Colour VocalTuneLookAndFeel::accentBlue  = juce::Colour::fromRGB(116, 224, 192);
const juce::Colour VocalTuneLookAndFeel::accentAmber = juce::Colour::fromRGB(116, 224, 192);
const juce::Colour VocalTuneLookAndFeel::textMuted   = juce::Colour::fromRGB(158, 175, 186);
const juce::Colour VocalTuneLookAndFeel::textLight   = juce::Colour::fromRGB(232, 240, 244);
const juce::Colour VocalTuneLookAndFeel::arcNeon     = juce::Colour::fromRGB(116, 224, 192);

VocalTuneLookAndFeel::VocalTuneLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, bgDark);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(32, 36, 44));
    setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(55, 62, 75));
    setColour(juce::ComboBox::textColourId, textLight);
    setColour(juce::ComboBox::arrowColourId, textMuted);

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB(26, 30, 38));
    setColour(juce::PopupMenu::textColourId, textLight);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accentBlue);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void VocalTuneLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(5.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getComponentID() == "formantShift";
    const float start = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, rotaryStartAngle, rotaryEndAngle, true);
    value.addCentredArc(centre.x, centre.y, radius, radius, 0, juce::jmin(start, angle), juce::jmax(start, angle), true);
    const auto stroke = juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(cardBorder);
    g.strokePath(track, stroke);
    g.setColour(slider.isEnabled() ? accentBlue : textMuted);
    g.strokePath(value, stroke);
    const auto cap = bounds.withSizeKeepingCentre((radius - 7) * 2, (radius - 7) * 2);
    g.setColour(juce::Colour::fromRGB(34, 43, 51));
    g.fillEllipse(cap);
    g.setColour(slider.isMouseOverOrDragging() ? accentBlue.withAlpha(0.6f) : cardBorder);
    g.drawEllipse(cap, 1.0f);
    const auto direction = juce::Point<float>(std::sin(angle), -std::cos(angle));
    g.setColour(textLight);
    g.drawLine(juce::Line<float>(centre + direction * (radius * 0.34f),
                                centre + direction * (radius * 0.64f)), 2.5f);
}

void VocalTuneLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                        int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                        juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    float corner = 6.0f;

    // Dark rounded background
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, corner);

    // Outline
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // Dropdown arrow (triangle)
    float arrowW = 8.0f;
    float arrowH = 5.0f;
    float arrowX = static_cast<float>(width) - 16.0f;
    float arrowY = static_cast<float>(height) * 0.5f - arrowH * 0.5f;

    juce::Path arrow;
    arrow.startNewSubPath(arrowX, arrowY);
    arrow.lineTo(arrowX + arrowW, arrowY);
    arrow.lineTo(arrowX + arrowW * 0.5f, arrowY + arrowH);
    arrow.closeSubPath();

    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void VocalTuneLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(juce::FontOptions(juce::jlimit(12.0f, 18.0f, box.getHeight() * 0.46f)));
    label.setJustificationType(juce::Justification::centredLeft);
}

void VocalTuneLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
    g.setColour(findColour(juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour::fromRGB(55, 62, 75));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
}

void VocalTuneLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool /*hasSubMenu*/,
                                             const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                             const juce::Drawable* /*icon*/, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        g.setColour(juce::Colour::fromRGB(45, 52, 65));
        g.fillRect(r.removeFromTop(1));
        return;
    }

    auto r = area.toFloat().reduced(2.0f, 1.0f);

    if (isHighlighted && isActive)
    {
        g.setColour(accentBlue);
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(bgDark);
    }
    else
    {
        g.setColour(textColour != nullptr ? *textColour : (isActive ? textLight : textMuted.darker(0.3f)));
    }

    g.setFont(juce::FontOptions(13.0f));

    auto textArea = area.reduced(10, 0);
    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);

    if (isTicked)
    {
        g.setColour((isHighlighted && isActive) ? bgDark : accentBlue);
        float tickSize = 6.0f;
        float tickX = static_cast<float>(area.getRight()) - 16.0f;
        float tickY = static_cast<float>(area.getCentreY());

        juce::Path tick;
        tick.startNewSubPath(tickX - tickSize, tickY);
        tick.lineTo(tickX - tickSize * 0.4f, tickY + tickSize * 0.6f);
        tick.lineTo(tickX + tickSize * 0.6f, tickY - tickSize * 0.6f);
        g.strokePath(tick, juce::PathStrokeType(2.0f));
    }
}

void VocalTuneLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    float corner = bounds.getHeight() * 0.5f; // Pill shaped

    bool isToggled = button.getToggleState();

    if (isToggled)
    {
        // Bright active blue pill button as seen in image ("Low Latency", "Follow")
        juce::Colour btnCol = shouldDrawButtonAsHighlighted ? accentBlue.brighter(0.1f) : accentBlue;
        g.setColour(btnCol);
        g.fillRoundedRectangle(bounds, corner);

        // Subtle top rim highlight
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, corner, 1.0f);
    }
    else
    {
        // Dark muted inactive pill
        juce::Colour inactiveBg = shouldDrawButtonAsHighlighted ? juce::Colour::fromRGB(45, 52, 63) : juce::Colour::fromRGB(35, 40, 50);
        g.setColour(inactiveBg);
        g.fillRoundedRectangle(bounds, corner);

        g.setColour(juce::Colour::fromRGB(60, 68, 80));
        g.drawRoundedRectangle(bounds, corner, 1.0f);
    }
}

void VocalTuneLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                         bool /*shouldDrawButtonAsHighlighted*/,
                                         bool /*shouldDrawButtonAsDown*/)
{
    g.setFont(juce::FontOptions(juce::jlimit(12.0f, 17.0f, button.getHeight() * 0.43f)).withStyle("Bold"));
    g.setColour(button.getToggleState() ? bgDark : textLight);

    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font VocalTuneLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}

} // namespace VocalTune

