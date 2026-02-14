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
    initializeFactoryPresets();
}

ClaudeAmpProcessor::~ClaudeAmpProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ClaudeAmpProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Channel Selection (Normal/Bright like real Plexi)
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "channel",
        "Channel",
        juce::StringArray ("Normal", "Bright"),
        0));  // 0 = Normal, 1 = Bright

    // Channel Linking (jumper cable simulation)
    layout.add (std::make_unique<juce::AudioParameterBool> (
        "link",
        "Link Channels",
        false));  // Default: not linked

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

    // Presence (0-10) - high-frequency clarity
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "presence",
        "Presence",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

    // Master Volume (0-10)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "master",
        "Master",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        5.0f));

    // Cabinet Simulation (Marshall 4x12)
    layout.add (std::make_unique<juce::AudioParameterBool> (
        "cabinet",
        "Cabinet",
        true));  // Default: enabled

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
    return static_cast<int> (factoryPresets.size());
}

int ClaudeAmpProcessor::getCurrentProgram()
{
    return currentPreset;
}

void ClaudeAmpProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < static_cast<int> (factoryPresets.size()))
    {
        currentPreset = index;
        loadPreset (index);
    }
}

const juce::String ClaudeAmpProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int> (factoryPresets.size()))
        return factoryPresets[index].name;
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

    // Initialize look-up tables for tube saturation curves
    // 12AX7 preamp tube: Asymmetric (harder positive clipping)
    tube12AX7LUT.initialise ([](float x) {
        if (x > 0.0f)
            return std::tanh (x * 1.8f);      // Harder positive clipping
        else
            return std::tanh (x * 1.2f);      // Softer negative
    }, -5.0f, 5.0f, 256);  // 256 points for accuracy

    // EL34 power amp tube: More symmetrical, softer clipping
    tubeEL34LUT.initialise ([](float x) {
        return std::tanh (x * 0.8f);          // Softer, more symmetrical
    }, -5.0f, 5.0f, 128);  // 128 points sufficient for symmetric curve

    // Prepare the entire Plexi chain with oversampled spec
    auto oversampledSpec = spec;
    oversampledSpec.sampleRate = sampleRate * 4.0;
    oversampledSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock * 4);
    plexiChain.prepare (oversampledSpec);

    // Configure Stage 0: Input gain (controlled by Drive parameter)
    auto& inputGain = plexiChain.get<0>();
    inputGain.setGainDecibels (0.0f);

    // Configure Stage 1: Channel brightness filter (configured in processBlock)
    // Normal: 10 Hz HPF (full-range), Bright: 500 Hz HPF (emphasizes highs)
    auto& channelFilter = plexiChain.get<1>();
    channelFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    // Configure Stage 2: Pre-emphasis (+6 dB @ 5 kHz before saturation)
    auto& preEmph = plexiChain.get<2>();
    *preEmph.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate * 4.0, 5000.0f, 0.707f, 1.995f);  // +6 dB

    // Configure Stage 3: Preamp stage 1 bias (asymmetric for 12AX7)
    auto& bias1 = plexiChain.get<3>();
    bias1.setBias (0.3f);

    // Configure Stage 4: Preamp stage 1 waveshaper (Enhanced 12AX7 model)
    auto& stage1 = plexiChain.get<4>();
    stage1.functionToUse = [](float x) {
        // More accurate 12AX7 triode curve with grid conduction
        if (x > 2.0f)
            return 1.0f;  // Hard clipping (grid conduction)
        else if (x > 0.0f)
            return x / (1.0f + std::pow (x * 0.8f, 3.0f));  // Asymmetric soft-knee
        else if (x > -2.0f)
            return x / (1.0f + std::pow (-x * 0.6f, 2.5f));  // Softer negative
        else
            return -0.9f;  // Grid cutoff
    };

    // Configure Stage 5: Coupling capacitor HPF (~20 Hz)
    auto& hpf1 = plexiChain.get<5>();
    hpf1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf1.setCutoffFrequency (20.0f);
    hpf1.setResonance (0.707f);

    // Configure Stage 6: Preamp stage 2 bias
    auto& bias2 = plexiChain.get<6>();
    bias2.setBias (0.35f);

    // Configure Stage 7: Preamp stage 2 waveshaper (Enhanced 12AX7)
    auto& stage2 = plexiChain.get<7>();
    stage2.functionToUse = [](float x) {
        // Same enhanced 12AX7 model
        if (x > 2.0f)
            return 1.0f;
        else if (x > 0.0f)
            return x / (1.0f + std::pow (x * 0.8f, 3.0f));
        else if (x > -2.0f)
            return x / (1.0f + std::pow (-x * 0.6f, 2.5f));
        else
            return -0.9f;
    };

    // Configure Stage 8: Coupling HPF 2
    auto& hpf2 = plexiChain.get<8>();
    hpf2.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf2.setCutoffFrequency (20.0f);
    hpf2.setResonance (0.707f);

    // Configure Stage 9: Preamp stage 3 bias
    auto& bias3 = plexiChain.get<9>();
    bias3.setBias (0.4f);  // Heaviest saturation stage

    // Configure Stage 10: Preamp stage 3 waveshaper (Enhanced 12AX7)
    auto& stage3 = plexiChain.get<10>();
    stage3.functionToUse = [](float x) {
        // Same enhanced 12AX7 model
        if (x > 2.0f)
            return 1.0f;
        else if (x > 0.0f)
            return x / (1.0f + std::pow (x * 0.8f, 3.0f));
        else if (x > -2.0f)
            return x / (1.0f + std::pow (-x * 0.6f, 2.5f));
        else
            return -0.9f;
    };

    // Configure Stage 11: De-emphasis (-6 dB @ 5 kHz after saturation)
    auto& deEmph = plexiChain.get<11>();
    *deEmph.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate * 4.0, 5000.0f, 0.707f, 0.501f);  // -6 dB

    // Configure Stage 12-14: Tone stack (configured in processBlock with proper coefficients)
    // These use IIR shelf/peak filters for proper EQ behavior

    // Configure Stage 15: Power amp (Enhanced EL34 push-pull model)
    auto& powerAmp = plexiChain.get<15>();
    powerAmp.functionToUse = [](float x) {
        // EL34 power tube: More symmetrical, softer knee than 12AX7
        // Push-pull operation cancels even harmonics
        if (x > 3.0f)
            return 0.95f;  // Soft limiting
        else if (x < -3.0f)
            return -0.95f;
        else
            return x / (1.0f + std::pow (std::abs (x) * 0.5f, 2.2f)) * std::copysign (1.0f, x);
    };

    // Configure Stage 16: Presence (configured in processBlock)
    // High-shelf boost for clarity and "air"

    // Configure Stage 17: DC blocker
    auto& dcBlocker = plexiChain.get<17>();
    dcBlocker.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency (5.0f);

    // Stage 18: Master volume (configured in processBlock)

    // Initialize parameter smoothing (5ms ramp time for responsive feel)
    driveSmoothed.reset (sampleRate, 0.005);
    bassSmoothed.reset (sampleRate, 0.005);
    midSmoothed.reset (sampleRate, 0.005);
    trebleSmoothed.reset (sampleRate, 0.005);
    presenceSmoothed.reset (sampleRate, 0.005);
    masterSmoothed.reset (sampleRate, 0.005);

    // Set initial target values to current parameter values
    driveSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("drive")->load());
    bassSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("bass")->load());
    midSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("mid")->load());
    trebleSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("treble")->load());
    presenceSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("presence")->load());
    masterSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("master")->load());

    // Initialize cabinet IR convolution
    cabinetIR.prepare (spec);

    // Create a simple impulse response for Marshall 4x12 cabinet simulation
    // TODO: Replace with actual measured IR from real Marshall cabinet
    // This is a basic synthetic IR approximating speaker resonance and room
    const int irLength = 2048;  // ~40ms @ 48kHz
    juce::AudioBuffer<float> ir (static_cast<int> (spec.numChannels), irLength);

    for (int channel = 0; channel < ir.getNumChannels(); ++channel)
    {
        auto* irData = ir.getWritePointer (channel);

        // Initial impulse with speaker resonance (~80 Hz)
        for (int i = 0; i < irLength; ++i)
        {
            float t = static_cast<float> (i) / static_cast<float> (sampleRate);

            // Direct impulse
            float direct = (i == 0) ? 0.8f : 0.0f;

            // Speaker resonance (damped sine wave at 80 Hz)
            float resonance = std::sin (2.0f * juce::MathConstants<float>::pi * 80.0f * t)
                            * std::exp (-t * 50.0f) * 0.3f;

            // High-frequency rolloff (speaker cone breakup ~4 kHz)
            float envelope = std::exp (-t * 8.0f);

            // Early reflections (simulated cabinet and room)
            float reflection1 = (i == 64) ? 0.15f : 0.0f;   // ~1.3ms
            float reflection2 = (i == 128) ? 0.08f : 0.0f;  // ~2.7ms
            float reflection3 = (i == 256) ? 0.04f : 0.0f;  // ~5.3ms

            irData[i] = (direct + resonance + reflection1 + reflection2 + reflection3) * envelope;
        }
    }

    // Load the IR into the convolution engine
    cabinetIR.loadImpulseResponse (std::move (ir),
                                    sampleRate,
                                    juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::no,
                                    juce::dsp::Convolution::Normalise::yes);

    // Report latency to DAW (from oversampling + convolution)
    auto latencySamples = oversampler->getLatencyInSamples() + cabinetIR.getLatency();
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
    auto channel = static_cast<int> (apvts.getRawParameterValue ("channel")->load());
    auto link = apvts.getRawParameterValue ("link")->load() > 0.5f;
    auto drive = apvts.getRawParameterValue ("drive")->load();
    auto bass = apvts.getRawParameterValue ("bass")->load();
    auto mid = apvts.getRawParameterValue ("mid")->load();
    auto treble = apvts.getRawParameterValue ("treble")->load();
    auto presence = apvts.getRawParameterValue ("presence")->load();
    auto master = apvts.getRawParameterValue ("master")->load();

    // Update smoothed target values
    driveSmoothed.setTargetValue (drive);
    bassSmoothed.setTargetValue (bass);
    midSmoothed.setTargetValue (mid);
    trebleSmoothed.setTargetValue (treble);
    presenceSmoothed.setTargetValue (presence);
    masterSmoothed.setTargetValue (master);

    // Advance smoothing by the number of samples in this block
    auto numSamples = buffer.getNumSamples();
    driveSmoothed.skip (numSamples);
    bassSmoothed.skip (numSamples);
    midSmoothed.skip (numSamples);
    trebleSmoothed.skip (numSamples);
    presenceSmoothed.skip (numSamples);
    masterSmoothed.skip (numSamples);

    // Get current smoothed values (after advancing)
    auto currentDrive = driveSmoothed.getCurrentValue();
    auto currentBass = bassSmoothed.getCurrentValue();
    auto currentMid = midSmoothed.getCurrentValue();
    auto currentTreble = trebleSmoothed.getCurrentValue();
    auto currentPresence = presenceSmoothed.getCurrentValue();
    auto currentMaster = masterSmoothed.getCurrentValue();

    // Calculate power supply sag (dynamic compression)
    float sagAmount = calculatePowerSupplySag (buffer);

    // Update input gain based on Drive with power supply sag compensation
    auto& inputGain = plexiChain.get<0>();
    auto driveGain = currentDrive * 2.0f - (sagAmount * 3.0f);  // Sag reduces drive
    inputGain.setGainDecibels (driveGain);

    // Update channel brightness filter (Normal = full-range, Bright = cuts bass, Linked = blend)
    auto& channelFilter = plexiChain.get<1>();
    if (link)  // Channel linking enabled - blend both channels
    {
        // Use intermediate cutoff to approximate Normal + Bright blend
        channelFilter.setCutoffFrequency (200.0f);  // Blend: some bass cut, fuller than Bright alone
    }
    else if (channel == 0)  // Normal channel
    {
        channelFilter.setCutoffFrequency (10.0f);  // Essentially full-range
    }
    else  // Bright channel
    {
        channelFilter.setCutoffFrequency (500.0f);  // Cuts bass, emphasizes highs
    }

    // Interactive tone stack (Marshall passive network style)
    auto oversampledRate = getSampleRate() * 4.0;

    // Marshall-style interactive controls where each affects the others
    // Bass control: Low-shelf with interaction from treble
    auto& bassTone = plexiChain.get<12>();
    auto bassFreq = 200.0f - (currentTreble * 15.0f);  // Treble reduces bass frequency
    auto bassGainDB = (currentBass - 5.0f) * 2.4f;
    auto bassQ = 0.707f + (currentMid * 0.05f);  // Mid affects bass Q
    auto bassGain = juce::Decibels::decibelsToGain (bassGainDB);
    *bassTone.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        oversampledRate, bassFreq, bassQ, bassGain);

    // Mid control: Scooping behavior (like real Plexi)
    // When turned down, scoops mids. When turned up, boosts mids.
    auto& midTone = plexiChain.get<13>();
    auto midFreq = 650.0f + (currentBass * 50.0f) + (currentTreble * 30.0f);  // Affected by bass and treble
    auto midGainDB = (currentMid - 5.0f) * 2.0f;  // ±10 dB (less than bass/treble)
    auto midQ = 1.2f - (currentBass * 0.05f) - (currentTreble * 0.05f);  // Narrower Q when bass/treble high
    auto midGain = juce::Decibels::decibelsToGain (midGainDB);
    *midTone.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        oversampledRate, midFreq, midQ, midGain);

    // Treble control: High-shelf with interaction from bass
    auto& trebleTone = plexiChain.get<14>();
    auto trebleFreq = 3000.0f + (currentBass * 100.0f);  // Bass pushes treble frequency up
    auto trebleGainDB = (currentTreble - 5.0f) * 2.4f;
    auto trebleQ = 0.707f + (currentMid * 0.05f);  // Mid affects treble Q
    auto trebleGain = juce::Decibels::decibelsToGain (trebleGainDB);
    *trebleTone.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        oversampledRate, trebleFreq, trebleQ, trebleGain);

    // Presence control: High-shelf @ 3 kHz (0-10 → 0 to +10 dB)
    auto& presenceFilter = plexiChain.get<16>();
    auto presenceGainDB = currentPresence * 1.0f;  // 0→0dB, 5→+5dB, 10→+10dB
    auto presenceGain = juce::Decibels::decibelsToGain (presenceGainDB);
    *presenceFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        oversampledRate, 3000.0f, 0.707f, presenceGain);

    // Update master volume (0-10 → -40 to +6 dB for more usable range)
    auto& masterGain = plexiChain.get<18>();
    auto masterDB = -40.0f + (currentMaster * 4.6f);  // 0→-40dB, 5→-17dB, 10→+6dB
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

    // Apply cabinet IR convolution if enabled
    auto cabinetEnabled = apvts.getRawParameterValue ("cabinet")->load() > 0.5f;
    if (cabinetEnabled)
    {
        juce::dsp::AudioBlock<float> cabinetBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> cabinetContext (cabinetBlock);
        cabinetIR.process (cabinetContext);
    }
}

