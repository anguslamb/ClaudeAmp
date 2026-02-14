#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class ClaudeAmpProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    ClaudeAmpProcessor();
    ~ClaudeAmpProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter management
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    // Create parameter layout
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Marshall Plexi amp modeling chain
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    using PlexiChain = juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,                    // 0: Input level (controlled by Drive)
        juce::dsp::StateVariableTPTFilter<float>,  // 1: Channel brightness (Normal=10Hz, Bright=500Hz HPF)
        Filter,                                     // 2: Pre-emphasis (+6dB @ 5kHz)
        juce::dsp::Bias<float>,                    // 3: Asymmetric bias stage 1
        juce::dsp::WaveShaper<float>,              // 4: Preamp stage 1 (12AX7)
        juce::dsp::StateVariableTPTFilter<float>,  // 5: Coupling HPF 1
        juce::dsp::Bias<float>,                    // 6: Asymmetric bias stage 2
        juce::dsp::WaveShaper<float>,              // 7: Preamp stage 2 (12AX7)
        juce::dsp::StateVariableTPTFilter<float>,  // 8: Coupling HPF 2
        juce::dsp::Bias<float>,                    // 9: Asymmetric bias stage 3
        juce::dsp::WaveShaper<float>,              // 10: Preamp stage 3 (12AX7)
        Filter,                                     // 11: De-emphasis (-6dB @ 5kHz)
        Filter,                                     // 12: Tone stack bass (low-shelf)
        Filter,                                     // 13: Tone stack mid (peaking)
        Filter,                                     // 14: Tone stack treble (high-shelf)
        juce::dsp::WaveShaper<float>,              // 15: Power amp (EL34)
        Filter,                                     // 16: Presence (high-shelf boost)
        juce::dsp::FirstOrderTPTFilter<float>,     // 17: DC blocker
        juce::dsp::Gain<float>                     // 18: Master volume
    >;

    PlexiChain plexiChain;

    // 4x oversampling for anti-aliasing
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Look-up tables for tube saturation (more authentic than tanh)
    juce::dsp::LookupTableTransform<float> tube12AX7LUT;  // Preamp tubes (asymmetric)
    juce::dsp::LookupTableTransform<float> tubeEL34LUT;   // Power amp tube (symmetric)

    // Cabinet IR convolution (Marshall 4x12)
    juce::dsp::Convolution cabinetIR;

    // Power supply sag modeling
    float sagEnvelope = 0.0f;  // Tracks signal level for sag
    float calculatePowerSupplySag (const juce::AudioBuffer<float>& buffer);

    // Parameter smoothing (prevents audio clicks)
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> bassSmoothed;
    juce::SmoothedValue<float> midSmoothed;
    juce::SmoothedValue<float> trebleSmoothed;
    juce::SmoothedValue<float> presenceSmoothed;
    juce::SmoothedValue<float> masterSmoothed;

    // Preset management
    int currentPreset = 0;
    void loadPreset (int presetIndex);
    void initializeFactoryPresets();

    struct PresetData
    {
        juce::String name;
        int channel;      // 0=Normal, 1=Bright
        bool link;
        float drive;
        float bass;
        float mid;
        float treble;
        float presence;
        float master;
    };
    std::vector<PresetData> factoryPresets;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClaudeAmpProcessor)
};
