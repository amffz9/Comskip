# Labelled recordings

Hand-checked commercial breaks for recordings used in accuracy measurements.
The recordings themselves are not in the repository (they are local captures).

| File | Recording | Edges |
| --- | --- | --- |
| `g5.labels.csv` | First 90 min of Chiefs at Seahawks, NBC | about 5 s |
| `g4.labels.csv` | First 90 min of Colts at Vikings, CBS | about 5 s |
| `faaea10a.labels.csv` | 2016 college football replay, Boise State at Wyoming | about 2 s |
| `77483aca.labels.csv` | Steelers at Patriots, CBS, with halftime report | about 2 s |
| `elsbeth303.labels.csv` | Elsbeth S03E03, CBS drama (Plex recording) | about 2 s |
| `nash107.labels.csv` | 9-1-1 Nashville S01E07, ABC drama (Plex recording) | about 2 s |

Labels were made by viewing contact sheets of frames. Rows are
`start_s,end_s,kind,note`. `kind` is `ad` (part of a break) or one of the
kinds scored as neither ad nor programme, whose cut seconds are reported
separately: `sponsor` (a sponsor segment during the programme),
`in-programme ad` (an ad shown inside the programme, such as a split-screen
whose audio is a real ad), `intro` and `credits`.

Score a Comskip cut list with `python score_labels.py LABELS.csv CUTLIST.txt`.

## October 27, 2026: TV-app settings, silence off (43) and on (107)

| Recording | 43 breaks | 43 missed / programme cut | 107 breaks | 107 missed / programme cut |
| --- | --- | --- | --- | --- |
| g5 (NBC) | 5/10 | 730 s / 31 s | 9/10 | 244 s / 43 s |
| g4 (CBS) | 13/13 | 107 s / 161 s | 13/13 | 82 s / 165 s |
| faaea10a | 6/12 | 596 s / 12 s | 6/12 | 596 s / 12 s |
| 77483aca | 24/24 | 201 s / 165 s | 24/24 | 201 s / 165 s |
| elsbeth303 | 4/6 | 271 s / 4 s | 6/6 | 21 s / 5 s |
| nash107 | 3/6 | 606 s / 10 s | 4/6 | 442 s / 65 s |

Full analysis allowed to finish, `live_tv` off, otherwise the TV ini.
Programme cut excludes the non-ad kinds; with silence on, 30 s of g5's Geico
split-screen and 15 s of g4's Verizon split-screen are cut.

These rows were scored at the scorer's default 29.97 fps, but the cut lists
are 30 fps (`FRAMES AT 3000`); pass `30` as the third argument. The table
below is scored at 30 fps.

## October 28, 2026: Plex's bundled comskip.ini against the TV ini

Our Comskip (comskip-test at 2e1e215) on the same inputs, scored at 30 fps.
Plex is `/usr/lib/plexmediaserver/Resources/comskip.ini` from the Plex
container, unchanged (detect_method 43) and with detect_method=107. Each
cell is breaks found, ad seconds missed / programme seconds cut.

| Recording | TV 43 | TV 107 | Plex 43 | Plex 107 |
| --- | --- | --- | --- | --- |
| g5 (NBC) | 5/10, 724 / 23 | 9/10, 232 / 30 | 9/10, 168 / 26 | 10/10, 96 / 31 |
| g4 (CBS) | 13/13, 87 / 139 | 13/13, 72 / 151 | 13/13, 87 / 139 | 13/13, 72 / 151 |
| faaea10a | 6/12, 591 / 6 | 6/12, 591 / 6 | 5/12, 657 / 6 | 5/12, 657 / 6 |
| 77483aca | 24/24, 132 / 96 | 24/24, 132 / 96 | 24/24, 132 / 97 | 24/24, 132 / 97 |
| elsbeth303 | 4/6, 267 / 0 | 6/6, 18 / 0 | 6/6, 12 / 0 | 6/6, 18 / 0 |
| nash107 | 3/6, 601 / 0 | 4/6, 436 / 57 | 4/6, 435 / 242 | 4/6, 436 / 242 |
| Total | 55/71, 2402 / 264 | 62/71, 1481 / 340 | 61/71, 1491 / 510 | 62/71, 1411 / 527 |

Non-ad kinds cut are as above except: g5 sponsor 0 s under both 43 runs and
30 s under both 107 runs; g4 sponsor 0 s (43) and 18 s (107).

Each Plex setting that differs, alone on top of the TV 107 ini:
min_show_segment_length=250 gives g5 10/10, 97 / 30 (136 s fewer missed)
and nash107 242 s programme cut (185 s more), everything else unchanged; it
is the whole difference between the TV 107 and Plex 107 columns on g5 and
nash107. max_avg_brightness=25 misses 5 s more on faaea10a and 1 s less on
77483aca. logo_threshold=0.75 loses 2 faaea10a breaks (4/12, 99 s more
missed). disable_heuristics=4, min_commercialbreak=25,
max_commercial_size=125 and ms_audio_delay=5 change nothing. None lowers
programme cut.

On nash107 Plex's 43 and 107 both join the 3120-3341 break to the end of
the file (3120.8-3596.5), cutting the last act (3341-3583). Plex's own
markers for the original file stop at 3341.4 and add 3583.5-3596.5.
Run on the original file (1080p 59.94 fps HEVC .mkv) instead of the
480x270 proxy, our Comskip with Plex's ini, 43 or 107, gives Plex's markers
to within 0.5 s: 4/6, 434 s missed, 0 s programme cut. The TV 107 ini on
the original gives 3/6, 600 s missed, 0 s programme cut. So the difference
is the input, not the code: on the proxy both inis cut programme at the end
of nash107 (57 s with the TV ini, 242 s with Plex's).
