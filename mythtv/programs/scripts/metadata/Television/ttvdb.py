#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: ttvdb.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform TV series data lookups
#   based on information found on the http://thetvdb.com/ website. It
#   follows the MythTV standards set for the movie data
#   lookups. e.g. the perl script "tmdb.pl" used to access themoviedb.com
#   This script uses the python module tvdb_api.py (v1.0 or higher) found at
#   http://pypi.python.org/pypi?%3Aaction=search&term=tvnamer&submit=search
#   thanks to the authors of this excellant module.
#   The tvdb_api.py module uses the full access XML api published by
#   thetvdb.com see:
#     http://thetvdb.com/wiki/index.php?title=Programmers_API
#   Users of this script are encouraged to populate thetvdb.com with TV show
#   information, posters, fan art and banners. The richer the source the more
#   valuable the script.
#   This python script was modified based on the "tvnamer.py" created by
#   "dbr/Ben" who is also
#   the author of the "tvdb_api.py" module. "tvnamer.py" is used to rename avi
#   files with series/episode information found at thetvdb.com
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Verify the command line options (display help or version and exit)
#   2) Verify that thetvdb.com has the series or series_season_ep being
#      requested exit if does not exit
#   3) Find the requested information and send to stdout if any found
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
# -------------------------------------
"""
Doctests

>>> sys.argv = shlex.split('./ttvdb.py -B Sanctuary')
>>> main()
Banner:http://thetvdb.com/banners/graphical/80159-g4.jpg,http://thetvdb.com/banners/graphical/80159-g5.jpg,http://thetvdb.com/banners/graphical/80159-g3.jpg,http://thetvdb.com/banners/graphical/80159-g6.jpg,http://thetvdb.com/banners/graphical/80159-g2.jpg,http://thetvdb.com/banners/graphical/80159-g.jpg,http://thetvdb.com/banners/graphical/80159-g8.jpg
0
>>> sys.argv = shlex.split('./ttvdb.py -S SG-1 1 10')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Stargate SG-1</title>
    <subtitle>Thor's Hammer</subtitle>
    <description>Teal'c and O'Neill are transported to an underground cage designed by the Asgard to protect an alien world from the Goa'uld.</description>
    <season>1</season>
    <episode>10</episode>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Action"/>
      <category type="genre" name="Adventure"/>
      <category type="genre" name="Fantasy"/>
      <category type="genre" name="Science-Fiction"/>
    </categories>
    <studios>
      <studio name="Syfy"/>
    </studios>
    <runtime>45</runtime>
    <inetref>72449</inetref>
    <collectionref>72449</collectionref>
    <imdb>0118480</imdb>
    <tmsref>EP00225421</tmsref>
    <language>en</language>
    <year>1997</year>
    <releasedate>1997-09-26</releasedate>
    <people>
      <person job="Actor" name="Richard Dean Anderson" character="Jack O'Neill" url="http://thetvdb.com/banners/actors/17720.jpg" thumb="http://thetvdb.com/banners/actors/17720.jpg"/>
...
      <person job="Guest Star" name="James Earl Jones"/>
      <person job="Guest Star" name="Galyn Gorg"/>
      <person job="Guest Star" name="Tamsin Kelsey"/>
      <person job="Guest Star" name="Vincent Hammond"/>
      <person job="Guest Star" name="Mark Gibbon"/>
      <person job="Director" name="Brad Turner"/>
      <person job="Author" name="Katharyn Michaelian Powers"/>
    </people>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/72449/85759.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/72449/85759.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72449-1-9.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72449-1-9.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72449-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72449-1.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72449-1-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72449-1-2.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72449-1-8.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72449-1-8.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/185-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/185-1.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72449-55.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72449-55.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72449-34.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72449-34.jpg" width="1280" height="720"/>
...
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72449-75.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72449-75.jpg" width="1280" height="720"/>
    </images>
  </item>
</metadata>
0
>>> sys.argv = shlex.split('ttvdb -PFB "Stargate SG-1"')
>>> main()
Coverart:http://thetvdb.com/banners/posters/72449....jpg
Fanart:http://thetvdb.com/banners/fanart/original/72449-....jpg
Banner:http://thetvdb.com/banners/graphical/72449-....jpg
0

# Coverart:http://www.thetvdb.com/banners/posters/72449-1.jpg
# Fanart:http://www.thetvdb.com/banners/fanart/original/72449-1.jpg
# Banner:http://www.thetvdb.com/banners/graphical/185-g3.jpg
>>> sys.argv = shlex.split('ttvdb -B "Night Gallery"')
>>> main()
Banner:http://thetvdb.com/banners/graphical/70382-g4.jpg,http://thetvdb.com/banners/graphical/1013-g.jpg,http://thetvdb.com/banners/blank/70382.jpg,http://thetvdb.com/banners/graphical/70382-g.jpg,http://thetvdb.com/banners/graphical/70382-g2.jpg,http://thetvdb.com/banners/graphical/70382-g3.jpg
0

# http://www.thetvdb.com/banners/blank/70382.jpg
>>> sys.argv = shlex.split('ttvdb -Bl en Lost')
>>> main()
Banner:http://thetvdb.com/banners/graphical/73739-g4.jpg,http://thetvdb.com/banners/graphical/73739-g13.jpg,http://thetvdb.com/banners/graphical/73739-g18.jpg,http://thetvdb.com/banners/graphical/73739-g6.jpg,http://thetvdb.com/banners/graphical/73739-g12.jpg,http://thetvdb.com/banners/graphical/73739-g3.jpg,http://thetvdb.com/banners/graphical/24313-g2.jpg,http://thetvdb.com/banners/graphical/73739-g8.jpg,http://thetvdb.com/banners/graphical/73739-g.jpg,http://thetvdb.com/banners/graphical/73739-g5.jpg,http://thetvdb.com/banners/graphical/73739-g7.jpg,http://thetvdb.com/banners/graphical/73739-g10.jpg,http://thetvdb.com/banners/graphical/73739-g11.jpg,http://thetvdb.com/banners/graphical/24313-g.jpg,http://thetvdb.com/banners/graphical/73739-g2.jpg,http://thetvdb.com/banners/blank/73739.jpg
0

# Banner:http://www.thetvdb.com/banners/graphical/73739-g4.jpg,http://www.thetvdb.com/banners/graphical/73739-g.jpg,http://www.thetvdb.com/banners/graphical/73739-g6.jpg,http://www.thetvdb.com/banners/graphical/73739-g8.jpg,http://www.thetvdb.com/banners/graphical/73739-g3.jpg,http://www.thetvdb.com/banners/graphical/73739-g7.jpg,http://www.thetvdb.com/banners/graphical/73739-g5.jpg,http://www.thetvdb.com/banners/graphical/24313-g2.jpg,http://www.thetvdb.com/banners/graphical/24313-g.jpg,http://www.thetvdb.com/banners/graphical/73739-g10.jpg,http://www.thetvdb.com/banners/graphical/73739-g2.jpg
> ttvdb -N --configure="/home/user/.tvdb/tvdb.conf" "Eleventh Hour" "H2O"
>>> sys.argv = shlex.split('ttvdb -N --configure=./tvdb_test.conf "Eleventh Hour" H2O')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Eleventh Hour (US)</title>
    <subtitle>H2O</subtitle>
    <description>An epidemic of sudden, violent outbursts by law-abiding citizens draws Dr. Jacob Hood to a quiet Texas community to investigate - but he soon succumbs to the same erratic behavior.</description>
    <season>1</season>
    <episode>10</episode>
    <certifications>
      <certification locale="us" name="TV-14"/>
    </certifications>
    <categories>
      <category type="genre" name="Drama"/>
      <category type="genre" name="Science-Fiction"/>
      <category type="genre" name="Thriller"/>
    </categories>
    <studios>
      <studio name="CBS"/>
    </studios>
    <runtime>45</runtime>
    <inetref>83066</inetref>
    <collectionref>83066</collectionref>
    <imdb>1118697</imdb>
    <language>en</language>
    <year>2009</year>
    <releasedate>2009-01-15</releasedate>
    <people>
      <person job="Actor" name="Rufus Sewell" character="Jacob Hood" url="http://thetvdb.com/banners/actors/78899.jpg" thumb="http://thetvdb.com/banners/actors/78899.jpg"/>
      <person job="Actor" name="Marley Shelton" character="Rachel Young" url="http://thetvdb.com/banners/actors/78898.jpg" thumb="http://thetvdb.com/banners/actors/78898.jpg"/>
      <person job="Actor" name="Omar Benson Miller" character="Felix Lee" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Chris Krauser" character="EMT" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Erica Frene" character="Receptionist" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Lei'lah Star" character="Sick Kid" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Mark C. Baldwin" character="Infomercial Announcer" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Director" name="McDonough"/>
      <person job="Author" name="Kim Newton"/>
    </people>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/83066/416216.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/83066/416216.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/83066-1-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/83066-1-2.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/83066-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/83066-1.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-1.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-3.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-3.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-5.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-5.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-2.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-4.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-4.jpg" width="1280" height="720"/>
    </images>
  </item>
</metadata>
0

#    <language>en</language>
# <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-4.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-4.jpg" width="1280" height="720"/>
# <image type="banner" url="http://www.thetvdb.com/banners/graphical/83066-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/83066-g.jpg"/>
(Return the season numbers for a series)
> ttvdb --configure="./tvdb_test.conf" -n "SG-1"
>>> sys.argv = shlex.split('ttvdb --configure=./tvdb_test.conf -n SG-1')
>>> main()
0,1,2,3,4,5,6,7,8,9,10
0

(Return the meta data for a specific series/season/episode)
> ttvdb.py -D 80159 2 2
>>> sys.argv = shlex.split('ttvdb -D 80159 2 2')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Sanctuary</title>
    <subtitle>End of Nights (2)</subtitle>
    <description>Furious at being duped into a trap, Magnus takes on Kate, demanding information and complete access to her Cabal contacts. The Cabal’s true agenda is revealed and Magnus realizes that they are not only holding Ashley as ransom to obtain complete control of the Sanctuary Network, but turning her into the ultimate weapon. Now transformed into a Super Abnormal with devastating powers, Ashley and her newly cloned fighters begin their onslaught, destroying Sanctuaries in cities around the world. Tesla and Henry attempt to create a weapon that can stop the attacks…without killing Ashley. As the team prepares to defend the Sanctuary with Tesla’s new weapon, Magnus must come to the realization that they may not be able to stop the Cabal’s attacks without harming Ashley. She realizes she might have to choose between saving her only daughter, or losing the Sanctuary and all the lives and secrets within it.</description>
    <season>2</season>
    <episode>2</episode>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Action"/>
      <category type="genre" name="Adventure"/>
      <category type="genre" name="Crime"/>
      <category type="genre" name="Mystery"/>
      <category type="genre" name="Science-Fiction"/>
    </categories>
    <studios>
      <studio name="Space"/>
    </studios>
    <runtime>60</runtime>
    <inetref>80159</inetref>
    <collectionref>80159</collectionref>
    <imdb>0965394</imdb>
    <tmsref>EP01085421</tmsref>
    <language>en</language>
    <year>2009</year>
    <releasedate>2009-10-16</releasedate>
    <people>
      <person job="Actor" name="Amanda Tapping" character="Dr. Helen Magnus" url="http://thetvdb.com/banners/actors/73053.jpg" thumb="http://thetvdb.com/banners/actors/73053.jpg"/>
      <person job="Actor" name="Robin Dunne" character="Will Zimmerman" url="http://thetvdb.com/banners/actors/73054.jpg" thumb="http://thetvdb.com/banners/actors/73054.jpg"/>
      <person job="Actor" name="Emilie Ullerup" character="Ashley Magnus" url="http://thetvdb.com/banners/actors/73055.jpg" thumb="http://thetvdb.com/banners/actors/73055.jpg"/>
      <person job="Actor" name="Christopher Heyerdahl" character="John Druitt" url="http://thetvdb.com/banners/actors/73056.jpg" thumb="http://thetvdb.com/banners/actors/73056.jpg"/>
      <person job="Actor" name="Christopher Heyerdahl" character="Bigfoot" url="http://thetvdb.com/banners/actors/309797.jpg" thumb="http://thetvdb.com/banners/actors/309797.jpg"/>
      <person job="Actor" name="Ryan Robbins" character="Henry Foss" url="http://thetvdb.com/banners/actors/80072.jpg" thumb="http://thetvdb.com/banners/actors/80072.jpg"/>
      <person job="Actor" name="Agam Darshi" character="Kate Freelander" url="http://thetvdb.com/banners/actors/118211.jpg" thumb="http://thetvdb.com/banners/actors/118211.jpg"/>
      <person job="Actor" name="Vincent Gale" character="Nigel Griffin" url="http://thetvdb.com/banners/actors/372548.jpg" thumb="http://thetvdb.com/banners/actors/372548.jpg"/>
      <person job="Actor" name="Peter Wingfield" character="James Watson" url="http://thetvdb.com/banners/actors/372549.jpg" thumb="http://thetvdb.com/banners/actors/372549.jpg"/>
      <person job="Actor" name="Jonathon Young" character="Nikola Tesla" url="http://thetvdb.com/banners/actors/372550.jpg" thumb="http://thetvdb.com/banners/actors/372550.jpg"/>
      <person job="Actor" name="Ian Tracey" character="Adam Worth" url="http://thetvdb.com/banners/actors/372551.jpg" thumb="http://thetvdb.com/banners/actors/372551.jpg"/>
      <person job="Actor" name="Jim Byrnes" character="Gregory Magnus" url="http://thetvdb.com/banners/actors/372552.jpg" thumb="http://thetvdb.com/banners/actors/372552.jpg"/>
      <person job="Actor" name="Polly Walker" character="Ranna Seneschal" url="http://thetvdb.com/banners/actors/372553.jpg" thumb="http://thetvdb.com/banners/actors/372553.jpg"/>
      <person job="Actor" name="Robert Lawrenson" character="Declan Macrae" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Pascale Hutton" character="Abby Corrigan" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Lynda Boyd" character="Dana Whitcomb" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Shekhar Paleja" character="Ravi" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Actor" name="Chuck Campbell" character="Two Faced Guy" url="http://thetvdb.com/banners/" thumb="http://thetvdb.com/banners/"/>
      <person job="Guest Star" name="Jonathon Young"/>
      <person job="Guest Star" name="Christine Chatelain"/>
      <person job="Guest Star" name="Robert Lawrenson"/>
      <person job="Guest Star" name="Maiko Yamamoto"/>
      <person job="Guest Star" name="Stanley Tsang"/>
      <person job="Guest Star" name="Darren A. Hebert"/>
      <person job="Guest Star" name="Lynda Boyd"/>
      <person job="Director" name="Martin Wood"/>
      <person job="Author" name="Damian Kindler"/>
    </people>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/80159/998441.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/80159/998441.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/80159-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/80159-2.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/80159-2-3.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/80159-2-3.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/80159-2-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/80159-2-2.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-10.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-10.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-6.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-6.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-3.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-3.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-9.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-9.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-7.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-7.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-8.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-8.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-2.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-4.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-4.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-5.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-5.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-21.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-21.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-16.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-16.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-1.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-15.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-15.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-17.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-17.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-18.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-18.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-19.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-19.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-20.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-20.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-22.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-22.jpg" width="1920" height="1080"/>
    </images>
  </item>
</metadata>
0

(Return a list of "thetv.com series id and series name" that contain specific search word(s) )
(!! Be careful with this option as poorly defined search words can result in large lists being returned !!)
> ttvdb.py -M "night a"
>>> sys.argv = shlex.split('ttvdb -M "night a"')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>en</language>
    <title>A Night of Numbers</title>
    <inetref>249306</inetref>
    <collectionref>249306</collectionref>
    <description>BBC FOUR celebrates mathematics and the beauty of numbers with a series of programmes about this most precise and exacting of all intellectual disciplines. Throughout the night, the channel will show films that offer insights into the minds of great mathematicians, and reveal the stories behind some of the great mathematical breakthroughs.</description>
    <releasedate>2005-12-06</releasedate>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/249306-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/249306-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A night at The Classic</title>
    <inetref>224951</inetref>
    <collectionref>224951</collectionref>
    <description>Each episode of A Night at The Classic follows MC Brendhan Lovegrove and guest comedians as they perform for a different crowd on a different "night" at The Classic. Along with stand-up comedy recorded in front of a live audience, viewers are given a glimpse of what the comedians are like backstage, providing a rare insight into the rivalries and rituals of stand-up comedians.</description>
    <releasedate>2010-11-03</releasedate>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/224951-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/224951-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A Night at the Rijksmuseum</title>
    <inetref>268908</inetref>
    <collectionref>268908</collectionref>
    <releasedate>2013-04-18</releasedate>
  </item>
  <item>
    <language>en</language>
    <title>A Night of Heroes: The Sun Military Awards</title>
    <inetref>270984</inetref>
    <collectionref>270984</collectionref>
    <description>Annual celebration, A Night of Heroes: Also known as The Millies, the awards recognize the excellence and sacrifice made by Britain's Armed Forces</description>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/270984-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/270984-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A Night of Exploration</title>
    <inetref>271528</inetref>
    <collectionref>271528</collectionref>
    <description>For well over a century the National Geographic Society has been synonymous with pioneering expeditions, groundbreaking discoveries and breathtaking imagery of world cultures and exotic locations. In celebration of the iconic yellow border’s 125th anniversary, National Geographic Channel pays tribute to the hotshots, the mavericks and the best in their field who have devoted their lives to exploring the world around us and the groundbreaking discoveries that are making a difference.</description>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/271528-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/271528-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A Night at the Office</title>
    <inetref>118511</inetref>
    <collectionref>118511</collectionref>
    <description>On August 11th 2009, it was announced that the cast of The Office would be reuniting for a special, called "A Night at The Office", available at BBC2 and online, it was the entire first series of the seminal BBC comedy 'The Office' with new comments from the writers and celebrity fans shown between each episode.</description>
    <releasedate>2009-08-17</releasedate>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/118511-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/118511-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A Night With The Stars</title>
    <inetref>256045</inetref>
    <collectionref>256045</collectionref>
    <description>For one night only, Professor Brian Cox goes unplugged in a specially recorded programme from the lecture theatre of the Royal Institution of Great Britain. In his own inimitable style, Brian takes an audience of famous faces, scientists and members of the public on a journey through some of the most challenging concepts in physics. With the help of Jonathan Ross, Simon Pegg, Sarah Millican and James May, Brian shows how diamonds - the hardest material in nature - are made up of nothingness; how things can be in an infinite number of places at once; why everything we see or touch in the universe exists; and how a diamond in the heart of London is in communication with the largest diamond in the cosmos.</description>
    <releasedate>2011-12-18</releasedate>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/256045-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/256045-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A Night at the Festival Club</title>
    <inetref>268969</inetref>
    <collectionref>268969</collectionref>
    <description>A Night at the Festival Club is an Australian stand-up comedy television event created and executive produced by the Comedy Channel programming director Darren Chau, produced by Ted Robinson and GNW TV Productions for the Comedy Channel as part of the Melbourne International Comedy Festival. The series centres around bottling the unique comedic live performances and moments that occur late night in the Festival Club during the Melbourne International Comedy Festival.</description>
    <releasedate>2008-05-02</releasedate>
  </item>
  <item>
    <language>en</language>
    <title>A Clear Midsummer Night</title>
    <inetref>286538</inetref>
    <collectionref>286538</collectionref>
    <description>The daughter of a real estate mogul Xia Wan Qing, has seemingly no way of retreating after a friend's betrayal and her boyfriend backing out of their wedding. Fortunately, she's saved by business genius Qiao Jin Fan. Jin Fan is a "playboy" and the future successor for Qiao corporation. He extends an offering hand and together they embark on a path of revenge. Each for reasons of their own, begin a love with "uncertain motives." After enduring circumstances because of their families' competing interests and a number of conspiracies the two find true love.</description>
  </item>
  <item>
    <language>en</language>
    <title>A Christmas Night with the Stars</title>
    <inetref>248911</inetref>
    <collectionref>248911</collectionref>
    <description>Christmas Night with the Stars was a television show broadcast each Christmas night by the BBC from 1958 to 1972 (with the exception of 1961, 1965 and 1966) and also revived in 1994. The show was hosted each year by a leading star of BBC TV and featured specially made short seasonal editions (typically about 10 minutes long) of the previous year's most popular BBC sitcoms and light entertainment programs. The show was voted 24th in the Channel 4 100 Greatest Christmas Moments. Most of the variety segments no longer exist.</description>
    <releasedate>1958-12-25</releasedate>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/248911-g2.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/248911-g2.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>A Night With My Ex</title>
    <inetref>331751</inetref>
    <collectionref>331751</collectionref>
    <description>Do you have unfinished business with a partner from a previous relationship? All of the onetime couples featured on ``A Night With My Ex'' do, and the show is letting them tie up loose ends from the past. In each episode, a pair of exes spend a night together in a one-bedroom apartment complete with a multiple-camera setup. They are left to their own devices -- with no producers and no interruptions -- to try to hash things out. The participants get things off their chests, ask hard-hitting questions and face accusations of infidelity with the ultimate goal of achieving closure on the relationship. Sometimes that closure means a clean break, and other times it leads to renewing the spark and rekindling the romance. Regardless of the outcome, anything goes on the road to reaching that point as the couples confront their pasts -- and their futures.</description>
    <releasedate>2017-07-18</releasedate>
  </item>
  <item>
    <language>en</language>
    <title>On a Lustful Night Mingling with a Priest...</title>
    <inetref>325375</inetref>
    <collectionref>325375</collectionref>
    <description>The reunion of Kujo with his old female classmate. He has inherited his parents' temple and became a priest. However, after the two became drunk, he does something unexpected of him to her!</description>
    <releasedate>2017-04-03</releasedate>
  </item>
  <item>
    <language>en</language>
    <title>Love on a Saturday Night</title>
    <inetref>74382</inetref>
    <collectionref>74382</collectionref>
    <releasedate>2004-02-01</releasedate>
  </item>
  <item>
    <language>en</language>
    <title>Britain's Tudor Treasure A Night At Hampton Court</title>
    <inetref>332440</inetref>
    <collectionref>332440</collectionref>
    <description>Lucy Worsley and David Starkey celebrate the 500th anniversary of Britain's finest surviving Tudor building, Hampton Court. As Henry VIII's pleasure palace, Hampton Court was a showcase for royal magnificence and ceremony - and the most important event of all was the christening of Henry's long-awaited son, Prince Edward, on October 15th, 1537. Lucy and David explore how Tudor art, architecture and ritual came together for this momentous occasion. Drawing on historical records and with the help of a team of experts, they recreate key elements of the christening ceremony - including a magnificent set-piece procession through Hampton Court involving nearly 100 people in full Tudor costume.</description>
  </item>
</metadata>
0

(Return TV series collection data of "thetv.com series id" for a specified language)
>>> sys.argv = shlex.split('ttvdb -l de -C 80159')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>de</language>
    <title>Sanctuary</title>
    <network>Space</network>
    <airday>Friday</airday>
    <airtime>10:00 PM</airtime>
    <description>Dr. Helen Magnus ist eine so brillante wie geheimnisvolle Wissenschaftlerin die sich mit den Kreaturen der Nacht beschäftigt. In ihrem Unterschlupf - genannt "Sanctuary" - hat sie ein Team versammelt, das seltsame und furchteinflößende Ungeheuer untersucht, die mit den Menschen auf der Erde leben. Konfrontiert mit ihren düstersten Ängsten und ihren schlimmsten Alpträumen versucht das Sanctuary-Team, die Welt vor den Monstern - und die Monster vor der Welt zu schützen.</description>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Action"/>
      <category type="genre" name="Adventure"/>
      <category type="genre" name="Crime"/>
      <category type="genre" name="Mystery"/>
      <category type="genre" name="Science-Fiction"/>
    </categories>
    <studios>
      <studio name="Space"/>
    </studios>
    <runtime>60</runtime>
    <inetref>80159</inetref>
    <imdb>0965394</imdb>
    <userrating>8.1</userrating>
    <ratingcount>168</ratingcount>
    <year>2007</year>
    <releasedate>2007-03-14</releasedate>
    <lastupdated>...</lastupdated>
    <status>Ended</status>
    <images>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-11.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-11.jpg"/>
      <image type="banner" url="http://thetvdb.com/banners/graphical/80159-g4.jpg" thumb="http://thetvdb.com/banners/_cache/graphical/80159-g4.jpg"/>
    </images>
  </item>
</metadata>
0

# test match is loose due ordering differences between py2 and 3
# i.e. dict key ordering
# key ordering is not sorted so that Title is first for existing client
# compatability
>>> sys.argv = shlex.split('ttvdb -l en -a US -D 281053')
>>> main()
Title:Fixer Upper
Season:0
Episode:1
Subtitle:The Waco Way of Life
Year:2014
ReleaseDate:2014-07-16
Director:
Plot:Chip and Joanna Gaines tell why they love raising a family in Waco, Texas.
UserRating:
Writers:
Screenshot:
Language:en
Airedseasonid:583817
Dvddiscid:
Id:5463514
Imdbid:
Lastupdated:...
Lastupdatedby:447800
Productioncode:
Seriesid:281053
Showurl:
Siterating:0
Siteratingcount:0
Thumbadded:
Thumbauthor:...
Cast:Chip Gaines, Joanna Gaines
Runtime:45
Title:Fixer Upper
...
Coverart:http://thetvdb.com/banners/posters/281053-....jpg
Fanart:http://thetvdb.com/banners/fanart/original/281053-....jpg
Banner:http://thetvdb.com/banners/graphical/281053-....jpg
0

>>> sys.argv = shlex.split('ttvdb.py -l en -a US -N 72108 Pyramid')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>NCIS</title>
    <subtitle>Pyramid</subtitle>
    <description>The lives of NCIS members are in jeopardy when they come face-to-face with the infamous Port-to-Port killer, on the eighth season finale of NCIS.</description>
    <season>8</season>
    <episode>24</episode>
    <certifications>
      <certification locale="us" name="TV-14"/>
    </certifications>
    <categories>
      <category type="genre" name="Action"/>
      <category type="genre" name="Adventure"/>
      <category type="genre" name="Crime"/>
      <category type="genre" name="Drama"/>
    </categories>
    <studios>
      <studio name="CBS"/>
    </studios>
    <runtime>45</runtime>
    <inetref>72108</inetref>
    <collectionref>72108</collectionref>
    <tmsref>EP00681911</tmsref>
    <imdb/>
    <language>en</language>
    <year>2011</year>
    <releasedate>2011-05-17</releasedate>
    <people>
      <person job="Actor" name="Mark Harmon" character="Leroy Jethro Gibbs" url="http://thetvdb.com/banners/actors/70164.jpg" thumb="http://thetvdb.com/banners/actors/70164.jpg"/>
      <person job="Actor" name="Sean Murray" character="Timothy McGee" url="http://thetvdb.com/banners/actors/70163.jpg" thumb="http://thetvdb.com/banners/actors/70163.jpg"/>
      <person job="Actor" name="David McCallum" character="Donald &quot;Ducky&quot; Mallard" url="http://thetvdb.com/banners/actors/70159.jpg" thumb="http://thetvdb.com/banners/actors/70159.jpg"/>
      <person job="Actor" name="Pauley Perrette" character="Abby Sciuto" url="http://thetvdb.com/banners/actors/70161.jpg" thumb="http://thetvdb.com/banners/actors/70161.jpg"/>
      <person job="Actor" name="Rocky Carroll" character="Leon Vance" url="http://thetvdb.com/banners/actors/127861.jpg" thumb="http://thetvdb.com/banners/actors/127861.jpg"/>
      <person job="Actor" name="Brian Dietzen" character="Jimmy Palmer" url="http://thetvdb.com/banners/actors/219761.jpg" thumb="http://thetvdb.com/banners/actors/219761.jpg"/>
      <person job="Actor" name="Wilmer Valderrama" character="Nicholas &quot;Nick&quot; Torres" url="http://thetvdb.com/banners/actors/394895.jpg" thumb="http://thetvdb.com/banners/actors/394895.jpg"/>
      <person job="Actor" name="Michael Weatherly" character="Anthony &quot;Tony&quot; DiNozzo" url="http://thetvdb.com/banners/actors/70160.jpg" thumb="http://thetvdb.com/banners/actors/70160.jpg"/>
      <person job="Actor" name="Sasha Alexander" character="Caitlin &quot;Kate&quot; Todd" url="http://thetvdb.com/banners/actors/70162.jpg" thumb="http://thetvdb.com/banners/actors/70162.jpg"/>
      <person job="Actor" name="Cote de Pablo" character="Ziva David" url="http://thetvdb.com/banners/actors/70165.jpg" thumb="http://thetvdb.com/banners/actors/70165.jpg"/>
      <person job="Actor" name="Lauren Holly" character="Jennifer &quot;Jenny&quot; Shepard" url="http://thetvdb.com/banners/actors/77850.jpg" thumb="http://thetvdb.com/banners/actors/77850.jpg"/>
      <person job="Actor" name="Jennifer Esposito" character="Alex Quinn" url="http://thetvdb.com/banners/actors/402126.jpg" thumb="http://thetvdb.com/banners/actors/402126.jpg"/>
      <person job="Actor" name="Emily Wickersham" character="Ellie Bishop" url="http://thetvdb.com/banners/actors/321201.jpg" thumb="http://thetvdb.com/banners/actors/321201.jpg"/>
      <person job="Actor" name="Duane Henry" character="Clayton Reeves" url="http://thetvdb.com/banners/actors/418530.jpg" thumb="http://thetvdb.com/banners/actors/418530.jpg"/>
      <person job="Actor" name="Maria Bello" character="Jacqueline “Jack” Sloane" url="http://thetvdb.com/banners/actors/461645.jpg" thumb="http://thetvdb.com/banners/actors/461645.jpg"/>
      <person job="Guest Star" name="Kerr Smith"/>
      <person job="Guest Star" name="Sarah Jane Morris"/>
      <person job="Guest Star" name="Matt Craven"/>
      <person job="Guest Star" name="David Dayan Fisher"/>
      <person job="Guest Star" name="Muse Watson"/>
      <person job="Guest Star" name="Alimi Ballard"/>
      <person job="Guest Star" name="Matthew Willig"/>
      <person job="Guest Star" name="Tehmina Sunny"/>
      <person job="Guest Star" name="Enrique Murciano"/>
      <person job="Guest Star" name="Jude Ciccolella"/>
      <person job="Guest Star" name="Vera Miao"/>
      <person job="Director" name="Dennis Smith"/>
      <person job="Author" name="Gary Glasberg"/>
    </people>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/72108/4078484.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/72108/4078484.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72108-8-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72108-8-2.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72108-8.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72108-8.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/72108-8-7.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/72108-8-7.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72108-20.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72108-20.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72108-28.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72108-28.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72108-29.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72108-29.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72108-31.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72108-31.jpg" width="1920" height="1080"/>
...
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/72108-33.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/72108-33.jpg" width="1920" height="1080"/>
    </images>
  </item>
</metadata>
0

>>> sys.argv = shlex.split('ttvdb.py -l en -a US -N 283661 "Egg Hunt"')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Jack Hanna's Wild Countdown</title>
    <subtitle>Egg Hunt</subtitle>
    <description>Jungle Jack takes off on a very special Egg Hunt, looking for creatures big and small that hatch from eggs! Crocodiles, Bald Eagles, Sea Turtles, Ostriches, Penguins, and more!</description>
    <season>6</season>
    <episode>17</episode>
    <certifications>
      <certification locale="us" name="TV-G"/>
    </certifications>
    <categories>
      <category type="genre" name="Family"/>
      <category type="genre" name="Special Interest"/>
    </categories>
    <studios>
      <studio name="ABC (US)"/>
    </studios>
    <runtime>30</runtime>
    <inetref>283661</inetref>
    <collectionref>283661</collectionref>
    <imdb>3062384</imdb>
    <tmsref>EP01441760</tmsref>
    <language>en</language>
    <year>2017</year>
    <releasedate>2017-04-15</releasedate>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/283661/6050716.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/283661/6050716.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/posters/283661-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/posters/283661-1.jpg" width="680" height="1000"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/283661-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/283661-1.jpg" width="1920" height="1080"/>
    </images>
  </item>
</metadata>
0

>>> sys.argv = shlex.split('ttvdb.py -l en -a US -N 76119 "Eclipse Over America"')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>NOVA</title>
    <subtitle>Eclipse Over America</subtitle>
    <description>...</description>
    <season>44</season>
    <episode>11</episode>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Documentary"/>
    </categories>
    <studios>
      <studio name="PBS"/>
    </studios>
    <runtime>60</runtime>
    <inetref>76119</inetref>
    <collectionref>76119</collectionref>
    <imdb>0206501</imdb>
    <tmsref>EP00003163</tmsref>
    <language>en</language>
    <year>2017</year>
    <releasedate>2017-08-21</releasedate>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/76119/....jpg" thumb="http://thetvdb.com/banners/_cache/episodes/76119/....jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/76119-44.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/76119-44.jpg"/>
      <image type="coverart" url="http://www.thetvdb.com/banners/seasons/76119-44-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/seasons/76119-44-2.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/76119-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/76119-1.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/76119-2.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/76119-2.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/76119-3.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/76119-3.jpg" width="1280" height="720"/>
    </images>
  </item>
</metadata>
0

>>> sys.argv = shlex.split('ttvdb.py -l en -a GB -C 330432')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>en</language>
    <title>Nine Minute Ninja</title>
    <network>CBBC</network>
    <description>Art Ninja spinoff. Ricky makes art in nine minutes.</description>
    <categories>
      <category type="genre" name="Children"/>
    </categories>
    <studios>
      <studio name="CBBC"/>
    </studios>
    <runtime>10</runtime>
    <inetref>330432</inetref>
    <ratingcount>0</ratingcount>
    <year>2015</year>
    <releasedate>2015-06-24</releasedate>
    <lastupdated>...</lastupdated>
    <status>Continuing</status>
    <images>
      <image type="coverart" url="http://www.thetvdb.com/banners/posters/330432-1.jpg" thumb="http://www.thetvdb.com/banners/_cache/posters/330432-1.jpg"/>
    </images>
  </item>
</metadata>
0

>>> sys.argv = shlex.split('ttvdb.py -l de -a DE -D 282022 1 1')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Wilder Westen</title>
    <subtitle>Gebirge</subtitle>
    <description>Der Wilde Westen. ...</description>
    <season>1</season>
    <episode>1</episode>
    <categories>
      <category type="genre" name="Documentary"/>
    </categories>
    <studios>
      <studio name="BBC Four"/>
    </studios>
    <runtime>60</runtime>
    <inetref>282022</inetref>
    <collectionref>282022</collectionref>
    <language>de</language>
    <year>2014</year>
    <releasedate>2014-05-22</releasedate>
    <people>
      <person job="Actor" name="Ray Mears" character="Presenter" url="http://thetvdb.com/banners/actors/330506.jpg" thumb="http://thetvdb.com/banners/actors/330506.jpg"/>
      <person job="Director" name="Phil Coles"/>
    </people>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/282022/4890147.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/282022/4890147.jpg"/>
    </images>
  </item>
</metadata>
0

>>> sys.argv = shlex.split('ttvdb.py -l de -a DE -D 282022')
>>> main()
Title:Wilder Westen
Season:1
Episode:1
Subtitle:Gebirge
Year:2014
ReleaseDate:2014-05-22
Director:Phil Coles
Plot:Der Wilde Westen. Die Vorstellung ...
UserRating:
Writers:
Screenshot:http://thetvdb.com/banners/episodes/282022/4890147.jpg
Language:de
Absolutenumber:1
Airedseasonid:586419
Dvddiscid:
Id:4890147
Imdbid:
...
Productioncode:b044jl70
Seriesid:282022
Showurl:
Siterating:0
Siteratingcount:0
...
Cast:Ray Mears
Runtime:60
Title:Wilder Westen
Season:1
Episode:2
Subtitle:Great Plains
Year:2014
ReleaseDate:2014-05-29
Director:Phil Coles
Plot:Die Great Plains,...
UserRating:
Writers:
Screenshot:http://thetvdb.com/banners/episodes/282022/4890148.jpg
Language:de
Absolutenumber:2
Airedseasonid:586419
Dvddiscid:
Id:4890148
Imdbid:
...
Productioncode:b044z1k0
Seriesid:282022
Showurl:
Siterating:0
Siteratingcount:0
...
Cast:Ray Mears
Runtime:60
Title:Wilder Westen
Season:1
Episode:3
Subtitle:W...
Year:2014
ReleaseDate:2014-06-05
Director:
Plot:
UserRating:
Writers:
Screenshot:http://thetvdb.com/banners/episodes/282022/4890149.jpg
Language:de
Absolutenumber:3
Airedseasonid:586419
Dvddiscid:
Id:4890149
Imdbid:
...
Productioncode:b045nz9q
Seriesid:282022
Showurl:
Siterating:0
Siteratingcount:0
...
Cast:Ray Mears
Runtime:60
0

>>> sys.argv = shlex.split('ttvdb.py -l de -a DE -N 78804 10 1')
>>> main()
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Doctor Who (2005)</title>
    <subtitle>42</subtitle>
    <description>Der Doktor und Martha Jones betätigen sich als interplanetare Verkehrswacht - und eilen der Besatzung eines Raumfrachters zu Hilfe, der steuerlos auf einen Stern zutreibt. Den beiden bleiben nur 42 Minuten, bevor das Schiff in der Hitze der Sonne verglüht. Als wäre dieses Zeitlimit noch nicht schlimm genug, bekommen sie es auch mit einer mysteriösen Macht zu tun, die den Crewmitgliedern nach dem Leben trachtet...</description>
    <season>3</season>
    <episode>7</episode>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Adventure"/>
      <category type="genre" name="Drama"/>
      <category type="genre" name="Science-Fiction"/>
    </categories>
    <studios>
      <studio name="BBC One"/>
    </studios>
    <runtime>45</runtime>
    <inetref>78804</inetref>
    <collectionref>78804</collectionref>
    <imdb>0436992</imdb>
    <tmsref>EP00750178</tmsref>
    <language>de</language>
    <year>2007</year>
    <releasedate>2007-05-19</releasedate>
    <people>
      <person job="Actor" name="Jodie Whitaker" character="Thirteenth Doctor" url="http://thetvdb.com/banners/actors/451936.jpg" thumb="http://thetvdb.com/banners/actors/451936.jpg"/>
      <person job="Actor" name="Peter Capaldi" character="Twelfth Doctor" url="http://thetvdb.com/banners/actors/333133.jpg" thumb="http://thetvdb.com/banners/actors/333133.jpg"/>
      <person job="Actor" name="Matt Smith" character="Eleventh Doctor" url="http://thetvdb.com/banners/actors/129621.jpg" thumb="http://thetvdb.com/banners/actors/129621.jpg"/>
      <person job="Actor" name="David Tennant" character="Tenth Doctor" url="http://thetvdb.com/banners/actors/67922.jpg" thumb="http://thetvdb.com/banners/actors/67922.jpg"/>
      <person job="Actor" name="Billie Piper" character="Rose Tyler" url="http://thetvdb.com/banners/actors/67923.jpg" thumb="http://thetvdb.com/banners/actors/67923.jpg"/>
      <person job="Actor" name="Freema Agyeman" character="Martha Jones" url="http://thetvdb.com/banners/actors/67924.jpg" thumb="http://thetvdb.com/banners/actors/67924.jpg"/>
      <person job="Actor" name="John Barrowman" character="Jack Harkness" url="http://thetvdb.com/banners/actors/67925.jpg" thumb="http://thetvdb.com/banners/actors/67925.jpg"/>
      <person job="Actor" name="Dan Starkey" character="Strax" url="http://thetvdb.com/banners/actors/343990.jpg" thumb="http://thetvdb.com/banners/actors/343990.jpg"/>
      <person job="Actor" name="Catrin Stewart" character="Jenny Flint" url="http://thetvdb.com/banners/actors/343989.jpg" thumb="http://thetvdb.com/banners/actors/343989.jpg"/>
      <person job="Actor" name="Neve McIntosh" character="Madame Vastra" url="http://thetvdb.com/banners/actors/343988.jpg" thumb="http://thetvdb.com/banners/actors/343988.jpg"/>
      <person job="Actor" name="Christopher Eccleston" character="Ninth Doctor" url="http://thetvdb.com/banners/actors/77724.jpg" thumb="http://thetvdb.com/banners/actors/77724.jpg"/>
      <person job="Actor" name="Catherine Tate" character="Donna Noble" url="http://thetvdb.com/banners/actors/77751.jpg" thumb="http://thetvdb.com/banners/actors/77751.jpg"/>
      <person job="Actor" name="John Simm" character="The Master" url="http://thetvdb.com/banners/actors/79737.jpg" thumb="http://thetvdb.com/banners/actors/79737.jpg"/>
      <person job="Actor" name="Karen Gillan" character="Amy Pond" url="http://thetvdb.com/banners/actors/129611.jpg" thumb="http://thetvdb.com/banners/actors/129611.jpg"/>
      <person job="Actor" name="Arthur Darvill" character="Rory Williams" url="http://thetvdb.com/banners/actors/200011.jpg" thumb="http://thetvdb.com/banners/actors/200011.jpg"/>
      <person job="Actor" name="Alex Kingston" character="River Song" url="http://thetvdb.com/banners/actors/286535.jpg" thumb="http://thetvdb.com/banners/actors/286535.jpg"/>
      <person job="Actor" name="Jenna Coleman" character="Clara Oswald" url="http://thetvdb.com/banners/actors/305854.jpg" thumb="http://thetvdb.com/banners/actors/305854.jpg"/>
      <person job="Actor" name="Bernard Cribbins" character="Wilfred Mott" url="http://thetvdb.com/banners/actors/308282.jpg" thumb="http://thetvdb.com/banners/actors/308282.jpg"/>
      <person job="Actor" name="Camille Coduri" character="Jackie Tyler" url="http://thetvdb.com/banners/actors/337609.jpg" thumb="http://thetvdb.com/banners/actors/337609.jpg"/>
      <person job="Actor" name="Noel Clarke" character="Mickey Smith" url="http://thetvdb.com/banners/actors/337610.jpg" thumb="http://thetvdb.com/banners/actors/337610.jpg"/>
      <person job="Actor" name="Samuel Anderson" character="Danny Pink" url="http://thetvdb.com/banners/actors/339332.jpg" thumb="http://thetvdb.com/banners/actors/339332.jpg"/>
      <person job="Actor" name="Michelle Gomez" character="Missy" url="http://thetvdb.com/banners/actors/356014.jpg" thumb="http://thetvdb.com/banners/actors/356014.jpg"/>
      <person job="Actor" name="Jemma Redgrave" character="Kate Stewart" url="http://thetvdb.com/banners/actors/385486.jpg" thumb="http://thetvdb.com/banners/actors/385486.jpg"/>
      <person job="Actor" name="Matt Lucas" character="Nardole" url="http://thetvdb.com/banners/actors/418731.jpg" thumb="http://thetvdb.com/banners/actors/418731.jpg"/>
      <person job="Actor" name="Pearl Mackie" character="Bill Potts" url="http://thetvdb.com/banners/actors/418730.jpg" thumb="http://thetvdb.com/banners/actors/418730.jpg"/>
      <person job="Guest Star" name="Rebecca Oldfield"/>
      <person job="Guest Star" name="Gary Powell"/>
      <person job="Guest Star" name="Vinette Robinson"/>
      <person job="Guest Star" name="Matthew Chambers"/>
      <person job="Guest Star" name="William Ash"/>
      <person job="Guest Star" name="Anthony Flanagan"/>
      <person job="Guest Star" name="Michelle Collins"/>
      <person job="Guest Star" name="Adjoa Andoh"/>
      <person job="Guest Star" name="Elize Du Toit"/>
      <person job="Director" name="Graeme Harper"/>
      <person job="Author" name="Chris Chibnall"/>
    </people>
    <images>
      <image type="screenshot" url="http://thetvdb.com/banners/episodes/78804/327376.jpg" thumb="http://thetvdb.com/banners/_cache/episodes/78804/327376.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-54.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-54.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-61.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-61.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-155.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-155.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-122.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-122.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-60.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-60.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-93.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-93.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-59.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-59.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-64.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-64.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-148.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-148.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-13.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-13.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-125.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-125.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-73.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-73.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-85.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-85.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-115.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-115.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-113.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-113.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-143.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-143.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-116.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-116.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-62.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-62.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-114.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-114.jpg" width="1280" height="720"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-117.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-117.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-144.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-144.jpg" width="1920" height="1080"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/78804-63.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/78804-63.jpg" width="1280" height="720"/>
    </images>
  </item>
</metadata>
0

"""
from __future__ import print_function

