/** Renders the interface offscreen and writes it to a PNG.

    A screenshot of the running app cannot be taken repeatably or compared across a
    change; this can. It builds the real MainComponent, feeds it real audio, waits for
    the analysis to land, and captures the result.

    Usage: qc_uishot <output.png> [width height] [audio path or folder...]
*/

#include "../Source/App/GlassLookAndFeel.h"
#include "../Source/App/MainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
    /** Pumps the message loop for a while so queued analysis callbacks are delivered
        and the component has laid itself out before the capture.
    */
    void settle (int milliseconds)
    {
        const auto deadline = juce::Time::getMillisecondCounter()
                            + static_cast<juce::uint32> (milliseconds);

        while (juce::Time::getMillisecondCounter() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: qc_uishot <output.png> [width height] [audio path...]\n");
        return 1;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));

    int width = 1280;
    int height = 860;
    int firstPathArgument = 2;

    if (argc >= 4 && juce::String (argv[2]).containsOnly ("0123456789"))
    {
        width = juce::String (argv[2]).getIntValue();
        height = juce::String (argv[3]).getIntValue();
        firstPathArgument = 4;
    }

    qc::GlassLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    {
        auto component = std::make_unique<qc::MainComponent>();
        component->setSize (width, height);

        juce::StringArray paths;

        for (int i = firstPathArgument; i < argc; ++i)
            paths.add (juce::String (argv[i]));

        if (! paths.isEmpty())
        {
            component->openPaths (paths);

            // Long enough for a handful of short test files to finish analysing.
            settle (4000);
        }
        else
        {
            settle (200);
        }

        const auto image = component->createComponentSnapshot (component->getLocalBounds(), true);

        output.deleteFile();
        juce::FileOutputStream stream (output);

        if (! stream.openedOk())
        {
            std::fprintf (stderr, "could not open %s for writing\n", output.getFullPathName().toRawUTF8());
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
            return 1;
        }

        juce::PNGImageFormat png;

        if (! png.writeImageToStream (image, stream))
        {
            std::fprintf (stderr, "could not encode the snapshot\n");
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
            return 1;
        }

        std::printf ("wrote %s (%d x %d)\n", output.getFullPathName().toRawUTF8(), width, height);
    }

    // The component must be gone before the look and feel it points at.
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    return 0;
}
