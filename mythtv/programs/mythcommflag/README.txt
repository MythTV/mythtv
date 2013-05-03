There are a number of flags to mythcommflag that can be useful when
running the program, which can be seen by running "mythcommflag --help"
This documents some of the more useful ones when flagging without
touching the DB.

First, --skipdb tells mythcommflag to try to avoid touching the
database; obviously you want to use this if you aren't running
a mysql instance with a mythconverg database.

--file specifies the input file

--outputfile specifies the output file. Notably if you use "-" as
the outputfile name, the output is sent to the standard output.

--very-quiet specifies a compact output format and is essential
when redirecting output from mythcommflag to the standard output
for further processing.

--outputmethod specifies the output format and will be explained
later in this file, options are essentials and full

--method comma separated list of commercial flagging methods to
use, by default all methods are employed. But you can choose other
combinations or employ a single method. "mythcommflag --help"
shows all options available.

=============================================================================

The commercial flagger is normally run by MythTV so you do not need to
understand the format of the output. However it be run as a command
line program to flag commercials without touching the database, in that
case you do need to understand the output formats.

NOTE: This describes the output of the normal "Classic" commercial
flagger, the new "Experimental" commercial flagger does not collect
most of the information in the more useful full output format.

The basic outputmethod is "essentials" and this will be used if you don't
specify an output format.

Here is an example input:
  mythcommflag --skipdb --file /path/my.avi --outputfile output.txt

This will put something like this in the output file:
commercialBreakListFor: /path/my.avi
totalframecount: 53346
----------------------------
framenum: 17468 marktype: 4
framenum: 21592 marktype: 5
framenum: 25263 marktype: 4
framenum: 29842 marktype: 5
framenum: 38003 marktype: 4
framenum: 42578 marktype: 5
framenum: 48436 marktype: 4
framenum: 53290 marktype: 5
----------------------------

As you can see there are frame numbers and mark types listed.
There are two mark types: 4 for COMM_START and 5 for COMM_END

This simple output format is enough if you just want to know where
the commercials are.

The other output format is "full" and it comes in two forms.
If you use the --quiet or --very-quiet paramater it will be
in a compact form, and if not it will be in a verbose format.
The compact for is intended for further processing so each output
value will always be at the same number of bytes from the start of
the line and so "cut -bSTART-END" can be used to extract particular
values. The verbose format is easier for humans to read.

Here are some examples commands:

mythcommflag --skipdb --file /path/my.avi \
             --outputmethod full --outputfile output.txt

mythcommflag --very-quiet --skipdb --file /path/my.avi \
             --outputmethod full --outputfile output.txt

The first will have a header like this:
commercialBreakListFor: /path/my.avi
totalframecount: 53346
  frame     min/max/avg scene aspect format flags mark

And entries like this:
     17468:  11/ 27/ 18  17%  normal normal blank,scene COMM_START
     21592:  16/ 16/ 16   2%  normal normal blank,scene COMM_END

And the second, quiet version will not have a header and will only
have entries like this:
     17468:  11/ 27/ 18  17%  n N  BS    4
     21592:  16/ 16/ 16   2%  n N  BS    5

There will be an entry for every frame in the video in the full
format, so the extra info can be used in another program.

The mininum, maximum and average are values for blank frame detection.
Here is an example of frames from the same video that are not detected
as blanks frames:
        3:   9/255/122  99%  normal normal logo
        4:   9/255/122  99%  normal normal logo
Here on frame 3 and 4 we have the blackest pixel at 9 out of 255,
the brightest pixel is at 255 out of 255 and the average is 122.
We can tell from this that this is a pre-broadcast video, since
otherwise the values would be in the range 16-240 or so. But more
importantly the range is quite large and the average value is far
from both the top an bottom of the range, so this is neither not
a single brightness frame, like the typical all black or sometimes
all white frames used for unstructured cuts or fades.

The next value is the frame likeness percentage. Here we see that
frames 3 and 4 are very similar to the neighboring frames since
the value is high. If we look at a later set of frames we see this:
       392:  10/255/122  95%  normal normal logo
       393:  10/255/122  67%  normal normal scene,logo
       394:   0/239/ 84  90%  normal normal logo

This tells us there was a scene change at frame 393, this is not
flagged as a commercial breakpoint because there was no blanking
and the channel logo was present before, during and after the
scene change. In this case there was a cut from two news hosts
speaking to the news logo animation.

In the compact form of this you see the following:
       392:  10/255/122  95%  n N    L
       393:  10/255/122  67%  n N   SL
       394:   0/239/ 84  90%  n N    L
As you can see the percentage is the same but instead of "scene"
being printed in the flags column, we only see a little "S".
You'll also notice an L in place of the "logo", but we will
talk about the flags column later.

The next column is the "aspect" flag. This can also signal
scene changes, there are two values "normal" and "wide"
or "n" and "w" in the compact form. In most broadcasts
this never changes, since different aspect videos are simply
letterboxed to fit into one frame size. Australia is the
biggest exception to this rule.

The following column is the "format" flag. This is like the
aspect except it checks the letterboxing. This is more likely
to change in most of the world and has four values: "normal",
"letter", "pillar", and "max, or "N","L","P", and "M" in 
compact form. What the value is doesn't matter for most
applications but the change from one format to another 
indicates that a new clip is being played.

The next column is the flag column, and is the most useful for
many applications. In the verbose form this is a comma separated
set of flags that the commercial flagger sets when a threshold
in the detection values has been exceeded. The flags are: 
"skipped", "blank", "scene", "logo", "aspect", and "rating".
In the compact form this is a set of six characters that
are set to a letter, "s","B","S","L",A","R", resp.
So if blank and logo were were set you would get " B L  " in
that column. Skipped indicates that this frame was not actually
processed, but was skipped over and the values show are based
on the previous frame. Blank indicates a blank frame was detected.
Scene indicates a scene change. Logo indicates a logo was
present on the screen. Aspect that the Aspect Ratio Change
was detected. Rating that a US style rating symbol was detected,
but this is currently not checked for due to limited usefulness.

The final column is only filled when the application determines
that a commercial break has started or ended. In the verbose form
You will see either nothing or COMM_START or COMM_END. Indicating
no change, or that commercial start or end was detected. In the
compact form the column will have a nothing or 4 or 5, resp.