__title__ ="TheTVDB.com";
__author__="R.D.Vaughan"
__version__="2.0.0"

# Version .1    Initial development
# Version .2    Add an option to get season and episode numbers from ep name
# Version .3    Cleaned up the documentation and added a usage display option
# Version .4    Added override formating of the number option (-N)
# Version .5    Added a check that tvdb_api.py dependancies are installed
# Version .6    Added -M Series List functionality
# Version .7    Added -o Series name override file functionality
# Version .8    Removed a dependancy, fixed caching for multiple users and
#               used better method of supporting the -M option with tvdb_api
# Version .8.1  Cleaned up documentation errors.
# Version .8.2  Added the series name to output of meta data -D
# Version .8.3  Patched tv_api so even episode image is fully qualified URL
# Version .8.4  Fixed bug in -N when multiple episodes are returned from
#               a search on episode name.
# Version .8.5  Made option -N more flexible in finding a matching episode
# Version .8.6  Add option -t for top rated graphics (-P,B,F) for a series
#               Add option -l to filter graphics on the local language
#               Add season level granularity to Poster and Banner URLs
#               Changed the override file location to be specified on the
#               command line along with the option -o.
#               Increased the amount of massaging of episode data to improve
#               compatiblilty with data bases.
#               Changed the default season episode number format to SxxExx
#               Add an option (-n) to return the season numbers for a series
#               Added passing either a thetvdb.com SID (series identifcation
#               number) or series name for all functions except -M list.
#               Now ALL available episode meta data is returned. This
#               includes meta data that MythTv DB does not currently store.
#               Meta data 'Year' now derived from year in release date.
# Version .8.7  Fixed a bug with unicode meta data handling
# Version .8.8  Replaced the old configuration file with a true conf file
# Version .8.9  Add option -m to better conform to mythvideo standards
# Version .9.0  Now when a season level Banner is not found then the
#               top rated series level graphics is returned instead. This
#               feature had previously been available for posters only.
#               Add runtime to episode meta data (-D). It is always the
#               same for each episode as the information is only available
#               from the series itself.
#               Added the TV Series cast members as part of episode
#               meta data. Option (-D).
#               Added TV Series series genres as part of episode
#               meta data. Option (-D).
#               Resync with tvdb_api getting bug fixes and new features.
#               Add episode data download to a specific language see
#               -l 'es' option. If there is no data for a languages episode
#               then any engish episode data is returned. English is default.
#               The -M is still only in English. Waiting on tvdb_api fix.
# Version .9.1  Bug fix stdio abort when no genre exists for TV series
# Version .9.2  Bug fix stdio abort when no cast exists for TV series
# Version .9.3  Changed option -N when episodes partially match each
#               combination of season/episode numbers are returned. This was
#               added to deal with episodes which have a "(1)" trailing
#               the episode name. An episode in more than one part.
# Version .9.4  Option -M can now list multi-language TV Series
# Version .9.5  "Director" metadata field returned as "None" is changed to
#               "Unknown".
#               File name parsing was changed to use multi-language capable
#               regex patterns
# Version .9.6  Synced up to the 1.0 release of tvdb_api
#               Added a tvdb_api version check and abort if not at least v1.0
#               Changed to new tvdb_api's method of assigning the tvdb api key
# Version .9.7  Account for TVDB increasing the number of digits in their
#               SID number (now greater then 5
#               e,g, "Defying Gravity" is SID 104581)
# Version .9.8  Added a (-S) option for requesting a thetvdb
#               episode screen shot
# Version .9.9  Fixed the -S option when NO episode image exists
# Version 1.0.  Removed LF and replace with a space for all TVDB metatdata
#               fields
# Version 1.0.1 Return all graphics (series and season) in the order
#               highest to lowest as rated by users
# Version 1.0.2 Added better error messages to config file checking. Updated to
#               v1.0.2 tvdb_api which contained fixes for concurrent instances
#               of ttvdb.py generated by MythVideo.
# Version 1.0.3 Conform to new -D standards which return all graphics URLs along
#               with text meta data. Also Posters, Banners and Fan art have one or
#               comma separated URLs as one continuous string.
# Version 1.0.4 Poster Should be Coverart instead.
# Version 1.0.5 Added the TVDB URL to the episode metadata
# Version 1.0.6 When the language is invalid ignore language and continue processing
#               Changed all exit codes to be 0 for passed and 1 for failed
#               Removed duplicates when using the -M option and the -l option
# Version 1.0.7 Change over to the installed TTVDB api library
#               Fixed cast name bad data abort reported in ticket #7957
#               Fixed ticket #7900 - There is now fall back graphics when a
#               language is specified and there is no images for that language.
#               New default location and name of the ttvdb.conf file is '~/.mythtv/ttvdb.conf'. The command
#               line option "-c" can still override the default location and name.
# Version 1.0.8 Removed any stderr messages for non-critical events. They cause dual pop-ups in MythVideo.
# Version 1.0.9 Removed another stderr messages when a non-supported language code is passed.
# Version 1.1.0 Added support for XML output. See:
#               http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
# Version 1.1.1 Make XML output the default.
# Version 1.1.2 Convert version information to XML
# Version 1.1.3 Implement fuzzy matching for episode name lookup
# Version 1.1.4 Add test mode (replaces --toprated)
# Version 1.1.5 Add the -C (collection option) with corresponding XML output
#               and add a <collectionref> XML tag to Search and Query XML output
# Version 1.1.6 Honor series name overrides during TV series search
# Version 2.0.0 Update to API V2

