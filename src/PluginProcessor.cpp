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

    // Preamp Drive (0-10 like real Marshall Plexi)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "drive",
        "Drive",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

    // Tone Stack - Bass (0-10)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "bass",
        "Bass",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

    // Tone Stack - Mid (0-10)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "mid",
        "Mid",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

    // Tone Stack - Treble (0-10)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "treble",
        "Treble",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

    // Master Volume (0-10)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "master",
        "Master",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

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

    // Initialize 4x oversampling (2^2 = 4x)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels,
        2,  // 2^2 = 4x oversampling
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,  // Max quality
        true   // Integer latency
    );
    oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));

    // Prepare the entire Plexi chain with oversampled spec
    auto oversampledSpec = spec;
    oversampledSpec.sampleRate = sampleRate * 4.0;
    oversampledSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock * 4);
    plexiChain.prepare (oversampledSpec);

    // Configure Stage 0: Input gain (controlled by Drive parameter)
    auto& inputGain = plexiChain.get<0>();
    inputGain.setGainDecibels (0.0f);

    // Configure Stage 1: Preamp stage 1 bias (asymmetric for 12AX7)
    auto& bias1 = plexiChain.get<1>();
    bias1.setBias (0.3f);

    // Configure Stage 2: Preamp stage 1 waveshaper (12AX7 tube)
    auto& stage1 = plexiChain.get<2>();
    stage1.functionToUse = [](float x) {
        return std::tanh (x * 1.5f);  // Harder positive clipping
    };

    // Configure Stage 3: Coupling capacitor HPF (~20 Hz)
    auto& hpf1 = plexiChain.get<3>();
    hpf1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf1.setCutoffFrequency (20.0f);
    hpf1.setResonance (0.707f);

    // Configure Stage 4: Preamp stage 2 bias
    auto& bias2 = plexiChain.get<4>();
    bias2.setBias (0.35f);

    // Configure Stage 5: Preamp stage 2 waveshaper (12AX7)
    auto& stage2 = plexiChain.get<5>();
    stage2.functionToUse = [](float x) {
        return std::tanh (x * 1.5f);
    };

    // Configure Stage 6: Coupling HPF 2
    auto& hpf2 = plexiChain.get<6>();
    hpf2.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf2.setCutoffFrequency (20.0f);
    hpf2.setResonance (0.707f);

    // Configure Stage 7: Preamp stage 3 bias
    auto& bias3 = plexiChain.get<7>();
    bias3.setBias (0.4f);  // Heaviest saturation stage

    // Configure Stage 8: Preamp stage 3 waveshaper (12AX7)
    auto& stage3 = plexiChain.get<8>();
    stage3.functionToUse = [](float x) {
        return std::tanh (x * 1.8f);  // Hardest clipping
    };

    // Configure Stage 9: Tone stack bass filter
    auto& bassTone = plexiChain.get<9>();
    bassTone.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    // Configure Stage 10: Tone stack mid filter
    auto& midTone = plexiChain.get<10>();
    midTone.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

    // Configure Stage 11: Tone stack treble filter
    auto& trebleTone = plexiChain.get<11>();
    trebleTone.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    // Configure Stage 12: Power amp (EL34 - softer, more symmetrical)
    auto& powerAmp = plexiChain.get<12>();
    powerAmp.functionToUse = [](float x) {
        return std::tanh (x * 0.7f);  // Softer clipping than preamp
    };

    // Configure Stage 13: DC blocker
    auto& dcBlocker = plexiChain.get<13>();
    dcBlocker.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency (5.0f);

    // Stage 14: Master volume (configured in processBlock)

    // Initialize parameter smoothing (20ms ramp time)
    driveSmoothed.reset (sampleRate, 0.02);
    bassSmoothed.reset (sampleRate, 0.02);
    midSmoothed.reset (sampleRate, 0.02);
    trebleSmoothed.reset (sampleRate, 0.02);
    masterSmoothed.reset (sampleRate, 0.02);

    // Set initial target values to current parameter values
    driveSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("drive")->load());
    bassSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("bass")->load());
    midSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("mid")->load());
    trebleSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("treble")->load());
    masterSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("master")->load());

    // Report latency to DAW (from oversampling)
    auto latencySamples = oversampler->getLatencyInSamples();
    setLatencySamples (static_cast<int> (latencySamples));
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
    auto drive = apvts.getRawParameterValue ("drive")->load();
    auto bass = apvts.getRawParameterValue ("bass")->load();
    auto mid = apvts.getRawParameterValue ("mid")->load();
    auto treble = apvts.getRawParameterValue ("treble")->load();
    auto master = apvts.getRawParameterValue ("master")->load();

    // Update smoothed target values
    driveSmoothed.setTargetValue (drive);
    bassSmoothed.setTargetValue (bass);
    midSmoothed.setTargetValue (mid);
    trebleSmoothed.setTargetValue (treble);
    masterSmoothed.setTargetValue (master);

    // Get current smoothed values
    auto currentDrive = driveSmoothed.getNextValue();
    auto currentBass = bassSmoothed.getNextValue();
    auto currentMid = midSmoothed.getNextValue();
    auto currentTreble = trebleSmoothed.getNextValue();
    auto currentMaster = masterSmoothed.getNextValue();

    // Update input gain based on Drive (0-10 → 0-30dB)
    auto& inputGain = plexiChain.get<0>();
    inputGain.setGainDecibels (currentDrive * 3.0f);

    // Update tone stack filters
    // Bass control (0-10 → cutoff 100-500 Hz)
    auto& bassTone = plexiChain.get<9>();
    auto bassFreq = 100.0f + (currentBass * 40.0f);  // 100-500 Hz
    auto bassQ = 0.707f + (currentBass * 0.3f);      // More resonance = more boost
    bassTone.setCutoffFrequency (bassFreq);
    bassTone.setResonance (bassQ);

    // Mid control (0-10 → center 500-2000 Hz)
    auto& midTone = plexiChain.get<10>();
    auto midFreq = 500.0f + (currentMid * 150.0f);   // 500-2000 Hz
    auto midQ = 0.5f + (currentMid * 0.5f);          // 0.5-1.0
    midTone.setCutoffFrequency (midFreq);
    midTone.setResonance (midQ);

    // Treble control (0-10 → cutoff 2000-8000 Hz)
    auto& trebleTone = plexiChain.get<11>();
    auto trebleFreq = 2000.0f + (currentTreble * 600.0f);  // 2000-8000 Hz
    auto trebleQ = 0.707f;
    trebleTone.setCutoffFrequency (trebleFreq);
    trebleTone.setResonance (trebleQ);

    // Update master volume (0-10 → -60 to +6 dB)
    auto& masterGain = plexiChain.get<14>();
    auto masterDB = -60.0f + (currentMaster * 6.6f);  // 0→-60dB, 10→+6dB
    masterGain.setGainDecibels (masterDB);

    // Process audio through chain with oversampling
    juce::dsp::AudioBlock<float> block (buffer);

    // Upsample to 4x
    auto oversampledBlock = oversampler->processSamplesUp (block);

    // Process through Plexi chain
    juce::dsp::ProcessContextReplacing<float> context (oversampledBlock);
    plexiChain.process (context);

    // Downsample back to original rate
    oversampler->processSamplesDown (block);
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
