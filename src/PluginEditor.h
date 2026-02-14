#pragma once

#include "PluginProcessor.h"

//==============================================================================
class ClaudeAmpProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ClaudeAmpProcessorEditor (ClaudeAmpProcessor&);
    ~ClaudeAmpProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Reference to the processor
    ClaudeAmpProcessor& processorRef;

    // Channel selector
    juce::ComboBox channelSelector;
    juce::Label channelLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;

    // Link button
    juce::ToggleButton linkButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;

    // Rotary sliders (knobs) - Marshall Plexi style
    juce::Slider driveSlider;
    juce::Slider bassSlider;
    juce::Slider midSlider;
    juce::Slider trebleSlider;
    juce::Slider presenceSlider;
    juce::Slider masterSlider;

    // Labels
    juce::Label driveLabel;
    juce::Label bassLabel;
    juce::Label midLabel;
    juce::Label trebleLabel;
    juce::Label presenceLabel;
    juce::Label masterLabel;

    // APVTS attachments (automatic parameter sync)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trebleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> presenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClaudeAmpProcessorEditor)
};
