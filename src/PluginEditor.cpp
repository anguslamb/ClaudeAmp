#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ClaudeAmpProcessorEditor::ClaudeAmpProcessorEditor (ClaudeAmpProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Set editor size (800x240 for 5 knobs)
    setSize (800, 240);

    // Configure drive slider
    driveSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    driveSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    driveSlider.setTextValueSuffix ("");
    addAndMakeVisible (driveSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "drive", driveSlider);

    driveLabel.setText ("DRIVE", juce::dontSendNotification);
    driveLabel.setJustificationType (juce::Justification::centred);
    driveLabel.attachToComponent (&driveSlider, false);
    addAndMakeVisible (driveLabel);

    // Configure bass slider
    bassSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    bassSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    bassSlider.setTextValueSuffix ("");
    addAndMakeVisible (bassSlider);
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "bass", bassSlider);

    bassLabel.setText ("BASS", juce::dontSendNotification);
    bassLabel.setJustificationType (juce::Justification::centred);
    bassLabel.attachToComponent (&bassSlider, false);
    addAndMakeVisible (bassLabel);

    // Configure mid slider
    midSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    midSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    midSlider.setTextValueSuffix ("");
    addAndMakeVisible (midSlider);
    midAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "mid", midSlider);

    midLabel.setText ("MID", juce::dontSendNotification);
    midLabel.setJustificationType (juce::Justification::centred);
    midLabel.attachToComponent (&midSlider, false);
    addAndMakeVisible (midLabel);

    // Configure treble slider
    trebleSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    trebleSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    trebleSlider.setTextValueSuffix ("");
    addAndMakeVisible (trebleSlider);
    trebleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "treble", trebleSlider);

    trebleLabel.setText ("TREBLE", juce::dontSendNotification);
    trebleLabel.setJustificationType (juce::Justification::centred);
    trebleLabel.attachToComponent (&trebleSlider, false);
    addAndMakeVisible (trebleLabel);

    // Configure master slider
    masterSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    masterSlider.setTextValueSuffix ("");
    addAndMakeVisible (masterSlider);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "master", masterSlider);

    masterLabel.setText ("MASTER", juce::dontSendNotification);
    masterLabel.setJustificationType (juce::Justification::centred);
    masterLabel.attachToComponent (&masterSlider, false);
    addAndMakeVisible (masterLabel);
}

ClaudeAmpProcessorEditor::~ClaudeAmpProcessorEditor()
{
}

//==============================================================================
void ClaudeAmpProcessorEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (juce::Colours::darkgrey);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText ("ClaudeAmp Plexi", getLocalBounds().removeFromTop (40),
                juce::Justification::centred, true);
}

void ClaudeAmpProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (40);  // Space for title

    auto knobWidth = 120;
    auto knobHeight = 140;

    // Calculate spacing for 5 knobs
    auto totalKnobWidth = knobWidth * 5;
    auto spacing = (area.getWidth() - totalKnobWidth) / 6;

    // Position knobs from left to right
    auto x = spacing;
    auto y = area.getY() + 20;

    // Drive knob (leftmost)
    driveSlider.setBounds (x, y, knobWidth, knobHeight);
    x += knobWidth + spacing;

    // Bass knob
    bassSlider.setBounds (x, y, knobWidth, knobHeight);
    x += knobWidth + spacing;

    // Mid knob
    midSlider.setBounds (x, y, knobWidth, knobHeight);
    x += knobWidth + spacing;

    // Treble knob
    trebleSlider.setBounds (x, y, knobWidth, knobHeight);
    x += knobWidth + spacing;

    // Master knob (rightmost)
    masterSlider.setBounds (x, y, knobWidth, knobHeight);
}
