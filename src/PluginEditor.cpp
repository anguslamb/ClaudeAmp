#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ClaudeAmpProcessorEditor::ClaudeAmpProcessorEditor (ClaudeAmpProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Set editor size for 6 knobs + controls (960x300 - Marshall style)
    setSize (960, 300);

    // Marshall color scheme
    auto marshallGold = juce::Colour (0xffd4af37);  // Classic Marshall gold
    auto marshallWhite = juce::Colour (0xfff5f5dc); // Warm white

    // Configure channel selector
    channelSelector.addItem ("NORMAL", 1);
    channelSelector.addItem ("BRIGHT", 2);
    channelSelector.setSelectedId (1);
    channelSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colours::black);
    channelSelector.setColour (juce::ComboBox::textColourId, marshallGold);
    channelSelector.setColour (juce::ComboBox::outlineColourId, marshallGold);
    channelSelector.setColour (juce::ComboBox::arrowColourId, marshallGold);
    addAndMakeVisible (channelSelector);
    channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, "channel", channelSelector);

    channelLabel.setText ("CHANNEL", juce::dontSendNotification);
    channelLabel.setJustificationType (juce::Justification::centred);
    channelLabel.setColour (juce::Label::textColourId, marshallGold);
    channelLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (channelLabel);

    // Configure link button
    linkButton.setButtonText ("LINK");
    linkButton.setColour (juce::ToggleButton::textColourId, marshallGold);
    linkButton.setColour (juce::ToggleButton::tickColourId, marshallGold);
    linkButton.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
    addAndMakeVisible (linkButton);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, "link", linkButton);

    // Helper lambda for configuring knobs with Marshall styling
    auto configureKnob = [marshallGold](juce::Slider& slider) {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        slider.setColour (juce::Slider::rotarySliderFillColourId, marshallGold);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
        slider.setColour (juce::Slider::thumbColourId, marshallGold);
        slider.setColour (juce::Slider::textBoxTextColourId, marshallGold);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    };

    auto configureLabel = [marshallGold](juce::Label& label, const juce::String& text) {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, marshallGold);
        label.setFont (juce::Font (16.0f, juce::Font::bold));
    };

    // Configure drive slider
    configureKnob (driveSlider);
    addAndMakeVisible (driveSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "drive", driveSlider);

    configureLabel (driveLabel, "DRIVE");
    driveLabel.attachToComponent (&driveSlider, false);
    addAndMakeVisible (driveLabel);

    // Configure bass slider
    configureKnob (bassSlider);
    addAndMakeVisible (bassSlider);
    bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "bass", bassSlider);
    configureLabel (bassLabel, "BASS");
    bassLabel.attachToComponent (&bassSlider, false);
    addAndMakeVisible (bassLabel);

    // Configure mid slider
    configureKnob (midSlider);
    addAndMakeVisible (midSlider);
    midAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "mid", midSlider);
    configureLabel (midLabel, "MID");
    midLabel.attachToComponent (&midSlider, false);
    addAndMakeVisible (midLabel);

    // Configure treble slider
    configureKnob (trebleSlider);
    addAndMakeVisible (trebleSlider);
    trebleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "treble", trebleSlider);
    configureLabel (trebleLabel, "TREBLE");
    trebleLabel.attachToComponent (&trebleSlider, false);
    addAndMakeVisible (trebleLabel);

    // Configure presence slider
    configureKnob (presenceSlider);
    addAndMakeVisible (presenceSlider);
    presenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "presence", presenceSlider);
    configureLabel (presenceLabel, "PRESENCE");
    presenceLabel.attachToComponent (&presenceSlider, false);
    addAndMakeVisible (presenceLabel);

    // Configure master slider
    configureKnob (masterSlider);
    addAndMakeVisible (masterSlider);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "master", masterSlider);
    configureLabel (masterLabel, "MASTER");
    masterLabel.attachToComponent (&masterSlider, false);
    addAndMakeVisible (masterLabel);
}

ClaudeAmpProcessorEditor::~ClaudeAmpProcessorEditor()
{
}

//==============================================================================
void ClaudeAmpProcessorEditor::paint (juce::Graphics& g)
{
    // Marshall-style black background
    g.fillAll (juce::Colour (0xff1a1a1a));

    // Gold accent colors
    auto marshallGold = juce::Colour (0xffd4af37);
    auto marshallGoldDark = juce::Colour (0xffb8941c);

    // Draw simulated panel texture
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (0, 50, getWidth(), getHeight() - 50);

    // Title - Marshall script style
    g.setColour (marshallGold);
    g.setFont (juce::Font (36.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("PLEXI", getLocalBounds().removeFromTop (50),
                juce::Justification::centred, true);

    // Subtitle
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("SUPER LEAD", 10, 8, getWidth() - 20, 20,
                juce::Justification::centredTop, true);

    // Draw decorative panel lines (Marshall style)
    g.setColour (marshallGoldDark);
    g.drawLine (20, 60, getWidth() - 20, 60, 2.0f);
    g.drawLine (20, getHeight() - 20, getWidth() - 20, getHeight() - 20, 2.0f);
}

void ClaudeAmpProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Title area
    auto titleArea = area.removeFromTop (70);

    // Top controls area (Channel and Link)
    auto topControlsArea = titleArea.removeFromBottom (30).reduced (30, 0);

    // Channel selector on left
    auto channelArea = topControlsArea.removeFromLeft (200);
    channelLabel.setBounds (channelArea.removeFromLeft (80));
    channelSelector.setBounds (channelArea.withTrimmedLeft (10));

    // Link button on right
    auto linkArea = topControlsArea.removeFromRight (150);
    linkButton.setBounds (linkArea);

    // Knobs area
    auto knobsArea = area.reduced (20);

    auto knobWidth = 130;
    auto knobHeight = 160;

    // Calculate spacing for 6 knobs
    auto totalKnobWidth = knobWidth * 6;
    auto spacing = (knobsArea.getWidth() - totalKnobWidth) / 7;

    // Position knobs from left to right
    auto x = spacing;
    auto y = knobsArea.getY() + 20;

    // Drive knob
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

    // Presence knob
    presenceSlider.setBounds (x, y, knobWidth, knobHeight);
    x += knobWidth + spacing;

    // Master knob (rightmost)
    masterSlider.setBounds (x, y, knobWidth, knobHeight);
}
