#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ScaleDefinition.h"

namespace VocalTune
{

namespace ParamIDs
{
    inline const juce::ParameterID correction   { "correction",   1 };
    inline const juce::ParameterID vibrato      { "vibrato",      1 };
    inline const juce::ParameterID formantShift { "formantShift", 1 };
    inline const juce::ParameterID rootNote     { "rootNote",     1 };
    inline const juce::ParameterID scaleType    { "scaleType",    1 };
    inline const juce::ParameterID tuningRef    { "tuningRef",    1 };
    inline const juce::ParameterID lowLatency   { "lowLatency",   1 };
    inline const juce::ParameterID followScale  { "followScale",  1 };
    inline const juce::ParameterID customMask   { "customMask",   1 };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Correction Speed / Amount: 0 % to 100 %, default 80 %
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::correction,
        "Correction",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f),
        80.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")
    ));

    // 2. Vibrato: 0 % to 100 %, default 0 %
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::vibrato,
        "Vibrato",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")
    ));

    // 3. Formant Shift: -12.0 st to +12.0 st, default 0.0 st
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::formantShift,
        "Formant Shift",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")
    ));

    // 4. Root Note: 0 to 11 (C to B), default 5 (F)
    juce::StringArray noteChoices;
    for (int i = 0; i < 12; ++i)
        noteChoices.add(NoteNames[static_cast<size_t>(i)]);

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::rootNote,
        "Root Note",
        noteChoices,
        5 // F by default
    ));

    // 5. Scale Type: Natural Minor by default (0)
    juce::StringArray scaleChoices;
    for (size_t i = 0; i < static_cast<size_t>(ScaleType::NumScaleTypes); ++i)
        scaleChoices.add(ScaleNames[i]);

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::scaleType,
        "Scale",
        scaleChoices,
        0 // Natural Minor
    ));

    // 6. Tuning Reference: 415 to 465 Hz, default 440 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::tuningRef,
        "Tuning",
        juce::NormalisableRange<float>(415.0f, 465.0f, 0.1f),
        440.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
    ));

    // 7. Low Latency mode: default true
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::lowLatency,
        "Low Latency",
        true
    ));

    // 8. Follow Scale: default true
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::followScale,
        "Follow",
        true
    ));

    // 9. Custom Note Mask: 12-bit mask as int (0 to 4095)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamIDs::customMask,
        "Custom Mask",
        0,
        4095,
        computeActiveNotesMask(5, ScaleType::NaturalMinor)
    ));

    return { params.begin(), params.end() };
}

} // namespace VocalTune

