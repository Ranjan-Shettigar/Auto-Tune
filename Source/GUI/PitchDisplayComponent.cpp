#include "PitchDisplayComponent.h"
#include <cmath>

namespace VocalTune
{

PitchDisplayComponent::PitchDisplayComponent(const AutoTuneEngine& eng)
    : engine(eng)
{
    startTimerHz(60); // 60 FPS animation
}

PitchDisplayComponent::~PitchDisplayComponent()
{
    stopTimer();
}

void PitchDisplayComponent::timerCallback()
{
    auto state = engine.getVisualizerState();

    // Smooth animation ballistics
    float targetCents = state.centDeviation;
    currentCentDeviation = currentCentDeviation * 0.70f + targetCents * 0.30f;

    isVoiced = state.isVoiced;
    currentNote = state.noteInOctave;
    currentOctave = state.octave;

    repaint();
}

void PitchDisplayComponent::resized()
{
}

float PitchDisplayComponent::getArcY(float x, float width, float height) const
{
    // Arc spanning horizontally with apex in upper region
    float cx = width * 0.5f;
    float rx = width * 0.46f;
    float apexY = height * 0.30f;
    float sag = height * 0.38f;

    float dx = (x - cx) / rx;
    if (std::abs(dx) > 1.05f) dx = (dx > 0.0f) ? 1.05f : -1.05f;

    // Parabolic / elliptical curve
    return apexY + sag * (dx * dx);
}

void PitchDisplayComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float w = bounds.getWidth();
    float h = bounds.getHeight();

    // 1. Background gradient (dark navy cosmic gradient with subtle glow under arc)
    juce::ColourGradient bgGrad(juce::Colour::fromRGB(11, 22, 38), w * 0.5f, h * 0.4f,
                                juce::Colour::fromRGB(4, 7, 12), w * 0.5f, h, false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Subtle ambient glow centered on apex of arc
    juce::ColourGradient glowGrad(juce::Colour::fromRGB(15, 60, 110).withAlpha(0.35f), w * 0.5f, h * 0.32f,
                                 juce::Colours::transparentBlack, w * 0.5f, h * 0.9f, true);
    g.setGradientFill(glowGrad);
    g.fillEllipse(w * 0.15f, h * 0.05f, w * 0.70f, h * 0.85f);

    // 2. The Curved Arc
    float leftX = w * 0.06f;
    float rightX = w * 0.94f;
    int numPoints = 80;

    juce::Path arcPath;
    arcPath.startNewSubPath(leftX, getArcY(leftX, w, h));

    for (int i = 1; i <= numPoints; ++i)
    {
        float x = leftX + (rightX - leftX) * (static_cast<float>(i) / static_cast<float>(numPoints));
        float y = getArcY(x, w, h);
        arcPath.lineTo(x, y);
    }

    // Outer glow for the arc
    g.setColour(juce::Colour::fromRGB(30, 120, 200).withAlpha(0.25f));
    g.strokePath(arcPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Core neon arc
    juce::ColourGradient arcGrad(juce::Colour::fromRGB(25, 95, 160), leftX, h * 0.5f,
                                juce::Colour::fromRGB(55, 175, 255), w * 0.5f, h * 0.3f, false);
    g.setGradientFill(arcGrad);
    g.strokePath(arcPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Calibration Ticks and Numbers (-100, -50, 0, 50, 100)
    struct CalibrationMark
    {
        float cents;
        const char* label;
    };

    const CalibrationMark marks[5] = {
        { -100.0f, "-100" },
        {  -50.0f,  "-50" },
        {    0.0f,    ""  }, // 0 has a tick at top
        {   50.0f,   "50" },
        {  100.0f,  "100" }
    };

    g.setFont(juce::FontOptions(13.0f));

    for (int i = 0; i < 5; ++i)
    {
        float u = (marks[i].cents + 100.0f) / 200.0f;
        float x = leftX + u * (rightX - leftX);
        float y = getArcY(x, w, h);

        // Tick mark projecting downwards perpendicular to arc tangent
        float tickLen = (i == 2) ? 14.0f : 9.0f;
        juce::Path tickPath;
        tickPath.startNewSubPath(x, y);
        tickPath.lineTo(x, y + tickLen);

        g.setColour(juce::Colour::fromRGB(45, 115, 165));
        g.strokePath(tickPath, juce::PathStrokeType(1.5f));

        // Label below tick mark
        if (marks[i].label[0] != '\0')
        {
            juce::Rectangle<float> labelBox(x - 28.0f, y + tickLen + 5.0f, 56.0f, 18.0f);
            g.setColour(juce::Colour::fromRGB(40, 115, 165));
            g.drawText(marks[i].label, labelBox, juce::Justification::centred, false);
        }
    }

    // 4. Center Note Readout Box
    float boxW = 125.0f;
    float boxH = 46.0f;
    float boxX = w * 0.5f - boxW * 0.5f;
    float boxY = h * 0.58f;
    auto noteBox = juce::Rectangle<float>(boxX, boxY, boxW, boxH);

    // Pill background
    g.setColour(juce::Colour::fromRGB(8, 17, 27));
    g.fillRoundedRectangle(noteBox, 10.0f);

    // Pill border
    g.setColour(juce::Colour::fromRGB(24, 46, 68));
    g.drawRoundedRectangle(noteBox, 10.0f, 1.2f);

    // Note Text (e.g. "D# 3")
    juce::String noteStr = NoteNames[static_cast<size_t>(juce::jlimit(0, 11, currentNote))];
    juce::String fullNoteText = noteStr + " " + juce::String(currentOctave);

    g.setFont(juce::FontOptions(26.0f).withStyle("Bold"));
    g.setColour(juce::Colour::fromRGB(165, 215, 255));
    g.drawText(fullNoteText, noteBox, juce::Justification::centred, false);

    // 5. Active Tuning Needle
    // Maps cent deviation (-100 to +100) to X position on arc
    float clampedCents = juce::jlimit(-100.0f, 100.0f, currentCentDeviation);
    float needleNorm = (clampedCents + 100.0f) / 200.0f;
    float curNeedleX = leftX + needleNorm * (rightX - leftX);
    float curArcY = getArcY(curNeedleX, w, h);

    // Needle vertical line
    float needleTopY = curArcY - 14.0f;
    float needleBottomY = boxY - 3.0f;

    // Glowing cyan needle
    juce::Colour needleColor = isVoiced ? juce::Colour::fromRGB(40, 185, 255) : juce::Colour::fromRGB(30, 110, 160).withAlpha(0.6f);

    // Outer glow
    g.setColour(needleColor.withAlpha(0.30f));
    g.drawLine(curNeedleX, needleTopY, curNeedleX, needleBottomY, 6.0f);

    // Solid core line
    g.setColour(needleColor);
    g.drawLine(curNeedleX, needleTopY, curNeedleX, needleBottomY, 3.2f);

    // Horizontal cross-tick marker attached to the needle (as seen in image)
    float markerW = 18.0f;
    float markerY = curArcY;
    g.drawLine(curNeedleX - markerW, markerY, curNeedleX, markerY, 3.0f);
}

} // namespace VocalTune

