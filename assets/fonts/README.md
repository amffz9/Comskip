# Review font

Noto Sans is bundled for portable review help and detail text. It is distributed
under the SIL Open Font License; see [OFL.txt](OFL.txt).

Source: [Google Fonts Noto Sans](https://github.com/google/fonts/tree/main/ofl/notosans).
The bundled file is the variable `NotoSans[wdth,wght].ttf`, renamed `NotoSans.ttf`.

GUI builds embed this asset in the executable so review text also works after
relocation. Packages install this notice and `OFL.txt` under
`share/licenses/comskip/fonts`. The `review_font_file` setting can select an
external UTF-8 font path; an empty value uses this bundled font.
