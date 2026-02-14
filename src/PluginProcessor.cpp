#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ClaudeAmpProcessor::ClaudeAmpProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

ClaudeAmpProcessor::~ClaudeAmpProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ClaudeAmpProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bass - Low Shelf @ 200 Hz
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "bass",
        "Bass",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f,
        "dB"));

    // Mid - Peaking @ 1 kHz, Q=0.7
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "mid",
        "Mid",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f,
        "dB"));

    // Treble - High Shelf @ 5 kHz
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "treble",
        "Treble",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f,
        "dB"));

    // Master Volume
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "master",
        "Master",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f),
        0.0f,
        "dB"));

    return layout;
}

//==============================================================================
const juce::String ClaudeAmpProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ClaudeAmpProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ClaudeAmpProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ClaudeAmpProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ClaudeAmpProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ClaudeAmpProcessor::getNumPrograms()
{
    return 1;
}

int ClaudeAmpProcessor::getCurrentProgram()
{
    return 0;
}

void ClaudeAmpProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String ClaudeAmpProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void ClaudeAmpProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void ClaudeAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize DSP spec
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    // Prepare filters
    bassFilter.prepare (spec);
    midFilter.prepare (spec);
    trebleFilter.prepare (spec);

    // Reset filters
    bassFilter.reset();
    midFilter.reset();
    trebleFilter.reset();

    // Reset smoothing (20ms ramp time)
    bassSmoothed.reset (sampleRate, 0.02);
    midSmoothed.reset (sampleRate, 0.02);
    trebleSmoothed.reset (sampleRate, 0.02);
    masterSmoothed.reset (sampleRate, 0.02);

    // Set initial target values to current parameter values
    bassSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("bass")->load());
    midSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("mid")->load());
    trebleSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("treble")->load());
    masterSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("master")->load());
}

void ClaudeAmpProcessor::releaseResources()
{
}

bool ClaudeAmpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void ClaudeAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get parameter values (thread-safe atomic read)
    auto bassDB = apvts.getRawParameterValue ("bass")->load();
    auto midDB = apvts.getRawParameterValue ("mid")->load();
    auto trebleDB = apvts.getRawParameterValue ("treble")->load();
    auto masterDB = apvts.getRawParameterValue ("master")->load();

    // Update smoothed target values
    bassSmoothed.setTargetValue (bassDB);
    midSmoothed.setTargetValue (midDB);
    trebleSmoothed.setTargetValue (trebleDB);
    masterSmoothed.setTargetValue (masterDB);

    // Get current smoothed values
    auto currentBass = bassSmoothed.getNextValue();
    auto currentMid = midSmoothed.getNextValue();
    auto currentTreble = trebleSmoothed.getNextValue();

    // Update filter coefficients
    auto sampleRate = getSampleRate();

    // Bass: Low-shelf @ 200 Hz, Q=0.707
    auto bassGain = juce::Decibels::decibelsToGain (currentBass);
    *bassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 200.0f, 0.707f, bassGain);

    // Mid: Peaking @ 1 kHz, Q=0.7
    auto midGain = juce::Decibels::decibelsToGain (currentMid);
    *midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 1000.0f, 0.7f, midGain);

    // Treble: High-shelf @ 5 kHz, Q=0.707
    auto trebleGain = juce::Decibels::decibelsToGain (currentTreble);
    *trebleFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 5000.0f, 0.707f, trebleGain);

    // Process audio through filter chain
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    bassFilter.process (context);
    midFilter.process (context);
    trebleFilter.process (context);

    // Apply master gain
    auto masterGain = juce::Decibels::decibelsToGain (masterSmoothed.getNextValue());
    buffer.applyGain (masterGain);
}

//==============================================================================
bool ClaudeAmpProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ClaudeAmpProcessor::createEditor()
{
    return new ClaudeAmpProcessorEditor (*this);
}

//==============================================================================
void ClaudeAmpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ClaudeAmpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClaudeAmpProcessor();
}
