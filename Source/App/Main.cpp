#include "GlassLookAndFeel.h"
#include "MainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace qc
{
    class QCApplication : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override    { return "QC App"; }
        const juce::String getApplicationVersion() override { return "0.1.0"; }
        bool moreThanOneInstanceAllowed() override          { return true; }

        void initialise (const juce::String& commandLine) override
        {
            // Set before any window exists, so nothing is ever painted with the stock
            // grey chrome and then repainted.
            juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

            mainWindow = std::make_unique<MainWindow> (getApplicationName());

            // Accepts paths so the app can be launched from a shell association or a
            // terminal, with a folder starting a batch exactly as dropping one would.
            // This is not a headless mode - the window still opens and results appear
            // there.
            juce::StringArray paths;

            for (const auto& argument : juce::JUCEApplication::getCommandLineParameterArray())
            {
                const juce::File candidate (argument.unquoted());

                if (candidate.exists())
                    paths.add (candidate.getFullPathName());
            }

            if (! paths.isEmpty())
                mainWindow->openPaths (paths);

            juce::ignoreUnused (commandLine);
        }

        void shutdown() override
        {
            // Window first: it holds components that point at the look and feel.
            mainWindow = nullptr;
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        }

        void systemRequestedQuit() override
        {
            quit();
        }

    private:
        class MainWindow : public juce::DocumentWindow
        {
        public:
            explicit MainWindow (const juce::String& name)
                : DocumentWindow (name,
                                  juce::Colour (0xff0f1319),
                                  DocumentWindow::allButtons)
            {
                setUsingNativeTitleBar (true);
                setContentOwned (new MainComponent(), true);
                setResizable (true, false);
                setResizeLimits (900, 620, 4000, 3000);
                centreWithSize (getWidth(), getHeight());
                setVisible (true);
            }

            void openPaths (const juce::StringArray& paths)
            {
                if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                    content->openPaths (paths);
            }

            void closeButtonPressed() override
            {
                JUCEApplication::getInstance()->systemRequestedQuit();
            }

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
        };

        GlassLookAndFeel lookAndFeel;
        std::unique_ptr<MainWindow> mainWindow;
    };
}

START_JUCE_APPLICATION (qc::QCApplication)
