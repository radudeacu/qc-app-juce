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
            mainWindow = std::make_unique<MainWindow> (getApplicationName());

            // Accepts a path so the app can be launched from a shell association or a
            // terminal. This is not a headless mode - the window still opens and the
            // result is shown there.
            const auto arguments = juce::JUCEApplication::getCommandLineParameterArray();

            for (const auto& argument : arguments)
            {
                const juce::File file (argument.unquoted());

                if (file.existsAsFile())
                {
                    mainWindow->openFile (file);
                    break;
                }
            }

            juce::ignoreUnused (commandLine);
        }

        void shutdown() override
        {
            mainWindow = nullptr;
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

            void openFile (const juce::File& file)
            {
                if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                    content->openFile (file);
            }

            void closeButtonPressed() override
            {
                JUCEApplication::getInstance()->systemRequestedQuit();
            }

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
        };

        std::unique_ptr<MainWindow> mainWindow;
    };
}

START_JUCE_APPLICATION (qc::QCApplication)
