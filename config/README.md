# Configuration

`defaults.ini` is the authoritative source for the existing 163 numeric and
14 string settings. CMake embeds it in the executable, so missing files or a
different working directory do not change the defaults. Rebuild after editing
the repository defaults. To override settings at runtime, copy the file or write
a smaller INI and pass `comskip --ini=path/to/custom.ini recording.ts`.

Existing INI option names and section labels remain supported. Keys are matched
exactly, comment lines are ignored, and the last duplicate key wins. Numbers use
a decimal dot regardless of OS locale. Boolean settings accept 0 or 1. Numeric
overflow, nonfinite numbers, malformed quoted strings, oversized string values,
and invalid worker/buffer counts are rejected before applying any settings.
Unknown keys remain ignored for compatibility with older INI files.

Quoted strings support `\\`, `\"`, `\n`, and `\t`. Escape backslashes in Windows
paths, or use forward slashes. `windowtitle` accepts one optional `%s` filename
placeholder and `%%` for a literal percent. Output-format strings and INI keys
are independent of the interface language.

These are configuration defaults, not runtime state: frame counters, decoded
dimensions, histograms, and format/protocol constants remain in code.