usage_txt='''
Usage: ttvdb.py usage: ttvdb -hdruviomMPFBDSC [parameters]
 <series name or 'series and season number' or 'series and season number and episode number'>

For details on using ttvdb with Mythvideo see the ttvdb wiki page at:
http://www.mythtv.org/wiki/Ttvdb.py

Options:
  -h, --help            show this help message and exit
  -d, --debug           Show debugging info
  -r, --raw             Dump raw data only
  -u, --usage           Display examples for executing the ttvdb script
  -v, --version         Display version and author
  -t                    Test mode, to check for installed dependencies
  -i, --interactive     Interaction mode (allows selection of a specific
                        Series)
  -c FILE, --configure=FILE
                        Use configuration settings
  -a COUNTRY, --area=COUNTRY
			Select data that matches the specified country
  -l LANGUAGE, --language=LANGUAGE
                        Select data that matches the specified language fall
                        back to english if nothing found (e.g. 'es' Español,
                        'de' Deutsch ... etc)
  -n, --num_seasons     Return the season numbers for a series
  -m, --mythvideo       Conform to mythvideo standards when processing -M, -P,
                        -F and -D
  -M, --list            Get matching TV Series list
  -P, --poster          Get Series Poster URL(s)
  -F, --fanart          Get Series fan art URL(s)
  -B, --backdrop        Get Series banner/backdrop URL(s)
  -S, --screenshot      Get Series episode screenshot URL
  -D, --data            Get Series episode data
  -N, --numbers         Get Season and Episode numbers
  -C, --collection      Get A TV Series (collection) series specific information

Command examples:
(Return the banner graphics for a series)
> ttvdb -B "Sanctuary"
Banner:http://www.thetvdb.com/banners/graphical/80159-g2.jpg,http://www.thetvdb.com/banners/graphical/80159-g3.jpg,http://www.thetvdb.com/banners/graphical/80159-g.jpg

(Return the banner graphics for a Series specific to a season)
> ttvdb -B "SG-1" 1
Banner:http://www.thetvdb.com/banners/graphical/72449-g2.jpg,http://www.thetvdb.com/banners/graphical/185-g3.jpg,http://www.thetvdb.com/banners/graphical/185-g2.jpg,http://www.thetvdb.com/banners/graphical/185-g.jpg,http://www.thetvdb.com/banners/graphical/72449-g.jpg,http://www.thetvdb.com/banners/text/185.jpg

(Return the screen shot graphic for a Series Episode)
> ttvdb -S "SG-1" 1 10
http://www.thetvdb.com/banners/episodes/72449/85759.jpg

(Return the banner graphics for a SID (series ID) specific to a season)
(SID "72449" is specific for the series "SG-1")
> ttvdb -B 72449 1
Banner:http://www.thetvdb.com/banners/graphical/72449-g2.jpg,http://www.thetvdb.com/banners/graphical/185-g3.jpg,http://www.thetvdb.com/banners/graphical/185-g2.jpg,http://www.thetvdb.com/banners/graphical/185-g.jpg,http://www.thetvdb.com/banners/graphical/72449-g.jpg,http://www.thetvdb.com/banners/text/185.jpg

(Return the banner graphics for a file name)
> ttvdb -B "Stargate SG-1 - S08E03 - Lockdown"
Banner:http://www.thetvdb.com/banners/graphical/72449-g2.jpg,http://www.thetvdb.com/banners/graphical/185-g3.jpg,http://www.thetvdb.com/banners/graphical/185-g2.jpg,http://www.thetvdb.com/banners/graphical/185-g.jpg,http://www.thetvdb.com/banners/graphical/72449-g.jpg,http://www.thetvdb.com/banners/text/185.jpg

(Return the posters, banners and fan art for a series)
> ttvdb -PFB "Sanctuary"
Coverart:http://www.thetvdb.com/banners/posters/80159-2.jpg,http://www.thetvdb.com/banners/posters/80159-1.jpg
Fanart:http://www.thetvdb.com/banners/fanart/original/80159-2.jpg,http://www.thetvdb.com/banners/fanart/original/80159-1.jpg,http://www.thetvdb.com/banners/fanart/original/80159-8.jpg,http://www.thetvdb.com/banners/fanart/original/80159-6.jpg,http://www.thetvdb.com/banners/fanart/original/80159-5.jpg,http://www.thetvdb.com/banners/fanart/original/80159-9.jpg,http://www.thetvdb.com/banners/fanart/original/80159-3.jpg,http://www.thetvdb.com/banners/fanart/original/80159-7.jpg,http://www.thetvdb.com/banners/fanart/original/80159-4.jpg
Banner:http://www.thetvdb.com/banners/graphical/80159-g2.jpg,http://www.thetvdb.com/banners/graphical/80159-g3.jpg,http://www.thetvdb.com/banners/graphical/80159-g.jpg

(Return thetvdb.com's top rated poster, banner and fan art for a TV Series)
(NOTE: If there is no graphic for a type or any graphics at all then those types are not returned)
> ttvdb -tPFB "Stargate SG-1"
Coverart:http://www.thetvdb.com/banners/posters/72449-1.jpg
Fanart:http://www.thetvdb.com/banners/fanart/original/72449-1.jpg
Banner:http://www.thetvdb.com/banners/graphical/185-g3.jpg
> ttvdb -tB "Night Gallery"
http://www.thetvdb.com/banners/blank/70382.jpg

(Return graphics only matching the local language for a TV series)
(In this case banner 73739-g9.jpg is not included because it does not match the language 'en')
> ttvdb -Bl en "Lost"
Banner:http://www.thetvdb.com/banners/graphical/73739-g4.jpg,http://www.thetvdb.com/banners/graphical/73739-g.jpg,http://www.thetvdb.com/banners/graphical/73739-g6.jpg,http://www.thetvdb.com/banners/graphical/73739-g8.jpg,http://www.thetvdb.com/banners/graphical/73739-g3.jpg,http://www.thetvdb.com/banners/graphical/73739-g7.jpg,http://www.thetvdb.com/banners/graphical/73739-g5.jpg,http://www.thetvdb.com/banners/graphical/24313-g2.jpg,http://www.thetvdb.com/banners/graphical/24313-g.jpg,http://www.thetvdb.com/banners/graphical/73739-g10.jpg,http://www.thetvdb.com/banners/graphical/73739-g2.jpg

(Return a season and episode numbers using the override file to identify the series as the US version)
> ttvdb -N --configure="/home/user/.tvdb/tvdb.conf" "Eleventh Hour" "H2O"
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Eleventh Hour (US)</title>
    <subtitle>H2O</subtitle>
    <language>en</language>
    <description>An epidemic of sudden, violent outbursts by law-abiding citizens draws Dr. Jacob Hood to a quiet Texas community to investigate - but he soon succumbs to the same erratic behavior.</description>
    <season>1</season>
    <episode>10</episode>
...
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/83066-4.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/83066-4.jpg" width="1280" height="720"/>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/83066-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/83066-g.jpg"/>
    </images>
  </item>
</metadata>

(Return the season numbers for a series)
> ttvdb --configure="/home/user/.tvdb/tvdb.conf" -n "SG-1"
0,1,2,3,4,5,6,7,8,9,10

(Return the meta data for a specific series/season/episode)
> ttvdb.py -D 80159 2 2
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Sanctuary</title>
    <subtitle>End of Nights (2)</subtitle>
    <language>en</language>
    <description>Furious at being duped into a trap, Magnus (AMANDA TAPPING) takes on Kate (AGAM DARSHI), demanding information and complete access to her Cabal contacts. The Cabal’s true agenda is revealed and Magnus realizes that they are not only holding Ashley (EMILIE ULLERUP) as ransom to obtain complete control of the Sanctuary Network, but turning her into the ultimate weapon. Now transformed into a Super Abnormal with devastating powers, Ashley and her newly cloned fighters begin their onslaught, destroying Sanctuaries in cities around the world. Tesla (JONATHON YOUNG) and Henry (RYAN ROBBINS) attempt to create a weapon that can stop the attacks…without killing Ashley. As the team prepares to defend the Sanctuary with Tesla’s new weapon, Magnus must come to the realization that they may not be able to stop the Cabal’s attacks without harming Ashley. She realizes she might have to choose between saving her only daughter, or losing the Sanctuary and all the lives and secrets within it.</description>
    <season>2</season>
    <episode>2</episode>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Action and Adventure"/>
      <category type="genre" name="Science-Fiction"/>
    </categories>
    <studios>
      <studio name="SciFi"/>
    </studios>
...
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/80159-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/80159-g.jpg"/>
    </images>
  </item>
</metadata>

(Return a list of "thetv.com series id and series name" that contain specific search word(s) )
(!! Be careful with this option as poorly defined search words can result in large lists being returned !!)
> ttvdb.py -M "night a"
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>en</language>
    <title>Love on a Saturday Night</title>
    <inetref>74382</inetref>
    <releasedate>2004-02-01</releasedate>
  </item>
  <item>
    <language>en</language>
    <title>A Night on Mount Edna</title>
    <inetref>108281</inetref>
  </item>
  <item>
    <language>en</language>
    <title>A Night at the Office</title>
    <inetref>118511</inetref>
    <description>On August 11th 2009, it was announced that the cast of The Office would be reuniting for a special, called "A Night at The Office", available at BBC2 and online.</description>
    <releasedate>2009-01-01</releasedate>
    <images>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/118511-g.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/118511-g.jpg"/>
    </images>
  </item>
  <item>
    <language>en</language>
    <title>Star For A Night</title>
    <inetref>71476</inetref>
    <releasedate>1999-01-01</releasedate>
  </item>
</metadata>

(Return TV series collection data of "thetv.com series id" for a specified language)
> ttvdb.py -l de -C 80159
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>de</language>
    <title>Sanctuary</title>
    <network>Syfy</network>
    <airday>Friday</airday>
    <airtime>22:00</airtime>
    <description>Dr. Helen Magnus ist eine so brillante wie geheimnisvolle Wissenschaftlerin die sich mit den Kreaturen der Nacht beschäftigt. In ihrem Unterschlupf - genannt "Sanctuary" - hat sie ein Team versammelt, das seltsame und furchteinflößende Ungeheuer untersucht, die mit den Menschen auf der Erde leben. Konfrontiert mit ihren düstersten Ängsten und ihren schlimmsten Alpträumen versucht das Sanctuary-Team, die Welt vor den Monstern - und die Monster vor der Welt zu schützen.</description>
    <certifications>
      <certification locale="us" name="TV-PG"/>
    </certifications>
    <categories>
      <category type="genre" name="Action and Adventure"/>
      <category type="genre" name="Science-Fiction"/>
    </categories>
    <studios>
      <studio name="Syfy"/>
    </studios>
    <runtime>60</runtime>
    <inetref>80159</inetref>
    <imdb>0965394</imdb>
    <tmsref>EP01085421</tmsref>
    <userrating>8.0</userrating>
    <ratingcount>128</ratingcount>
    <year>2007</year>
    <releasedate>2007-05-14</releasedate>
    <lastupdated>Fri, 17 Feb 2012 16:57:02 GMT</lastupdated>
    <status>Continuing</status>
    <images>
      <image type="coverart" url="http://www.thetvdb.com/banners/posters/80159-4.jpg" thumb="http://www.thetvdb.com/banners/_cache/posters/80159-4.jpg"/>
      <image type="fanart" url="http://www.thetvdb.com/banners/fanart/original/80159-8.jpg" thumb="http://www.thetvdb.com/banners/_cache/fanart/original/80159-8.jpg"/>
      <image type="banner" url="http://www.thetvdb.com/banners/graphical/80159-g6.jpg" thumb="http://www.thetvdb.com/banners/_cache/graphical/80159-g6.jpg"/>
    </images>
  </item>
</metadata>
'''

