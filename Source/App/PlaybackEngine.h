#pragma once

#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace qc
{
    /** Auditioning for the file currently on screen.

        Playback is entirely separate from measurement. The analyser still reads the
        file offline, start to finish, and nothing here can influence a reading - which
        matters, because a meter whose numbers depend on whether you happened to be
        listening would be worthless.

        The output device is opened lazily on first use rather than at startup, so an
        app run purely to check files never takes an audio device from anything else,
        and a machine with no output device still runs normally.
    */
    class PlaybackEngine : private juce::ChangeListener
    {
    public:
        PlaybackEngine();
        ~PlaybackEngine() override;

        /** Prepares a file for playback.
            @returns false with a reason in `error` if the device or the file failed.
        */
        bool load (const juce::File& file, juce::String& error);

        /** Stops and releases the current file. Safe to call when nothing is loaded. */
        void unload();

        bool hasFile() const noexcept { return loadedFile != juce::File(); }

        void play();
        void pause();
        void stop();
        void togglePlayPause();

        bool isPlaying() const;

        double getPositionSeconds() const;
        double getLengthSeconds() const;
        void setPositionSeconds (double seconds);

        /** Fired on the message thread when the transport starts, stops, or reaches the
            end of the file.
        */
        std::function<void()> onStateChanged;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster* source) override;
        bool ensureDeviceOpen (juce::String& error);

        juce::AudioDeviceManager deviceManager;
        juce::AudioSourcePlayer sourcePlayer;
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        std::unique_ptr<juce::TimeSliceThread> readAheadThread;

        juce::File loadedFile;
        bool deviceOpen { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackEngine)
    };
}
