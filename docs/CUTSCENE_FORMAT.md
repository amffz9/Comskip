# Cutscene sample format

Cutscene sample files contain a four-byte signed brightness value in little-endian order,
followed by 1 to 120,000 sampled luma bytes. This matches files written by the historical
x86 implementation while making the byte order and integer width independent of the host.

The former implementation wrote a native `int`, ignored short writes and close failures,
and could therefore create apparently successful truncated files. The focused format module
validates empty, truncated, and oversized records before publishing them. Loading remains
nonfatal because configured cutscene samples are optional; saving reports open, write, and
close failures as owned output diagnostics.
