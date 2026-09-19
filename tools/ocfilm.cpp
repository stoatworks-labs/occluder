/*
 * ocfilm - renders the project video's footage: frames of the real editor and
 * the audio the real processor made, from one knob automation.
 *
 *   ocfilm <voice.wav|aiff> <automation.txt> <frames-dir> <audio-out.wav> [fps]
 *
 * automation.txt is "seconds amount" per line, amount in 0..100, linear
 * between points, held after the last. The take runs for the length of the
 * audio file.
 *
 * Not a screen recording, and not a mock-up: the frames are snapshots of the
 * shipped editor with the parameter set to the automation's value, and the
 * audio is the shipped processor run over the file with the same automation
 * applied block by block - so what the video shows and what it plays are the
 * plugin doing the same thing at the same moment. A window recording would
 * need a quiet machine for forty seconds and would catch whatever came to the
 * front; this is reproducible from any build.
 *
 * Frames are 1920x1080: the editor snapshotted at 2x (720x1008) on brand navy,
 * set to the right so the series' captions, bottom left, sit on plain navy
 * rather than across the knob.
 */

#include <JuceHeader.h>

#include <cstdio>
#include <vector>

#include "Source/PluginEditor.h"
#include "Source/PluginProcessor.h"

namespace
{
    struct Breakpoint { double t, amount; };

    double amountAt(const std::vector<Breakpoint>& pts, double t)
    {
        if (pts.empty()) return 0.0;
        if (t <= pts.front().t) return pts.front().amount;
        for (size_t i = 1; i < pts.size(); ++i)
            if (t <= pts[i].t)
            {
                const auto& a = pts[i - 1];
                const auto& b = pts[i];
                const double u = (t - a.t) / juce::jmax(1e-9, b.t - a.t);
                return a.amount + u * (b.amount - a.amount);
            }
        return pts.back().amount;
    }
}

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr, "usage: ocfilm <voice> <automation.txt> <frames-dir> <audio-out.wav> [fps]\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto voiceFile = cwd.getChildFile(argv[1]);
    const auto autoFile = cwd.getChildFile(argv[2]);
    const auto framesDir = cwd.getChildFile(argv[3]);
    const auto audioOut = cwd.getChildFile(argv[4]);
    const int fps = argc > 5 ? juce::String(argv[5]).getIntValue() : 30;

    std::vector<Breakpoint> automation;
    for (const auto& line : juce::StringArray::fromLines(autoFile.loadFileAsString()))
    {
        const auto parts = juce::StringArray::fromTokens(line.trim(), " \t", "");
        if (parts.size() >= 2 && ! line.trim().startsWith("#"))
            automation.push_back({ parts[0].getDoubleValue(), parts[1].getDoubleValue() });
    }
    if (automation.empty())
    {
        std::fprintf(stderr, "no automation points in %s\n", autoFile.getFullPathName().toRawUTF8());
        return 1;
    }

    // ------------------------------------------------------------------ audio
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(voiceFile));
    if (reader == nullptr)
    {
        std::fprintf(stderr, "cannot read %s\n", voiceFile.getFullPathName().toRawUTF8());
        return 1;
    }
    const int channels = (int) reader->numChannels;
    const double sampleRate = reader->sampleRate;
    const int block = 256;   // fine-grained enough for the automation to be smooth on its own
    const double seconds = (double) reader->lengthInSamples / sampleRate;

    OccluderAudioProcessor processor;
    {
        juce::AudioProcessor::BusesLayout layout;
        const auto set = juce::AudioChannelSet::canonicalChannelSet(channels);
        layout.inputBuses.add(set);
        layout.outputBuses.add(set);
        processor.setBusesLayout(layout);
    }
    auto* param = processor.apvts.getParameter(occluder::ParamIDs::amount);
    param->setValueNotifyingHost((float) (amountAt(automation, 0.0) / 100.0));
    processor.setRateAndBufferSizeDetails(sampleRate, block);
    processor.prepareToPlay(sampleRate, block);

    audioOut.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(audioOut),
                                                                        sampleRate, (unsigned) channels, 24, {}, 0));
    if (writer == nullptr)
    {
        std::fprintf(stderr, "cannot write %s\n", audioOut.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::AudioBuffer<float> buffer(channels, block);
    juce::MidiBuffer midi;
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += block)
    {
        const int n = (int) juce::jmin<juce::int64>(block, reader->lengthInSamples - pos);
        param->setValueNotifyingHost((float) (amountAt(automation, (double) pos / sampleRate) / 100.0));
        buffer.clear();
        reader->read(&buffer, 0, n, pos, true, true);
        juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(), channels, n);
        processor.processBlock(view, midi);
        writer->writeFromAudioSampleBuffer(view, 0, n);
    }
    writer.reset();
    std::printf("audio: %s (%.2f s)\n", audioOut.getFileName().toRawUTF8(), seconds);

    // ----------------------------------------------------------------- frames
    framesDir.createDirectory();
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf(stderr, "no editor\n");
        return 1;
    }
    const int frames = (int) std::ceil(seconds * fps);
    const juce::Colour navy(0xff0e2942);
    juce::PNGImageFormat png;
    for (int i = 0; i < frames; ++i)
    {
        const double t = (double) i / fps;
        param->setValueNotifyingHost((float) (amountAt(automation, t) / 100.0));
        // The display's timer sees the new value on the next tick (30 Hz) and
        // repaints; two ticks' worth of loop makes sure the frame is current.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(70);

        const auto shot = editor->createComponentSnapshot(editor->getLocalBounds(), false, 2.0f);
        juce::Image frame(juce::Image::RGB, 1920, 1080, true);
        {
            juce::Graphics g(frame);
            g.fillAll(navy);
            g.drawImageAt(shot, 1920 - shot.getWidth() - 160, (1080 - shot.getHeight()) / 2);
        }
        const auto out = framesDir.getChildFile(juce::String::formatted("frame-%05d.png", i));
        out.deleteFile();
        juce::FileOutputStream stream(out);
        if (! stream.openedOk() || ! png.writeImageToStream(frame, stream))
        {
            std::fprintf(stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
            return 1;
        }
        if (i % fps == 0)
            std::printf("frame %d/%d  t=%.1f  amount=%.0f\n", i, frames, t, amountAt(automation, t));
    }
    std::printf("frames: %d in %s\n", frames, framesDir.getFullPathName().toRawUTF8());
    return 0;
}
