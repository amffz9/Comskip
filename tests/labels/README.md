# Labelled recordings

Hand-checked commercial breaks for recordings used in accuracy measurements.
The recordings themselves are not in the repository (they are local captures).

| File | Recording | Edges |
| --- | --- | --- |
| `g5.labels.csv` | First 90 min of Chiefs at Seahawks, NBC | about 5 s |
| `g4.labels.csv` | First 90 min of Colts at Vikings, CBS | about 5 s |
| `faaea10a.labels.csv` | 2016 college football replay, Boise State at Wyoming | about 2 s |
| `77483aca.labels.csv` | Steelers at Patriots, CBS, with halftime report | about 2 s |

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

Full analysis allowed to finish, `live_tv` off, otherwise the TV ini.
Programme cut excludes the non-ad kinds; with silence on, 30 s of g5's Geico
split-screen and 15 s of g4's Verizon split-screen are cut.