# Episode keys that can be used in a episode data/information search.
# All keys are currently being used.
'''
'episodenumber'
'rating'
'overview'
'dvd_episodenumber'
'dvd_discid'
'combined_episodenumber'
'epimgflag'
'id'
'seasonid'
'seasonnumber'
'writer'
'lastupdated'
'filename'
'absolute_number'
'combined_season'
'imdb_id'
'director'
'dvd_chapter'
'dvd_season'
'gueststars'
'seriesid'
'language'
'productioncode'
'firstaired'
'episodename'
'''

# System modules
import sys, os, re
from optparse import OptionParser
from copy import deepcopy
# shlex for doctest
import shlex

# import logging
# logger = logging.getLogger()
# ch = logging.StreamHandler()
# fh = logging.FileHandler("ttvdb.log")
# #ch.setLevel(logging.DEBUG)
# fh.setLevel(logging.DEBUG)
# logging.getLogger("dicttoxml").setLevel(logging.WARN)
# logging.getLogger("tvdb_api").setLevel(logging.DEBUG)
# logger.addHandler(ch)
# logger.addHandler(fh)

IS_PY2 = sys.version_info[0] == 2
if IS_PY2:
    import ConfigParser
else:
    import configparser as ConfigParser

class tvdb_account:
    # explicit username and account id are not required
    # to use the API. API documentation is unclear in this regard
    username = ""
    account_identifier = ""
    apikey = '0BB856A59C51D607'

