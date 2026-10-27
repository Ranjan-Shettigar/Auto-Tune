#include "VocalTuneLookAndFeel.h"
#include <cmath>

namespace VocalTune
{

const juce::Colour VocalTuneLookAndFeel::bgDark      = juce::Colour::fromRGB(19, 23, 29);
const juce::Colour VocalTuneLookAndFeel::cardBg      = juce::Colour::fromRGB(54, 58, 66);
const juce::Colour VocalTuneLookAndFeel::cardBorder  = juce::Colour::fromRGB(40, 44, 50);
const juce::Colour VocalTuneLookAndFeel::accentBlue  = juce::Colour::fromRGB(15, 120, 240);
const juce::Colour VocalTuneLookAndFeel::accentAmber = juce::Colour::fromRGB(240, 175, 45);
const juce::Colour VocalTuneLookAndFeel::textMuted   = juce::Colour::fromRGB(160, 170, 185);
const juce::Colour VocalTuneLookAndFeel::textLight   = juce::Colour::fromRGB(235, 240, 248);
const juce::Colour VocalTuneLookAndFeel::arcNeon     = juce::Colour::fromRGB(45, 155, 235);

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
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();

    bool isCorrection = (slider.getComponentID() == "correction");
    float arcThickness = isCorrection ? 4.5f : 3.0f;
    float trackRadius = radius - arcThickness * 0.5f;

    // 1. Background arc track
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);

    g.setColour(juce::Colour::fromRGB(24, 28, 35));
    g.strokePath(backgroundArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Active arc value
    float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                               rotaryStartAngle, currentAngle, true);

        if (isCorrection)
        {
            // Glowing amber/yellow arc as seen in image
            g.setColour(accentAmber);
            g.strokePath(valueArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Soft glow overlay
            g.setColour(accentAmber.withAlpha(0.25f));
            g.strokePath(valueArc, juce::PathStrokeType(arcThickness + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            // Subtle cool metallic/blue arc
            g.setColour(juce::Colour::fromRGB(80, 110, 140));
            g.strokePath(valueArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 3. Inner metallic knob cap
    float knobRadius = radius - arcThickness - (isCorrection ? 4.0f : 2.5f);
    auto knobArea = juce::Rectangle<float>(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    // Drop shadow under knob
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(knobArea.translated(0.0f, 2.0f));

    // Outer bezel rim
    juce::ColourGradient rimGrad(juce::Colour::fromRGB(65, 72, 82), knobArea.getX(), knobArea.getY(),
                                juce::Colour::fromRGB(20, 24, 30), knobArea.getRight(), knobArea.getBottom(), false);
    g.setGradientFill(rimGrad);
    g.fillEllipse(knobArea);

    // Inner dark brushed metallic face
    auto innerFace = knobArea.reduced(2.0f);
    juce::ColourGradient faceGrad(juce::Colour::fromRGB(44, 48, 56), innerFace.getCentreX(), innerFace.getY(),
                                 juce::Colour::fromRGB(25, 28, 33), innerFace.getCentreX(), innerFace.getBottom(), false);
    g.setGradientFill(faceGrad);
    g.fillEllipse(innerFace);

    // Subtle radial metallic sheen
    juce::ColourGradient sheenGrad(juce::Colours::white.withAlpha(0.07f), innerFace.getCentreX(), innerFace.getY(),
                                  juce::Colours::transparentWhite, innerFace.getCentreX(), innerFace.getCentreY(), true);
    g.setGradientFill(sheenGrad);
    g.fillEllipse(innerFace);

    // 4. Pointer indicator notch / line
    juce::Path pointer;
    float pointerLength = knobRadius * 0.70f;
    float pointerThickness = isCorrection ? 3.0f : 2.0f;

    pointer.startNewSubPath(centre.x, centre.y - knobRadius + 3.0f);
    pointer.lineTo(centre.x, centre.y - knobRadius + 3.0f + pointerLength * 0.5f);

    pointer.applyTransform(juce::AffineTransform::rotation(currentAngle, centre.x, centre.y));

    if (isCorrection)
    {
        g.setColour(juce::Colour::fromRGB(240, 245, 255));
    }
    else
    {
        g.setColour(juce::Colour::fromRGB(180, 195, 210));
    }
    g.strokePath(pointer, juce::PathStrokeType(pointerThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
    label.setFont(getLabelFont(label));
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
        g.setColour(juce::Colours::white);
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
        g.setColour((isHighlighted && isActive) ? juce::Colours::white : accentBlue);
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
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.setColour(button.getToggleState() ? juce::Colours::white : textMuted);

    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font VocalTuneLookAndFeel::getLabelFont(juce::Label& /*label*/)
{
    return juce::FontOptions(13.0f);
}

} // namespace VocalTune

