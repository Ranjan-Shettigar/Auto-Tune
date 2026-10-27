#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace VocalTune
{

class InteractiveKeyboardComponent : public juce::Component
{
public:
    InteractiveKeyboardComponent();
    ~InteractiveKeyboardComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;

    void setActiveMask(uint16_t mask);
    uint16_t getActiveMask() const { return activeMask; }

    std::function<void(uint16_t)> onMaskChanged;

private:
    uint16_t activeMask = 0x0FFF;

    // Key geometry
    // 7 white keys: C(0), D(2), E(4), F(5), G(7), A(9), B(11)
    static constexpr int whiteNotes[7] = { 0, 2, 4, 5, 7, 9, 11 };
    
    // 5 black keys: C#(1), D#(3), F#(6), G#(8), A#(10)
    // Black key positions relative to white key indices:
    // C# between C(0) and D(1) -> pos 0.7
    // D# between D(1) and E(2) -> pos 1.7
    // F# between F(3) and G(4) -> pos 3.7
    // G# between G(4) and A(5) -> pos 4.7
    // A# between A(5) and B(6) -> pos 5.7
    struct BlackKeyDef
    {
        int noteIndex;
        float position; // relative white key index
    };

    static constexpr BlackKeyDef blackKeyDefs[5] = {
        { 1, 0.65f },
        { 3, 1.75f },
        { 6, 3.65f },
        { 8, 4.70f },
        { 10, 5.75f }
    };

    juce::Rectangle<float> getWhiteKeyRect(int whiteIndex) const;
    juce::Rectangle<float> getBlackKeyRect(int blackIndex) const;

    int getNoteAtPosition(juce::Point<float> pt) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractiveKeyboardComponent)
};

} // namespace VocalTune

