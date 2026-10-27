#!/bin/bash
# Time ways of getting a recording's frames into Comskip.
#
# usage: bench SRC INI OUT [MODE...]
#   SRC   recording (a capture, or an existing 480x270 proxy)
#   INI   Comskip settings
#   OUT   directory for results; OUT/results.txt gets one line per process
# MODE (default: all of proxy gray hitch tee):
#   file   run Comskip on SRC itself (SRC is already a proxy)
#   proxy  make the TV app's 480x270 H.264 proxy, then run Comskip on it
#   gray   run Comskip on that proxy with the --enable-gray FFmpeg libraries
#   hitch  one FFmpeg decodes, scales and pipes grey frames + PCM into Comskip
#   tee    one FFmpeg writes the proxy and the grey pipe together
# HW=vaapi decodes and scales on the GPU (needs /dev/dri), as the TV app does.
set -euo pipefail
SRC=$1 INI=$2 OUT=$3; shift 3
MODES=${*:-proxy gray hitch tee}
mkdir -p "$OUT"; R=$OUT/results.txt
NOGRAY=/opt/ffmpeg-nogray GRAY=/opt/ffmpeg-gray
FF=$NOGRAY/bin/ffmpeg

if [ "${HW:-}" = vaapi ]; then
    DEC=(-hwaccel vaapi -hwaccel_output_format vaapi)
    SCALE='scale_vaapi=w=480:h=270,hwdownload,format=nv12,fps=30'
else
    DEC=()
    SCALE='fps=30,scale=480:270'
fi
X264=(-c:v libx264 -preset veryfast -bf 0 -b:v 500k -r 30 -c:a copy -f mpegts)
RAW=(-c:v rawvideo -c:a pcm_s16le -f nut)

timed() {  # name command... : append "name user sys wall" to results
    local n=$1; shift
    /usr/bin/time -f "$n %U %S %e" -a -o "$R" "$@"
}
comskip_on() {  # name libdir input
    mkdir -p "$OUT/$1"
    LD_LIBRARY_PATH=$2/lib timed "$1:comskip" comskip --ini="$INI" --output="$OUT/$1" "$3" \
        >"$OUT/$1/stdout.txt" 2>"$OUT/$1/stderr.txt" || echo "$1:comskip exit $?" >>"$R"
}

for m in $MODES; do case $m in
file)
    comskip_on file $NOGRAY "$SRC" ;;
proxy)
    timed proxy:ffmpeg $FF -v error -y "${DEC[@]}" -i "$SRC" -map 0:v:0 -map 0:a:0 -vf "$SCALE" "${X264[@]}" "$OUT/proxy.ts"
    comskip_on proxy $NOGRAY "$OUT/proxy.ts" ;;
gray)
    comskip_on gray $GRAY "$OUT/proxy.ts" ;;
hitch)
    mkdir -p "$OUT/hitch"
    timed hitch:ffmpeg $FF -v error "${DEC[@]}" -i "$SRC" -map 0:v:0 -map 0:a:0 -vf "$SCALE,format=gray" "${RAW[@]}" pipe:1 \
        | comskip_on hitch $NOGRAY /dev/stdin ;;
tee)
    mkdir -p "$OUT/tee"
    timed tee:ffmpeg $FF -v error -y "${DEC[@]}" -i "$SRC" -filter_complex "[0:v:0]$SCALE,split[p][g];[g]format=gray[gg]" \
        -map '[p]' -map 0:a:0 "${X264[@]}" "$OUT/tee.ts" -map '[gg]' -map 0:a:0 "${RAW[@]}" pipe:1 \
        | comskip_on tee $NOGRAY /dev/stdin ;;
*) echo "unknown mode $m" >&2; exit 2 ;;
esac; done
cat "$R"