# Verify that tvdb_api.py are available
try:
    # thetvdb.com specific modules
    import MythTV.ttvdb.tvdb_api as tvdb_api
    from MythTV.ttvdb.tvdb_api import (tvdb_error, tvdb_shownotfound, tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_episodenotfound, tvdb_attributenotfound, tvdb_userabort)

    # verify version of tvdbapi to make sure it is at least 1.0
    if tvdb_api.__version__ < '2.0':
        print("\nYour current installed tvdb_api.py version is (%s)\n" % tvdb_api.__version__)
        raise
except Exception as e:
    print('''
The modules tvdb_api.py (v2.0 or greater).
They should have been installed along with the MythTV python bindings.
Error:(%s)
''' % e)
    sys.exit(1)
finally:
    pass

try:
    from MythTV.utility import levenshtein
except Exception as e:
    print("""Could not import levenshtein string distance method from MythTV Python Bindings
Error:(%s)
""" % e)
    sys.exit(1)

try:
    if IS_PY2:
        from StringIO import StringIO
    else:
        from io import StringIO
    from lxml import etree as etree
except Exception as e:
    sys.stderr.write(u'\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
    sys.exit(1)

from MythTV.utility.dicttoxml import dicttoxml
try:
    import json
    from lxml import etree as eTree
except Exception as e:
    sys.stderr.write(u'\n! Error - Importing the "lxml" python library failed on error(%s)\n' % e)
    sys.exit(1)

if IS_PY2:
    stdio_type = file
else:
    import io
    stdio_type = io.TextIOWrapper
    unicode = str

# disable the insecure request warning that we know we are going to get
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
version = ''
for digit in etree.LIBXML_VERSION:
    version+=str(digit)+'.'
version = version[:-1]
if version < '2.7.2':
    sys.stderr.write(u'''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


# Global variables
http_find="http://www.thetvdb.com"
http_replace="http://www.thetvdb.com" #Keep replace code "just in case"

name_parse=[
            # foo_[s01]_[e01]
            re.compile('''^(.+?)[ \._\-]\[[Ss]([0-9]+?)\]_\[[Ee]([0-9]+?)\]?[^\\/]*$'''),
            # foo.1x09*
            re.compile('''^(.+?)[ \._\-]\[?([0-9]+)x([0-9]+)[^\\/]*$'''),
            # foo.s01.e01, foo.s01_e01
            re.compile('''^(.+?)[ \._\-][Ss]([0-9]+)[\.\- ]?[Ee]([0-9]+)[^\\/]*$'''),
            # foo.103*
            re.compile('''^(.+)[ \._\-]([0-9]{1})([0-9]{2})[\._ -][^\\/]*$'''),
            # foo.0103*
            re.compile('''^(.+)[ \._\-]([0-9]{2})([0-9]{2,3})[\._ -][^\\/]*$'''),
] # contains regex parsing filename parsing strings used to extract info from video filenames

# Episode meta data that is massaged
massage={'writer':'|','director':'|', 'overview':'&', 'gueststars':'|' }
# Keys and titles used for episode data (option '-D')
data_keys =['airedSeason','airedEpisodeNumber','episodeName','firstAired','directors','overview','rating','writers','filename','language' ]
data_titles=['Season:','Episode:','Subtitle:','ReleaseDate:','Director:','Plot:','UserRating:','Writers:','Screenshot:','Language:' ]
# High level dictionay keys for select graphics URL(s)
fanart_key='fanart'
banner_key='series'
poster_key='poster'
season_key='season'
# Lower level dictionay keys for select graphics URL(s)
poster_series_key='680x1000'
poster_season_key='season'
fanart_hires_key='1920x1080'
fanart_lowres_key='1280x720'
banner_series_key='graphical'
banner_season_key='seasonwide'
# Type of graphics being requested
poster_type='Poster'
fanart_type='Fanart'
banner_type='Banner'
screenshot_request = False

# Cache directory name specific to the user. This avoids permission denied error with a common cache dirs
confdir = os.environ.get('MYTHCONFDIR', '')
if (not confdir) or (confdir == '/'):
    confdir = os.environ.get('HOME', '')
    if (not confdir) or (confdir == '/'):
        print("Unable to find MythTV directory for metadata cache.")
        sys.exit(1)
    confdir = os.path.join(confdir, '.mythtv')
# different cache dirs due to different pickle protocols
# TODO massage pickle so python3 generates python2 compatible pickles
if IS_PY2:
    cache_dir=os.path.join(confdir, "cache/tvdb_api/")
else:
    cache_dir=os.path.join(confdir, "cache/tvdb_api3/")
if not os.path.exists(cache_dir):
    os.makedirs(cache_dir)

def _can_int(x):
    """Takes a string, checks if it is numeric.
    >>> _can_int("2")
    True
    >>> _can_int("A test")
    False
    """
    try:
        int(x)
    except ValueError:
        return False
    else:
        return True
# end _can_int

class OutStreamEncoder(object):
    """Wraps a stream with an encoder"""
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, unicode):
            obj = obj.encode(self.encoding)
        if IS_PY2:
            self.out.write(obj)
        else:
            self.out.buffer.write(obj)

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
if isinstance(sys.stdout, stdio_type):
    sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
    sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')

# modified Show class implementing a fuzzy search
class Show( tvdb_api.Show ):
    def fuzzysearch(self, term = None, key = None):
        results = []
        for cur_season in self.values():
            searchresult = cur_season.fuzzysearch(term = term, key = key)
            if len(searchresult) != 0:
                results.extend(searchresult)
        return results
# end Show

# modified Season class implementing a fuzzy search
class Season( tvdb_api.Season ):
    def fuzzysearch(self, term = None, key = None):
        results = []
        for episode in self.values():
            searchresult = episode.fuzzysearch(term = term, key = key)
            if searchresult is not None:
                results.append(searchresult)
        return results
# end Season

# modified Episode class implementing a fuzzy search
class Episode( tvdb_api.Episode ):
    _re_strippart = re.compile('(.*) \([0-9]+\)')
    def fuzzysearch(self, term = None, key = None):
        if term is None:
            raise TypeError("must supply string to search for (contents)")

        term = unicode(term).lower()
        for cur_key, cur_value in self.items():
            cur_key, cur_value = [unicode(a).lower() for a in [cur_key, cur_value]]
            if key is not None and cur_key != key:
                continue
            distance = levenshtein(cur_value, term)
            if distance <= 3:
                # handle most matches
                self.distance = distance
                return self
            if distance <= 5:
                # handle part numbers, 'subtitle (nn)'
                match = self._re_strippart.match(cur_value)
                if match:
                    tmp = match.group(1)
                    if levenshtein(tmp, term) <= 3:
                        self.distance = distance
                        return self
        return None
#end Episode

# modified Tvdb API class using modified show classes
class Tvdb( tvdb_api.Tvdb ):
    def series_by_sid(self, sid, language):
        "Lookup a series via it's sid"
        seriesid = 'sid:' + sid
        sid = int(sid)
        if not seriesid in self.corrections:
            self._getShowData(sid, language=language)
            self.corrections[seriesid] = sid
        return self.shows[sid]
    #end series_by_sid

    # override the existing method, using modified show classes
    def _setItem(self, sid, seas, ep, attrib, value):
        if sid not in self.shows:
            self.shows[sid] = Show()
        if seas not in self.shows[sid]:
            self.shows[sid][seas] = Season()
        if ep not in self.shows[sid][seas]:
            self.shows[sid][seas][ep] = Episode()
        self.shows[sid][seas][ep][attrib] = value
    #end _setItem

    # override the existing method, using modified show class
    def _setShowData(self, sid, key, value):
        if sid not in self.shows:
            self.shows[sid] = Show()
        self.shows[sid].data[key] = value
    #end _setShowData
#end Tvdb

# Search for a series by SID or Series name
def search_for_series(tvdb, sid_or_name, language):
    "Get series data by sid or series name of the Tv show"
    if SID == True:
        return tvdb.series_by_sid(sid_or_name, language)
    else:
        return tvdb[sid_or_name]
# end search_for_series

# Verify that a Series or Series and Season exists on thetvdb.com
def searchseries(t, opts, series_season_ep):
    global SID
    series_name=''
    key = series_season_ep[0].lower()
    if opts.configure != "" and key in override:
        if override[key][2]:
            series_name=override[key][2] # Override series name
            SID = True
        else:
            series_name=override[key][0] # Override series name
    else:
        series_name=series_season_ep[0] # Leave the series name alone
    try:
        # Search for the series or series & season or series & season & episode
        series_data = search_for_series(t, series_name, opts.language)
        if len(series_season_ep)>1:
            if len(series_season_ep)>2: # series & season & episode
                seriesfound=series_data[ int(series_season_ep[1]) ][ int(series_season_ep[2]) ]
            else:
                seriesfound=series_data[ int(series_season_ep[1]) ] # series & season
        else:
            seriesfound=series_data # Series only
    except tvdb_shownotfound:
        # No such show found.
        # Use the show-name from the files name, and None as the ep name
        sys.exit(0)
    except (tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_attributenotfound):
        # The season, episode or name wasn't found, but the show was.
        # Use the corrected show-name, but no episode name.
        sys.exit(0)
    except tvdb_error as errormsg:
        # Error communicating with thetvdb.com
        if SID == True: # Maybe the digits were a series name (e.g. 90210)
            SID = False
            return searchseries(t, opts, series_season_ep)
        sys.exit(0)
    except tvdb_userabort as errormsg:
        # User aborted selection (q or ^c)
        print("\n", errormsg)
        sys.exit(0)
    else:
        if opts.raw==True:
            print("="*20)
            print("Raw Series Data:\n")
            if len(series_season_ep)>1:
                print(t[ series_name ][ int(series_season_ep[1]) ])
            else:
                print(t[ series_name ])
            print("="*20)
        return(seriesfound)
# end searchseries

# Retrieve Poster or Fan Art or Banner graphics URL(s)
def get_graphics(t, opts, series_season_ep, graphics_type, single_option, language=False):
    banners='_banners'
    series_name=''
    graphics=[]
    key = series_season_ep[0].lower()
    if opts.configure != "" and key in override:
        series_name=override[key][0] # Override series name
    else:
        series_name=series_season_ep[0] # Leave the series name alone

    if SID == True:
        t._parseBanners(series_name)
    else:
        t._parseBanners(t._nameToSid(series_name))
    bid_order = {'fanart': [], 'poster': [], 'series': [], 'season': [], 'seasonwide': []}
    URLs = {'fanart': [], 'poster': [], 'series': [], 'season': [], 'seasonwide': []}

    # get the urls in presented order
    for key in t.shows.keys():
        banner = t.shows[key].data['_banners']
        for graphic_type_items in bid_order.keys():
            if graphic_type_items in banner:
                for graphic_item in banner[graphic_type_items]['raw']:
                    url = banner[graphic_type_items][graphic_item['resolution']][graphic_item['id']]
                    url['rating'] =  graphic_item['ratingsInfo']['average']
                    URLs[graphic_type_items].append(url)

    if graphics_type == fanart_type: # Series fanart graphics
        if not len(URLs[u'fanart']):
            return []
        for url in URLs[u'fanart']:
            graphics.append(url)
    elif len(series_season_ep) == 1:
        if not len(URLs[u'series']):
            return []
        if graphics_type == banner_type: # Series Banners
            for url in URLs[u'series']:
                graphics.append(url)
        else: # Series Posters
            for url in URLs[u'poster']:
                graphics.append(url)
    else:
        if not len(URLs[u'season']):
            return []
        if graphics_type == banner_type: # Season Banners
            season_banners=[]
            # seasonwide has blank resolution
            for url in URLs[u'seasonwide']:
                if url[u'resolution'] == u'' and url[u'subKey'] == series_season_ep[1]:
                    season_banners.append(url)
            if not len(season_banners):
                return []
            graphics = season_banners
        else: # Season Posters
            # season has blank resolution
            season_posters=[]
            for url in URLs[u'season']:
                if url[u'resolution'] == u'' and url[u'subKey'] == series_season_ep[1]:
                    season_posters.append(url)
            if not len(season_posters):
                return []
            graphics = season_posters

    graphicsURLs=[]
    if single_option==False:
        graphicsURLs.append(graphics_type+':')

    count = 0
    wasanythingadded = 0
    anyotherlanguagegraphics=[]
    englishlanguagegraphics=[]
    graphics = sorted(graphics, key=lambda k: k['rating'], reverse=True)
    for URL in graphics:
        if graphics_type == 'filename':
            if URL[graphics_type] is None:
                continue
        if language and 'language' in URL:        # Is there a language to filter URLs on?
            if language == URL['language']:
                graphicsURLs.append((URL['_bannerpath']).replace(http_find, http_replace))
            else: # Check for fall back graphics in case there are no selected language graphics
                if u'en' == URL['language']:
                    englishlanguagegraphics.append((URL['_bannerpath']).replace(http_find, http_replace))
                else:
                    anyotherlanguagegraphics.append((URL['_bannerpath']).replace(http_find, http_replace))
        else:
            graphicsURLs.append((URL['_bannerpath']).replace(http_find, http_replace))
        if wasanythingadded == len(graphicsURLs):
            continue
        wasanythingadded = len(graphicsURLs)

    if not len(graphicsURLs):
        if len(englishlanguagegraphics): # Fall back to English graphics
            graphicsURLs = englishlanguagegraphics
        elif len(anyotherlanguagegraphics):  # Fall back-back to any available graphics
            graphicsURLs = anyotherlanguagegraphics

    if opts.debug == True:
        print(u"\nGraphics:\n", graphicsURLs)

    if len(graphicsURLs) == 1 and graphicsURLs[0] == graphics_type+':':
        return [] # Due to the language filter there may not be any URLs
    return(graphicsURLs)
# end get_graphics

# Massage episode name to match those in thetvdb.com for this series
def massageEpisode_name(ep_name, series_season_ep):
    for edit in override[series_season_ep[0].lower()][1]:
        ep_name=ep_name.replace(edit[0],edit[1]) # Edit episode name for each set of strings
    return ep_name
# end massageEpisode_name

# Remove '|' and replace with commas
def change_to_commas(meta_data):
    if not meta_data: return meta_data
    meta_data = (u'|'.join([d for d in meta_data.split('| ') if d]))
    return (u', '.join([d for d in meta_data.split('|') if d]))
# end change_to_commas

# Change &amp; values to ascii equivalents
def change_amp(text):
    if not text: return text
    try:
        text = text.replace("&quot;", "'").replace("\r\n", " ")
        text = text.replace(r"\'", "'")
    except Exception as e:
        pass
    return text
# end change_amp

# Prepare for includion into a DB
def make_db_ready(text):
    if not text: return text
    text = text.replace(u'\u2013', "-")
    text = text.replace(u'\u2014', "-")
    text = text.replace(u'\u2018', "'")
    text = text.replace(u'\u2019', "'")
    text = text.replace(u'\u2026', "...")
    text = text.replace(u'\u201c', '"')
    text = text.replace(u'\u201d', '"')
    text = text.encode('latin-1', 'backslashreplace')
    return text
# end make_db_ready

# Get Series Episode data by season
def Getseries_episode_data(t, opts, series_season_ep, language = None):
    global screenshot_request, http_find, http_replace

    args = len(series_season_ep)
    series_name=''
    key = series_season_ep[0].lower()
    if opts.configure != "" and key in override:
        series_name=override[key][0] # Override series name
    else:
        series_name=series_season_ep[0] # Leave the series name alone

    # Get Cast members
    cast_members=''
    tmp_cast = ''
    try:
        series_data = search_for_series(t, series_name, opts.language)
        tmp_cast = series_data['_actors']
        tmp_cast = sorted(tmp_cast, key=lambda k: k['sortOrder'])
    except:
        cast_members=''
    if len(tmp_cast):
        cast_members=''.encode('utf8')
        for cast in tmp_cast:
            if cast['name']:
                cast_members+=(cast['name']+', ').encode('utf8')
        if cast_members != '':
            try:
                cast_members = cast_members[:-2].encode('utf8')
            except (UnicodeDecodeError, AttributeError):
                cast_members = unicode(cast_members[:-2],'utf8')
            cast_members = change_amp(cast_members)
            cast_members = change_to_commas(cast_members)
            cast_members=cast_members.replace('\n',' ')

    # Get genre(s)
    genres=''
    try:
        genres_string = series_data[u'genre'].encode('utf-8')
    except:
        genres_string=''
    if genres_string is not None and genres_string != '':
        genres = change_amp(genres_string)
        genres = change_to_commas(genres)

    seasons=sorted(series_data.keys()) # Get the seasons for this series
    for season in seasons:
        if args > 1: # If a season was specified skip other seasons
            if season != int(series_season_ep[1]):
                continue
        episodes=sorted(series_data[season].keys()) # Get the episodes for this season
        for episode in episodes: # If an episode was specified skip other episodes
            if args > 2:
                if episode != int(series_season_ep[2]):
                    continue
            # get more detailed episode info
            t.getDetailedEpisodeInfo(int(series_data['id']), season, episode)
            extra_ep_data=[]
            available_keys=series_data[season][episode].keys()
            if screenshot_request:
                if u'filename' in available_keys:
                    screenshot = series_data[season][episode][u'filename']
                    if screenshot:
                        print(screenshot.replace(http_find, http_replace))
                    return
                else:
                    return
            # key ordering is not sorted so that Title is first for existing client
            # compatability
            key_values=[]
            for values in data_keys: # Initialize an array for each possible data element for
                key_values.append('') # each episode within a season
            for key in sorted(available_keys):
                try:
                    # skip deprecated keys
                    if key in ['director']:
                        continue
                    i = data_keys.index(key) # Include only specific episode data
                except ValueError:
                    if series_data[season][episode][key] is not None:
                        text = series_data[season][episode][key]
                        if isinstance(text, dict):
                            # handle language tuple
                            text = list(text.values())[0]
                        elif isinstance(text, list):
                            # handle guest stars lists
                            text = ', '.join(text)
                        text = change_amp(unicode(text))
                        text = change_to_commas(text)
                        if text == 'None' and key.title() == 'Director':
                            text = u"Unknown"
                        try:
                            extra_ep_data.append(u"%s:%s" % (key.title(), text))
                        except UnicodeDecodeError:
                            extra_ep_data.append(u"%s:%s" % (key.title(), unicode(text, "utf8")))
                    continue
                text = series_data[season][episode][key]

                if text is None and key.title() == 'Director':
                    text = u"Unknown"
                if isinstance(text, list):
                    text = ', '.join(text)
                if text is None or text == 'None':
                    continue
                else:
                    # handle language tuple
                    if isinstance(text, dict):
                        # handle language tuple
                        text = list(text.values())[0]
                    text = change_amp(unicode(text))
                    value = change_to_commas(text)
                    value = value.replace(u'\n', u' ')
                key_values[i]=value
            index = 0
            if SID == False:
                print(u"Title:%s" % series_name)    # Ouput the full series name
            else:
                print(u"Title:%s" % series_data[u'seriesname'])

            for key in data_titles:
                if key_values[index] is not None:
                    if data_titles[index] == u'ReleaseDate:' and len(key_values[index]) > 4:
                        print(u'%s%s'% (u'Year:', key_values[index][:4]))
                    if key_values[index] != 'None':
                        print(u'%s%s' % (data_titles[index], key_values[index]))
                index+=1
            cast_print=False
            for extra_data in extra_ep_data:
                if extra_data[:extra_data.index(':')] == u'Gueststars':
                    extra_cast = extra_data[extra_data.index(':')+1:]
                    if not extra_cast:
                        continue
                    if (len(extra_cast)>128) and not extra_cast.count(','):
                        continue
                    if cast_members:
                        extra_data=(u"Cast:%s" % cast_members)+', '+extra_cast
                    else:
                        extra_data=u"Cast:%s" % extra_cast
                    cast_print=True
                print(extra_data)
            if cast_print == False:
                print(u"Cast:%s" % cast_members)
            if genres != '':
                print(u"Genres:%s" % genres)
            print(u"Runtime:%s" % series_data[u'runtime'])

            # URL to TVDB web site episode web page for this series
            for url_data in [u'seriesid', u'seasonid', u'id']:
                if not url_data in available_keys:
                    break
            else:
                results = series_data
                print(u'URL:http://www.thetvdb.com/?tab=episode&seriesid=%s&seasonid=%s&id=%s' %
                      (results[season][episode][u'seriesid'],
                       results[season][episode][u'seasonid'],
                       results[season][episode][u'id']))
# end Getseries_episode_data

# Get Series Season and Episode numbers
def Getseries_episode_numbers(t, opts, series_season_ep):
    def _episode_sort(episode):
        seasonnumber = 0
        episodenumber = 0
        try: seasonnumber = int(episode['seasonnumber'])
        except: pass
        try: episodenumber = int(episode['episodenumber'])
        except: pass
        return (-episode.distance, seasonnumber, episodenumber)

    global xmlFlag
    series_name=''
    ep_name=''
    series_sid = None
    key = series_season_ep[0].lower()
    if opts.configure != "" and key in override:
        if override[key][2]:
            series_sid=override[key][2] # Override series name
            SID = True
        else:
            series_name=override[key][0] # Override series name
        ep_name=series_season_ep[1]
        if len(override[series_season_ep[0].lower()][1]) != 0: # Are there search-replace strings?
            ep_name=massageEpisode_name(ep_name, series_season_ep)
    else:
        series_name=series_season_ep[0] # Leave the series name alone
        ep_name=series_season_ep[1] # Leave the episode name alone

    if series_sid:
        series = search_for_series(t, series_sid, opts.language)
    else:
        series = search_for_series(t, series_name, opts.language)
    season_ep_num = series.fuzzysearch(ep_name, 'episodename')
    if len(season_ep_num) != 0:
        for episode in sorted(season_ep_num, key=lambda ep: _episode_sort(ep), reverse=True):
#            if episode.distance == 0: # exact match
                if xmlFlag:
                    # get more detailed episode info
                    t.getDetailedEpisodeInfo(series['id'], episode['airedSeason'], episode)
                    convert_series_to_xml(t, series_season_ep, season_ep_num)
                    displaySeriesXML(t, [series_name, episode['seasonnumber'], episode['episodenumber']])
                    return 0
                print(season_and_episode_num.replace('\\n', '\n') %
                      (int(episode['seasonnumber']), int(episode['episodenumber'])))
#            elif (episode['episodename'].lower()).startswith(ep_name.lower()):
#                if len(episode['episodename']) > (len(ep_name)+1):
#                    if episode['episodename'][len(ep_name):len(ep_name)+2] != ' (':
#                        continue # Skip episodes the are not part of a set of (1), (2) ... etc
#                    if xmlFlag:
#                        displaySeriesXML(t, [series_name, episode['seasonnumber'], episode['episodenumber']])
#                        sys.exit(0)
#                    print(season_and_episode_num.replace('\\n', '\n') % (int(episode['seasonnumber']), int(episode['episodenumber'])))
# end Getseries_episode_numbers

# Set up a custom interface to get all series matching a partial series name
class returnAllSeriesUI(tvdb_api.BaseUI):
    def __init__(self, config, log=None):
        self.config = config
        self.log = log

    def selectSeries(self, allSeries):
        return allSeries
# ends returnAllSeriesUI

def initialize_override_dictionary(useroptions, language):
    """ Change variables through a user supplied configuration file
    return False and exit the script if there are issues with the configuration file values
    """
    if useroptions[0]=='~':
        useroptions=os.path.expanduser("~")+useroptions[1:]
    if os.path.isfile(useroptions) == False:
        sys.stderr.write(
            "! The specified user configuration file (%s) is not a file\n" % useroptions
        )
        sys.exit(1)
    massage = {}
    overrides = {}
    overrides_id = {}
    cfg = ConfigParser.SafeConfigParser()
    cfg.read(useroptions)

    for section in cfg.sections():
        if section == 'regex':
            # Change variables per user config file
            for option in cfg.options(section):
                name_parse.append(re.compile(cfg.get(section, option)))
            continue
        if section =='ep_name_massage':
            for option in cfg.options(section):
                tmp =cfg.get(section, option).split(',')
                if len(tmp)%2 and len(cfg.get(section, option)) != 0:
                    sys.stderr.write("! For (%s) 'ep_name_massage' values must be in pairs\n" % option)
                    sys.exit(1)
                tmp_array=[]
                i=0
                while i != len(tmp):
                    tmp_array.append([tmp[i].replace('"',''), tmp[i+1].replace('"','')])
                    i+=2
                massage[option]=tmp_array
            continue
        if section =='series_name_override':
            for option in cfg.options(section):
                overrides[option] = cfg.get(section, option)
            tvdb = Tvdb(banners=False,
                        debug = False,
                        interactive = False,
                        cache = cache_dir,
                        custom_ui=returnAllSeriesUI,
                        apikey=tvdb_account.apikey,  # thetvdb.com API key requested by MythTV
                        username=tvdb_account.username,
                        userkey=tvdb_account.account_identifier)
            tvdb.session.verify = False
            for key in overrides.keys():
                sid = overrides[key]
                if len(sid) == 0:
                    continue
                try: # Check that the SID (Series id) is numeric
                    dummy = int(sid)
                except:
                    sys.stdout.write("! Series (%s) Invalid SID (not numeric) [%s] in config file\n" % (key, sid))
                    sys.exit(1)
                # Make sure that the series name is not empty or all blanks
                if len(key.replace(' ','')) == 0:
                    sys.stdout.write("! Invalid Series name (must have some non-blank characters) [%s] in config file\n" % key)
                    print(overrides.keys())
                    sys.exit(1)

                try:
                    series_name_sid=tvdb.series_by_sid(sid, language)
                except:
                    sys.stdout.write("! Invalid Series (no matches found in thetvdb,com) (%s) sid (%s) in config file\n" % (key, sid))
                    sys.exit(1)
                overrides[key]=unicode(series_name_sid.data[u'seriesName'])   #.encode('utf-8')
                overrides_id[key] = sid
            continue

    for key in overrides.keys():
        override[key] = [overrides[key],[],overrides_id[key]]

    for key in massage.keys():
        if key in override:
            override[key][1]=massage[key]
        else:
            override[key]=[key, massage[key], None]
    return
# END initialize_override_dictionary

def convert_search_to_xml(t, allSeries):
    """
    Convert json to xml and set up tvdb_api object as other stuff expects
    :param t:   tvdb_api object
    :param allSeries: json array of series
    :return: xml version of allseries
    """
    # Initialize XML display value to off
    t.xml = False
    def series_item_func(parent):
        if parent == "root":
            return "series"
        return "alias"
    xml = dicttoxml(allSeries, item_func=series_item_func, attr_type=False)
    t.searchTree = eTree.XML(xml)
    t.seriesInfoTree = None
    t.epInfoTree = None
    t.actorsInfoTree = None
    t.imagesInfoTree = None
    t.baseXsltDir = xslt.baseXsltPath

def convert_series_to_xml(t, series_season_ep, ep_info):
    """
    Convert json to xml and set up tvdb_api object as other stuff expects
    :param t:   tvdb_api object
    :param ep_info: json array of series
    """
    # Initialize XML display value to off
    t.xml = False
    def series_ep_item_func(parent):
        if parent == "data":
            return "series"
        if parent == "_banners_raw":
            return "banner"
        if parent == "_actors":
            return "actor"
        return "item"
    def series_people_item_func(parent):
        if parent == "Actors":
            return "Actor"
        return "item"
    def series_images_item_func(parent):
        if parent == "root":
            return "images"
        return "Banner"
    for show_id in t.shows.keys():
        break

    # dict for 'data['_banners']['poster']['raw'] must exist for fetching coverarts,
    # check with ttvdb.py -l de -a CH -D 89901 36 4
    try:
        if 'poster' not in t.shows[show_id].data['_banners'].keys():
            t.shows[show_id].data['_banners']['poster'] = {}
            t.shows[show_id].data['_banners']['poster']['raw'] = {}
    except KeyError:
        # no banner fanart exists
        pass

    # sort the cast into sort order
    t.shows[show_id].data['_actors'] = sorted(t.shows[show_id].data['_actors'], key=lambda k: k['sortOrder'])
    t.searchTree = None
    t.seriesInfoTree = None
    t.epInfoTree = None
    t.actorsInfoTree = None
    t.imagesInfoTree = None
    sxml = dicttoxml(t.shows[show_id].data, custom_root='series', item_func=series_ep_item_func, attr_type=False)
    exml = dicttoxml(t.shows[show_id], custom_root='data', item_func=series_ep_item_func, attr_type=False)
    t.seriesInfoTree = eTree.XML(exml)
    t.seriesInfoTree.append(eTree.XML(sxml))
    t.baseXsltDir = xslt.baseXsltPath

def initializeXslt(language):
    ''' Initalize all data and functions for XSLT stylesheet processing
    return nothing
    '''
    global xslt, tvdbXpath
    try:
        import MythTV.ttvdb.tvdbXslt as tvdbXslt
    except Exception as errmsg:
        sys.stderr.write('! Error: Importing tvdbXslt error(%s)\n' % errmsg)
        sys.exit(1)

    xslt = tvdbXslt.xpathFunctions()
    xslt.language = language
    xslt.buildFuncDict()
    xslt.baseXsltPath = tvdbXslt.baseXsltDir
    tvdbXpath = etree.FunctionNamespace('http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format')
    tvdbXpath.prefix = 'tvdbXpath'
    for key in xslt.FuncDict.keys():
        tvdbXpath[key] = xslt.FuncDict[key]
    return
# end initializeXslt()

def displaySearchXML(tvdb_api):
    '''Using a XSLT style sheet translate TVDB search results into the MythTV Universal Query format
    return nothing
    '''
    global xslt, tvdbXpath

    # Remove duplicates when a non-English language code is specified
    if xslt.language != 'en':
        compareFilter = etree.XPath('//id[text()=$Id]')
        idLangFilter = etree.XPath('//id[text()=$Id]/../language[text()="en"]/..')
        tmpTree = deepcopy(tvdb_api.searchTree)
        for seriesId in tmpTree.xpath('//Series/id/text()'):
            if len(compareFilter(tvdb_api.searchTree, Id=seriesId)) > 1:
                tmpList = idLangFilter(tvdb_api.searchTree, Id=seriesId)
                if len(tmpList):
                    tvdb_api.searchTree.remove(tmpList[0])

    tvdbQueryXslt = etree.XSLT(etree.parse(u'%s%s' % (tvdb_api.baseXsltDir, u'tvdbQuery.xsl')))
    items = tvdbQueryXslt(tvdb_api.searchTree)
    if items.getroot() is not None:
        if len(items.xpath('//item')):
            sys.stdout.write(etree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
    return 0
# end displaySearchXML()

def displaySeriesXML(tvdb_api, series_season_ep):
    '''Using a XSLT style sheet translate TVDB Series data results into the
    MythTV Universal Query format
    return nothing
    '''
    global xslt, tvdbXpath
    allDataElement = etree.XML(u'<allData></allData>')
    requestDetails = etree.XML(u'<requestDetails></requestDetails>')
    requestDetails.attrib['lang'] = xslt.language
    requestDetails.attrib['series'] = series_season_ep[0]
    requestDetails.attrib['season'] = series_season_ep[1]
    requestDetails.attrib['episode'] = series_season_ep[2]
    allDataElement.append(requestDetails)

    # Combine the various XML inputs into a single XML element and send to the XSLT stylesheet
    allDataElement.append(tvdb_api.seriesInfoTree)

    tvdbQueryXslt = etree.XSLT(etree.parse(u'%s%s' % (tvdb_api.baseXsltDir, u'tvdbVideo.xsl')))
    items = tvdbQueryXslt(allDataElement)

    # temporary fix for missing coverart: use global coverart from series
    if len(items.xpath("//image[@type='coverart']")) == 0:
        for el in allDataElement.iter("series"):
            glob_poster = el.find("poster")
            if glob_poster is not None and glob_poster.text != 'http://thetvdb.com/banners/':
                glob_url = glob_poster.text
                glob_thumb = glob_url.replace("posters", "_cache/posters")
                glob_coverart = etree.Element("image", type = "coverart", url = glob_url, thumb = glob_thumb)
                image_items = items.find("item").find("images")
                if image_items is None:
                    etree.SubElement(items.find("item"), "images")
                items.find("item").find("images").append(glob_coverart)
                break

    if items.getroot() is not None:
        if len(items.xpath('//item')):
            sys.stdout.write(etree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
    return 0
# end displaySeriesXML()

def displayCollectionXML(tvdb_api):
    '''Using a XSLT style sheet translate TVDB series results into the MythTV Universal Query format
    return nothing
    '''
    global xslt, tvdbXpath

    # Remove duplicates when non-English language code is specified
    if xslt.language != 'en':
        compareFilter = etree.XPath('//id[text()=$Id]')
        idLangFilter = etree.XPath('//id[text()=$Id]/../language[text()="en"]/..')
        tmpTree = deepcopy(tvdb_api.seriesInfoTree)
        for seriesId in tmpTree.xpath('//Series/id/text()'):
            if len(compareFilter(tvdb_api.seriesInfoTree, Id=seriesId)) > 1:
                tmpList = idLangFilter(tvdb_api.seriesInfoTree, Id=seriesId)
                if len(tmpList):
                    tvdb_api.seriesInfoTree.remove(tmpList[0])

    tvdbCollectionXslt = etree.XSLT(etree.parse(u'%s%s' % (tvdb_api.baseXsltDir, u'tvdbCollection.xsl')))
    items = tvdbCollectionXslt(tvdb_api.seriesInfoTree)
    if items.getroot() is not None:
        if len(items.xpath('//item')):
            sys.stdout.write(etree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
    return 0
# end displayCollectionXML()


def doc_test(opts):
    import doctest

    if not IS_PY2:
        # python 3 doctest capture when teh output is utf8 (bytes)
        # convert it back to str
        if isinstance(sys.stdout, OutStreamEncoder):
            sys.stdout = sys.stdout.out
            sys.stderr = sys.stderr.out

            class SpoofDocTestWriter(doctest._SpoofOut):
                """Wraps a stream with an decoder"""
                def __init__(self, *cwargs, **kwargs):
                    super(SpoofDocTestWriter, self).__init__(*cwargs, **kwargs)

                def write(self, obj):
                    """Wraps the output stream, encoding Unicode strings with the specified encoding"""
                    if isinstance(obj, bytes):
                        obj = obj.decode('utf-8')
                    return super(SpoofDocTestWriter, self).write(obj)

                def __getattr__(self, attr):
                    """Delegate everything but write to the stream"""
                    return getattr(self.out, attr)

            # replace _SpoofOut with our massager
            doctest._SpoofOut = SpoofDocTestWriter
    return doctest.testmod(verbose=opts.debug, optionflags=doctest.ELLIPSIS, )

def main():
    global season_and_episode_num, screenshot_request
    # reset some globals for doctest mode
    screenshot_request = False

    parser = OptionParser(usage=u"%prog usage: ttvdb -hdruviomMPFBDS [parameters]\n <series name or 'series and season number' or 'series and season number and episode number'>\n\nFor details on using ttvdb with Mythvideo see the ttvdb wiki page at:\nhttp://www.mythtv.org/wiki/Ttvdb.py")

    parser.add_option(  "--doctest", action="store_true", default=False, dest="doctest",
                        help=u"Run doctests")
    parser.add_option(  "-d", "--debug", action="store_true", default=False, dest="debug",
                        help=u"Show debugging info")
    parser.add_option(  "-r", "--raw", action="store_true",default=False, dest="raw",
                        help=u"Dump raw data only")
    parser.add_option(  "-u", "--usage", action="store_true", default=False, dest="usage",
                        help=u"Display examples for executing the ttvdb script")
    parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                        help=u"Display version and author")
    parser.add_option(  "-i", "--interactive", action="store_true", default=False, dest="interactive",
                        help=u"Interaction mode (allows selection of a specific Series)")
    parser.add_option(  "-c", "--configure", metavar="FILE", default="", dest="configure",
                        help=u"Use configuration settings")
    parser.add_option(  "-l", "--language", metavar="LANGUAGE", default=u'en', dest="language",
                        help=u"Select data that matches the specified language fall back to english if nothing found (e.g. 'es' Español, 'de' Deutsch ... etc)")
    parser.add_option(  "-a", "--area", metavar="COUNTRY", default=u'', dest="country",
			help=u"Select data that matches the specificed country (e.g. 'de' Germany 'gb' UK). This option is currently not used.")
    parser.add_option(  "-n", "--num_seasons", action="store_true", default=False, dest="num_seasons",
                        help=u"Return the season numbers for a series")
    parser.add_option(  "-t", action="store_true", default=False, dest="test",
                        help=u"Test for the availability of runtime dependencies")
    parser.add_option(  "-m", "--mythvideo", action="store_true", default=False, dest="mythvideo",
                        help=u"Conform to mythvideo standards when processing -M, -P, -F and -D")
    parser.add_option(  "-M", "--list", action="store_true", default=False, dest="list",
                        help=u"Get matching TV Series list")
    parser.add_option(  "-P", "--poster", action="store_true", default=False, dest="poster",
                        help=u"Get Series Poster URL(s)")
    parser.add_option(  "-F", "--fanart", action="store_true", default=False, dest="fanart",
                        help=u"Get Series fan art URL(s)")
    parser.add_option(  "-B", "--backdrop", action="store_true", default=False, dest="banner",
                        help=u"Get Series banner/backdrop URL(s)")
    parser.add_option(  "-S", "--screenshot", action="store_true", default=False, dest="screenshot",
                        help=u"Get Series episode screenshot URL")
    parser.add_option(  "-D", "--data", action="store_true", default=False, dest="data",
                        help=u"Get Series episode data")
    parser.add_option(  "-N", "--numbers", action="store_true", default=False, dest="numbers",
                        help=u"Get Season and Episode numbers")
    parser.add_option(  "-C", "--collection", action="store_true", default=False, dest="collection",
                        help=u'Get a TV Series (collection) "series" level information')

    opts, series_season_ep = parser.parse_args()

    if opts.doctest:
        return doc_test(opts)

    # Test mode, if we've made it here, everything is ok
    if opts.test:
        print("Everything appears to be in order")
        return 0

    # Make everything unicode utf8
    if IS_PY2:
        for index in range(len(series_season_ep)):
            series_season_ep[index] = unicode(series_season_ep[index], 'utf8')

    if opts.debug == True:
        print("opts", opts)
        print("\nargs", series_season_ep)

    # Process version command line requests
    if opts.version == True:
        version = etree.XML(u'<grabber></grabber>')
        etree.SubElement(version, "name").text = __title__
        etree.SubElement(version, "author").text = __author__
        etree.SubElement(version, "thumbnail").text = 'ttvdb.png'
        etree.SubElement(version, "command").text = 'ttvdb.py'
        etree.SubElement(version, "type").text = 'television'
        etree.SubElement(version, "description").text = 'Search and metadata downloads for thetvdb.com'
        etree.SubElement(version, "version").text = __version__
        sys.stdout.write(etree.tostring(version, encoding='UTF-8', pretty_print=True))
        return 0

    # Process usage command line requests
    if opts.usage == True:
        sys.stdout.write(usage_txt)
        return 0

    if len(series_season_ep) == 0:
        parser.error("! No series or series season episode supplied")
        return 1

    # Default output format of season and episode numbers
    season_and_episode_num='S%02dE%02d' # Format output example "S04E12"

    if opts.numbers == False:
        if len(series_season_ep) > 1:
            if not _can_int(series_season_ep[1]):
                parser.error("! Season is not numeric")
                return 1
        if len(series_season_ep) > 2:
            if not _can_int(series_season_ep[2]):
                parser.error("! Episode is not numeric")
                return 1
    else:
        if len(series_season_ep) < 2:
            parser.error("! An Episode name must be included")
            return 1
        if len(series_season_ep) == 3:
            season_and_episode_num = series_season_ep[2] # Override default output format

    if opts.screenshot:
        if len(series_season_ep) > 1:
            if not _can_int(series_season_ep[1]):
                parser.error("! Season is not numeric")
                return 1
        if len(series_season_ep) > 2:
            if not _can_int(series_season_ep[2]):
                parser.error("! Episode is not numeric")
                return 1
        if not len(series_season_ep) > 2:
            parser.error("! Option (-S), episode screenshot search requires Season and Episode numbers")
            return 1
        screenshot_request = True

    if opts.debug == True:
        print(series_season_ep)

    if opts.debug == True:
        print("#"*20)
        print("# series_season_ep array(",series_season_ep,")")

    if opts.debug == True:
        print("#"*20)
        print("# Starting tvtvb")
        print("# Processing (%s) Series" % ( series_season_ep[0] ))

    # List of language from http://www.thetvdb.com/api/0629B785CE550C8D/languages.xml
    # Hard-coded here as it is realtively static, and saves another HTTP request, as
    # recommended on http://thetvdb.com/wiki/index.php/API:languages.xml
    valid_languages = ["da", "fi", "nl", "de", "it", "es", "fr","pl", "hu","el","tr", "ru","he","ja","pt","zh","cs","sl", "hr","ko","en","sv","no"]

    # Validate language as specified by user
    if not opts.language in valid_languages:
        opts.language = 'en'    # Set the default to English when an invalid language was specified

    # Set XML to be the default display mode for -N, -M, -D, -C
    opts.xml = True
    initializeXslt(opts.language)

    # Access thetvdb.com API with banners (Posters, Fanart, banners, screenshots) data retrieval enabled
    if opts.list ==True:
        t = Tvdb(banners=False,
                 debug = opts.debug,
                 cache = cache_dir,
                 custom_ui=returnAllSeriesUI,
                 language = opts.language,
                 apikey=tvdb_account.apikey,    # thetvdb.com API key requested by MythTV
                 username=tvdb_account.username,
                 userkey=tvdb_account.account_identifier)
        if opts.xml:
            t.xml = True
    elif opts.interactive == True:
        t = Tvdb(banners=True,
                 debug=opts.debug,
                 interactive=True,
                 select_first=False,
                 cache=cache_dir,
                 actors = True,
                 language = opts.language,
                 apikey=tvdb_account.apikey,    # thetvdb.com API key requested by MythTV
                 username=tvdb_account.username,
                 userkey=tvdb_account.account_identifier)
        if opts.xml:
            t.xml = True
    else:
        t = Tvdb(banners=True,
                 debug = opts.debug,
                 cache = cache_dir,
                 actors = True,
                 language = opts.language,
                 apikey=tvdb_account.apikey,    # thetvdb.com API key requested by MythTV
                 username=tvdb_account.username,
                 userkey=tvdb_account.account_identifier)
        if opts.xml:
            t.xml = True

    # disable certificate check
    t.session.verify = False

    # Determine if there is a SID or a series name to search with
    global SID
    SID = False
    if _can_int(series_season_ep[0]): # if it is numeric then assume it is a series ID number
        SID = True
    else:
        SID = False

    # The -C collections options only supports a SID as input
    if opts.collection:
        if SID:
            pass
        else:
            parser.error("! Option (-C), collection requires an inetref number")
            return 1

    if opts.debug == True:
        print("# ..got tvdb mirrors")
        print("# Start to process series or series_season_ep")
        print("#"*20)

    global override
    override={} # Initialize series name override dictionary
    # If the user wants Series name overrides and a override file exists then create an overide dictionary
    if opts.configure != '':    # Did the user want to override the default config file name/location
        if opts.configure[0]=='~':
            opts.configure=os.path.expanduser("~")+opts.configure[1:]
        if os.path.exists(opts.configure) == 1: # Do overrides exist?
            initialize_override_dictionary(opts.configure, opts.language)
        else:
            sys.stderr.write("! The specified override file (%s) does not exist\n" % opts.configure)
            return 1
    else: # Check if there is a default configuration file
        default_config = u"%s/%s" % (os.path.expanduser(u"~"), u".mythtv/ttvdb.conf")
        if os.path.isfile(default_config):
            opts.configure = default_config
            initialize_override_dictionary(opts.configure, opts.language)

    if len(override) == 0:
        opts.configure = False # Turn off the override option as there is nothing to override

    # Check if a video name was passed and if so parse it for series name, season and episode numbers
    if not opts.collection and len(series_season_ep) == 1:
        for r in name_parse:
            match = r.match(series_season_ep[0])
            if match:
                seriesname, seasno, epno = match.groups()
                #remove ._- characters from name (- removed only if next to end of line)
                seriesname = re.sub("[\._]|\-(?=$)", " ", seriesname).strip()
                series_season_ep = [seriesname, seasno, epno]
                break # Matched - to the next file!

    # Fetch a list of matching series names
    if opts.list ==True:
        try:
            key = series_season_ep[0].lower()
            if opts.configure != "" and key in override:
                if override[key][2]:
                    allSeries = t._getSeries(override[key][2], True)
                else:
                    allSeries = t._getSeries(override[key][0])
            else:
                allSeries=t._getSeries(series_season_ep[0])
        except tvdb_shownotfound:
            return 0 # No matching series
        except Exception as e:
            sys.stderr.write("! Error: %s\n" % (e))
            raise
            return 1 # Most likely a communications error
        if opts.xml:
            convert_search_to_xml(t, allSeries)
            displaySearchXML(t)
            return 0
        match_list = []
        for series_name_sid in allSeries: # list search results
            key_value = u"%s:%s" % (series_name_sid['sid'], series_name_sid['name'])
            if not key_value in match_list: # Do not add duplicates
                match_list.append(key_value)
                print(key_value)
        return 0 # The Series list option (-M) is the only option honoured when used

    # Fetch TV series collection information
    if opts.collection:
        try:
            t._getShowData(series_season_ep[0], opts.language)
        except tvdb_shownotfound:
            return 0 # No matching series
        except Exception as e:
            sys.stderr.write("! Error: %s\n" % (e))
            raise
            return 1 # Most likely a communications error
        convert_series_to_xml(t, series_season_ep, None)
        displayCollectionXML(t)
        return 0 # The TV Series collection option (-C) is the only option honoured when used

    # Verify that thetvdb.com has the desired series_season_ep.
    # Exit this module if series_season_ep is not found
    if opts.numbers == False and opts.num_seasons == False:
        seriesfound=searchseries(t, opts, series_season_ep)
        x=1
    else:
        x=[]
        x.append(series_season_ep[0]) # Only use series name in check
        seriesfound=searchseries(t, opts, x)

    # Return the season numbers for a series
    if opts.num_seasons == True:
        season_numbers=''
        for x in sorted(seriesfound.keys()):
            season_numbers+='%d,' % x
        print(season_numbers[:-1])
        return 0 # Option (-n) is the only option honoured when used

    # Dump information accessible for a Series and ONLY first season of episoded data
    if opts.debug == True:
        print("#"*20)
        print("# Starting Raw keys call")
        print("Lvl #1:")    # Seasons for series
        x = t[series_season_ep[0]].keys()
        print(t[series_season_ep[0]].keys())
        print("#"*20)
        print("Lvl #2:")    # Episodes for each season
        for y in x:
            print(t[series_season_ep[0]][y].keys())
        print("#"*20)
        print("Lvl #3:")    # Keys for each episode within the 1st season
        z = t[series_season_ep[0]][1].keys()
        for aa in z:
            print(t[series_season_ep[0]][1][aa].keys())
        print("#"*20)
        print("Lvl #4:")    # Available data for each episode in 1st season
        for aa in z:
            codes = t[series_season_ep[0]][1][aa].keys()
            print("\n\nStart:")
            for c in codes:
                print("="*50)
                print('Key Name=('+c+'):')
                print(t[series_season_ep[0]][1][aa][c])
                print("="*50)
        print("#"*20)
        sys.exit (True)

    if opts.numbers == True: # Fetch and output season and episode numbers
        global xmlFlag
        if opts.xml:
            xmlFlag = True
        else:
            xmlFlag = False
        Getseries_episode_numbers(t, opts, series_season_ep)
        return 0 # The Numbers option (-N) is the only option honoured when used

    if opts.data or screenshot_request: # Fetch and output episode data
        if opts.mythvideo:
            if len(series_season_ep) != 3:
                print(u"Season and Episode numbers required.")
            else:
                if opts.xml:
                    t.getDetailedEpisodeInfo(seriesfound[u'id'], series_season_ep[1], series_season_ep[2])
                    convert_series_to_xml(t, series_season_ep, seriesfound)
                    displaySeriesXML(t, series_season_ep)
                    return 0
                Getseries_episode_data(t, opts, series_season_ep, language=opts.language)
        else:
            if opts.xml and len(series_season_ep) == 3:
                t.getDetailedEpisodeInfo(list(t.shows.values())[0].data['id'], series_season_ep[1], series_season_ep[2])
                convert_series_to_xml(t, series_season_ep, seriesfound)
                displaySeriesXML(t, series_season_ep)
                return 0
            Getseries_episode_data(t, opts, series_season_ep, language=opts.language)

    # Fetch the requested graphics URL(s)
    if opts.debug == True:
        print("#"*20)
        print("# Checking if Posters, Fanart or Banners are available")
        print("#"*20)

    key = series_season_ep[0].lower()
    if opts.configure != "" and key in override:
        series_info = search_for_series(t, override[key][0], opts.language)
    else:
        series_info = search_for_series(t, series_season_ep[0], opts.language)
    try:
        banners_keys = series_info['_banners'].keys()
    except:
        banners_keys = None

    banner= False
    poster= False
    fanart= False

    if banners_keys:
        for x in banners_keys: # Determine what type of graphics is available
            if x == fanart_key:
                fanart=True
            elif x == poster_key:
                poster=True
            elif x == season_key or x == banner_key:
                banner=True

    # Make sure that some graphics URL(s) (Posters, FanArt or Banners) are available
    if ( fanart!=True and poster!=True and banner!=True ):
        return 0

    if opts.debug == True:
        print("#"*20)
        print("# One or more of Posters, Fanart or Banners are available")
        print("#"*20)

    # Determine if graphic URL identification output is required
    if opts.data:    # Along with episode data get all graphics
        opts.poster = True
        opts.fanart = True
        opts.banner = True
        single_option = True
        fanart, banner, poster = (True, True, True)
    else:
        y=0
        single_option=True
        if opts.poster==True:
            y+=1
        if opts.fanart==True:
            y+=1
        if opts.banner==True:
            y+=1

    if (poster==True and opts.poster==True and opts.raw!=True): # Get posters and send to stdout
        season_poster_found = False
        if opts.mythvideo:
            if len(series_season_ep) < 2:
                print(u"Season and Episode numbers required.")
                return 0
        all_posters = u'Coverart:'
        all_empty = len(all_posters)
        for p in get_graphics(t, opts, series_season_ep, poster_type, single_option, opts.language):
            all_posters = all_posters+p+u','
            season_poster_found = True
        if season_poster_found == False: # If there were no season posters get the series top poster
            series_name=''
            key = series_season_ep[0].lower()
            if opts.configure != "" and key in override:
                series_name=override[key][0] # Override series name
            else:
                series_name=series_season_ep[0] # Leave the series name alone
            for p in get_graphics(t, opts, [series_name], poster_type, single_option, opts.language):
                all_posters = all_posters+p+u','
        if len(all_posters) > all_empty:
            if all_posters[-1] == u',':
                print(all_posters[:-1])
            else:
                print(all_posters)

    if (fanart==True and opts.fanart==True and opts.raw!=True): # Get Fan Art and send to stdout
        all_fanart = u'Fanart:'
        all_empty = len(all_fanart)
        for f in get_graphics(t, opts, series_season_ep, fanart_type, single_option, opts.language):
            all_fanart = all_fanart+f+u','
        if len(all_fanart) > all_empty:
            if all_fanart[-1] == u',':
                print(all_fanart[:-1])
            else:
                print(all_fanart)

    if (banner==True and opts.banner==True and opts.raw!=True): # Also change to get ALL Series graphics
        season_banner_found = False
        if opts.mythvideo:
            if len(series_season_ep) < 2:
                print(u"Season and Episode numbers required.")
                return 0
        all_banners = u'Banner:'
        all_empty = len(all_banners)
        for b in get_graphics(t, opts, series_season_ep, banner_type, single_option, opts.language):
            all_banners = all_banners+b+u','
            season_banner_found = True
        if not season_banner_found: # If there were no season banner get the series top banner
            series_name=''
            key = series_season_ep[0].lower()
            if opts.configure != "" and key in override:
                series_name=override[key][0] # Override series name
            else:
                series_name=series_season_ep[0] # Leave the series name alone
            for b in get_graphics(t, opts, [series_name], banner_type, single_option, opts.language):
                all_banners = all_banners+b+u','
        if len(all_banners) > all_empty:
            if all_banners[-1] == u',':
                print(all_banners[:-1])
            else:
                print(all_banners)

    if opts.debug == True:
        print("#"*20)
        print("# Processing complete")
        print("#"*20)
    return 0
#end main

if __name__ == "__main__":
    sys.exit(main())
