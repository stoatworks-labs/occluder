#pragma once

#include <JuceHeader.h>

#include "DSP/Engine.h"
#include "PluginParameters.h"

class OccluderAudioProcessor : public juce::AudioProcessor
{
public:
    OccluderAudioProcessor();
    ~OccluderAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** The knob as the host has it, 0..1. What the editor draws. */
    double getAmount() const { return amountParam->load() / 100.0; }

    /** For the DSP checks: blocks are currently going through untouched. */
    bool isPassingThrough() const { return engine.isPassingThrough(); }

    /** The rate the audio is running at, for drawing the curve at the same
        one. 48 kHz before the host has said. */
    double getCurrentSampleRate() const
    {
        const auto sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

private:
    occluder::Engine engine;
    std::atomic<float>* amountParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OccluderAudioProcessor)
};
