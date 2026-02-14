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
    using PlexiChain = juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,                    // 0: Input level (controlled by Drive)
        juce::dsp::Bias<float>,                    // 1: Asymmetric bias stage 1
        juce::dsp::WaveShaper<float>,              // 2: Preamp stage 1 (12AX7)
        juce::dsp::StateVariableTPTFilter<float>,  // 3: Coupling HPF 1
        juce::dsp::Bias<float>,                    // 4: Asymmetric bias stage 2
        juce::dsp::WaveShaper<float>,              // 5: Preamp stage 2 (12AX7)
        juce::dsp::StateVariableTPTFilter<float>,  // 6: Coupling HPF 2
        juce::dsp::Bias<float>,                    // 7: Asymmetric bias stage 3
        juce::dsp::WaveShaper<float>,              // 8: Preamp stage 3 (12AX7)
        juce::dsp::StateVariableTPTFilter<float>,  // 9: Tone stack bass
        juce::dsp::StateVariableTPTFilter<float>,  // 10: Tone stack mid
        juce::dsp::StateVariableTPTFilter<float>,  // 11: Tone stack treble
        juce::dsp::WaveShaper<float>,              // 12: Power amp (EL34)
        juce::dsp::FirstOrderTPTFilter<float>,     // 13: DC blocker
        juce::dsp::Gain<float>                     // 14: Master volume
    >;

    PlexiChain plexiChain;

    // 4x oversampling for anti-aliasing
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Parameter smoothing (prevents audio clicks)
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> bassSmoothed;
    juce::SmoothedValue<float> midSmoothed;
    juce::SmoothedValue<float> trebleSmoothed;
    juce::SmoothedValue<float> masterSmoothed;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClaudeAmpProcessor)
};
