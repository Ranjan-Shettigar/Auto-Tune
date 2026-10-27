#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/AutoTuneEngine.h"

namespace VocalTune
{

class PitchDisplayComponent : public juce::Component, public juce::Timer
{
public:
    PitchDisplayComponent(const AutoTuneEngine& engine);
    ~PitchDisplayComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void timerCallback() override;

private:
    const AutoTuneEngine& engine;

    float currentCentDeviation = 0.0f;
    int currentNote = 3; // D#
    int currentOctave = 3;
    bool isVoiced = false;

    float getArcY(float x, float width, float height) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDisplayComponent)
};

} // namespace VocalTune
