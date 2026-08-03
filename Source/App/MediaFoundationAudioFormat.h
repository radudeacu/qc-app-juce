#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace qc
{
    /** Reads the AAC family through Windows Media Foundation.

        JUCE's own WindowsMediaAudioFormat registers only .wmv/.asf/.wm/.wma, which
        leaves out the container podcast and streaming deliverables actually arrive in.
        This format fills that gap using the decoders already present on every supported
        Windows install - no bundled codec, no extra licensing.

        Read-only by design: this app measures files and never writes audio.

        Limitation worth knowing: Media Foundation decodes from a path, not from an
        arbitrary stream, so createReaderFor only succeeds for a juce::FileInputStream.
        That covers every route into this app, which opens files from disk.
    */
    class MediaFoundationAudioFormat : public juce::AudioFormat
    {
    public:
        MediaFoundationAudioFormat();

        juce::Array<int> getPossibleSampleRates() override;
        juce::Array<int> getPossibleBitDepths() override;

        bool canDoStereo() override { return true; }
        bool canDoMono() override   { return true; }
        bool isCompressed() override { return true; }

        juce::AudioFormatReader* createReaderFor (juce::InputStream* sourceStream,
                                                  bool deleteStreamIfOpeningFails) override;

        /** Always null - the app measures files and never writes audio. */
        std::unique_ptr<juce::AudioFormatWriter> createWriterFor (
            std::unique_ptr<juce::OutputStream>& streamToWriteTo,
            const juce::AudioFormatWriterOptions& options) override;

        /** Opens a file directly, bypassing the stream-based entry point. */
        static std::unique_ptr<juce::AudioFormatReader> createReaderForFile (const juce::File& file);

        /** True on platforms where this format does anything at all. */
        static bool isAvailable();
    };
}
