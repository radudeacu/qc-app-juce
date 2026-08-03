#include "PlaybackEngine.h"

#include "FileAnalysisJob.h"

#if JUCE_WINDOWS
 #include <objbase.h>
#endif

namespace qc
{
    namespace
    {
        constexpr int kReadAheadSamples = 65536;

        /** The read-ahead thread that feeds the transport.

            On Windows it initialises COM for itself. The Media Foundation reader used
            for AAC and M4A creates its objects on whichever thread opened the file, and
            then has its samples pulled from this one; calling into those objects from a
            thread with no apartment fails at run time, and only for compressed formats,
            which is exactly the sort of bug that ships.
        */
        class ReadAheadThread final : public juce::TimeSliceThread
        {
        public:
            ReadAheadThread() : juce::TimeSliceThread ("QC playback read-ahead") {}

            void run() override
            {
               #if JUCE_WINDOWS
                const auto comResult = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
                const bool ownsCom = SUCCEEDED (comResult);
               #endif

                juce::TimeSliceThread::run();

               #if JUCE_WINDOWS
                if (ownsCom)
                    CoUninitialize();
               #endif
            }
        };
    }

    PlaybackEngine::PlaybackEngine()
    {
        transport.addChangeListener (this);
    }

    PlaybackEngine::~PlaybackEngine()
    {
        transport.removeChangeListener (this);

        // Order matters: the player must stop pulling before the sources it pulls from
        // are destroyed, and the reader must outlive the thread reading ahead of it.
        transport.setSource (nullptr);
        sourcePlayer.setSource (nullptr);

        if (deviceOpen)
            deviceManager.removeAudioCallback (&sourcePlayer);

        readerSource.reset();
        readAheadThread.reset();
    }

    bool PlaybackEngine::ensureDeviceOpen (juce::String& error)
    {
        if (deviceOpen)
            return true;

        // Output only: this app never records, and asking for inputs would prompt for
        // microphone permission the user has no reason to grant.
        const auto result = deviceManager.initialiseWithDefaultDevices (0, 2);

        if (result.isNotEmpty())
        {
            error = "No audio output available: " + result;
            return false;
        }

        if (deviceManager.getCurrentAudioDevice() == nullptr)
        {
            error = "No audio output device is available on this machine.";
            return false;
        }

        deviceManager.addAudioCallback (&sourcePlayer);
        sourcePlayer.setSource (&transport);
        deviceOpen = true;
        return true;
    }

    bool PlaybackEngine::load (const juce::File& file, juce::String& error)
    {
        unload();

        if (! ensureDeviceOpen (error))
            return false;

        auto* reader = getFormatManager().createReaderFor (file);

        if (reader == nullptr)
        {
            error = "Cannot play " + file.getFileName() + ": no decoder for this format.";
            return false;
        }

        if (readAheadThread == nullptr)
        {
            readAheadThread = std::make_unique<ReadAheadThread>();
            readAheadThread->startThread (juce::Thread::Priority::normal);
        }

        readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        transport.setSource (readerSource.get(), kReadAheadSamples, readAheadThread.get(),
                             reader->sampleRate);

        loadedFile = file;
        return true;
    }

    void PlaybackEngine::unload()
    {
        transport.stop();
        transport.setSource (nullptr);
        readerSource.reset();
        loadedFile = juce::File();
    }

    void PlaybackEngine::play()
    {
        if (readerSource == nullptr)
            return;

        // Starting from the very end would play nothing and look like a broken button.
        if (transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.01)
            transport.setPosition (0.0);

        transport.start();
    }

    void PlaybackEngine::pause()
    {
        transport.stop();
    }

    void PlaybackEngine::stop()
    {
        transport.stop();
        transport.setPosition (0.0);
    }

    void PlaybackEngine::togglePlayPause()
    {
        if (isPlaying())
            pause();
        else
            play();
    }

    bool PlaybackEngine::isPlaying() const
    {
        return transport.isPlaying();
    }

    double PlaybackEngine::getPositionSeconds() const
    {
        return transport.getCurrentPosition();
    }

    double PlaybackEngine::getLengthSeconds() const
    {
        return transport.getLengthInSeconds();
    }

    void PlaybackEngine::setPositionSeconds (double seconds)
    {
        if (readerSource == nullptr)
            return;

        transport.setPosition (juce::jlimit (0.0, transport.getLengthInSeconds(), seconds));
    }

    void PlaybackEngine::changeListenerCallback (juce::ChangeBroadcaster* source)
    {
        if (source == &transport && onStateChanged != nullptr)
            onStateChanged();
    }
}
