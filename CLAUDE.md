# QC App

## What This Is
A standalone Windows desktop app that analyses audio files for loudness compliance (BS.1770-4 LUFS, true peak, LRA) and reports whether they pass broadcast, streaming, and podcast delivery specs. Not a plugin. See `prd.md`.

## Tech Stack
- **Framework:** JUCE (`juce::AudioAppComponent` shell, no device I/O opened)
- **Language:** C++17
- **Build:** CMake + CPM, JUCE fetched at a pinned version
- **Platform:** Windows 10+ only. AAC/M4A decode via Media Foundation
- **Playback:** output-only device, opened lazily for auditioning. Never used for measurement
- **Tests:** unit-test target driven by the EBU Tech 3341/3342 compliance signals

## Coding Principles
- **Single responsibility.** Every function does one thing. If describing it needs an "and", split it.
- **Small functions.** Over ~30 lines usually means it's doing too much — extract named sub-operations.
- **No god classes.** One clear concept per class. Decompose when unrelated methods accumulate.
- **Composition over deep inheritance.** One or two levels is fine; five is a design smell.
- **Descriptive names over comments.** `calculateGatedLoudness()` needs no comment; `calc()` needs a refactor.
- **Comment why, not what.** Especially for spec decisions — cite the clause of BS.1770-4 or Tech 3342 that a non-obvious constant comes from.
- **Fail early and clearly.** Validate at function boundaries. A sample-rate or channel-count mismatch is an error, never a silent truncation.

## Architecture Preferences
- The measurement engine is a pure library with no JUCE UI dependency: audio buffer in, result struct out. It must be fully testable without opening a window.
- Separate the layers cleanly — measurement, verdict evaluation (result + target → PASS/FAIL/WARN + fix hint), and presentation (graph, PDF, JSON). Each is independently testable.
- One class per DSP stage: K-weighting filter, gated-loudness accumulator, true-peak oversampler, LRA statistics. No class does two of these.
- Target specs load from `targets.json` at runtime. No spec value is ever a literal in C++.
- Platform-specific decoding (Media Foundation) sits behind an interface so the engine stays portable.

## Code Style
- JUCE conventions: camelCase for variables and methods, PascalCase for classes.
- Explicit `juce::` prefix. No `using namespace juce;`.
- RAII throughout. `std::unique_ptr` over raw owning pointers.
- Loudness values are `double` end to end — accumulating mean square in `float` loses precision over long files.

## What NOT To Do
- Don't hardcode platform loudness targets — they change; they live in `targets.json`.
- Don't reuse 48 kHz K-weighting coefficients at other sample rates. Re-derive them.
- Don't render a Netflix pass/fail without a dialogue stem loaded.
- Don't capture audio or measure from a live source. The output device exists to audition files; every number comes from an offline pass.
- Don't allocate, lock, or block on the audio thread. Playback runs through `AudioTransportSource` with a read-ahead thread — keep it that way rather than reading files in the callback.
- Don't write audio files. The app measures and advises; it never corrects.
- Don't add third-party dependencies without asking first.
- Don't add features not specified in `prd.md`.
