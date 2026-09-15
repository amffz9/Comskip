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
