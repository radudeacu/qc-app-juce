# PRD: QC App — Loudness Compliance Analyser

## Goal
A standalone Windows desktop app that analyses audio files for loudness compliance and answers one question directly: *does this pass the spec I'm delivering to?* The user drops in files, picks target platforms, and gets per-target PASS / FAIL / WARN verdicts, a loudness-over-time graph, a printable QC report, and a plain-language hint describing what to change to pass.

## Features

### Measurement engine (ITU-R BS.1770-4, EBU Tech 3341/3342)
- K-weighting: stage 1 high-shelf +4 dB @ 1681 Hz, stage 2 RLB high-pass @ 38 Hz. Coefficients are specified at 48 kHz — re-derive per sample rate, never reuse the 48 kHz values at other rates.
- Momentary loudness (M): 400 ms window, ungated, 100 ms step.
- Short-term loudness (S): 3 s window, ungated, 100 ms step.
- Integrated loudness (I): 400 ms blocks at 75 % overlap, absolute gate −70 LUFS, relative gate −10 LU below the ungated mean of surviving blocks.
- Loudness Range (LRA): 3 s window, 1 s step, absolute gate −70 LUFS, relative gate −20 LU, 10th–95th percentile.
- True peak: 4× oversampling (≥192 kHz effective rate); 2× is sufficient above 96 kHz. Reported in dBTP.
- Peak-to-loudness ratio (PLR): true peak − integrated loudness.
- Channel weights: L = R = C = 1.0. Mono and stereo only.
- All measurements run offline, faster than real time. No audio device is opened.

### Non-loudness QC checks
- Sample peak per channel, in dBFS.
- True-peak overs: count plus a timestamp list of every excursion above the selected target's dBTP ceiling.
- Clipping: flag runs of ≥ 3 consecutive samples at ≥ −0.1 dBFS. Threshold and run length configurable.
- Stereo correlation: −1…+1, warn if negative for more than 5 % of duration.
- Mono compatibility: warn if the mono sum's integrated loudness is more than 3 dB below the stereo integrated loudness.

### Target definitions
- Targets live in a versioned `targets.json` data file, never hardcoded. Each entry carries a `lastVerified` date so spec drift is a data edit, not a recompile.
- Seed values — `[NEED: confirm each against the current published spec before first release]`:

| Target | Integrated | Tolerance | Max true peak | Gating |
|---|---|---|---|---|
| EBU R128 | −23.0 LUFS | ±0.5 LU | −1.0 dBTP | Program |
| ATSC A/85 | −24.0 LKFS | ±2.0 LU | −2.0 dBTP | Program |
| Spotify | −14.0 LUFS | — | −1.0 dBTP | Program |
| YouTube | −14.0 LUFS | — | −1.0 dBTP | Program |
| Apple Music | −16.0 LUFS | — | −1.0 dBTP | Program |
| Tidal | −14.0 LUFS | — | −1.0 dBTP | Program |
| Amazon Music | −14.0 LUFS | — | −2.0 dBTP | Program |
| Apple Podcasts | −16.0 LUFS | ±1.0 LU | −1.0 dBTP | Program |
| Spotify (spoken word) | −14.0 LUFS | — | −1.0 dBTP | Program |
| Netflix | −27.0 LKFS | ±2.0 LU | −2.0 dBTP | **Dialogue** |

### Dialogue gating (Netflix)
- The user may load an optional second file: a dialogue stem matching the main mix.
- The Netflix verdict gates the mix's loudness to the intervals where the dialogue stem is active.
- Without a dialogue stem, the Netflix row shows `NOT MEASURED — dialogue stem required`. It is never rendered as a pass or a fail.
- Mix and stem must share sample rate and duration; mismatch is an error, not a silent truncation.

### Fix Hint
- One line per failing target, generated from the measurements. No hint is shown for a passing target.
- Gain-fixes-it case: `Apply −2.3 dB gain → −14.0 LUFS, true peak lands at −1.4 dBTP. Passes.`
- Gain-would-clip case: `Apply +1.8 dB gain → −14.0 LUFS, but true peak would hit −0.2 dBTP (ceiling −1.0). Needs limiting, ~0.8 dB of gain reduction.`
- True-peak-only case: `Loudness passes. Reduce true peak by 1.3 dB — limiter ceiling at −1.0 dBTP.`
- Wide-LRA case: `LRA 21.4 LU is wide for this target — compression or level-riding needed.`
- The gain figure and the post-gain true peak are exact (gain is linear). The limiting amount is an estimate and must be worded as one.

### Batch mode
- Accepts a folder drop. Recursion into subfolders is off by default, with a toggle.
- Files processed in parallel across `hardware_concurrency − 1` workers, capped at 4.
- Processing continues on error. Unreadable files appear in the results table as `ERROR` with the reason.
- Sortable results table, one row per file, showing PASS / FAIL / WARN per selected target.

### Input formats
- WAV, AIFF, BWF — all bit depths and sample rates, via JUCE's native readers.
- MP3 via JUCE's native reader.
- AAC / M4A via Windows Media Foundation.
- Any file the app cannot decode fails with a specific message naming the format, never a generic failure.

### Output
- Loudness-over-time graph on screen: short-term and momentary LUFS across the file duration, target band shaded, true-peak overs marked.
- Printable PDF QC report: one page per file with measurements, the per-target pass/fail table, the graph, and any fix hints.
- JSON result file per analysis, containing every measurement including the time series. This is the format the test suite asserts against.

## Behaviour
- Single window, file/folder drag-and-drop plus a browse button.
- Target selection is multi-select; the last-used selection is remembered between launches. Nothing else persists.
- Progress is shown per file during batch runs and the run is cancellable.
- A file whose measured integrated loudness falls within tolerance but whose true peak exceeds the ceiling is a FAIL, not a WARN.
- WARN is reserved for the non-loudness checks (correlation, mono compatibility, clipping) and for LRA guidance. Loudness and true peak produce PASS or FAIL only.
- Validation against reference material is a shipping requirement, not a nice-to-have: the EBU Tech 3341/3342 compliance signals run as an automated unit-test target, asserting each measurement within the tolerance the spec states.
- Platform: Windows only for V1. Nothing in the code should assume Windows beyond the Media Foundation AAC path, which must be isolated behind an interface.

## Out of Scope
The user chose not to define additional scope boundaries, preferring to build and iterate. The following are excluded as a direct consequence of decisions made during planning, not as separate restrictions:

- **Live audio input, playback, and transport** — the app is offline file analysis; no audio device is opened.
- **Surround, 5.1, and immersive/Atmos** — mono and stereo only.
- **Video containers (MP4, MOV, MXF)** — audio files only; no FFmpeg dependency.
- **Rendering corrected audio** — the app measures and advises; it never writes an audio file.

`[NEED: revisit]` No boundaries were set around session history, per-client presets, version-comparison, or a plugin build target. Recommend deciding these before implementation starts — undefined edges are where coding agents over-build.
