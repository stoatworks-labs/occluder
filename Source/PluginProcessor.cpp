#include "PluginProcessor.h"
#include "PluginEditor.h"

OccluderAudioProcessor::OccluderAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", occluder::createParameterLayout())
{
    amountParam = apvts.getRawParameterValue(occluder::ParamIDs::amount);
}

void OccluderAudioProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate, juce::jmax(1, getTotalNumOutputChannels()));
    // No ramp on the first block after a prepare: the host is not playing yet,
    // and a saved session should come back sounding as it was saved.
    engine.setAmount(getAmount(), true);
}

bool OccluderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Any layout, as long as it goes out the way it came in. Every channel is
    // filtered identically, so a surround stem is as good as a mono voice.
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && in == out;
}

void OccluderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    engine.setAmount(getAmount());
    engine.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);
}

juce::AudioProcessorEditor* OccluderAudioProcessor::createEditor()
{
    return new OccluderAudioProcessorEditor(*this);
}

void OccluderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OccluderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OccluderAudioProcessor();
}
