/*
 * ocshot - renders the real editor to a PNG, offscreen.
 *
 * The README's picture of the plugin has to be the plugin, so this creates the
 * shipped processor and its editor, sets the knob, and snapshots the component
 * tree - no window, no host, no screen grab that depends on what else is on
 * the desktop. Reproducible from any build.
 *
 *   ocshot <out.png> [amount 0..100] [--about]
 */

#include <JuceHeader.h>

#include <cstdio>

#include "Source/PluginEditor.h"
#include "Source/PluginProcessor.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: ocshot <out.png> [amount 0..100] [--about]\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    double amount = 60.0;
    bool about = false;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--about")
            about = true;
        else
            amount = arg.getDoubleValue();
    }

    OccluderAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    processor.apvts.getParameter(occluder::ParamIDs::amount)->setValueNotifyingHost((float) (amount / 100.0));

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf(stderr, "no editor\n");
        return 1;
    }

    if (about)
        if (auto* e = dynamic_cast<OccluderAudioProcessorEditor*>(editor.get()))
            e->showAbout();

    // Let the display's timer notice the knob and lay everything out.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), false, 2.0f);
    juce::PNGImageFormat png;
    // FileOutputStream appends to an existing file, and a PNG decoder reads
    // whichever image comes first - so a re-render would show the old one.
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (! stream.openedOk() || ! png.writeImageToStream(image, stream))
    {
        std::fprintf(stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf("%s %dx%d\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