//==============================================================================
// Power Supply Sag Modeling
float ClaudeAmpProcessor::calculatePowerSupplySag (const juce::AudioBuffer<float>& buffer)
{
    // Calculate RMS level of signal (envelope following)
    float rms = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            rms += data[sample] * data[sample];
        }
    }
    rms = std::sqrt (rms / (buffer.getNumSamples() * buffer.getNumChannels()));

    // Envelope follower with attack/release
    const float attackCoeff = 0.999f;   // Slow attack (power supply droop)
    const float releaseCoeff = 0.9995f; // Very slow release (capacitor discharge)

    if (rms > sagEnvelope)
        sagEnvelope = attackCoeff * sagEnvelope + (1.0f - attackCoeff) * rms;
    else
        sagEnvelope = releaseCoeff * sagEnvelope + (1.0f - releaseCoeff) * rms;

    // Convert envelope to sag amount (0-1 range)
    // More signal = more sag (power supply compression)
    float sagAmount = std::tanh (sagEnvelope * 8.0f) * 0.4f;  // Max 40% sag

    return sagAmount;
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
// Preset Management

void ClaudeAmpProcessor::initializeFactoryPresets()
{
    // Preset 0: Clean
    factoryPresets.push_back ({
        "Clean",
        0,      // Normal channel
        false,  // Not linked
        2.5f,   // Drive
        5.0f,   // Bass
        5.0f,   // Mid
        6.0f,   // Treble
        4.0f,   // Presence
        6.0f    // Master
    });

    // Preset 1: Crunch
    factoryPresets.push_back ({
        "Crunch",
        1,      // Bright channel
        false,  // Not linked
        6.0f,   // Drive
        4.0f,   // Bass
        6.0f,   // Mid
        7.0f,   // Treble
        5.0f,   // Presence
        5.0f    // Master
    });

    // Preset 2: Lead
    factoryPresets.push_back ({
        "Lead",
        0,      // Normal channel
        false,  // Not linked
        8.5f,   // Drive
        4.0f,   // Bass
        7.0f,   // Mid
        8.0f,   // Treble
        6.0f,   // Presence
        4.5f    // Master
    });

    // Preset 3: Plexi Stack (Linked channels)
    factoryPresets.push_back ({
        "Plexi Stack",
        0,      // Channel (ignored when linked)
        true,   // Linked
        7.5f,   // Drive
        5.0f,   // Bass
        6.5f,   // Mid
        7.5f,   // Treble
        5.5f,   // Presence
        5.0f    // Master
    });
}

void ClaudeAmpProcessor::loadPreset (int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= static_cast<int> (factoryPresets.size()))
        return;

    const auto& preset = factoryPresets[presetIndex];

    // Update all parameters
    apvts.getParameter ("channel")->setValueNotifyingHost (static_cast<float> (preset.channel));
    apvts.getParameter ("link")->setValueNotifyingHost (preset.link ? 1.0f : 0.0f);
    apvts.getParameter ("drive")->setValueNotifyingHost (preset.drive / 10.0f);
    apvts.getParameter ("bass")->setValueNotifyingHost (preset.bass / 10.0f);
    apvts.getParameter ("mid")->setValueNotifyingHost (preset.mid / 10.0f);
    apvts.getParameter ("treble")->setValueNotifyingHost (preset.treble / 10.0f);
    apvts.getParameter ("presence")->setValueNotifyingHost (preset.presence / 10.0f);
    apvts.getParameter ("master")->setValueNotifyingHost (preset.master / 10.0f);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClaudeAmpProcessor();
}
