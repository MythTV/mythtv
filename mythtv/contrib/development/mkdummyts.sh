#!/bin/sh

# Converts png's in /tmp to streams that can be used by DummyDTVecorder.

create_ts()
{
  convert /tmp/$1.png ppm:- | \
    ppmtoy4m -n15 -F$3 -A$4 -I$5 -r -S 420mpeg2 | \
    mpeg2enc -c -n n -a$6 --no-constraints -s -o /tmp/tmp.es
  vlc /tmp/tmp.es --sout \
    '#std{access=file,mux=ts{pid-pmt=0x20,pid-video=0x21.tsid=1},\
            url="/tmp/tmp.ts"}' vlc:quit
  mv /tmp/tmp.ts /tmp/dummy$1$2.ts
}

create_ts 640x480   p29.97  30000:1001  10:11 p 2
create_ts 720x480   p29.97  30000:1001  1:1   p 2
create_ts 768x576   p50.00  50:1        59:54 p 2
create_ts 1280x720  p29.97  30000:1001  1:1   p 3
create_ts 1920x1088 p29.97  30000:1001  1:1   p 3
