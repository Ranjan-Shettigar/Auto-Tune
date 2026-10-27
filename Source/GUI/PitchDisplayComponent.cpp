#include "PitchDisplayComponent.h"
#include <cmath>

namespace VocalTune
{

PitchDisplayComponent::PitchDisplayComponent(const AutoTuneEngine& eng)
    : engine(eng)
{
    startTimerHz(60);
}

PitchDisplayComponent::~PitchDisplayComponent()
{
    stopTimer();
}

void PitchDisplayComponent::timerCallback()
{
    auto state = engine.getVisualizerState();

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
    float cx = width * 0.5f;
    float rx = width * 0.46f;
    float apexY = height * 0.28f;
    float sag = height * 0.36f;

    float dx = (x - cx) / rx;
    if (std::abs(dx) > 1.05f) dx = (dx > 0.0f) ? 1.05f : -1.05f;

    return apexY + sag * (dx * dx);
}

void PitchDisplayComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float w = bounds.getWidth();
    float h = bounds.getHeight();

    // 1. Dark cosmic navy background
    juce::ColourGradient bgGrad(juce::Colour::fromRGB(11, 22, 38), w * 0.5f, h * 0.35f,
                                juce::Colour::fromRGB(4, 7, 12), w * 0.5f, h, false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Subtle ambient glow
    juce::ColourGradient glowGrad(juce::Colour::fromRGB(15, 65, 115).withAlpha(0.35f), w * 0.5f, h * 0.30f,
                                 juce::Colours::transparentBlack, w * 0.5f, h * 0.85f, true);
    g.setGradientFill(glowGrad);
    g.fillEllipse(w * 0.15f, h * 0.05f, w * 0.70f, h * 0.85f);

    // 2. Base Arc Path
    float leftX = w * 0.06f;
    float rightX = w * 0.94f;
    float centerX = w * 0.5f;
    int numPoints = 80;

    juce::Path arcPath;
    arcPath.startNewSubPath(leftX, getArcY(leftX, w, h));

    for (int i = 1; i <= numPoints; ++i)
    {
        float x = leftX + (rightX - leftX) * (static_cast<float>(i) / static_cast<float>(numPoints));
        float y = getArcY(x, w, h);
        arcPath.lineTo(x, y);
    }

    // Outer subtle arc stroke
    g.setColour(juce::Colour::fromRGB(25, 75, 120).withAlpha(0.35f));
    g.strokePath(arcPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour::fromRGB(35, 105, 160));
    g.strokePath(arcPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Calibration Ticks and Cent Numbers (-100, -50, 0, 50, 100)
    struct CalibrationMark
    {
        float cents;
        const char* label;
    };

    const CalibrationMark marks[5] = {
        { -100.0f, "-100" },
        {  -50.0f,  "-50" },
        {    0.0f,    ""  },
        {   50.0f,   "50" },
        {  100.0f,  "100" }
    };

    g.setFont(juce::FontOptions(13.0f));

    for (int i = 0; i < 5; ++i)
    {
        float u = (marks[i].cents + 100.0f) / 200.0f;
        float x = leftX + u * (rightX - leftX);
        float y = getArcY(x, w, h);

        float tickLen = (i == 2) ? 14.0f : 8.0f;
        g.setColour(juce::Colour::fromRGB(45, 120, 170));
        g.drawLine(x, y, x, y + tickLen, 1.5f);

        if (marks[i].label[0] != '\0')
        {
            juce::Rectangle<float> labelBox(x - 28.0f, y + tickLen + 5.0f, 56.0f, 18.0f);
            g.setColour(juce::Colour::fromRGB(45, 120, 170));
            g.drawText(marks[i].label, labelBox, juce::Justification::centred, false);
        }
    }

    // 4. Center Note Readout Box
    float boxW = std::max(120.0f, w * 0.16f);
    float boxH = 44.0f;
    float boxX = centerX - boxW * 0.5f;
    float boxY = h * 0.56f;
    auto noteBox = juce::Rectangle<float>(boxX, boxY, boxW, boxH);

    g.setColour(juce::Colour::fromRGB(8, 17, 27));
    g.fillRoundedRectangle(noteBox, 8.0f);

    g.setColour(juce::Colour::fromRGB(24, 48, 72));
    g.drawRoundedRectangle(noteBox, 8.0f, 1.2f);

    juce::String noteStr = NoteNames[static_cast<size_t>(juce::jlimit(0, 11, currentNote))];
    juce::String fullNoteText = noteStr + " " + juce::String(currentOctave);

    g.setFont(juce::FontOptions(25.0f).withStyle("Bold"));
    g.setColour(juce::Colour::fromRGB(170, 220, 255));
    g.drawText(fullNoteText, noteBox, juce::Justification::centred, false);

    // 5. Dynamic Deviation Bar (Increases or decreases along the arc from center according to note deviation)
    if (isVoiced && std::abs(currentCentDeviation) > 0.5f)
    {
        float clampedCents = juce::jlimit(-100.0f, 100.0f, currentCentDeviation);
        float targetNorm = (clampedCents + 100.0f) / 200.0f;
        float targetX = leftX + targetNorm * (rightX - leftX);

        float barStartX = std::min(centerX, targetX);
        float barEndX   = std::max(centerX, targetX);

        juce::Path devBar;
        devBar.startNewSubPath(barStartX, getArcY(barStartX, w, h));

        int barSteps = std::max(10, static_cast<int>(std::abs(barEndX - barStartX) / 4.0f));
        for (int s = 1; s <= barSteps; ++s)
        {
            float curX = barStartX + (barEndX - barStartX) * (static_cast<float>(s) / static_cast<float>(barSteps));
            devBar.lineTo(curX, getArcY(curX, w, h));
        }

        // Glowing fill bar
        juce::Colour barCol = juce::Colour::fromRGB(20, 175, 255);
        g.setColour(barCol.withAlpha(0.35f));
        g.strokePath(devBar, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(barCol);
        g.strokePath(devBar, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 6. Fixed Center Needle (Fixed at Center Apex)
    float centerArcY = getArcY(centerX, w, h);
    float needleTopY = centerArcY - 14.0f;
    float needleBottomY = boxY - 3.0f;

    juce::Colour needleColor = isVoiced ? juce::Colour::fromRGB(50, 195, 255) : juce::Colour::fromRGB(35, 120, 175).withAlpha(0.6f);

    // Needle glow
    g.setColour(needleColor.withAlpha(0.35f));
    g.drawLine(centerX, needleTopY, centerX, needleBottomY, 6.0f);

    // Needle core
    g.setColour(needleColor);
    g.drawLine(centerX, needleTopY, centerX, needleBottomY, 3.2f);

    // Horizontal target marker on center needle (as in design reference)
    float markerW = 18.0f;
    float markerY = centerArcY;
    g.drawLine(centerX - markerW, markerY, centerX, markerY, 3.0f);
}

} // namespace VocalTune
