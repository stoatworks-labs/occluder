/*
 * ocrender - runs an audio file through the shipped processor, offline.
 *
 *   ocrender <in.wav|aiff|flac> <out.wav> <amount 0..100>
 *
 * For listening without a host, for A/B sets, and for the demo. It is the
 * real OccluderAudioProcessor at the file's own sample rate and channel count,
 * with the knob set before the first block so nothing ramps.
 */

#include <JuceHeader.h>

#include <cstdio>

#include "Source/PluginProcessor.h"

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::fprintf(stderr, "usage: ocrender <in> <out.wav> <amount 0..100>\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const auto inFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
    const double amount = juce::String(argv[3]).getDoubleValue();

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(inFile));
    if (reader == nullptr)
    {
        std::fprintf(stderr, "cannot read %s\n", inFile.getFullPathName().toRawUTF8());
        return 1;
    }

    const int channels = (int) reader->numChannels;
    const double sampleRate = reader->sampleRate;
    const int block = 1024;

    OccluderAudioProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    const auto set = juce::AudioChannelSet::canonicalChannelSet(channels);
    layout.inputBuses.add(set);
    layout.outputBuses.add(set);
    if (! processor.setBusesLayout(layout))
    {
        std::fprintf(stderr, "%d-channel layout refused\n", channels);
        return 1;
    }
    processor.apvts.getParameter(occluder::ParamIDs::amount)->setValueNotifyingHost((float) (amount / 100.0));
    processor.setRateAndBufferSizeDetails(sampleRate, block);
    processor.prepareToPlay(sampleRate, block);

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(outFile),
                                                                        sampleRate, (unsigned) channels, 24, {}, 0));
    if (writer == nullptr)
    {
        std::fprintf(stderr, "cannot write %s\n", outFile.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::AudioBuffer<float> buffer(channels, block);
    juce::MidiBuffer midi;
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += block)
    {
        const int n = (int) juce::jmin<juce::int64>(block, reader->lengthInSamples - pos);
        buffer.clear();
        reader->read(&buffer, 0, n, pos, true, true);
        // The processor sees the host's block size; a short last block is normal.
        juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(), channels, n);
        processor.processBlock(view, midi);
        writer->writeFromAudioSampleBuffer(view, 0, n);
    }

    std::printf("%s -> %s at %.0f %% (%d ch, %.0f Hz, %.2f s)\n", inFile.getFileName().toRawUTF8(),
                outFile.getFileName().toRawUTF8(), amount, channels, sampleRate,
                (double) reader->lengthInSamples / sampleRate);
    return 0;
}
