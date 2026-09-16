# Modernization completion checklist

The remaining work is complete only when all items have implementation and
verification evidence. Passing the current media smoke tests alone is insufficient.

- [ ] Settings are ordinary validated application-owned values, including regional
  commercial profiles; loading does not mutate process-wide globals.
- [ ] RecordingState owns per-recording detection, logo, caption, timing, output,
  and UI state. Interfaces receive their dependencies explicitly. No shared mutable
  globals, singleton accessors, thread-local replacements, or global aliases remain.
- [ ] FFmpeg resources and files have automatic ownership across normal, error,
  seek, and reopen paths. Lower-level functions return/throw actionable errors
  rather than terminate the process.
- [ ] The review UI uses SDL across supported platforms with explicit event state,
  RAII graphics resources, and a deliberate headless backend.
- [ ] Human-facing messages and review labels use committed catalogs with locale
  selection, fallback, and validated formatting. Machine-readable formats stay stable.
- [ ] Tests cover independent repeated analyses, seeking/reopening, damaged and
  truncated media, stream format changes, known commercial intervals, and exact
  output serializers including escaping and time/frame boundary cases.
- [ ] Windows, Linux, and macOS headless/SDL builds and tests are verified;
  sanitizer checks run where supported. CI configuration alone is not proof.
- [ ] Media and output code have focused interfaces and source modules; obsolete
  shared declarations and unsafe ownership/buffer patterns have been removed.

Use modern C++ facilities where they clarify ownership and contracts: value types,
unique ownership, spans, chrono, typed flags, ranges/algorithms, expected errors,
and standard synchronization. Supporting libraries retain responsibility for
media formats, configuration syntax, XML syntax, localization catalog syntax,
command-line parsing, graphics, and testing.

Commit each verified migration step. Keep remaining limitations explicit rather
than redefine completion around whichever subset currently passes tests.

## Verified migration stages

- Owned FFmpeg input/codec/frame/packet/dictionary/scaler resources are integrated
  in the recording-owned media coordinator. Networking follows decoder ownership.
- C++ lower-level exit requests unwind to the application boundary, and scan task
  failures propagate to the caller. Workers support captured recording dependencies.
- English/Spanish primary CLI, media, and review catalogs, external editable
  catalogs, and real CLI selection tests are integrated. Configuration warnings
  and argument errors have catalog coverage and actual error-path tests. Some
  detector and output diagnostics remain.
- Settings values own every committed configuration field and regional profile;
  application callsites now receive an explicit recording context. Detection,
  media, output, and review state have moved into that context. Dynamic detection
  buffers, recording files, argument snapshots, and XDS metadata are owned values.
  Pixel buffers and live logo rings allocate for validated recording geometry;
  caption grids use owned arrays. Geometry changes invalidate stale logo state.
- The portable review backend is integrated with explicit event state and
  English/Spanish help and labels. Both backend variants pass their six tests;
  SDL rendering verification currently uses the dummy driver on Windows.
- Pure timed EDL serialization is integrated for standard/live/plus output, with
  boundary/offset/locale/error tests. Seven pugixml XML serializers are integrated
  into normal and review exports, with ten serializer and seven application
  adapter tests. Live DVRMSTB uses the same library, with four conversion and
  two actual live-detector tests. The obsolete custom XML escaper and seven
  unused XML file owners are removed. The plist fragment remains to migrate.
- The A53 caption bridge bounds each packet to 31 intact triplets, preserves
  oversized payloads through ordered chunks, and rejects malformed lengths.
  Four tests cover framing and data preservation. The supporting caption
  decoder still has shared globals and needs an owned replacement.
- Windows headless and SDL-enabled application builds each pass 119 tests at
  f032ea3. Scoped Windows
  scheduling/power policies have three native restoration
  tests. Coverage includes
  seeking/reopening, damaged/truncated media, stream format changes, independent
  repeated analyses within one process, and failure cleanup followed by success.
  CSV replay, weighted-score boundaries, bounded review sampling, and persisted
  logo bounds. SDL rendering uses the dummy driver. Ubuntu GCC 14 / FFmpeg 6.1
  headless and SDL builds each pass 89 tests. The same suite passes with address,
  undefined-behavior, and leak sanitizers, including the CSV replay histogram
  bounds fix (2131eee). These results precede XML integration. macOS and
  interactive SDL application verification still require evidence.
