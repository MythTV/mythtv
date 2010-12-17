#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: jamu.py      Just.Another.Metadata.Utility
# Python Script
# Author:     R.D. Vaughan
# Purpose:     This python script is intended to perform a variety of utility functions on mythvideo
#           metadata and the associated video files.
#
#           The primary movie source for graphics and data is themoviedb.com wiki.
#           The primary TV Series source for graphics and data is thetvdb.com wiki.
#           Users of this script are encouraged to populate both themoviedb.com and thetvdb.com
#           with posters, fan art and banners and meta data. The richer the source the more valuable
#           the script.
#           This script uses the python module tvdb_api.py (v0.6DEV or higher) found at
#           http://pypi.python.org/pypi?%3Aaction=search&term=tvnamer&submit=search thanks
#           to the authors of this excellent module.
#           The tvdb_api.py module uses the full access XML api published by thetvdb.com see:
#           http://thetvdb.com/wiki/index.php?title=Programmers_API
#           This python script's functionality is enhanced if you have installed "tvnamer.py" created by
#           "dbr/Ben" who is also the author of the "tvdb_api.py" module.
#           "tvnamer.py" is used to rename avi files with series/episode information found at
#           thetvdb.com
#           Python access to the tmdb api started with a module from dbr/Ben and then enhanced for
#           Jamu's needs.
#           The routines to select video files was copied and modified from tvnamer.py mentioned above.
#           The routine "_save_video_metadata_to_mythdb" has been taken and modified from
#           "find_meta.py" author Pekka Jääskeläinen.
#           The routine "_addCastGenre" was taken and modified from "tvdb-bulk-update.py" by
#           author David Shilvock <davels@telus.net>.
#
# Command line examples:
# See help (-u and -h) options
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="JAMU - Just.Another.Metadata.Utility";
__author__="R.D.Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions on mythvideo metadata
and the associated video files.

The primary movie source for graphics and data is themoviedb.com wiki.
The primary TV Series source for graphics and data is thetvdb.com wiki.
Users of this script are encouraged to populate both themoviedb.com and thetvdb.com with posters,
fan art and banners and meta data. The richer the source the more valuable the script.
'''

__version__=u"v0.7.9"
 # 0.1.0 Initial development
 # 0.2.0 Inital beta release
 # 0.3.0 Add mythvideo metadata updating including movie graphics through
 #       the use of tmdb.pl when the perl script exists
 # 0.3.1 Add mythvideo meta data add and update functionality. Intend use for
 #       maintenance cron jobs.
 #       Increase integration with mythtvideo download meta data and MythUI
 #       Added the ability to movie video files while maintaining the metadata
 # 0.3.2 Fixed bug where some poster downloads were unnecessary
 #       Fixed bug where the mythtv database was updated for no reason
 #       Fixed bug in jamu-example.conf "min_poster_size" variable had '=' not ':'
 #       Fixed bug where a unicode URL would abort the script
 #       Using ffmpeg added setting accurate video length in minutes. A hack but
 #       lacked python method to find audio/video properties.
 # 0.3.3 Add logic to skip any video with a inetref of '99999999'. Meta data and
 #       graphics are all manually entered and should not be altered by Jamu.
 #       Currently used for any meta data that you do not want modified by Jamu.
 #       Fixed issues with filenames containing Unicode characters.
 # 0.3.4 Added logic to skip any secondary source meta data plot less than 10 words.
 #       Properly initialized a new record so warning messages do not display.
 #       In plot meta data replace line-feeds with a space (e.g. Space Cowboys
 #       plot contains line-feeds). Mythvideo does not expect line-feeds in a plot.
 #       Significant improvements in combining meta data between primary and
 #       secondary data sources.
 #       Remove 'tmdb.pl' calls and use the tmdb api directly.
 #       Added detection of broken symbolic links and fixed those links.
 #       Fixed inconsistencies in graphics file extentions (as received from the
 #       sources), made all extentions lowercase and changed ".jpeg" to ".jpg".
 # 0.3.5 Fixed bug when themoviedb.com times out from an api request.
 #       A few documentation corrections.
 #       Fixed a bug with utf8 directory names.
 #       Added code to not abort script when themoviedb.com has problems. The issue
 #       is reported but the scripts continues processing.
 #       Added option "-W" to download graphics for Scheduled and Recorded videos.
 #       Change the "-J" Janitor function to avoid deleting graphics for Scheduled
 #       and Recorded videos.
 #       Fixed bug where a TMDB Poster image was not found when it was really
 #       available.
 # 0.3.6 Fixed bug when searching themoviedb.com for a movie by title or
 #       alternate title.
 #       Increased accuracy of non-interactive TMDB movie searching and matching.
 #       Set up for transition to TMDB's beta v2.1 api which adds language support.
 #       Corrected Watched Recording graphic file naming convention for movies.
 #       If interactive mode is selected but an exact match is found for a movie
 #       then the exact match is chosen and no interative session is initiated.
 #       Added additional messages when access to MythTV python bindings has issues.
 # 0.3.7 Removed some redundant code.
 #       Sync up with v1.0 of tvdb_api and new way to assign tvdb api key
 #       Added an option (-MG) to allow Jamu best guessing at a video's inetref
 #       number. To guess accurately the video file name must be very close to
 #       those found on tmdb or imdb and tvdb web sites.
 #       Remove all use of the MythVideo.py "pruneMetadata" routine as it deletes
 #       records from the Mythvideo table for all video files with relative file
 #       paths.
 #       Jamu will skip processing any videometadata which is using a Storage group.
 #       Jamu will now restrict itself to updating only videometadata records whose
 #       video files reside on the current host machine. In the case where a user
 #       has multiple backends jamu must run on each of those backends.
 #       The Janitor option (-MJ) now checks if the users has set the plugins
 #       MythGallery, MythGame and MythMusic to use the same graphics directories as
 #       MythVideo. If they share directories the Janitor option will exit
 #       without removing any graphics files. Messages indicating which directories
 #       are in conflict will be displayed.
 #       Added the detection of video or graphics on an NFS mount exiting jamu without
 #       any processing and displaying a message why this has been done. A new option
 #       for NFS (-MN) will allow a user to override this check and jamu will continue
 #       processing.
 #       Fixed a bug when TMDB does not have a 'year' for a movie (e.g. 'Bambi')
 #       Added compatibility with or without the MythTV.py Ticket #6678
 #       Fixed a bug when ffmpeg cannot find the true length in minutes of a video
 #       Cleaned up documenation consistency with Warning and Error messages.
 #       Added to the existing TV episode video file renaming (-MF) option.
 #       Now movie video files can also be renamed to the format "title (year)"
 #       e.g. "The Duchess (2008)". If tmdb.com has no year for the movie then only
 #       the movie title will be used when renaming. Any existing metadata is
 #       preserved.
 # 0.3.8 Made changes to sync up with MythTV trunk change set [r21138].
 #       Now handles TVDB's change from a 5 digit inetref number to 6 digits.
 # 0.3.9 Check accessability (Read and Write) to directories and files before
 #       including them in files/directories to process.
 #       Add the ability to process Storage Groups for all Videos and graphics.
 #       Jamu now uses MythVideo.py binding's Genre and Cast routines
 #       Fixed a unicode bug with file paths.
 #       Fixed a unicode bug with some URLs containing UTF8 characters.
 #       Fixed a bug were a bad image file could avbort the script.
 #       Changed all subdirectory cover art to a copied graphic file "folder.jpg/png"
 #       to conform to the Storage Group standard. This also works for local subdirs.
 #       Fixed a bug where a TV series with out a season specific poster or
 #       banner would get repeatedly download.
 # 0.4.0 Removed a few lines of debugging code which should never have been left in a
 #       distrubuted version.
 #       Fixed the check that confirms that all Video and graphic directories are
 #       read and writable.
 #       Fixed a bug where under rare circumstances a graphic would be repeatedly
 #       downloaded.
 #       Made the installation of the python IMDbPy library manditory.
 #       For all movies IMDB numbers will be used instead of converting to TMDB
 #       numbers. This is done to maintain consistency with MythVideo movie inetref
 #       numbers.
 # 0.4.1 Fixed an obscure video file rename (-F option) error
 # 0.4.2 Fixed a bug where bad data for either TMDB or TVDB would abort script
 # 0.4.3 Recent changes in the MythVideo UI graphic hunts (cover art and fanart)
 #       have made Jamu's creation of "folder.xxx" graphics redundant. This
 #       feature has been turned off in Jamu. There is a new user option
 #       "folderart" that can reactivate this feature through the Jamu
 #       configuration file.
 # 0.4.4 Changes to assist SG image hunting Jamu now adds the suffix "_coverart,
 #       fanart, _banner, _screenshot" respectively to downloaded graphics.
 #       With the use of a graphic suffix the requirement for unique graphics
 #       directories is gone. The check has been removed.
 # 0.4.5 Fixed a bug where lowercase tv video filenames caused graphics files to
 #       also be lowercase which can cause graphics to be downloaded twice.
 #       Fixed a bug in graphics file name creation for a TV season.
 #       Added checks for compatible python library versions of xml and MySQLdb
 # 0.4.6 Fixed a bug where a bad IMDB number in TMDB caused an abort.
 # 0.4.7 Fixed a bug where a 'recordedprogram' record is not properly paired with a
 #       'recorded' record. This results in no "airdate" information being available
 #       and a script abort. An airdate year of u'0000' will be assumed.
 #       Fix an abort bug when IMDB is having service problems and a list of
 #       movies cannot be retrieved.
 # 0.4.8 Fixed a bug in a -MJ option check that removing graphics would not
 #       conflict with graphic directories for non-Mythvideo plugins.
 # 0.4.9 Combine the video file extentions found in the "videotypes" table with those
 #       in Jamu to avoid possible issues in the (-MJ) option and to have tighter
 #       integration with MythVideo user file extention settings.
 # 0.5.0 Fixed a bug where a filename containing invalid characters caused an abort.
 #       Such invalid filenames are now skipped with an appropriate message.
 #       Added to the -MW option the fetching of graphics from TVDB and TMDB for
 #       videos added by Miro Bridge to either Watched Recordings or MythVideo.
 #       If Miro Bridge is not being used no additional processing is performed.
 #       Two new sections ([mb_tv] and [mb_movies]) were added to the Jamu
 #       configuration file to accomodate this new functionality.
 #       The jamu configuration file now has a default name and location of
 #       "~/.mythtv/jamu.conf". This can be overridden with the command line option.
 #       This has been done so Jamu can better support Mythbuntu.
 #       Removed code that was required until ticket #6678 was committed with
 #       change set [21191]
 #       Filtered out checks for video run length on iso, img ... etc potentially
 #       large video files due to processing overhead especially on NFS mounts.
 #       With the -MW option skip any recordings who's recgroup is "Deleted"
 #       Fixed an abort where a TVDB TV series exists for a language but does not
 #       have a series name in other languages.
 # 0.5.1 Fixed an abort when a user specifies secondary source input parameters
 #       that cannot be parsed from the file name. This
 #       covers secondary sources for metadata and graphics.
 #       Fixed an abort when thetvdb.com cannot be contact due to network or
 #       site issues.
 #       Added detection of erroneous graphics file downloads that are actually HTML
 #       due to source Web site issues. Jamu's (-MJ) janitor option also detects,
 #       deletes these files and repairs the MythVideo record if necessary.
 #       For the -MW option any downloaded graphics names will use the title of the
 #       recorded program instead of that found on sources like TVDB and TMDB. This
 #       resolves Watch Recordings image hunt issues when Schedule Direct uses a
 #       different program title then is on either TVDB or TMDB.
 #       Fixed an obscure bug where TVDB returns empty graphics URLs along with
 #       proper URLs. The empty URLs are now ignored.
 #       Fixed a bug when a language was specified and there were no graphics
 #       for the specified language none were returned/downloaded. This even when
 #       graphics for other languages were available. Now if there are no selected
 #       language graphics English graphics are the fall back and if there are no
 #       English graphics then any available graphics will be returned.
 # 0.5.2 Fixed an abort when trying to add a storage group graphics without a
 #       proper file path.
 # 0.5.3 Fixed a bug where the filemarkup table is not cleaned up if Jamu renames
 #       a Miro movie trailer video file that the user wants to keep in MythVideo.
 #       Fixed a bug with Miro video file renaming of Miro Movie trailers
 #       for the same movie but which had different file extentions.
 # 0.5.4 Conform to changeset 22104 setting of SG graphics directories to default to SG Videos if not configured.
 # 0.5.5 Deal with TV Series and Movie titles with a "/" forward slash in their name e.g. "Face/Off"
 # 0.5.6 Correct an issue when a user has a mixture of local and SG video records in MythVideo. Jamu was
 #       adding a hostname when the video had an absolute path. This caused issues with playback.
 #       Added more informative error messages when TMDB is returning bad xml responses.
 #       Fixed an error in the graphic file naming convention when graphics share the same download directory.
 # 0.5.7 Remove the override of the TVDB graphics URL to the mirror site. See Kobe's comment:
 #       http://forums.thetvdb.com/viewtopic.php?f=4&t=2161#p9089
 # 0.5.8 The issue fixed in v0.5.5 with invalid file name creation did not fully cover TV shows It does now.
 # 0.5.9 Changed permissions checks on video directories to only require RW for the destination directories
 #       involved in the move. With this change if a user requested a file rename (-F) option and the video
 #       file does not have RW access the rename will be ignored.
 #       Uses that have their Video directories set to access and read-only can now use Jamu.
 #       Added a stdout display of the directories that Jamu will use for processing. This information may help
 #       users resolve issues. The display happens ONLY when the -V (verbose) option is used.
 # 0.6.0 Changed The Janitor -J option to deal with graphics associated with a VIDEO_TS directory.
 #       Stopped Jamu from processing any files in a "VIDEO_TS" directory as it was leading to multiple
 #       MythVideo entires for *.VOB files. Jamu does not process multi-part videos.
 #       Added the use of PID files to prevent two instances of the same Jamu -M options running at the same
 #       time. This deals with issues when a meta data source is off line for an extended
 #       period of time and Jamu runs as a cronjob. Options effected are (-M, -MW and -MG).
 #       Change to have jamu use the TMDB Movie title as is done in MythVideo rather than the file name.
 #       Fixed a bug when TMDB genres are filtered and none remain they were still being added. This bug was
 #       spotted and correct by Mathieu Brabant (thanks).
 #       Added the ability for a user to filter additional characters from a file name this is important for
 #       users using MS-Windows file systems as a CIFS mount.
 # 0.6.1 Added directory name parsing support for TV series title, series number and episode numbers. Patch
 #       contributed by Mitko Haralanov (thanks).
 # 0.6.2 Added updating the 'homepage', 'releasedata' and 'hash' fields in the videometadata table
 #       is the field exists. These fields is only present in trunk.
 #       Properly initalize the homepage, hash, releasedate fields when adding a new videometadata record.
 # 0.6.3 Convert to new python bindings and replace all direct mysql data base calls. See ticket #7264
 #       Remove the requirement for the MySQLdb python library.
 #       Removed the folder icon symlink code as it is redundant with MythVideo internal functionality.
 #       The 'folderart' option is no longer support on a jamu.conf file and will be ignored if present.
 #       Fixed a bug where a FE video directory was set but there were no FE image directories.
 #       If there were local SG images directories set they were being used. This is an invalid
 #       configuration that should have caused an error.
 # 0.6.4 Added a new option (-R) to allow just interactively populate the video reference numbers from
 #       TVDB and TMDB without any meta data downloads. After that the user runs Jamu with the -M option
 #       and the meta data and images are downloaded.
 #       Added to the interactive interface the ability to select a reference number of '99999999'
 #       which effectively tells jamu to ignore the specific video from all further processing.
 #       Changed the return code from 1 to 0 when Jamu exits without processing if there is already an
 #       instance of Jamu running. This was causing issues for Mythbuntu when TVDB or TMDB was down for an
 #       extended period.
 #       Added a new jamu.conf section [ignore-directory] to list Video sub-directories that Jamu should
 #       ignore.
 #       Change Jamu's import of tvdb and tmdb api libraries to use the installed versions found with the
 #       MythTV python bindings.
 #       Changed Jamu to use the tmdb API v2.0 python library
 #       Jamu will always use the TMDB reference number over the IMDB number but still supports IMDB#s
 #       Jamu interactive sessions for movies now lists the TMDB#s instead of IMDB#s
 #       Jamu will convert any IMDB#s to TMDB#s when themoviedb.org includes that movie. This is in line
 #       with MythVideo changes. Graphics for the movie will also be renamed so they do not need to be
 #       re-downloaded.
 #       Add the production countries for movies when TMDB provides that information.
 #       Adjusted the -MW option to add a " Season 1" to any downloaded image filename for TV shows.
 #       This must be done to make sure that TV shows like "24" do not clash with a movie's TMDB# like
 #       (Kill Bill Vol.1) which is "24".
 #       Added message display for exceptions where the message may enhance problem analysis.
 #       Removed logic which checked that a TV episode was using Season graphics rather than Series graphics.
 #       Unfortunately there was a chance that the a Series's graphics could clobber a movie with the same TMDB#
 #       as the series title (e.g. the movie Kill Bill Vol.1 and the TV series 24). A positive is that a number
 #       of redundant TV Series images can be removed through the jamu -MJ option.
 #       Improved the -MW options detection of TV series when the EPG data does not include a subtitle. Users
 #       can add the specific TVDB numbers to the 'series_name_override' section of the jamu.conf file.
 #       Australian users had mentioned this as an issue, previously the TV series was always being mistaken
 #       for a movie.
 #       Jamu will now download the top rated TV Series season coverart and banner images. This enhancement
 #       matches MythVideo processing.
 # 0.6.5 Small fix related to the bindings changes.
 # 0.6.6 Fixed Exception messages
 #       Change all occurances of 'mythbeconn.host' to 'mythbeconn.hostname' to be consistent with bindings
 # 0.6.7 Fixed the (-J) janitor option from removing the Mirobridge default images when they are not being used
 # 0.6.8 Fixed a (-J) janitor option statistics error due to skipping Mirbridge default images
 # 0.6.9 Fixed an abort when IMDBpy returns movie matches with incomplete data
 #       Fixed an abort where an IMDB# was being used instead of a TMDB#
 #       Fixed an abort when a storage directory name caused an UnicodeEncodeError or TypeError exception
 # 0.7.0 Fixed an (-MW) option abort when a recorded program or upcoming program did not have a title
 # 0.7.1 Fixed a bug where movies with punctutation ("Mr. Magoo") were not finding matches
 #       Fixed bug with interactive mode when a user enters a reference number directly rather than
 #       making a list selection
 #       These bugs were both identified by Edi Iten (thanks)
 # 0.7.2 Fixed a bug where an inetref field was not properly initialized and caused an abort. Ticket #8243
 # 0.7.3 Fixed a bug where a user selected TMDB# was not being used.
 #       Minor change to fuzzy matching of a file named parsed title with those from TMDB and TVDB.
 # 0.7.4 Update for changes in Python bindings
 # 0.7.5 Added the TMDB MovieRating as videometadata table "rating" field
 # 0.7.6 Modifications to support MythTV python bindings changes
 # 0.7.7 Pull hostname from python bindings instead of socket libraries
 # 0.7.8 Replace uses of MythVideo.getVideo()
 # 0.7.9 Deal with jamu.conf entries that have unicode characters


usage_txt=u'''
JAMU - Just.Another.Metadata.Utility is a versatile utility for downloading graphics and meta data
for both movies and TV Series information from themoviedb.com wiki and thetvdb.com wiki. In addition
the MythTV data base is updated with the downloaded information.
Here are the main uses for this utility:
MythTV users should review the Jamu wiki page at http://www.mythtv.org/wiki/Jamu for details.

1) Simple command line invocation to display or download data from thetvdb.com.
   Data can be one or more of: Posters/Cover art, Banners, Fan art,
   Episode Images and Episode meta data. use the command "jamu -e | less" to see
   command line examples.
2) Mass downloads of data matching your video files. **
   This typically done once to download the information for your video collection.
3) Automated maintenance of the information in your video collection. **
4) The creation of video file names which can be used to set the file name of your recorded TV shows.
   File names can be formated to the users preference with information like series name, season number,
   episode number and episode name. MythTV users may find this valuable as part of a user job
   that is spawned automatically by mythbackend when recording is finished.
5) Jamu's modules can be imported into your own python scripts to create enhanced functionality.
6) With the installation of free ImageMagick's utilities (specifically 'mogrify') you can resize
   graphics when they are downloaded.
7) Update the MythTV data base with links to posters, banners, fanart and episode images and optionally
   download missing graphics if they exist. This feature can be used for mass updates and regular
   maintenance.

'''

examples_txt=u'''
MythTV users should review the Jamu wiki page at http://www.mythtv.org/wiki/Jamu for details.
These examples are primarily for non-MythTV users of Jamu.

jamu command line examples:
NOTE: Included here are simple examples of jamu in action.
      Please review jamu_README for advise on how to get the most out of jamu.

( Display a TV series top rated poster fanart and banner URLs)
> jamu -tS PBF "Sanctuary"
poster:http://www.thetvdb.com/banners/posters/80159-1.jpg
fanart:http://www.thetvdb.com/banners/fanart/original/80159-2.jpg
banner:http://www.thetvdb.com/banners/graphical/80159-g2.jpg

( Display the URL for a TV series episode )
> jamu -tS I "Fringe" 1 5
filename:http://www.thetvdb.com/banners/episodes/82066-391049.jpg

( Display poster, fanart and banner graphics for a TV series but limited to two per type in a season )
> jamu -S PBF -m 2 "24" 4
poster:http://www.thetvdb.com/banners/seasons/76290-4-3.jpg
poster:http://www.thetvdb.com/banners/seasons/76290-4.jpg
fanart:http://www.thetvdb.com/banners/fanart/original/76290-1.jpg
fanart:http://www.thetvdb.com/banners/fanart/original/76290-2.jpg
banner:http://www.thetvdb.com/banners/seasonswide/76290-4.jpg
banner:http://www.thetvdb.com/banners/seasonswide/76290-4-3.jpg

( Display a file name string (less file extention and directory path) for a TV episode )
> jamu -F "24" 4 3
24 - S04E03 - Day 4: 9:00 A.M.-10:00 A.M.

> jamu -F "24" "Day 4: 9:00 A.M.-10:00 A.M."
24 - S04E03 - Day 4: 9:00 A.M.-10:00 A.M.

( Using SID number instead of series name )
> jamu -F 76290 4 3
24 - S04E03 - Day 4: 9:00 A.M.-10:00 A.M.

( Simulate a dry run for the download of a TV series top rated poster and fanart )
> jamu -sdtS PF "Fringe"
Simulation download of URL(http://www.thetvdb.com/banners/posters/82066-6.jpg) to File(~/Pictures/Poster - 82066-6.jpg)
Get_Poster downloading successfully processed
Simulation download of URL(http://www.thetvdb.com/banners/fanart/original/82066-11.jpg) to File(~/Pictures/Fanart - 82066-11.jpg)
Get_Fanart downloading successfully processed

( Download the Episode meta data and episode image for a video file whose file name contains the series and season/episode numbers)
> jamu -dS EI "~/Pictures/Fringe - S01E01.mkv"
Episode meta data and/or images downloads successfully processed
> ls -ls
total 2
60 -rw-r--r-- 1 user user 53567 2009-03-12 22:05 Fringe - S01E01 - Pilot.jpg
 4 -rw-r--r-- 1 user user  1059 2009-03-12 22:05 Fringe - S01E01 - Pilot.meta
 4 -rw-r--r-- 1 user user   811 2009-03-12 13:22 Fringe - S01E01.mkv

( Display Episode meta data for a TV series )
> jamu -S E "24" 5 3
series:24
seasonnumber:5
episodenumber:3
episodename:Day 5: 9:00 A.M.-10:00 A.M.
rating:None
overview:Jack conceals himself inside the airport hanger and surveys the Russian separatists, feeding information to Curtis and his assault team.
The terrorists begin executing hostages in an attempt to make Logan cave into their demands.
Martha discovers that all traces of her conversation with Palmer may not have been erased.
director:Brad Turner
writer:Manny Coto
gueststars:John Gleeson Connolly, V.J. Foster, David Dayan Fisher, Taylor Nichols, Steve Edwards, Taras Los, Joey Munguia, Reggie Jordan, Lou Richards, Karla Zamudio
imdb_id:None
filename:http://www.thetvdb.com/banners/episodes/76290-306117.jpg
epimgflag:None
language:en
firstaired:2006-01-16
lastupdated:1197942225
productioncode:5AFF03
id:306117
seriesid:76290
seasonid:10067
absolute_number:None
combined_season:5
combined_episodenumber:4.0
dvd_season:5
dvd_discid:None
dvd_chapter:None
dvd_episodenumber:4.0

( Specify a user defined configuration file to set most of the configuration variables )
> jamu -C "~/.jamu/jamu.conf" -S P "Supernatural"
poster:http://www.thetvdb.com/banners/posters/78901-3.jpg
poster:http://www.thetvdb.com/banners/posters/78901-1.jpg

( Display in alphabetical order the state of all configuration variables )
> jamu -f
allgraphicsdir (~/Pictures)
bannerdir (None)
config_file (False)
data_flags (None)
debug_enabled (False)
download (False)
... lots of configuration variables ...
video_dir (None)
video_file_exts (['3gp', 'asf', 'asx', 'avi', 'mkv', 'mov', 'mp4', 'mpg', 'qt', 'rm', 'swf', 'wmv', 'm2ts', 'evo', 'ts', 'img', 'iso'])
with_ep_name (%(series)s - S%(seasonnumber)02dE%(episodenumber)02d - %(episodename)s.%(ext)s)
without_ep_name (%(series)s - S%(seasonnumber)02dE%(episodenumber)02d.%(ext)s)
'''

# System modules
import sys, os, re, locale, subprocess, locale, ConfigParser, urllib, codecs, shutil, datetime, fnmatch, string
from datetime import date
from optparse import OptionParser
from socket import gethostbyname
import tempfile, struct
import logging

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
            try:
                self.out.write(obj.encode(self.encoding))
            except IOError:
                pass
        else:
            try:
                self.out.write(obj)
            except IOError:
                pass

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')

try:
    import xml
except Exception, e:
    print '''The python module xml must be installed. error(%s)''' % e
    sys.exit(1)
if xml.__version__ < u'41660':
    print '''
\n! Warning - The module xml (v41660 or greater) must be installed. Your version is different (v%s) than what Jamu was tested with. Jamu may not work on your installation.\nIt is recommended that you upgrade.\n''' % xml.__version__
import xml.etree.cElementTree as ElementTree


# Find out if the MythTV python bindings can be accessed and instances can be created
try:
    '''If the MythTV python interface is found, we can insert data directly to MythDB or
    get the directories to store poster, fanart, banner and episode graphics.
    '''
    from MythTV import MythDB, Video, MythVideo, MythBE, MythError, MythLog, RecordedProgram
    from MythTV.database import DBData
    mythdb = None
    mythvideo = None
    mythbeconn = None
    try:
        '''Create an instance of each: MythDB, MythVideo
        '''
        MythLog._setlevel('none') # Some non option -M cannot have any logging on stdout
        mythdb = MythDB()
        mythvideo = MythVideo(mythdb)
        MythLog._setlevel('important,general')
    except MythError, e:
        print u'\n! Warning - %s' % e.args[0]
        filename = os.path.expanduser("~")+'/.mythtv/config.xml'
        if not os.path.isfile(filename):
            print u'\n! Warning - A correctly configured (%s) file must exist\n' % filename
        else:
            print u'\n! Warning - Check that (%s) is correctly configured\n' % filename
    except Exception, e:
        print u"\n! Warning - Creating an instance caused an error for one of: MythDB or MythVideo, error(%s)\n" % e
    localhostname = mythdb.gethostname()
    try:
        MythLog._setlevel('none') # Some non option -M cannot have any logging on stdout
        mythbeconn = MythBE(backend=localhostname, db=mythdb)
        MythLog._setlevel('important,general')
    except MythError, e:
        print u'\nWith any -M option Jamu must be run on a MythTV backend'
        print u'! Warning - %s' % e.args[0]
        mythbeconn = None
except Exception, e:
    print u"\n! Warning - MythTV python bindings could not be imported, error(%s)\n" % e
    mythdb = None
    mythvideo = None
    mythbeconn = None


# Verify that tvdb_api.py, tvdb_ui.py and tvdb_exceptions.py are available
try:
    # thetvdb.com specific modules
    import MythTV.ttvdb.tvdb_ui as tvdb_ui
    # from tvdb_api import Tvdb
    import MythTV.ttvdb.tvdb_api as tvdb_api
    from MythTV.ttvdb.tvdb_exceptions import (tvdb_error, tvdb_shownotfound, tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_episodenotfound, tvdb_attributenotfound, tvdb_userabort)

    # verify version of tvdbapi to make sure it is at least 1.0
    if tvdb_api.__version__ < '1.0':
        print "\nYour current installed tvdb_api.py version is (%s)\n" % tvdb_api.__version__
        raise
except Exception, e:
    print '''
The modules tvdb_api.py (v1.0.0 or greater), tvdb_ui.py, tvdb_exceptions.py and cache.py.
They should have been installed along with the MythTV python bindings.
Error(%s)
''' % e
    sys.exit(1)


try:
    import MythTV.tmdb.tmdb_api as tmdb_api
    from MythTV.tmdb.tmdb_exceptions import (TmdBaseError, TmdHttpError, TmdXmlError, TmdbUiAbort, TmdbMovieOrPersonNotFound,)
except Exception, e:
    sys.stderr.write('''
The subdirectory "tmdb" containing the modules tmdb_api.py (v0.1.3 or greater), tmdb_ui.py,
tmdb_exceptions.py must have been installed with the MythTV python bindings.
Error:(%s)
''' %  e)
    sys.exit(1)

if tmdb_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed tmdb_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % tmdb_api.__version__)
    sys.exit(1)


imdb_lib = True
try:            # Check if the installation is equiped to directly search IMDB for movies
    import imdb
except ImportError, e:
    sys.stderr.write("\n! Error: To search for movies movies the IMDbPy library must be installed."\
        "Check your installation's repository or check the following link."\
        "from (http://imdbpy.sourceforge.net/?page=download)\nError:(%s)\n" % e)
    sys.exit(1)

if imdb_lib:
    if imdb.__version__ < "3.8":
        sys.stderr.write("\n! Error: You version the IMDbPy library (%s) is too old. You must use version 3.8 of higher." % imdb.__version__)
        sys.stderr.write("Check your installation's repository or check the following link."\
            "from (http://imdbpy.sourceforge.net/?page=download)\n")
        sys.exit(1)

class VideoTypes( DBData ):
    _table = 'videotypes'
    _where = 'intid=%s'
    _setwheredat = 'self.intid,'
    _logmodule = 'Python VideoType'
    def __str__(self):
        return "<VideoTypes '%s'>" % self.extension
    def __repr__(self):
        return str(self).encode('utf-8')
    def __init__(self, id=None, ext=None, db=None):
        if id is not None:
            DBData.__init__(self, data=(id,), db=db)
        elif ext is not None:
            self.__dict__['_where'] = 'extension=%s'
            self.__dict__['_wheredat'] = 'self.extension,'
            DBData.__init__(self, data=(ext,), db=db)
        else:
            DBData.__init__(self, None, db=db)
# end VideoTypes()

def isValidPosixFilename(name, NAME_MAX=255):
    """Checks for a valid POSIX filename

    Filename: a name consisting of 1 to {NAME_MAX} bytes used to name a file.
        The characters composing the name may be selected from the set of
        all character values excluding the slash character and the null byte.
        The filenames dot and dot-dot have special meaning.
        A filename is sometimes referred to as a "pathname component".

    name: (base)name of the file
    NAME_MAX: is defined in limits.h (implementation-defined constants)
              Maximum number of bytes in a filename
              (not including terminating null).
              Minimum Acceptable Value: {_POSIX_NAME_MAX}
              _POSIX_NAME_MAX: Maximum number of bytes in a filename
                               (not including terminating null).
                               Value: 14

    More information on http://www.opengroup.org/onlinepubs/009695399/toc.htm
    """
    return 1<=len(name)<= NAME_MAX and "/" not in name and "\000" not in name
# end isValidPosixFilename()


# Two routines used for movie title search and matching
def is_punct_char(char):
    '''check if char is punctuation char
    return True if char is punctuation
    return False if char is not punctuation
    '''
    return char in string.punctuation

def is_not_punct_char(char):
    '''check if char is not punctuation char
    return True if char is not punctuation
    return False if chaar is punctuation
    '''
    return not is_punct_char(char)

def _getExtention(URL):
    """Get the graphic file extension from a URL
    return the file extention from the URL
    """
    (dirName, fileName) = os.path.split(URL)
    (fileBaseName, fileExtension)=os.path.splitext(fileName)
    return fileExtension[1:]
# end getExtention

def _getFileList(dst):
    ''' Create an array of fully qualified file names
    return an array of file names
    '''
    file_list = []
    names = []

    try:
        for directory in dst:
            try:
                directory = unicode(directory, 'utf8')
            except (UnicodeEncodeError, TypeError):
                pass
            for filename in os.listdir(directory):
                names.append(os.path.join(directory, filename))
    except OSError, e:
        sys.stderr.write(u"\n! Error: Getting a list of files for directory (%s)\nThis is most likely a 'Permission denied' error\nError:(%s)\n\n" % (dst, e))
        return file_list

    for video_file in names:
        if os.path.isdir(video_file):
            new_files = _getFileList([video_file])
            for new_file in new_files:
                file_list.append(new_file)
        else:
            file_list.append(video_file)
    return file_list
# end _getFileList


class singleinstance(object):
    '''
    singleinstance - based on Windows version by Dragan Jovelic this is a Linux
                     version that accomplishes the same task: make sure that
                     only a single instance of an application is running.
    '''

    def __init__(self, pidPath):
        '''
        pidPath - full path/filename where pid for running application is to be
                  stored.  Often this is ./var/<pgmname>.pid
        '''
        from os import kill
        self.pidPath=pidPath
        #
        # See if pidFile exists
        #
        if os.path.exists(pidPath):
            #
            # Make sure it is not a "stale" pidFile
            #
            try:
                pid=int(open(pidPath, 'r').read().strip())
                #
                # Check list of running pids, if not running it is stale so
                # overwrite
                #
                try:
                    kill(pid, 0)
                    pidRunning = 1
                except OSError:
                    pidRunning = 0
                if pidRunning:
                    self.lasterror=True
                else:
                    self.lasterror=False
            except:
                self.lasterror=False
        else:
            self.lasterror=False

        if not self.lasterror:
            #
            # Write my pid into pidFile to keep multiple copies of program from
            # running.
            #
            fp=open(pidPath, 'w')
            fp.write(str(os.getpid()))
            fp.close()

    def alreadyrunning(self):
        return self.lasterror

    def __del__(self):
        if not self.lasterror:
            import os
            os.unlink(self.pidPath)
    # end singleinstance()


# Global variables
graphicsDirectories = {'banner': u'bannerdir', 'screenshot': u'episodeimagedir', 'coverfile': u'posterdir', 'fanart': u'fanartdir'}
dir_dict={'posterdir': "VideoArtworkDir", 'bannerdir': 'mythvideo.bannerDir', 'fanartdir': 'mythvideo.fanartDir', 'episodeimagedir': 'mythvideo.screenshotDir', 'mythvideo': 'VideoStartupDir'}
storagegroupnames = {u'Videos': u'mythvideo', u'Coverart': u'posterdir', u'Banners': u'bannerdir', u'Fanart': u'fanartdir', u'Screenshots': u'episodeimagedir'}
storagegroups={u'mythvideo': [], u'posterdir': [], u'bannerdir': [], u'fanartdir': [], u'episodeimagedir': []} # The gobal dictionary is only populated with the current hosts storage group entries
image_extensions = ["png", "jpg", "bmp"]

def getStorageGroups():
    '''Populate the storage group dictionary with the local host's storage groups.
    return nothing
    '''
    records = mythdb.getStorageGroup(hostname=localhostname)
    for record in records:
        # Only include Video, coverfile, banner, fanart, screenshot and trailers storage groups
        if record.groupname in storagegroupnames.keys():
            dirname = record.dirname
            try:
                dirname = unicode(record.dirname, 'utf8')
            except (UnicodeDecodeError):
                sys.stderr.write(u"\n! Error: The local Storage group (%s) directory contained\ncharacters that caused a UnicodeDecodeError. This storage group has been rejected.'\n" % (record['groupname']))
                continue    # Skip any line that has non-utf8 characters in it
            except (UnicodeEncodeError, TypeError):
                pass
            # Strip the trailing slash so it is consistent with all other directory paths in Jamu
            if dirname[-1:] == u'/':
                storagegroups[storagegroupnames[record.groupname]].append(dirname[:-1])
            else:
                storagegroups[storagegroupnames[record.groupname]].append(dirname)
        continue

    any_storage_group = False
    tmp_storagegroups = dict(storagegroups)
    for key in tmp_storagegroups.keys():
        if len(tmp_storagegroups[key]):
            any_storage_group = True
        else:
            del storagegroups[key]    # Remove empty SG directory arrays
    if any_storage_group:
        # Verify that each storage group is an existing local directory
        storagegroup_ok = True
        for key in storagegroups.keys():
            for directory in storagegroups[key]:
                if not os.access(directory, os.F_OK):
                    sys.stderr.write(u"\n! Error: The local Storage group (%s) directory (%s) does not exist\n" % (key, directory))
                    storagegroup_ok = False
        if not storagegroup_ok:
            sys.exit(1)
# end getStorageGroups


def _can_int(x):
    """Takes a string, checks if it is numeric.
    >>> _can_int("2")
    True
    >>> _can_int("A test")
    False
    """
    if x == None:
        return False
    try:
        int(x)
    except ValueError:
        return False
    else:
        return True
# end _can_int

class BaseUI:
    """Default non-interactive UI, which auto-selects first results
    """
    def __init__(self, config, log):
        self.config = config
        self.log = log

    def selectSeries(self, allSeries):
        return allSeries[0]

    def selectMovieOrPerson(self, allElements):
        return makeDict([allElements[0]])

# Local variable
video_type = u''
UI_title = u''
UI_search_language = u''
UI_selectedtitle = u''
# List of language from http://www.thetvdb.com/api/0629B785CE550C8D/languages.xml
# Hard-coded here as it is realtively static, and saves another HTTP request, as
# recommended on http://thetvdb.com/wiki/index.php/API:languages.xml
UI_langid_dict = {u'da': u'10', 'fi': u'11', u'nl': u'13', u'de': u'14', u'it': u'15', u'es': u'16', u'fr': u'17', u'pl': u'18', u'hu': u'19', u'el': u'20', u'tr': u'21', u'ru': u'22', u'he': u'24', u'ja': u'25', u'pt': u'26', u'zh': u'27', u'cs': u'28', u'sl': u'30', u'hr': u'31', u'ko': u'32', u'en': '7', u'sv': u'8', u'no': u'9',}

class jamu_ConsoleUI(BaseUI):
    """Interactively allows the user to select a show or movie from a console based UI
    """

    def _displaySeries(self, allSeries_array):
        """Helper function, lists series with corresponding ID
        """
        if video_type == u'IMDB':
            URL = u'http://www.imdb.com/title/tt'
            URL2 = u'http://www.imdb.com/find?s=all&q='+urllib.quote_plus(UI_title.encode("utf-8"))+'&x=0&y=0'
            reftype = u'IMDB'
        elif video_type == u'TMDB':
            URL = u'http://themoviedb.org/movie/'
            URL2 = u'http://themoviedb.org/'
            reftype = u'TMDB'
        else: # TVDB
            URL = u'http://thetvdb.com/index.php?tab=series&id=%s&lid=%s'
            URL2 = u'http://thetvdb.com/?tab=advancedsearch'
            reftype = u'thetvdb'
            tmp_title = u''

        allSeries={}
        for index in range(len(allSeries_array)):
            allSeries[allSeries_array[index]['name']] = allSeries_array[index]
        tmp_names = allSeries.keys()
        tmp_names.sort()
        most_likely = []

        # Find any TV Shows or Movies who's titles start with the video's title
        for name in tmp_names:
            if filter(is_not_punct_char, name.lower()).startswith(filter(is_not_punct_char, UI_title.lower())):
                most_likely.append(name)

        # IMDB can return titles that are a movies foriegn title. The titles that do not match
        # the requested title need to be added to the end of the most likely titles list.
        if video_type == u'IMDB' and len(most_likely):
            for name in tmp_names:
                try:
                    dummy = most_likely.index(name)
                except ValueError:
                    most_likely.append(name)

        names = []
        # Remove any name that does not start with a title like the TV Show/Movie (except for IMDB)
        if len(most_likely):
            for likely in most_likely:
                names.append(likely)
        else:
            names = tmp_names

        if not video_type == u'IMDB':
            names.sort()

        # reorder the list of series and sid's
        new_array=[]
        for key in names: # list all search results
            new_array.append(allSeries[key])

        # If there is only one to select and it is an exact match then return with no interface display
        if len(new_array) == 1:
            if filter(is_not_punct_char, allSeries_array[0]['name'].lower()) == filter(is_not_punct_char, UI_title.lower()):
                return new_array

        # Add the ability to select the skip inetref of '99999999'
        new_array.append( {'sid': '99999999', 'name': u'User choses to ignore video'} )
        names.append(u'User choses to ignore video')

        i_show=0
        for key in names: # list all search results
            i_show+=1 # Start at more human readable number 1 (not 0)
            if key == u'User choses to ignore video':
                print u"% 2s -> %s # %s" % (
                    i_show,
                    '99999999', "Set this video to be ignored by Jamu with a reference number of '99999999'"
                )
                continue
            if video_type != u'IMDB' and video_type != u'TMDB':
                tmp_URL = URL % (allSeries[key]['sid'], UI_langid_dict[UI_search_language])
                print u"% 2s -> %s # %s" % (
                    i_show,
                    key, tmp_URL
                )
            else:
                print u"% 2s -> %s # %s%s" % (
                    i_show,
                    key, URL,
                    allSeries[key]['sid']
                )
        print u"Direct search of %s # %s" % (
            reftype,
            URL2
        )
        return new_array

    def selectSeries(self, allSeries):
        global UI_selectedtitle
        UI_selectedtitle = u''
        allSeries = self._displaySeries(allSeries)

        # Check for an automatic choice
        if len(allSeries) <= 2:
            for series in allSeries:
                if filter(is_not_punct_char, series['name'].lower()) == filter(is_not_punct_char, UI_title.lower()):
                    UI_selectedtitle = series['name']
                    return series

        display_total = len(allSeries)

        if video_type == u'IMDB':
            reftype = u'IMDB #'
            refsize = 7
            refformat = u"%07d"
        elif video_type == u'TMDB':
            reftype = u'TMDB #'
            refsize = 5
            refformat = u"%05d"
        else:
            reftype = u'Series id'
            refsize = 5
            refformat = u"%6d"    # Attempt to have the most likely TV/Movies at the top of the list

        while True: # return breaks this loop
            try:
                print u'Enter choice ("Enter" key equals first selection (1)) or input the %s directly, ? for help):' % reftype
                ans = raw_input()
            except KeyboardInterrupt:
                raise tvdb_userabort(u"User aborted (^c keyboard interupt)")
            except EOFError:
                raise tvdb_userabort(u"User aborted (EOF received)")

            self.log.debug(u'Got choice of: %s' % (ans))
            try:
                if ans == '':
                    selected_id = 0
                else:
                    selected_id = int(ans) - 1 # The human entered 1 as first result, not zero
            except ValueError: # Input was not number
                if ans == u"q":
                    self.log.debug(u'Got quit command (q)')
                    raise tvdb_userabort(u"User aborted ('q' quit command)")
                elif ans == u"?":
                    print u"## Help"
                    print u"# Enter the number that corresponds to the correct video."
                    print u"# Enter the %s number for the %s." % (reftype, video_type)
                    print u"# ? - this help"
                    print u"# q - abort"
                else:
                    self.log.debug(u'Unknown keypress %s' % (ans))
            else:
                self.log.debug(u'Trying to return ID: %d' % (selected_id))
                try:
                    UI_selectedtitle = allSeries[selected_id]['name']
                    return allSeries[selected_id]
                except IndexError:
                    if len(ans) == refsize and reftype != u'Series id':
                        UI_selectedtitle = u''
                        return {'name': u'User input', 'sid': ans}
                    elif reftype == u'Series id':
                        if len(ans) >= refsize:
                            UI_selectedtitle = u''
                            return {'name': u'User input', 'sid': ans}
                    self.log.debug(u'Invalid number entered!')
                    print u'Invalid number (%d) input! A directly entered %s must be a full %d zero padded digits (e.g. 905 should be entered as %s)' % (selected_id, reftype, refsize, refformat % 905)
                    UI_selectedtitle = u''
                    self._displaySeries(allSeries)
            #end try
        #end while not valid_input

def _useImageMagick(cmd):
    """ Process graphics files using ImageMagick's utility 'mogrify'.
    >>> _useImageMagick('-resize 50% "poster.jpg"')
    >>> 0
    >>> -1
    """
    return subprocess.call(u'mogrify %s > /dev/null' % cmd, shell=True)
# end verifyImageMagick

# Call a execute a command line process
def callCommandLine(command):
    '''Call a command line script or program. Display any errors
    return all stdoutput as a string
    '''
    p = subprocess.Popen(command, shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)

    while 1:
        data = p.stderr.readline()
        if len(data):
            sys.stderr.write(u'%s\n' % data)
        if data == '' and p.poll() != None:
            break

    returned_data=''
    while 1:
        data = p.stdout.readline()
        if len(data):
            returned_data+=data
        if data == '' and p.poll() != None:
            break
    return returned_data
# end callCommandLine


# All functionality associated with configuration options
class Configuration(object):
    """Set defaults, apply user configuration options, validate configuration settings and display the
    settings.
    To view all available options run:
    >>> config = Configuration()
    >>> config.displayOptions()
    """
    def __init__(self, interactive = False, debug = False):
        """Initialize default configuration settings
        """
        self.config = {}
        # Set all default variables
        self.config['interactive'] = interactive
        self.config['debug_enabled'] = debug
        self.config['flags_options'] = False
        self.config['local_language'] = u'en'
        self.config['simulation'] = False
        self.config['toprated'] = False
        self.config['download'] = False
        self.config['nokeys'] = False
        self.config['maximum'] = None
        self.config['user_config'] = None
        self.config['overwrite'] = False
        self.config['update'] = False
        self.config['mythtvdir'] = False
        self.config['hd_dvd'] = ' HD - On DVD'     # Used for HD DVD collection zero length video files
        self.config['dvd'] = ' - On DVD'        # Used for DVD collection zero length video files

        self.config['video_dir'] = None
        self.config['recursive'] = True
        self.config['series_name'] = None
        self.config['sid'] = None
        self.config['season_num'] = None
        self.config['episode_num'] = None
        self.config['episode_name'] = None
        self.config['ret_filename'] = False

        # Flags for which data to perform actions on
        self.config['get_poster'] = False
        self.config['get_banner'] = False
        self.config['get_fanart'] = False
        self.config['get_ep_image'] = False
        self.config['get_ep_meta'] = False
        self.config['data_flags'] = ''
        self.config['tmdb_genre_filter'] = ['action film', 'adventure film', 'comedy', 'crime film', 'disaster film', 'documentary film', 'drama film', 'eastern', 'environmental', 'fantasy film', 'historical film', 'horror film', 'musical film', 'mystery', 'mystery film', 'road movie', 'science fiction film', 'sport', 'thriller', 'western', 'film noir', 'cult movie', 'neo-noir', 'guy movie',]

        self.config['log'] = self._initLogger() # Setups the logger (self.log.debug() etc)

        # The default format of the file names (with and without episode names)
        self.config['with_ep_name'] = u'%(series)s - S%(seasonnumber)02dE%(episodenumber)02d - %(episodename)s.%(ext)s'
        self.config['without_ep_name'] = u'%(series)s - S%(seasonnumber)02dE%(episodenumber)02d.%(ext)s'
        self.config['ep_metadata'] = self.config['with_ep_name']

        # The default format of the graphics file names (with and without seasons and/or episode names)
        # The default is to use the URL's filename from thetvdb.com
        self.config['g_defaultname']=True
        # e.g. "Fringe - 01.jpg"
        self.config['g_series'] = u'%(series)s - %(seq)s.%(ext)s'
        # e.g. "SG-1 - 07-02.jpg"
        self.config['g_season'] = u'%(series)s - %(seasonnumber)02d-%(seq)s.%(ext)s'

        # Set default configuration variables
        # Start - Variables the user can override through option "-u" with their own file of variables
        self.config['allgraphicsdir'] = os.getcwd()
        self.config['posterdir'] = None
        self.config['bannerdir'] = None
        self.config['fanartdir'] = None
        self.config['episodeimagedir'] = None
        self.config['metadatadir'] = None
        self.config['mythtvmeta'] = False
        self.config['myth_secondary_sources'] = {}
        self.config['posterresize'] = False
        self.config['fanartresize'] = False
        self.config['min_poster_size'] = 400
        self.config['image_library'] = False
        self.config['ffmpeg'] = True
        self.config['folderart'] = False
        self.config['metadata_exclude_as_update_trigger'] = ['intid', 'season', 'episode', 'showlevel', 'filename', 'coverfile', 'childid', 'browse', 'playcommand', 'trailer', 'host', 'screenshot', 'banner', 'fanart']
        self.config['filename_char_filter'] = u"/%\000"
        self.config['ignore-directory'] = []


        # Dictionaries for Miro Bridge metadata downlods
        self.config['mb_tv_channels'] = {}
        self.config['mb_movies'] = {}

        # Episode data keys that you want to display or download.
        # This includes the order that you want them display or in the downloaded file.
        self.config['ep_include_data'] = [u'series', u'seasonnumber', u'episodenumber', u'episodename', u'rating', u'overview', u'director', u'writer', u'cast', u'gueststars', u'imdb_id', u'filename', u'epimgflag', u'language', u'runtime', u'firstaired', u'genres', u'lastupdated', u'productioncode', u'id', u'seriesid', u'seasonid', u'absolute_number', u'combined_season', u'combined_episodenumber', u'dvd_season', u'dvd_discid', u'dvd_chapter', u'dvd_episodenumber']

        self.config['config_file'] = False
        self.config['series_name_override'] = False
        self.config['ep_name_massage'] = False
        self.config['video_file_exts'] = [u'3gp', u'asf', u'asx', u'avi', u'mkv', u'mov', u'mp4', u'mpg', u'qt', u'rm', u'swf', u'wmv', u'm2ts', u'ts', u'evo', u'img', u'iso']


        # Regex pattern strings used to check for season number from directory names
        self.config['season_dir_pattern'] = [
            # foo_01 ????
            re.compile(u'''^.+?[ \._\-]([0-9]+)[^\\/]*$''', re.UNICODE),
            # foo_S01 ????
            re.compile(u'''^.+?[ \._\-][Ss]([0-9]+)[^\\/]*$''', re.UNICODE),
            # 01 ????
            re.compile(u'''([0-9]+)[^\\/]*$''', re.UNICODE),
            # s01 ????
            re.compile(u'''[Ss]([0-9]+)[^\\/]*$''', re.UNICODE),
            ]


        # Set default regex pattern strings used to extract series name , season and episode numbers for file name
        self.config['name_parse'] = [
            # foo_[s01]_[e01]
            re.compile(u'''^(.+?)[ \._\-]\[[Ss]([0-9]+?)\]_\[[Ee]([0-9]+?)\]?[^\\/]*$''', re.UNICODE),
            # foo.1x09*
            re.compile(u'''^(.+?)[ \._\-]\[?([0-9]+)x([0-9]+)[^\\/]*$''', re.UNICODE),
            # foo.s01.e01, foo.s01_e01
            re.compile(u'''^(.+?)[ \._\-][Ss]([0-9]+)[\.\- ]?[Ee]([0-9]+)[^\\/]*$''' , re.UNICODE),
            # foo.103*
            re.compile(u'''^(.+)[ \._\-]([0-9]{1})([0-9]{2})[\._ -][^\\/]*$''' , re.UNICODE),
            # foo.0103*
            re.compile(u'''^(.+)[ \._\-]([0-9]{2})([0-9]{2,3})[\._ -][^\\/]*$''' , re.UNICODE),
        ]

        # regex strings to parse folder names for TV series title, season and episode numbers
        self.config['fullname_parse_season_episode_translation'] = {u'slash': u'\\', u'season': u'Season', u'episode': u'Episode'}
        self.config['fullname_parse_regex'] = [
            # Title/Season 1/s01e01 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/[Ss][0-9]+[Ee](?P<epno>[0-9]+).+$''',
            # Title/Season 1/1x01 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/(?:(?P=seasno))[Xx](?P<epno>[0-9]+).+$''',
            # Title/Season 1/01 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/(?P<epno>[0-9]+).+$''',
            # Title/Season 1/Title s01e01 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/(?:(?P=seriesname))%(slash)s [Ss][0-9]+'''+
                   u'''[Ee](?P<epno>[0-9]+).+$''',
            # Title/Season 1/Title 1x01 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/(?:(?P=seriesname))%(slash)s (?:(?P=seasno))'''+
                   u'''[Xx](?P<epno>[0-9]+).+$''',
            # Title/Season 1/Episode 1 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/%(episode)s%(slash)s (?P<epno>[0-9]+).+$''',
            # Title/Season 1/Season 1 Episode 1 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)/%(season)s%(slash)s '''+
                   u'''(?P<seasno>[0-9]+)/%(season)s%(slash)s (?:(?P=seasno))%(slash)s '''+
                   u'''%(episode)s%(slash)s (?P<epno>[0-9]+).+$''',
            # Title Season 1/01 Subtitle
            u'''^.+?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/(?P<epno>[0-9]+).+$''',
            # Title Season 1/s01e01 Subtitle
            u'''^.*?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/[Ss][0-9]+[Ee](?P<epno>[0-9]+).+''',
            # Title Season 1/1x01 Subtitle
            u'''^.*?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/(?:(?P=seasno))[Xx](?P<epno>[0-9]+).+$''',
            # Title Season 1/Title s01e01 Subtitle
            u'''^.*?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/(?:(?P=seriesname))%(slash)s [Ss][0-9]+[Ee](?P<epno>[0-9]+).+$''',
            # Title Season 1/Title 1x01 Subtitle
            u'''^.*?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/(?:(?P=seriesname))%(slash)s (?:(?P=seasno))[Xx](?P<epno>[0-9]+).+$''',
            # Title Season 1/Episode 1 Subtitle
            u'''^.*?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/%(episode)s%(slash)s (?P<epno>[0-9]+).+$''',
            # Title Season 1/Season 1 Episode 1 Subtitle
            u'''^.*?/(?P<seriesname>[^/]+)%(slash)s %(season)s%(slash)s (?P<seasno>[0-9]+)'''+
                   u'''/%(season)s%(slash)s (?:(?P=seasno))%(slash)s %(episode)s%(slash)s (?P<epno>[0-9]+).+$'''
            ]

        # Initalize a valriable used by the -MW option
        self.program_seriesid = None
        self.config[u'file_move_flag'] = False

    # end __init__

    # Local variable
    data_flags_table={ 'P': 'get_poster', 'B': 'get_banner', 'F': 'get_fanart', 'I': 'get_ep_image', 'E': 'get_ep_meta'}


    def _initLogger(self):
        """Sets up a logger using the logging module, returns a log object
        """
        logger = logging.getLogger(u"jamu")
        formatter = logging.Formatter(u'%(asctime)s) %(levelname)s %(message)s')

        hdlr = logging.StreamHandler(sys.stdout)

        hdlr.setFormatter(formatter)
        logger.addHandler(hdlr)

        if self.config['debug_enabled']:
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.WARNING)
        return logger
    #end initLogger

    def setUseroptions(self, useroptions):
        """    Change variables through a user supplied configuration file
        return False and exit the script if there are issues with the configuration file values
        """
        if useroptions[0]=='~':
            useroptions=os.path.expanduser("~")+useroptions[1:]
        if os.path.isfile(useroptions) == False:
            sys.stderr.write(
                "\n! Error: The specified user configuration file (%s) is not a file\n" % useroptions
            )
            sys.exit(1)
        cfg = ConfigParser.SafeConfigParser()
        cfg.readfp(codecs.open(useroptions, "r", "utf8"))
        for section in cfg.sections():
            if section[:5] == 'File ':
                self.config['config_file'] = section[5:]
                continue
            if section == 'variables':
                # Change variables per user config file
                for option in cfg.options(section):
                    if option == 'video_file_exts' or option == 'tmdb_genre_filter' or option == 'metadata_exclude_as_update_trigger':
                        tmp_list = (cfg.get(section, option).rstrip()).split(',')
                        for i in range(len(tmp_list)): tmp_list[i] = (tmp_list[i].strip()).lower()
                        self.config[option] = tmp_list
                        continue
                    if option == 'filename_char_filter':
                        for char in cfg.get(section, option):
                            self.config['filename_char_filter']+=char
                        continue
                    if option == 'translate':
                        s_e = (cfg.get(section, option).rstrip()).split(',')
                        if not len(s_e) == 2:
                            continue
                        for index in range(len(s_e)):
                            s_e[index] = s_e[index].strip()
                        self.config['fullname_parse_season_episode_translation'] = {u'slash': u'\\', u'season': s_e[0], u'episode': s_e[1]}
                        continue

                    # Ignore user settings for Myth Video and graphics file directories
                    # when the MythTV metadata option (-M) is selected
                    if self.config['mythtvmeta'] and option in ['posterdir', 'bannerdir', 'fanartdir', 'episodeimagedir', 'mythvideo']:
                        continue
                    self.config[option] = cfg.get(section, option)
                continue
            if section == 'regex':
                # Change variables per user config file
                for option in cfg.options(section):
                    self.config['name_parse'].append(re.compile(unicode(cfg.get(section, option), 'utf8'), re.UNICODE))
                continue
            if section == 'ignore-directory':
                # Video directories to be excluded from Jamu processing
                for option in cfg.options(section):
                    self.config['ignore-directory'].append(unicode(cfg.get(section, option), 'utf8'))
                continue
            if section =='series_name_override':
                overrides = {}
                for option in cfg.options(section):
                    overrides[option] = cfg.get(section, option)
                if len(overrides) > 0:
                    self.config['series_name_override'] = overrides
                continue
            if section =='ep_name_massage':
                massage = {}
                for option in cfg.options(section):
                    tmp =cfg.get(section, option).split(',')
                    if len(tmp)%2 and len(cfg.get(section, option)) != 0:
                        sys.stderr.write(u"\n! Error: For (%s) 'ep_name_massage' values must be in pairs\n" % option)
                        sys.exit(1)
                    tmp_array=[]
                    i=0
                    while i != len(tmp):
                        tmp[i] = tmp[i].strip()
                        tmp[i+1] = tmp[i+1].strip()
                        tmp_array.append([tmp[i].replace('"',''), tmp[i+1].replace('"','')])
                        i+=2
                    massage[option]=tmp_array
                if len(massage) > 0:
                    self.config['ep_name_massage'] = massage
                continue
            if section == 'ep_metadata_to_download':
                if len(cfg.options(section)):
                    if cfg.options(section)[0] == 'ep_include_data':
                        tmp=cfg.get(section, cfg.options(section)[0])
                        overrides=tmp.split(',')
                        for index in range(len(overrides)):
                            x = overrides[index].replace(' ','')
                            if len(x) != 0:
                                overrides[index]=x
                            else:
                                del overrides[index]
                    self.config['ep_include_data']=overrides
                continue
            if section == 'data_flags':
                if len(cfg.options(section)):
                    for option in cfg.options(section):
                        if cfg.get(section, option).lower() != 'False'.lower():
                            for key in self.data_flags_table.keys():
                                if option == self.data_flags_table[key]:
                                    self.config[option] = True
                continue
            for sec in ['movies-secondary-sources', 'tv-secondary-sources']:
                if section == sec:
                    secondary = {}
                    for option in cfg.options(section):
                        secondary[option] = cfg.get(section, option)
                    if len(secondary) > 0:
                        self.config['myth_secondary_sources'][sec[:sec.index('-')]] = secondary
                continue
            if section == u'mb_tv':
                # Add the channel names and their corresponding thetvdb.com id numbers
                for option in cfg.options(section):
                    self.config['mb_tv_channels'][filter(is_not_punct_char, option.lower())] = [cfg.get(section, option), u'']
                continue
            if section == u'mb_movies':
                # Add the channel names for movie trailer Channels
                for option in cfg.options(section):
                    self.config['mb_movies'][filter(is_not_punct_char, option.lower())] = cfg.get(section, option)
                continue

        # Expand any home directories that are not fully qualified
        dirs_to_check= [u'bannerdir', u'episodeimagedir', u'metadatadir', u'posterdir', u'video_dir', u'fanartdir']
        for item in dirs_to_check:
            if self.config[item]:
                if item == u'metadatadir' and not self.config[item]:
                    continue
                if self.config[item][0]=='~':
                    self.config[item]=os.path.expanduser("~")+self.config[item][1:]
    # end setUserconfig

    def displayOptions(self):
        """ Display all of the configuration values. This is used to verify that the user has the
        variables set as they want before running jamu live.
        """
        keys=self.config.keys()
        keys.sort()

################### Used to create the example configuration file "jamu-example-conf"
#        for key in keys:    # Used to create the example configuration file "jamu-example-conf"
#            print "#%s: %s" % (key, self.config[key])
#        sys.exit(0)
##################

        for key in keys:
            if key == 'log':    # Do not display the logger instance it is irrelevant for display
                continue
            try:
                if key == 'name_parse':
                    print u"%s (%d items)" % (key, len(self.config[key]))
                else:
                    print u"%s (%s)" % (key, str(self.config[key]))
            except:
                try:
                    print u"%s (%d items)" % (key, len(self.config[key]))
                except:
                    print u"%s:" % key, self.config[key]
    # end set_Userconfig

    def changeVariable(self, key, value):
        """Change any configuration variable - caution no validation is preformed
        """
        self.config[key]=value
    # end changeVariable


    def _checkNFS(self, dirs, ext_filter):
        '''Check if any of the files are on NFS shares. If they are then the user must be warned.
        return True if there are at least one file is on a NFS share.
        return False if no graphic files are on an NFS share.
        '''
        tmp_dirs = []
        for d in dirs:        # Get rid of Null directories
            if d:
                tmp_dirs.append(d)
        dirs = tmp_dirs

        global localhostname, graphicsDirectories
        try:
            localip = gethostbyname(localhostname) # Get the local hosts IP address
        except Exception, e:
            sys.stderr.write("\n! Error: There is no valid address-to-host mapping for the host (%s)\nThe Jamu Janitor (-MJ) option cannot be used while this issue remains un-resolved.\nError:(%s)\n" % (localhostname, e))
            sys.exit(1)

        # Get all curently mounted NFS shares
        tmp_mounts = callCommandLine("mount -l | grep '//'").split('\n')
        nfs = []
        for mount in tmp_mounts:
            mount.rstrip()
            parts = mount.split(' ')
            tmparray=[P for P in parts]
            if tmparray[0].startswith('//'):        # Is this a NFS share definition
                if not tmparray[0].startswith(u'//%s' % localip) and not tmparray[0].startswith(u'//%s' % localhostname):
                    nfs.append(tmparray[2])            # Add an NFS mount name

        if not len(nfs):    # Check if there are any NFS mounts
            return False

        # Check if any of the directories have files on an NFS share
        for directory in dirs:    # Check the base directories first
            for mount in nfs:
                if os.path.realpath(directory).startswith(mount):
                    return True
        for directory in dirs:    # Check the actual files
            file_list = _getFileList([directory])
            if not len(file_list):
                continue
            tmp_list = []
            for fle in file_list: # Make a copy of file_list
                tmp_list.append(fle)
            for g_file in tmp_list:        # Cull the list removing dirs and non-extention files
                if os.path.isdir(g_file):
                    file_list.remove(g_file)
                    continue
                g_ext = _getExtention(g_file)
                if not g_ext.lower() in ext_filter:
                    file_list.remove(g_file)
                    continue
            for filename in file_list:        # Actually check each file against the NFS mounts
                for mount in nfs:
                    if os.path.realpath(filename).startswith(mount):
                        return True
        return False
    # end _checkNFS


    def _getMythtvDirectories(self):
        """Get all graphics directories found in the MythTV DB and change their corresponding
        configuration values.  /media/video:/media/virtual/VB_Share/Review
        """
        # Stop processing if this local host has any storage groups
        global localhostname, storagegroups
        # Make sure Jamu is being run on a MythTV backend
        if not mythbeconn:
            sys.stderr.write(u"\n! Error: Jamu must be run on a MythTV backend. Local host (%s) is not a MythTV backend.\n" % localhostname)
            sys.exit(1)

        global dir_dict
        for key in dir_dict.keys():
            graphics_dir = mythdb.settings[localhostname][dir_dict[key]]
            # Only use path from MythTV if one was found
            self.config[key] = []
            if key == 'mythvideo' and graphics_dir:
                tmp_directories = graphics_dir.split(':')
                if len(tmp_directories):
                    for i in range(len(tmp_directories)):
                        tmp_directories[i] = tmp_directories[i].strip()
                        if tmp_directories[i] != '':
                            if os.access(tmp_directories[i], os.F_OK):
                                self.config[key].append(tmp_directories[i])
                                continue
                            else:
                                 sys.stderr.write(u"\n! Warning: MythTV video directory (%s) does not exist.\n" % (tmp_directories[i]))
                                 continue

            if key != 'mythvideo' and graphics_dir:
                if os.path.os.access(graphics_dir, os.F_OK):
                    self.config[key] = [graphics_dir]
                else:
                    sys.stderr.write(u"\n! Warning: MythTV (%s) directory (%s) does not exist.\n" % (key, graphics_dir))

        # Save the FE path settings local to this backend
        self.config['localpaths'] = {}
        for key in dir_dict.keys():
            self.config['localpaths'][key] = []
            local_paths = []
            if len(self.config[key]):
                self.config['localpaths'][key] = list(self.config[key])

        # If there is a Videos SG then there is always a Graphics SG using Videos as a fallback
        getStorageGroups()
        for key in dir_dict.keys():
            if key == 'episodeimagedir' or key == 'mythvideo':
                continue
            if storagegroups.has_key(u'mythvideo') and not storagegroups.has_key(key):
                storagegroups[key] = list(storagegroups[u'mythvideo'])        # Set fall back

        # Use Storage Groups as the priority but append any FE directory settings that
        # are local to this BE but are not already used as a storage group
        if storagegroups.has_key(u'mythvideo'):
            for key in storagegroups.keys():
                self.config[key] = list(storagegroups[key])
                for k in self.config['localpaths'][key]:
                    if not k in self.config[key]:
                        self.config[key].append(k)    # Add any FE settings local directories not already included
                    else:
                        if key == 'mythvideo':
                            sys.stdout.write(u"\n! Warning: You have a front end video directory path that is a duplicate of this backend's 'Videos' storage group.\nFront end directory (%s)\nThe Front end setting has been ignored.\nThis Front end video directory will cause duplicate entires in MythVideo.\n" % (k))
                        else:
                            sys.stdout.write(u"\n! Info: You have a front end directory path that is a duplicate of this backend's storage group.\nFront end directory (%s)\nThe Front end setting has been ignored.\n" % (k))
                    continue

        # Make sure there is a directory set for Videos and other graphics directories on this host
        exists = True
        for key in dir_dict.keys():
            if key == 'episodeimagedir': # Jamu does nothing with Screenshots
                continue
            # The fall back graphics SG is the Videos SG directory as of changeset 22104
            if storagegroups.has_key(u'mythvideo') and not len(self.config[key]):
                self.config[key] = storagegroups[u'mythvideo']
            if not len(self.config[key]):
                sys.stderr.write(u"\n! Error: There must be a directory for Videos and each graphic type. The (%s) directory is missing.\n" % (key))
                exists = False
        if not exists:
            sys.exit(1)

        # Make sure that the directory set for Videos and other graphics directories have the proper permissions
        accessable = True
        for key in dir_dict.keys():
            for directory in self.config[key]:
                if key == 'episodeimagedir': # Jamu does nothing with Screenshots
                    continue
                if key == 'mythvideo':
                    if not os.access(directory, os.F_OK | os.R_OK):
                        sys.stderr.write(u"\n! Error: This video directory must have read access for Jamu to function.\nThere is a permissions issue with (%s).\n" % (directory, ))
                        accessable = False
                    continue
                if not os.access(directory, os.F_OK | os.R_OK | os.W_OK):
                    sys.stderr.write(u"\n! Error: The (%s) directory (%s) must be read/writable for Jamu to function.\n" % (key, directory, ))
                    accessable = False
        if not accessable:
            sys.exit(1)

        # Print out the video and image directories that will be used for processing
        if self.config['mythtv_verbose']:
            dir_types={'posterdir': "Cover art  ", 'bannerdir': 'Banners    ', 'fanartdir': 'Fan art    ', 'episodeimagedir': 'Screenshots', 'mythvideo': 'Video      '}
            sys.stdout.write(u"\n==========================================================================================\n")
            sys.stdout.write(u"Listed below are the types and base directories Jamu will use for processing.\nThe list reflects your current configuration for the '%s' back end\nand whether a directory is a 'SG' (storage group) or not.\n" % localhostname)
            sys.stdout.write(u"Note: All directories are from settings in the MythDB specific to hostname (%s).\n" % localhostname)
            sys.stdout.write(u"Note: Screenshot directories are not listed as Jamu does not process Screenshots.\n")
            sys.stdout.write(u"------------------------------------------------------------------------------------------\n")
            for key in dir_dict.keys():
                if key == 'episodeimagedir':
                    continue
                for directory in self.config[key]:
                    sg_flag = 'NO '
                    if storagegroups.has_key(key):
                        if directory in storagegroups[key]:
                            sg_flag = 'YES'
                    sys.stdout.write(u"Type: %s - SG-%s - Directory: (%s)\n" % (dir_types[key], sg_flag, directory))
            sys.stdout.write(u"------------------------------------------------------------------------------------------\n")
            sys.stdout.write(u"If a directory you set from a separate Front end is not displayed it means\nthat the directory is not accessible from this backend OR\nyou must add the missing directories using the Front end on this Back end.\nFront end settings are host machine specific.\n")
            sys.stdout.write(u"==========================================================================================\n\n")

        if self.config[u'file_move_flag']:    # verify the destination directory in a move is read/writable
            index = 0
            accessable = True
            for arg in self.args:
                if index % 2 == 0:
                    index+=1
                    continue
                if not os.access(arg, os.F_OK):
                    for dirct in self.config['mythvideo']:
                        if arg.startswith(dirct):
                            if not os.access(dirct, os.F_OK | os.R_OK | os.W_OK):
                                sys.stderr.write(u"! Error: Your move destination root MythVideo directory (%s) must be read/writable for Jamu to function.\n\n" % (dirct, ))
                                accessable = False
                            break
                    else:
                        sys.stderr.write(u"! Error: Your move destination directory (%s) must be a MythVideo directory OR a subdirectory of a MythVideo directory.\n\n" % (arg, ))
                        accessable = False
                elif not os.access(arg, os.F_OK | os.R_OK | os.W_OK):
                    sys.stderr.write(u"! Error: Your move destination directory (%s) must be read/writable for Jamu to function.\n\n" % (arg, ))
                    accessable = False
                index+=1
            if not accessable:
                sys.exit(1)

        # Check if any Video files are on a NFS shares
        if not self.config['mythtvNFS']:    # Maybe the NFS check is to be skipped
            if self._checkNFS(self.config['mythvideo'], self.config['video_file_exts']):
                sys.stderr.write(u"\n! Error: Your video files reside on a NFS mount.\nIn the case where you have more than one MythTV backend using the same directories to store either video files\nor graphics any Jamu's option (-M) can adversly effect your MythTV database by mistakenly adding videos\nfor other backends or with the Janitor (-J) option mistakenly remove graphics files.\n\nIf you only have one backend or do not mix the Video or graphic file directories between backends and still want to use\nJamu add the options (N) to your option string e.g. (-MJN), which will skip this check.\n\n")
                sys.exit(1)
    # end _getMythtvDirectories


    def _JanitorConflicts(self):
        '''Verify that there are no conflict between the graphics directories of MythVideo and
        other MythTV plugins. Write an warning message if a conflict is found.
        return True when there is a conflict
        return False when there is no conflict
        '''
        # Except for the plugins below no other plugins have non-theme graphics
        # MythGallery:
        #     Table 'settings' fields 'GalleryDir', 'GalleryImportDirs', 'GalleryThumbnailLocation'
        # MythGame:
        #    Table 'settings' fields 'mythgame.screenshotDir', 'mythgame.fanartDir', 'mythgame.boxartDir'
        # MythMusic:
        #    Table 'settings' fields 'MusicLocation'
        global graphicsDirectories, localhostname
        tablefields = ['GalleryDir', 'GalleryImportDirs', 'GalleryThumbnailLocation', 'mythgame.screenshotDir', 'mythgame.fanartDir', 'mythgame.boxartDir', 'MusicLocation', 'ScreenShotPath']
        returnvalue = False    # Initalize as no conflicts
        for field in tablefields:
            tmp_setting = mythdb.settings[localhostname][field]
            if not tmp_setting:
                continue
            settings = tmp_setting.split(':')    # Account for multiple dirs per setting
            if not len(settings):
                continue
            for setting in settings:
                for directory in graphicsDirectories.keys():
                    if not self.config[graphicsDirectories[directory]]:
                        continue
                    # As the Janitor processes subdirectories matching must be a starts with check
                    for direc in self.config[graphicsDirectories[directory]]:
                        if os.path.realpath(setting).startswith(os.path.realpath(direc)):
                            sys.stderr.write(u"\n! Error - The (%s) directory (%s) conflicts\nwith the MythVideo (%s) directory (%s).\nThe Jamu Janitor (-MJ) option cannot be used.\n\n" % (field, setting, direc, self.config[graphicsDirectories[directory]]) )
                            returnvalue = True
        return returnvalue
    # end _JanitorConflicts


    def _addMythtvUserFileTypes(self):
        """Add video file types to the jamu list from the "videotypes" table
        """
        # Get videotypes table field names:
        try:
            records = VideoTypes.getAllEntries(mythdb)
        except MythError, e:
            sys.stderr.write(u"\n! Error: Reading videotypes MythTV table: %s\n" % e.args[0])
            return False

        for record in records:
            # Remove any extentions that are in Jamu's list but the user wants ignore
            if record.f_ignore:
                if record.extension in self.config['video_file_exts']:
                    self.config['video_file_exts'].remove(record.extension)
                if record.extension.lower() in self.config['video_file_exts']:
                    self.config['video_file_exts'].remove(record.extension.lower())
            else: # Add extentions that are not in the Jamu list
                if not record.extension in self.config['video_file_exts']:
                    self.config['video_file_exts'].append(record.extension)
        # Make sure that all video file extensions are lower case
        for index in range(len(self.config['video_file_exts'])):
            self.config['video_file_exts'][index] = self.config['video_file_exts'][index].lower()
    # end _addMythtvUserFileTypes()


    def validate_setVariables(self, args):
        """Validate the contents of specific configuration variables
        return False and exit the script if an invalid configuation value is found
        """
        # Fix all variables which were changed by a users configuration files
        # to 'None', 'False' and 'True' literals back to their intended values
        keys=self.config.keys()
        types={'None': None, 'False': False, 'True': True}
        for key in keys:
            for literal in types.keys():
                if self.config[key] == literal:
                    self.config[key] = types[literal]

        # Compile regex strings to parse folder names for TV series title, season and episode numbers
        self.config['fullname_parse'] = []
        for index in range(len(self.config['fullname_parse_regex'])):
            self.config['fullname_parse'].append(re.compile(self.config['fullname_parse_regex'][index] % self.config['fullname_parse_season_episode_translation'], re.UNICODE))

        if self.config['mythtvmeta']:
            if mythdb == None or mythvideo == None:
                sys.stderr.write(u"\n! Error: The MythTV python interface is not installed or Cannot connect to MythTV Backend. MythTV meta data cannot be updated\n\n")
                sys.exit(1)
            try:
                import Image
                self.config['image_library'] = Image
            except Exception, e:
                sys.stderr.write(u"""\n! Error: Python Imaging Library is required for figuring out the sizes of
the fetched poster images.

In Debian/Ubuntu it is packaged as 'python-imaging'.
http://www.pythonware.com/products/pil/\nError:(%s)\n""" % e)
                sys.exit(1)

        if not _can_int(self.config['min_poster_size']):
            sys.stderr.write(u"\n! Error: The poster minimum value must be an integer (%s)\n" % self.config['min_poster_size'])
            sys.exit(1)
        else:
            self.config['min_poster_size'] = int(self.config['min_poster_size'])

        if self.config['maximum'] != None:
            if _can_int(self.config['maximum']) == False:
                sys.stderr.write(u"\n! Error: Maximum option is not an integer (%s)\n" % self.config['maximum'])
                sys.exit(1)

        # Detect if this is a move request
        self.config[u'file_move_flag'] = False
        if len(args) != 0:
            if os.path.isfile(args[0]) or os.path.isdir(args[0]) or args[0][-1:] == '*':
                self.config[u'file_move_flag'] = True
                self.args = list(args)

        if self.config['mythtvdir']:
            if mythdb == None or mythvideo == None:
                sys.stderr.write(u"\n! Error: MythTV python interface is not available\n")
                sys.exit(1)
        if self.config['mythtvdir'] or self.config['mythtvmeta']:
            self._addMythtvUserFileTypes() # add user filetypes from the "videotypes" table
            self._getMythtvDirectories()
        if self.config['mythtvjanitor']: # Check for graphic directory conflicts with other plugins
            if self._JanitorConflicts():
                sys.exit(1)
            if not self.config['mythtvNFS']:
                global graphicsDirectories, image_extensions
                dirs = []
                for key in graphicsDirectories:
                    if key != u'screenshot':
                        for directory in self.config[graphicsDirectories[key]]:
                            dirs.append(directory)
                # Check if any Graphics files are on NFS shares
                if self._checkNFS(dirs, image_extensions):
                    sys.stderr.write(u"\n! Error: Your metadata graphics reside on a NFS mount.\nIn the case where you have more than one MythTV backend using the same directories to store your graphics\nthe Jamu's Janitor option (-MJ) will be destructive removing graphics used by the other backend(s).\n\nIf you only have one backend or do not mix the graphics directories between backends and still want to use\nJamu's Janitor use the options (-MJN) which will skip this check.\n\n")
                    sys.exit(1)

        if self.config['posterresize'] != False or self.config['fanartresize'] != False:
            if _useImageMagick("-version"):
                sys.stderr.write(u"\n! Error: ImageMagick is not installed, graphics cannot be resized. posterresize(%s), fanartresize(%s)\n" % (str(self.config['posterresize']), str(self.config['fanartresize'])))
                sys.exit(1)

        if self.config['mythtvmeta'] and len(args) == 0:
            args=['']

        if len(args) == 0:
            sys.stderr.write(u"\n! Error: At least a video directory, SID or season name must be supplied\n")
            sys.exit(1)

        if os.path.isfile(args[0]) or os.path.isdir(args[0]) or args[0][-1:] == '*':
            self.config['video_dir'] = []
            for arg in args:
                self.config['video_dir'].append(unicode(arg,'utf8'))
        elif not self.config['mythtvmeta']:
            if _can_int(args[0]) and len(args[0]) >= 5:
                self.config['sid'] = unicode(args[0], 'utf8') # There is still a chance that this is a series name "90210"
            else:
                if self.config['series_name_override']:
                    if self.config['series_name_override'].has_key(args[0].lower()):
                        self.config['sid'] = unicode((self.config['series_name_override'][args[0].lower()]).strip(), 'utf8')
                    else:
                        self.config['series_name'] = unicode(args[0].strip(), 'utf8')
                else:
                    self.config['series_name'] = unicode(args[0].strip(), 'utf8')
            if len(args) != 1:
                if len(args) > 3:
                    sys.stderr.write("\n! Error: Too many arguments (%d), maximum is three.\n" % len(args))
                    print "! args:", args
                    sys.exit(1)
                if len(args) == 3 and _can_int(args[1]) and _can_int(args[2]):
                    self.config['season_num'] = args[1]
                    self.config['episode_num'] = args[2]
                elif len(args) == 3:
                    sys.stderr.write(u"\n! Error: Season name(%s), season number(%s), episode number (%s) combination is invalid\n" % (args[0], args[1], args[2]))
                    sys.exit(1)
                elif len(args) == 2 and _can_int(args[1]):
                    self.config['season_num'] = args[1]
                else:
                    if self.config['ep_name_massage']:
                        if self.config['ep_name_massage'].has_key(self.config['series_name']):
                            tmp_ep_name=args[1].strip()
                            tmp_array=self.config['ep_name_massage'][self.config['series_name']]
                            for pair in tmp_array:
                                tmp_ep_name = tmp_ep_name.replace(pair[0],pair[1])
                            self.config['episode_name'] = unicode(tmp_ep_name, 'utf8')
                        else:
                            self.config['episode_name'] = unicode(args[1].strip(), 'utf8')
                    else:
                        self.config['episode_name'] = unicode(args[1].strip(), 'utf8')

        # List of language from http://www.thetvdb.com/api/0629B785CE550C8D/languages.xml
        # Hard-coded here as it is realtively static, and saves another HTTP request, as
        # recommended on http://thetvdb.com/wiki/index.php/API:languages.xml
        valid_languages = ["da", "fi", "nl", "de", "it", "es", "fr","pl", "hu","el","tr", "ru","he","ja","pt","zh","cs","sl", "hr","ko","en","sv","no"]

        # Validate language as specified by user
        if self.config['local_language']:
            if not self.config['local_language'] in valid_languages:
                valid_langs = ''
                for lang in valid_languages: valid_langs+= lang+', '
                valid_langs=valid_langs[:-2]
                sys.stderr.write(u"\n! Error: Specified language(%s) must match one of the following languages supported by thetvdb.com wiki:\n (%s)\n" % (self.config['local_language'], valid_langs))
                sys.exit(1)
        global UI_search_language
        UI_search_language = self.config['local_language']

        if self.config['data_flags']:
            for data_type in self.config['data_flags']:
                if self.data_flags_table.has_key(data_type):
                    self.config[self.data_flags_table[data_type]]=True
    # end validate_setVariables

    def __repr__(self):
        """Return a copy of the configuration variables
        """
        return self.config
    #end __repr__
# end class Configuration


class Tvdatabase(object):
    """Process direct thetvdb.com requests
    """
    def __init__(self, configuration):
        """Retrieve all configuration options and get an instance of tvdb_api which is used to
        access thetvdb.com wiki.
        """
        self.config = configuration
        cache_dir=u"/tmp/tvdb_api_%s/" % os.geteuid()
        if self.config['interactive']:
            self.config['tvdb_api'] = tvdb_api.Tvdb(banners=True, debug=self.config['debug_enabled'], interactive=True,  select_first=False, cache=cache_dir, actors = True, language = self.config['local_language'], custom_ui=jamu_ConsoleUI, apikey="0BB856A59C51D607")  # thetvdb.com API key requested by MythTV)
        else:
            self.config['tvdb_api'] = tvdb_api.Tvdb(banners=True, debug = self.config['debug_enabled'], cache = cache_dir, actors = True, language = self.config['local_language'], apikey="0BB856A59C51D607")  # thetvdb.com API key requested by MythTV)

    # Local variables
    # High level dictionay keys for select graphics URL(s)
    fanart_key=u'fanart'
    banner_key=u'series'
    poster_key=u'poster'
    season_key=u'season'
    # Lower level dictionay keys for select graphics URL(s)
    poster_series_key=u'680x1000'
    poster_season_key=u'season'
    fanart_hires_key=u'1920x1080'
    fanart_lowres_key=u'1280x720'
    banner_series_key=u'graphical'
    banner_season_key=u'seasonwide'
    # Type of graphics being requested
    poster_type=u'poster'
    fanart_type=u'fanart'
    banner_type=u'banner'
    ep_image_type=u'filename'

    def sanitiseFileName(self, name):
        '''Take a file name and change it so that invalid or problematic characters are substituted with a "_"
        return a sanitised valid file name
        '''
        if name == None or name == u'':
            return u'_'
        for char in self.config['filename_char_filter']:
            name = name.replace(char, u'_')
        if name[0] == u'.':
            name = u'_'+name[1:]
        return name
    # end sanitiseFileName()


    def _getSeriesBySid(self, sid):
        """Lookup a series via it's sid
        return tvdb_api Show instance
        """
        seriesid = u'sid:' + sid
        if not self.corrections.has_key(seriesid):
            self._getShowData(sid)
            self.corrections[seriesid] = sid
        return self.shows[sid]
    tvdb_api.Tvdb.series_by_sid = _getSeriesBySid
    # end _getSeriesBySid

    def _searchforSeries(self, sid_or_name):
        """Get TV series data by sid or series name
        return None if the TV show was not found
        return an tvdb_api instance of the TV show data if it was found
        """
        if self.config['sid']:
            show = self.config['tvdb_api'].series_by_sid(self.config['sid'])
            if len(show) != 0:
                self.config['series_name']=show[u'seriesname']
            return show
        else:
            if self.config['series_name_override']:
                if self.config['series_name_override'].has_key(sid_or_name.lower()):
                    self.config['sid'] = (self.config['series_name_override'][sid_or_name.lower()])
                    show = self.config['tvdb_api'].series_by_sid(self.config['sid'])
                    if len(show) != 0:
                        self.config['series_name'] = show[u'seriesname']
                    return show
                else:
                    show = self.config['tvdb_api'][sid_or_name]
                    if len(show) != 0:
                        self.config['series_name'] = show[u'seriesname']
                    return show
            else:
                show = self.config['tvdb_api'][sid_or_name]
                if len(show) != 0:
                    self.config['series_name'] = show[u'seriesname']
                return show
    # end _searchforSeries

    def verifySeriesExists(self):
        """Verify that a:
        Series or
        Series and Season or
        Series and Season and Episode number or
        Series and Episode name
        passed by the user exists on thetvdb.com
        return False and display an appropriate error if the TV data was not found
        return an tvdb_api instance of the TV show/season/episode data if it was found
        """
        sid=self.config['sid']
        series_name=self.config['series_name']
        season=self.config['season_num']
        episode=self.config['episode_num']
        episode_name=self.config['episode_name']
        try:
            self.config['log'].debug(u'Checking for series(%s), sid(%s), season(%s), episode(%s), episode name(%s)' % (series_name, sid, season, episode, episode_name))
            if episode_name:        # Find an exact match for the series and episode name
                series_sid=''
                if sid:
                    seriesfound=self._searchforSeries(sid).search(episode_name)
                else:
                    seriesfound=self._searchforSeries(series_name).search(episode_name)
                if len(seriesfound) != 0:
                    for ep in seriesfound:
                        if ep['seriesid'] == '999999999':
                            self.config['sid'] = ep['seriesid']
                            return(ep)
                        if (ep['episodename'].lower()).startswith(episode_name.lower()):
                            if len(ep['episodename']) > (len(episode_name)+1):
                                # Skip episodes the are not part of a set of (1), (2) ... etc
                                if ep['episodename'][len(episode_name):len(episode_name)+2] != ' (':
                                    continue
                                series_sid = ep['seriesid']
                                self.config['sid'] = ep['seriesid']
                                self.config['season_num'] = ep['seasonnumber']
                                self.config['episode_num'] = ep['episodenumber']
                                return(ep)
                            else: # Exact match
                                series_sid = ep['seriesid']
                                self.config['sid'] = ep['seriesid']
                                self.config['season_num'] = ep['seasonnumber']
                                self.config['episode_num'] = ep['episodenumber']
                                return(ep)
                raise tvdb_episodenotfound
            # Search for the series or series & season or series & season & episode
            elif season:
                if episode:     # series & season & episode
                    seriesfound=self._searchforSeries(series_name)[int(season)][int(episode)]
                    if seriesfound['seriesid'] == '999999999':
                        return(seriesfound)
                    self.config['sid'] = seriesfound['seriesid']
                    self.config['episode_name'] = seriesfound['episodename']
                else:                            # series & season
                    seriesfound=self._searchforSeries(series_name)[int(season)]
            else:
                seriesfound=self._searchforSeries(series_name)    # Series only
        except tvdb_shownotfound:
            # No such show found.
            # Use the show-name from the files name, and None as the ep name
            if series_name:
                sys.stderr.write(u"\n! Warning: Series (%s) not found\n" % (
                    series_name )
                )
            else:
                sys.stderr.write(u"\n! Warning: Series TVDB number (%s) not found\n" % (
                    sid )
                )
            return(False)
        except (tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_attributenotfound):
            # The season, episode or name wasn't found, but the show was.
            # Use the corrected show-name, but no episode name.
            if series_name == None:
                series_name = sid
            if episode:
                sys.stderr.write(u"\n! Warning: For Series (%s), season (%s) or Episode (%s) not found \n"
                 % (series_name, season, episode )
                )
            elif episode_name:
                sys.stderr.write(u"\n! Warning: For Series (%s), Episode (%s) not found \n"
                 % (series_name, episode_name )
                )
            else:
                sys.stderr.write(u"\n! Warning: For Series (%s), season (%s) not found \n" % (
                    series_name, season)
                )
            return(False)
        except tvdb_error, errormsg:
            # Error communicating with thetvdb.com
            if sid: # Maybe the 5 digit number was a series name (e.g. 90210)
                self.config['series_name']=self.config['sid']
                self.config['sid'] = None
                return self.verifySeriesExists()
            sys.stderr.write(
                u"\n! Warning: Error contacting www.thetvdb.com:\n%s\n" % (errormsg)
            )
            return(False)
        except tvdb_userabort, errormsg:
            # User aborted selection (q or ^c)
            print "\n", errormsg
            return(False)
        else:
            return(seriesfound)
    # end verifySeriesExists

    def _resizeGraphic(self, filename, resize):
        """Resize a graphics file
        return False and display an error message if the graphics resizing failed
        return True if the resize was succcessful
        """
        if self.config['simulation']:
            sys.stdout.write(
                u"Simulation resize command (mogrify -resize %s %s)\n" % (resize, filename)
            )
            return(True)
        if _useImageMagick('-resize %s "%s"' % (resize, filename)):
            sys.stderr.write(
                u'\n! Warning: Resizing failed command (mogrify -resize %s "%s")\n' % (resize, filename)
            )
            return(False)
        return True
    # end _resizeGraphic

    def _downloadURL(self, url, OutputFileName):
        """Download the specified graphic file from a URL
        return False if no file was downloaded
        return True if a file was successfully downloaded
        """
        # Only download a file if it does not exist or the option overwrite is selected
        if not self.config['overwrite'] and os.path.isfile(OutputFileName):
            return False

        if self.config['simulation']:
            sys.stdout.write(
                u"Simulation download of URL(%s) to File(%s)\n" % (url, OutputFileName)
            )
            return(True)

        org_url = url
        tmp_URL = url.replace("http://", "")
        url = "http://"+urllib.quote(tmp_URL.encode("utf-8"))

        try:
            dat = urllib.urlopen(url).read()
        except IOError, e:
            sys.stderr.write( u"\n! Warning: Download IOError on URL for Filename(%s)\nOrginal URL(%s)\nIOError urllib.quote URL(%s)\nError:(%s)\n" % (OutputFileName, org_url, url, e))
            return False

        try:
            target_socket = open(OutputFileName, "wb")
            target_socket.write(dat)
            target_socket.close()
        except IOError, e:
            sys.stderr.write( u"\n! Warning: Download IOError for Filename(%s), may be the directory is invalid\nError:(%s)\n" % (OutputFileName, e))
            return False

        # Verify that the downloaded file was NOT HTML instead of the intended file
        try:
            p = subprocess.Popen(u'file "%s"' % OutputFileName, shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
        except Exception, e:
            sys.stderr.write( u"\n! Warning: Download Exception for Filename(%s)\nError:(%s)\n" % (OutputFileName, e))
            return False
        except:
            return False
        data = p.stdout.readline()
        try:
            data = data.encode('utf8')
        except UnicodeDecodeError:
            data = unicode(data,'utf8')
        index = data.find(u'HTML document text')
        if index == -1:
            return True
        else:
            os.remove(OutputFileName) # Delete the useless HTML text
            if self.config['mythtv_verbose']:
                sys.stderr.write( u"\n! Warning: The web site may be having issues.\nURL (%s)\nReturned a file containing HTML\n(%s).\nThe bad downloaded file was removed.\n" % (url, OutputFileName))
            return False
    # end _downloadURL

    def _setGraphicsFileNameFormat(self):
        """Return a file name format (e.g. seriesname - episode name.extention)
        return a filename format string
        """
        if self.config['g_defaultname']:
            return u'%(url)s.%(ext)s'
        cfile={}
        cfile['seriesid']=self.config['sid']
        cfile['series'] = self.sanitiseFileName(self.config['series_name'])
        if cfile['series'] != self.config['series_name']:
            self.config['g_series'] = self.config['g_series'].replace(self.config['series_name'], cfile['series'])
        if self.config['season_num']:
            cfile['seasonnumber']=int(self.config['season_num'])
        else:
            cfile['seasonnumber']=0
        if self.config['episode_num']:
            cfile['episodenumber']=int(self.config['episode_num'])
        else:
            cfile['episodenumber']=0
        cfile['episodename']=self.config['episode_name']
        cfile['seq']=u'%(seq)02d'
        cfile['ext']=u'%(ext)s'

        if self.config['season_num']:
            return self.config['g_season'] % cfile

        return self.config['g_series'] % cfile
    # end _setGraphicsFileNameFormat

    def _downloadGraphics(self, urls, mythtv=False):
        """Download graphic file(s) from a URL list (string of one or more URLs separated by a CR
        character)
        return None   is the string of urls has no urls
        return False  if the any of the urls are corrupt
        return file name of the LAST file downloaded (special for MythTV data base updates)
        """
        global graphicsDirectories

        if urls == None: return None
        if urls == '': return None
        tmp_list=urls.split('\n')
        url_list=[]
        for x in tmp_list:
            x = x.rstrip()
            if x != '':
                url_list.append(x)
        if not len(url_list):
            return None            # There were no URLs in the list
        url_dict={}
        for x in url_list:
            try:
                self.config['log'].debug(u'Checking for a key in (%s)' % (x))
                i = x.index(':')
            except:
                sys.stderr.write(
                    u"\n! Warning: URL list does not have a graphics type key(%s)\n" % (x)
                )
                return(False)
            if url_dict.has_key(x[:i]):
                temp_array = [x[i+1:],'']
                url_dict[x[:i]].append(temp_array)# Collect a list of the same graphics type of URLs
            else: # The first URL of a new graphics type. Also URL replacement code left in place just in case
                url_dict[x[:i]]=[[(x[i+1:]).replace(u"http://www.thetvdb.com",u"http://www.thetvdb.com"),'']]

        unique_dir={u'poster': ['posterdir', True], u'banner': ['bannerdir', True], u'fanart': ['fanartdir', True], u'filename': ['episodeimagedir', True]}
        # If a graphics directory was not specified then default to the 'allgraphics' directory
        if not self.config['posterdir']: self.config['posterdir'] = self.config['allgraphicsdir']
        if not self.config['bannerdir']: self.config['bannerdir'] = self.config['allgraphicsdir']
        if not self.config['fanartdir']: self.config['fanartdir'] = self.config['allgraphicsdir']
        if not self.config['episodeimagedir']: self.config['episodeimagedir'] = self.config['allgraphicsdir']

        # Check if any of the downloaded graphics will share the same directory
        for key in unique_dir.keys():
            for k in unique_dir.keys():
                if key != k:
                    if self.config[unique_dir[key][0]] == self.config[unique_dir[k][0]]:
                        unique_dir[key][1] = False
                        break

        dirs={u'poster': self.config['posterdir'], u'banner': self.config['bannerdir'],
            u'fanart': self.config['fanartdir'], u'filename': self.config['episodeimagedir']}

        # Figure out filenaming convention
        file_format = self._setGraphicsFileNameFormat()

        # Set the graphics fully qualified filenames matched to a URL
        for URLtype in url_dict:
            if mythtv:
                if self.absolutepath:
                    if URLtype == 'poster':
                        tmpgraphicdir = graphicsDirectories['coverfile']
                    else:
                        tmpgraphicdir = graphicsDirectories[URLtype]
                    if not len(self.config['localpaths'][tmpgraphicdir]):
                        return None
                    else:
                        directory = self.config['localpaths'][tmpgraphicdir][0]
                else:
                    directory = dirs[URLtype][0]
            else:
                directory = dirs[URLtype]
            seq_num = 0
            for url in url_dict[URLtype]:
                (dirName, fileName) = os.path.split(url[0])
                (fileBaseName, fileExtension)=os.path.splitext(fileName)
                fileBaseName = self.sanitiseFileName(fileBaseName)
                # Fix file extentions in all caps or 4 character JPEG extentions
                fileExtension = fileExtension.lower()
                if fileExtension == '.jpeg':
                    fileExtension = '.jpg'
                cfile={u'url': fileBaseName, u'seq': seq_num, u'ext': fileExtension[1:]}
                if not isValidPosixFilename(self.config['series_name']):
                    if file_format.startswith(self.config['series_name']):
                        file_format = file_format.replace(self.config['series_name'], self.sanitiseFileName(self.config['series_name']))
                cfile['series'] = self.sanitiseFileName(self.config['series_name'])
                cfile['seriesid'] = self.config['sid']

                if URLtype != 'filename':
                    if unique_dir[URLtype][1]:
                        url_dict[URLtype][seq_num][1] = directory+'/'+file_format % cfile
                    else:
                        if mythtv:
                            url_dict[URLtype][seq_num][1] = directory+'/'+file_format % cfile
                        else:
                            url_dict[URLtype][seq_num][1] = directory+'/'+URLtype.capitalize()+' - '+file_format % cfile
                else:
                    if self.config['season_num']:
                        cfile['seasonnumber']=int(self.config['season_num'])
                    else:
                        cfile['seasonnumber'] = 0
                    if self.config['episode_num']:
                        cfile['episodenumber']=int(self.config['episode_num'])
                    else:
                        cfile['episodenumber'] = 0
                    cfile['episodename'] = self.sanitiseFileName(self.config['episode_name'])
                    url_dict[URLtype][seq_num][1] = directory+'/'+self.config['ep_metadata'] % cfile
                seq_num+=1

        # Download the graphics and resize if requested - Ignore download or resize issues!
        failed_download = False
        for URLtype in url_dict:
            seq_num=0
            for pairs in url_dict[URLtype]:
                if self._downloadURL(pairs[0], pairs[1]):
                    if URLtype == u'poster' and self.config['posterresize']:
                        self._resizeGraphic(pairs[1], self.config['posterresize'])
                    elif URLtype == u'fanart' and self.config['fanartresize']:
                        self._resizeGraphic(pairs[1], self.config['fanartresize'])
                elif not os.path.isfile(pairs[1]): # Check if the file already was downloaded
                    failed_download = True # The download failed
                    if self.config['mythtv_verbose']:
                        sys.stderr.write(u'\nA graphics file failed to be downloaded. A file issue or a corrupt (HTML) file.(%s)\n' % pairs[1])
                seq_num+=1
                if self.config['maximum']:    # Has the maximum number of graphics been downloaded?
                    if seq_num == int(self.config['maximum']):
                        break
        if failed_download:
            return None
        else:
            return pairs[1]                # The name of the LAST graphics successfully downloaded
    # end _downloadGraphics

    def getGraphics(self, graphics_type):
        """Retrieve Poster or Fan Art or Banner or Episode image graphics URL(s)
        return None if no graphics URLs were found
        return a string of URLs
        """
        banners=u'_banners'
        series_name=self.config['series_name']
        season=self.config['season_num']
        episode=self.config['episode_num']
        episode_name=self.config['episode_name']
        lang=self.config['local_language']
        graphics=[]

        try:
            if self.config['sid']:
                URLs = self.config['tvdb_api'].ttvdb_parseBanners(self.config['sid'])
            else:
                URLs = self.config['tvdb_api'].ttvdb_parseBanners(self.config['tvdb_api']._nameToSid(series_name))
        except Exception, e:
            return None

        if graphics_type == self.fanart_type: # Series fanart graphics
            if not len(URLs[u'fanart']):
                return None
            for url in URLs[u'fanart']:
                graphics.append(url)
        elif season == None and episode == None and episode_name == None:
            if not len(URLs[u'series']):
                return None
            if graphics_type == self.banner_type: # Series Banners
                for url in URLs[u'series']:
                    graphics.append(url)
            else: # Series Posters
                for url in URLs[u'poster']:
                    graphics.append(url)
        else:
            if not len(URLs[u'season']):
                return None
            if graphics_type == self.banner_type: # Season Banners
                season_banners=[]
                for url in URLs[u'season']:
                    if url[u'bannertype2'] == u'seasonwide' and url[u'season'] == season:
                        season_banners.append(url)
                if not len(season_banners):
                    return None
                graphics = season_banners
            else: # Season Posters
                season_posters=[]
                for url in URLs[u'season']:
                    if url[u'bannertype2'] == u'season' and url[u'season'] == season:
                        season_posters.append(url)
                if not len(season_posters):
                    return None
                graphics = season_posters

        graphicsURLs=u''
        if self.config['nokeys'] and not self.config['download']:
            key_tag=u''
        else:
            key_tag=graphics_type+u':'

        count = 0
        wasanythingadded = 0
        anyotherlanguagegraphics=u''
        englishlanguagegraphics=u''
        for URL in graphics:
            if graphics_type == 'filename':
                if URL[graphics_type] == None:
                    continue
            if lang:        # Is there a language to filter URLs on?
                if lang == URL['language']:
                    if graphics_type != self.ep_image_type:
                        graphicsURLs+=key_tag+URL['_bannerpath']+'\n'
                    else:
                        graphicsURLs+=key_tag+URL[graphics_type]+'\n'
                else: # Check for fall back graphics in case there are no selected language graphics
                    if u'en' == URL['language']:
                        if graphics_type != self.ep_image_type:
                            englishlanguagegraphics+=key_tag+URL['_bannerpath']+'\n'
                        else:
                            englishlanguagegraphics+=key_tag+URL[graphics_type]+'\n'
                    else:
                        if graphics_type != self.ep_image_type:
                            anyotherlanguagegraphics+=key_tag+URL['_bannerpath']+'\n'
                        else:
                            anyotherlanguagegraphics+=key_tag+URL[graphics_type]+'\n'
            else:
                if graphics_type != self.ep_image_type:
                    graphicsURLs+=key_tag+URL['_bannerpath']+'\n'
                else:
                    graphicsURLs+=key_tag+URL[graphics_type]+'\n'
            if wasanythingadded == len(graphicsURLs):
                continue
            wasanythingadded = len(graphicsURLs)
            count+=1
            if self.config['maximum']:    # Has the maximum number of graphics been downloaded?
                if count == int(self.config['maximum']):
                    break

        if not len(graphicsURLs):
            if len(englishlanguagegraphics): # Fall back to English graphics
                graphicsURLs = englishlanguagegraphics
            elif len(anyotherlanguagegraphics):  # Fall back-back to any available graphics
                graphicsURLs = anyotherlanguagegraphics

        if self.config['debug_enabled']:
            print "\nGraphics:\n", graphicsURLs

        if not len(graphicsURLs):            # Are there any graphics?
            return None

        if len(graphicsURLs) == 1 and graphicsURLs[0] == graphics_type+':':
            return None                        # Due to the language filter there may not be any URLs

        return(graphicsURLs)
    # end get_graphics

    def getTopRatedGraphics(self, graphics_type):
        """Retrieve only the top rated series Poster, Fan Art and Banner graphics URL(s)
        return None if no top rated graphics URLs were found
        return a string of top rated URLs
        """
        if graphics_type == u'filename':
            self.config['log'].debug(u'! There are no such thing as top rated Episode image URLs')
            return None
        toprated=None
        series_name=self.config['series_name']
        keys=self.config['nokeys']
        if self._searchforSeries(series_name)[graphics_type] != None:
            if keys and not self.config['download']:
                toprated=(self._searchforSeries(series_name)[graphics_type])+'\n'
            else:
                toprated=(u'%s:%s\n' % (graphics_type, self._searchforSeries(series_name)[graphics_type]))
        return toprated
    # end getTopRatedGraphics

    def _downloadEpisodeData(self,ep_data):
        """Down load episode meta data and episode image graphics
        return True whether or not there was episode data processed
        """
        if not len(ep_data):
            return True             # There were no episode data in the list
        ep_data_list=[]                # An array of episode meta data

        first_key=self.config['ep_include_data'][0]+':'
        key_size=len(first_key)

        while len(ep_data):            # Grab each episode's set of meta data
            try:
                self.config['log'].debug(u'Parse out the episode data from an episode meta dats string')
                end = ep_data[key_size:].index(first_key)
                ep_data_list.append(ep_data[:end+key_size])
                ep_data=ep_data[end+key_size:]
            except:
                ep_data_list.append(ep_data)
                break

        if not self.config['metadatadir']:
            self.config['metadatadir'] = os.getcwd()

        # Process each episode's meta data
        for episode in ep_data_list:
            tmp_data = episode.split('\n')
            for i in range(len(tmp_data)):
                tmp_data[i] = tmp_data[i].rstrip()# Remove \n characters from the end of each record
            tmp_dict ={}
            for data in tmp_data:
                try:
                    self.config['log'].debug(u'Checking for key in episode meta data')
                    tmp_dict[data[:data.index(':')]] = data[data.index(':')+1:]
                except ValueError:
                    continue
            tmp_dict['ext']='meta'
            tmp_dict['seq']=0
            for key in ['seasonnumber', 'episodenumber']:
                if tmp_dict.has_key(key):
                    tmp_dict[key] = int(tmp_dict[key])
            if not tmp_dict.has_key(u'episodename'):
                tmp_dict[u'episodename'] = u''
            filename="%s/%s" % (self.config['metadatadir'],self.config['ep_metadata'] % tmp_dict)
            image_filename = None
            if     self.config['get_ep_image'] and tmp_dict.has_key('filename'):
                url= tmp_dict['filename']
                if url != 'None':
                    if not self.config['episodeimagedir']:
                        self.config['episodeimagedir'] = self.config['allgraphicsdir']
                    (dirName, fileName) = os.path.split(url)
                    (fileBaseName, fileExtension)=os.path.splitext(fileName)
                    tmp_dict[u'ext']=fileExtension[1:]
                    image_filename = "%s/%s" % (self.config['episodeimagedir'], self.config['ep_metadata'] % tmp_dict)
            # Only download a file if it does not exist or the option overwrite is selected
            # or the option update is selected and the local meta data file is
            # older than the episode data on thetvdb.com wiki
            outofdate = False
            if self.config['update'] and tmp_dict.has_key('lastupdated') and os.path.isfile(filename):
                if  int(tmp_dict['lastupdated']) > int(os.path.getmtime(filename)):
                    outofdate= True

            if not self.config['overwrite'] and not outofdate:
                if self.config['get_ep_meta'] and self.config['get_ep_image']:
                    if image_filename:
                        if os.path.isfile(filename) and os.path.isfile(image_filename):
                            continue
                    else:
                        if os.path.isfile(filename):
                            continue
                elif self.config['get_ep_meta']:
                    if os.path.isfile(filename):
                        continue
                elif self.config['get_ep_image'] and tmp_dict.has_key('filename'):
                    url= tmp_dict['filename']
                    if url != 'None':
                        if os.path.isfile(image_filename):
                            continue
                    else:
                        continue
                else:
                    continue

            if self.config['simulation']:
                if self.config['get_ep_image'] and tmp_dict.has_key('filename'):
                    self.config['log'].debug(u'Simulate downloading an episode image')
                    url= tmp_dict['filename']
                    if url != 'None':
                        sys.stdout.write(u"Simulation create episode image file(%s)\n" % image_filename)
                if self.config['get_ep_meta']:
                    sys.stdout.write(
                        u"Simulation create meta data file(%s)\n" % filename
                    )
                continue

            if self.config['get_ep_image'] and tmp_dict.has_key('filename'):
                if tmp_dict['filename'] != 'None':
                    self._downloadGraphics('filename:'+tmp_dict['filename'])

            # Write out an episode meta data file
            if     self.config['get_ep_meta']:
                fHandle = codecs.open(filename, 'w', 'utf8')
                fHandle.write(episode)
                fHandle.close()

        return True
    # end _downloadEpisodeData

    def _changeToCommas(self,meta_data):
        """Remove '|' and replace with commas
        return the modified text
        """
        if not meta_data: return meta_data
        meta_data = (u'|'.join([d for d in meta_data.split('| ') if d]))
        return (u', '.join([d for d in meta_data.split(u'|') if d]))
    # end _changeToCommas

    def _changeAmp(self, text):
        """Change &amp; values to ASCII equivalents
        return the modified text
        """
        if not text: return text
        text = text.replace("&quot;", u"'").replace("\r\n", u" ")
        text = text.replace(r"\'", u"'")
        return text
    # end _changeAmp

    def getSeriesEpisodeData(self):
        """Get Series Episode meta data. This can be one specific episode or all of a seasons episodes
        or all episodes for an entire series.
        return an empy sting of no episode meta data was found
        reurn a string containing key value pairs of episode meta data
        """
        sid=self.config['sid']
        series_name=self.config['series_name']
        season_num=self.config['season_num']
        episode_num=self.config['episode_num']
        episode_name=self.config['episode_name']

        # Get Cast members
        tmp_cast={}
        cast_members=''
        try:
            tmp_cast = self._searchforSeries(series_name)[u'_actors']
        except:
            cast_members=''
        if len(tmp_cast):
            cast_members=''
            for cast in tmp_cast:
                cast_members+=(cast['name']+u', ').encode('utf8')
            if cast_members != '':
                try:
                    cast_members = cast_members[:-2].encode('utf8')
                except UnicodeDecodeError:
                    cast_members = unicode(cast_members[:-2],'utf8')
                cast_members = self._changeAmp(cast_members)
                cast_members = self._changeToCommas(cast_members)
                cast_members=cast_members.replace('\n',' ')

        # Get genre(s)
        genres=''
        try:
            genres_string = self._searchforSeries(series_name)[u'genre'].encode('utf8')
        except:
            genres_string=''
        if genres_string != None and genres_string != '':
            genres = self._changeAmp(genres_string)
            genres = self._changeToCommas(genres)

        seasons=self._searchforSeries(series_name).keys()    # Get the seasons for this series
        episodes_metadata=u''
        for season in seasons:
            if season_num:                        # If a season was specified skip other seasons
                if season != int(season_num):
                    continue
            episodes=self._searchforSeries(series_name)[season].keys()# Get the episodes for this season
            for episode in episodes:    # If an episode was specified skip other episodes
                if episode_num:
                    if episode != int(episode_num):
                        continue
                ep_data={}
                if sid:                    # Ouput the full series name
                    try:
                        ep_data["series"]=self._searchforSeries(sid)[u'seriesname'].encode('utf8')
                    except AttributeError:
                        return u''
                else:
                    try:
                        ep_data["series"]=self._searchforSeries(series_name)[u'seriesname'].encode('utf8')
                    except AttributeError:
                        return u''
                available_keys=self._searchforSeries(series_name)[season][episode].keys()
                tmp=u''
                ep_data[u'gueststars']=''
                for key in available_keys:
                    if self._searchforSeries(series_name)[season][episode][key] == None:
                        continue
                    # Massage meta data
                    text = self._searchforSeries(series_name)[season][episode][key]
                    text = self._changeAmp(text)
                    text = self._changeToCommas(text)
                    ep_data[key.lower()]=text.replace('\n',' ')
                for key in self.config['ep_include_data']:    # Select and sort the required meta data
                    if ep_data.has_key(key):
                        if key == u'gueststars':
                            if ep_data[key] == '':
                                tmp+=u'Cast:%s\n' % cast_members
                            else:
                                if (len(ep_data[key]) > 128) and not ep_data[key].count(','):
                                    tmp+=u'Cast:%s\n' % cast_members
                                else:
                                    tmp+=u'Cast:%s, %s\n' % (cast_members, ep_data[key])
                            continue
                        try:
                            tmp+=u'%s:%s\n' % (key, ep_data[key])
                        except UnicodeDecodeError:
                            tmp+=u'%s:%s\n' % (key, unicode(ep_data[key], "utf8"))
                tmp+=u'Runtime:%s\n' % self._searchforSeries(series_name)[u'runtime']
                if genres != '':
                    tmp+=u'Genres:%s\n' % genres
                if len(tmp) > 0:
                    episodes_metadata+=tmp
        return episodes_metadata
    # end Getseries_episode_data

    def returnFilename(self):
        """Return a single file name (excluding file extension and directory), limited by the current
        variables (sid, season name, season number ... etc). Typically used when writing a meta file
        or naming/renaming a video file after a TV show recording.
        return False and out put an error if there not either a series id (SID) or series name
        return False and out put an error if there proper episode information (numbers or name)
        return False if the option (-MGF) used and there is not exact TV series name match
        return a specific episode filename
        """
        sid=self.config['sid']
        series_name=self.config['series_name']
        season_num=self.config['season_num']
        episode_num=self.config['episode_num']
        episode_name=self.config['episode_name']

        if not sid and not series_name:
            sys.stderr.write(
                u"\n! Warning: There must be at least series name or SID to request a filename\n"
            )
            return False

        if season_num and episode_num:
            pass
        elif not episode_name:
            sys.stderr.write(
                u'\n! Error: There must be at least "season and episode numbers" or "episode name" to request a filename\n'
            )
            sys.exit(1)

        # Special logic must be used if the (-MG) guessing option has been requested
        if not self.config['sid'] and self.config['mythtv_guess']:
            try:
                allmatchingseries = self.config['tvdb_api']._getSeries(self.config['series_name'])
            except Exception, e:
                sys.stderr.write(u"\nErrors while trying to contact thetvbd.com for Series (%s)\ntherefore a file rename is not possible. error(%s)\n\n" % (self.config['series_name'], e))
                return False
            if filter(is_not_punct_char, allmatchingseries['name'].lower()) == filter(is_not_punct_char, self.config['series_name'].lower()):
                self.config['sid'] = allmatchingseries['sid']
                self.config['series_name'] = allmatchingseries['name']
            else:
                sys.stderr.write(u"\nGuessing could not find an exact match on tvdb for Series (%s)\ntherefore a file rename is not possible.\n\n" % self.config['series_name'])
                return False

        episode = self.verifySeriesExists()

        if not episode:            # Make sure an episode was found
            sys.stderr.write(
                u'\n! Error: The episode was not found for series(%s), Episode name(%s)\n' % (series_name, episode_name)
            )
            sys.exit(1)

        sid=self.config['sid']

        if UI_selectedtitle and (self.config['mythtv_inetref'] or self.config['mythtv_ref_num']):
            self.config['series_name'] = UI_selectedtitle

        series_name=self.config['series_name']
        season_num=self.config['season_num']
        episode_num=self.config['episode_num']
        episode_name=self.config['episode_name']

        tmp_dict ={'series': series_name, 'seasonnumber': season_num, 'episodenumber': episode_num, 'episodename': episode_name, 'sid': sid    }

        tmp_dict['ext']=''
        for key in ['seasonnumber', 'episodenumber']:
            if tmp_dict.has_key(key):
                tmp_dict[key] = int(tmp_dict[key])

        return self.sanitiseFileName(u"%s" % (self.config['ep_metadata'] % tmp_dict)[:-1])
    # end returnFilename

    def processTVdatabaseRequests(self):
        """Process the data/download requests as indicated by the variables
        return None if the series/season/episode does not exist
        return None if there is no data to process for the request actions
        return a string for display or further processing that satisfies the reqested actions
        """
        if self.verifySeriesExists():# Getting a filename is a single event nothing else is returned
            if self.config['ret_filename']:
                return self.returnFilename()
        else:
            return None

        types={'get_fanart': self.fanart_type, 'get_poster': self.poster_type, 'get_banner': self.banner_type}
        if self.config['toprated']:
            typegetGraphics=self.getTopRatedGraphics
        else:
            typegetGraphics=self.getGraphics
        results=u''
        if self.verifySeriesExists():
            if self.config['download']:    # Deal only with graphics display or downloads
                for key in types.keys():
                    if key == 'get_ep_image': # Ep image downloads processed below
                        continue
                    if self.config[key]:
                            if self._downloadGraphics(typegetGraphics(types[key])):
                                sys.stdout.write(
                                    u"%s downloading successfully processed\n" % key.title()
                                )
            else:
                url_string=u''
                for key in types.keys():
                    if self.config[key]:
                        string=typegetGraphics(types[key])
                        if string != None:
                            url_string+=string
                if url_string != '':
                    results+=url_string    # Add graphic URLs to returned results

            # Should episode meta data or episode image be processed?
            if self.config['get_ep_meta'] or self.config['get_ep_image']:
                if self.config['download']:    # Deal only with episode data display or download
                    if self._downloadEpisodeData(self.getSeriesEpisodeData()):
                        sys.stdout.write(
                            u"Episode meta data and/or images downloads successfully processed\n"
                        )
                else:
                    eps_string = self.getSeriesEpisodeData()
                    if eps_string != '':
                        results+=eps_string    # Add episode meta data to returned results
        else:
            return None

        if results != u'':
            if results[len(results)-1] == '\n':
                return results[:len(results)-1]
            else:
                return results
        else:
            return None
    # end processTVdatabaseRequests

    def __repr__(self):    # Just a place holder
        return self.config
    # end __repr__

# end Tvdatabase


class VideoFiles(Tvdatabase):
    """Process all video file and/or directories containing video files. These TV Series video
    files must be named so that a "series name or sid" and/or "season and episode number"
    can be extracted from the video file name. It is best to have renamed the TV series video files with
    tvnamer before using these files with jamu. Any video file without season and episode numbers is
    assumed to be a movie.    Files that do not match the previously described criterion will be skipped.
    tvnamer can be found at:
    http://pypi.python.org/pypi?%3Aaction=search&term=tvnamer&submit=search
    """
    def __init__(self, configuration):
        """Retrieve the configuration options
        """
        super(VideoFiles, self).__init__(configuration)
    # end __init__

    image_extensions = ["png", "jpg", "bmp"]

    def _findFiles(self, args, recursive = False, verbose = False):
        """
        Takes a file name or folder path and grabs files inside them. Does not recurse
        more than one level (if a folder is supplied, it will list files within),
        unless recurse is True, in which case it will recursively find all files.
        return an array of file names
        """
        allfiles = []

        for cfile in args: # Directories must exist and be both readable and writable
            if os.path.isdir(cfile) and not os.access(cfile, os.F_OK | os.R_OK):
                sys.stderr.write(u"\n! Error: Video directory (%s) does not exist or the permissions are not at least readable. Skipping this directory.\n" % (cfile))
                continue
            ignore = False
            if os.path.isdir(cfile):
                for directory in self.config['ignore-directory']: # ignore directory list
                    if not cfile.startswith(directory):
                        continue
                    ignore = True
            if ignore: # Skip this directory
                continue
            if os.path.isdir(cfile):
                index = cfile.find(u'VIDEO_TS')
                if index != -1:
                    sys.stderr.write(u"\n! Warning: Jamu does not process multi-part video files, video directory (%s).\nSkipping this directory. Use MythVideo to retrieve meta data for these video files.\n" % (cfile))
                    continue
                try:
                    cfile = unicode(cfile, u'utf8')
                except (UnicodeEncodeError, TypeError):
                    pass
                for sf in os.listdir(cfile):
                    try:
                        newpath = os.path.join(cfile, sf)
                    except:
                        sys.stderr.write(u"\n! Error: This video file cannot be processed skipping:\n")
                        sys.stderr.write(sf)
                        sys.stderr.write(u"\nIt may be advisable to rename this file and try again.\n\n")
                        continue
                    if os.path.isfile(newpath):
                        allfiles.append(newpath)
                    else:
                        if recursive:
                            allfiles.extend(
                                self._findFiles([newpath], recursive = recursive, verbose = verbose)
                            )
                        #end if recursive
                    #end if isfile
                #end for sf
            elif self.config[u'file_move_flag'] and not os.access(cfile, os.F_OK | os.R_OK | os.W_OK):
                sys.stderr.write(u"\n! Error: The Video file (%s) to be moved must have the read and write permissions. Skipping this video file.\n" % (cfile))
            elif os.path.isfile(cfile) and os.access(cfile, os.F_OK | os.R_OK):
                allfiles.append(cfile) # Files must exist and be at least readable
            #end if isdir
        #end for cfile
        return allfiles
    #end findFiles


    def _processNames(self, names, verbose=False, movies=False):
        """
        Takes list of names, runs them though the self.config['name_parse'] regex parsing strings
        to extract series name, season and episode numbers. Non-video files are skipped.
        return an array of dictionaries containing series name, season and episode numbers, file path             and full filename and file extention.
        """
        allEps = []
        for f in names:
            filepath, filename = os.path.split( f )
            filename, ext = os.path.splitext( filename )

            # Remove leading . from extension
            ext = ext.replace(u".", u"", 1)
            self.config['log'].debug(u'Checking for a valid video filename extension')
            if not ext.lower() in self.config[u'video_file_exts']:
                for key in self.image_extensions:
                    if key == ext:
                        break
                else:
                    sys.stderr.write(u"\n! Warning: Skipping non-video file name: (%s)\n" % (f))
                continue

            match = None
            for r in self.config['name_parse']:
                match = r.match(filename)
                if match: break
            # If the filename does not match the default regular
            # expressions, try to match the file path + filename with the
            # extended fullpath regular expression so we can extract the
            # needed information out of the pathname
            if not match:
                for r in self.config['fullname_parse']:
                    match = r.match(os.path.join(filepath, filename))
                    if match: break

            categories=''
            if match:
                self.config['log'].debug(u'matched reg:%s'%match.re.pattern)
                seriesname, seasno, epno = match.groups()

                #remove ._- characters from name (- removed only if next to end of line)
                seriesname = re.sub("[\._]|\-(?=$)", " ", seriesname).strip()
                seasno, epno = int(seasno), int(epno)

                if self.config['series_name_override']:
                    if self.config['series_name_override'].has_key(seriesname.lower()):
                        if len((self.config['series_name_override'][seriesname.lower()]).strip()) == 7:
                            categories+=u', Movie'
                            movie = filename
                            if movie.endswith(self.config['hd_dvd']):
                                movie = movie.replace(self.config['hd_dvd'], '')
                                categories+=u', DVD'
                                categories+=u', HD'
                            else:
                                if movie.endswith(self.config['dvd']):
                                    movie = movie.replace(self.config['dvd'], '')
                                    categories+=u', DVD'
                            movie = re.sub("[\._]|\-(?=$)", " ", movie).strip()
                            try:
                                allEps.append({ 'file_seriesname':movie,
                                        'seasno':0,
                                        'epno':0,
                                        'filepath':filepath,
                                        'filename':filename,
                                        'ext':ext,
                                        'categories': categories
                                        })
                            except UnicodeDecodeError:
                                allEps.append({ 'file_seriesname':unicode(movie,'utf8'),
                                        'seasno':0,
                                        'epno':0,
                                        'filepath':unicode(filepath,'utf8'),
                                        'filename':unicode(filename,'utf8'),
                                        'ext':unicode(ext,'utf8'),
                                        'categories': categories
                                        })

                categories+=u', TV Series'
                try:
                    allEps.append({ 'file_seriesname':seriesname,
                            'seasno':seasno,
                            'epno':epno,
                            'filepath':filepath,
                            'filename':filename,
                            'ext':ext,
                            'categories': categories
                            })
                except UnicodeDecodeError:
                    allEps.append({ 'file_seriesname':unicode(seriesname,'utf8'),
                            'seasno':seasno,
                            'epno':epno,
                            'filepath':unicode(filepath,'utf8'),
                            'filename':unicode(filename,'utf8'),
                            'ext':unicode(ext,'utf8'),
                            'categories': categories
                            })
            else:
                if movies: # Account for " - On DVD" and " HD - On DVD" extra text on file names
                    categories+=u', Movie'
                    movie = filename

                    if movie.endswith(self.config['hd_dvd']):
                        movie = movie.replace(self.config['hd_dvd'], '')
                        categories+=u', DVD'
                        categories+=u', HD'
                    else:
                        if movie.endswith(self.config['dvd']):
                            movie = movie.replace(self.config['dvd'], '')
                            categories+=u', DVD'
                    movie = re.sub("[\._]|\-(?=$)", " ", movie).strip()
                    try:
                        allEps.append({ 'file_seriesname':movie,
                                        'seasno':0,
                                        'epno':0,
                                        'filepath':filepath,
                                        'filename':filename,
                                        'ext':ext,
                                        'categories': categories
                                        })
                    except UnicodeDecodeError:
                        allEps.append({ 'file_seriesname':unicode(movie,'utf8'),
                                        'seasno':0,
                                        'epno':0,
                                        'filepath':unicode(filepath,'utf8'),
                                        'filename':unicode(filename,'utf8'),
                                        'ext':unicode(ext,'utf8'),
                                        'categories': categories
                                        })
                else:
                    sys.stderr.write(u"\n! Warning: Skipping invalid name: %s\n" % (f))
            #end for r
        #end for f

        return allEps
    #end processNames


    def processFileOrDirectory(self):
        '''This routine is NOT used for MythTV meta data processing.
        If directory path has been specified then create a list of files that qualify as video
        files / including recursed directories.
        Then parse the list of file names to determine (series, season number, ep number and ep name).
        Skip any video file that cannot be parsed for sufficient info.
        Loop through the list:
            > Check if the series, season, ... exists, skip with debug message if none found
            > Set variable with proper info: sid, series, season and episode numbers
            > Process the file's information per the variable to get graphics and or meta data
        return False and an error message and exist the script if there are no video files to process
        return None when all processing was complete
        return a string of file names if the "Filename" process option was True
        '''
        filenames=''
        allFiles = self._findFiles(self.config['video_dir'], self.config['recursive'] , verbose = self.config['debug_enabled'])
        validFiles = self._processNames(allFiles, verbose = self.config['debug_enabled'])

        if len(validFiles) == 0:
            sys.stderr.write(u"\n! Error: No valid video files found\n")
            sys.exit(1)

        path_flag = self.config['metadatadir']
        for cfile in validFiles:
            sys.stdout.write(u"# Processing %(file_seriesname)s (season: %(seasno)d, episode %(epno)d)\n" % (cfile))
            self.config['sid']=None
            self.config['episode_name'] = None
            self.config['series_name']=cfile['file_seriesname']
            self.config['season_num']=u"%d" % cfile['seasno']
            self.config['episode_num']=u"%d" % cfile['epno']
            if not path_flag:    # If no metaddata directory specified then default to the video file dir
                self.config['metadatadir'] = cfile['filepath']
            if self.verifySeriesExists():
                self.config['log'].debug(u"Found series(%s) season(%s) episode(%s)" % (self.config['series_name'], self.config['season_num'], self.config['episode_num']))
                if self.config['ret_filename']:
                    returned = self.processTVdatabaseRequests()
                    if returned != None and returned != False:
                        filenames+=returned+'\n'
                else:
                    self.processTVdatabaseRequests()
            else:
                sys.stderr.write(u"\n! Warning: Did not find series(%s) season(%s) episode(%s)\n" % (self.config['series_name'], self.config['season_num'], self.config['episode_num']))
            self.config['log'].debug("# Done")
        if len(filenames) == 0:
            return None
        else:
            return filenames[:-1] # drop the last '\n'
    # end processFileOrDirectory

    def __repr__(self):    # Just a place holder
        return self.config
    # end __repr__

# end VideoFiles


class MythTvMetaData(VideoFiles):
    """Process all mythvideo video files, update the video files associated MythTV meta data.
    Download graphics for those video files from either thetvdb.com or themovie.com. Video file names
    for TV episodes must series name, season and episode numbers. The video file's movie name must be
    an exact match with a movie title in themoviedb.com or the MythTV database must have an entry for
    the video file with a TMDB or an IMDB number (db field 'intref').
    """
    def __init__(self, configuration):
        """Retrieve the configuration options
        """
        super(MythTvMetaData, self).__init__(configuration)
    # end __init__

    # Local variables
    # A dictionary of meta data keys and initialized values
    global graphicsDirectories
    movie_file_format=u"%s/%s.%s"
    graphic_suffix = {u'coverfile': u'_coverart', u'fanart': u'_fanart', u'banner': u'_banner'}
    graphic_name_suffix = u"%s/%s%s.%s"
    graphic_name_season_suffix = u"%s/%s Season %d%s.%s"


    def _getSubtitle(self, cfile):
        '''Get the MythTV subtitle (episode name)
        return None
        return episode name string
        '''
        self.config['sid']=None
        self.config['episode_name'] = None
        self.config['series_name']=cfile['file_seriesname']
        self.config['season_num']=u"%d" % cfile['seasno']
        self.config['episode_num']=u"%d" % cfile['epno']
        self.verifySeriesExists()
        return self.config['episode_name']
    # end _getSubtitle


    def hashFile(self, name):
        '''Create metadata hash values for mythvideo files
        return a hash value
        return u'' if the was an error with the video file or the video file length was zero bytes
        '''
        filename = self.rtnRelativePath(name, u'mythvideo')
        # Use the MythVideo hashing protocol when the video is in a storage groups
        if filename[0] != u'/':
            hash_value = mythbeconn.getHash(filename, u'Videos')
            if hash_value == u'NULL':
                return u''
            else:
                return hash_value

        # Use a local hashing routine when video is not in a Videos storage group
        # For original code: http://trac.opensubtitles.org/projects/opensubtitles/wiki/HashSourceCodes#Python
        try:
            longlongformat = 'q'  # long long
            bytesize = struct.calcsize(longlongformat)
            f = open(name, "rb")
            filesize = os.path.getsize(name)
            hash = filesize
            if filesize < 65536 * 2:    # Video file is too small
                   return u''
            for x in range(65536/bytesize):
                    buffer = f.read(bytesize)
                    (l_value,)= struct.unpack(longlongformat, buffer)
                    hash += l_value
                    hash = hash & 0xFFFFFFFFFFFFFFFF #to remain as 64bit number
            f.seek(max(0,filesize-65536),0)
            for x in range(65536/bytesize):
                    buffer = f.read(bytesize)
                    (l_value,)= struct.unpack(longlongformat, buffer)
                    hash += l_value
                    hash = hash & 0xFFFFFFFFFFFFFFFF
            f.close()
            returnedhash =  "%016x" % hash
            return returnedhash

        except(IOError):    # Accessing to this video file caused and error
            return u''
    # end hashFile()

    def rtnRelativePath(self, abpath, filetype):
        '''Check if there is a Storage Group for the file type (video, coverfile, banner, fanart, screenshot)
        and form an apprioriate relative path and file name.
        return a relative path and file name
        return an absolute path and file name if there is no storage group for the file type
        '''
        if abpath == None:
            return abpath

        # There is a chance that this is already a relative path or there is no Storage group for file type
        if not len(storagegroups):
            return abpath
        if not storagegroups.has_key(filetype) or abpath[0] != '/':
            return abpath

        # The file must already be in one of the directories specified by the file type's storage group
        for directory in storagegroups[filetype]:
            if abpath.startswith(directory):
                return abpath[len(directory)+1:]
        else:
            return abpath
    # end rtnRelativePath

    def rtnAbsolutePath(self, relpath, filetype):
        '''Check if there is a Storage Group for the file type (mythvideo, coverfile, banner, fanart,
        screenshot)    and form an appropriate absolute path and file name.
        return an absolute path and file name
        return the relpath sting if the file does not actually exist in the absolute path location
        '''
        if relpath == None or relpath == u'':
            return relpath

        # There is a chance that this is already an absolute path
        if relpath[0] == u'/':
            return relpath

        if self.absolutepath:
            if not len(self.config['localpaths'][filetype]):
                return relpath
            directories = self.config['localpaths'][filetype]
        else:
            directories = self.config[filetype]

        for directory in directories:
            abpath = u"%s/%s" % (directory, relpath)
            if os.path.isfile(abpath): # The file must actually exist locally
                return abpath
        else:
            return relpath    # The relative path does not exist at all the metadata entry is useless
    # end rtnAbsolutePath


    def removeCommonWords(self, title):
        '''Remove common words from a title
        return title striped of common words
        '''
        if not title:
            return u' '
        wordList = [u'the ', u'a ', u'  '] # common word list. Leave double space as the last value.
        title = title.lower()
        for word in wordList:
            title = title.replace(word, u'')
        if not title:
            return u' '
        return filter(is_not_punct_char, title.strip())
    # end removeCommonWords()


    def _getTmdbIMDB(self, title, watched=False, IMDB=False, rtnyear=False):
        '''Find and exact match of the movie name with what's on themoviedb.com
        If IMDB is True return an imdb#
        If rtnyear is True return IMDB# and the movie year in a dictionary
        return False (no matching movie found)
        return imdb# and/or tmdb#
        '''
        global video_type, UI_title
        UI_title = title.replace(self.config[u'hd_dvd'], u'')
        UI_title = UI_title.replace(self.config[u'dvd'], u'')

        if UI_title[-1:] == ')': # Get rid of the (XXXX) year from the movie title
            tmp_title = UI_title[:-7].lower()
        else:
            tmp_title = UI_title.lower()

        if self.config['series_name_override']:
            if self.config['series_name_override'].has_key(tmp_title):
                return (self.config['series_name_override'][tmp_title]).strip()

        TMDB_movies=[]
        IMDB_movies=[]
        user_tmdb = False

        while True:
            try:
                if IMDB:
                    results = [self.config['tmdb_api'].searchIMDB(IMDB)]
                elif user_tmdb:
                    results = self.config['tmdb_api'].searchTMDB(user_tmdb)
                    if rtnyear:
                        if results.has_key('releasedate'):
                            return {'name': "%s (%s)" % (results['title'], results['releasedate'][:4]), u'sid': results[u'inetref']}
                        else:
                            return {'name': "%s" % (results['title'], ), u'sid': results[u'inetref']}
                    else:
                        return results['inetref']
                else:
                    results = self.config['tmdb_api'].searchTitle(tmp_title)
            except TmdbMovieOrPersonNotFound, e:
                results = [{}]
            except Exception, errormsg:
                self._displayMessage(u"themoviedb.com error for Movie(%s) invalid data error (%s)" % (title, errormsg))
                return False
            except:
                self._displayMessage(u"themoviedb.com error for Movie(%s)" % title)
                return False

            # Check if user's interactive response (Skip, selection, input #)
            if len(results[0]) and self.config['interactive']:
                if results[0].has_key('userResponse'):
                    # Check if the user selected a specific movie from the list
                    if results[0]['userResponse'] == 'User selected':
                        if rtnyear:
                            if results[0].has_key('released'):
                                data = {'name': "%s (%s)" % (results[0]['name'], results[0]['released'][:4]), u'sid': results[0][u'id']}
                            else:
                                data = {'name': "%s" % (results[0]['name'], ), u'sid': results[0][u'id']}
                            return data
                        else:
                            return results[0]['id']
                    # Check if the user has entered a TMDB number themselves
                    if results[0]['userResponse'] == 'User input':
                        user_tmdb = results[0]['id']
                        continue
                # Check if the user wants this video to be ignored by Jamu from now on
                if results[0]['id'] == '99999999':
                    if rtnyear:
                        return False
                    else:
                        return results[0]['id']
            break

        if IMDB: # This is required to allow graphic file searching both by a TMDB and IMDB numbers
            if len(results[0]):
                if results[0].has_key('imdb_id'):
                    return results[0]['imdb_id'][2:]
                else:
                    return False
            else:
                return False

        if UI_title[-1:] == ')':
            name = UI_title[:-7].lower() # Just the movie title
            year = UI_title[-5:-1] # The movie release year
        else:
            name = tmp_title.lower()
            year = ''
        name = name.strip().replace('  ', ' ')

        if len(results[0]):
            for movie in results:
                if self.removeCommonWords(movie['name']) == self.removeCommonWords(name):
                    if not year:
                        if movie.has_key('released'):
                            TMDB_movies.append({'name': "%s (%s)" % (movie['name'], movie['released'][:4]), u'sid': movie[u'id']})
                        else:
                            TMDB_movies.append({'name': "%s" % (movie['name'], ), u'sid': movie[u'id']})
                        continue
                    if movie.has_key(u'released'):
                        if movie['released'][:4] == year:
                            if rtnyear:
                                return {'name': "%s (%s)" % (movie['name'], movie['released'][:4]), u'sid': movie[u'id']}
                            else:
                                return movie[u'id']
                        TMDB_movies.append({'name': "%s (%s)" % (movie['name'], movie['released'][:4]), u'sid': movie[u'id']})
                        continue
                    else:
                        TMDB_movies.append({'name': "%s" % (movie['name'], ), u'sid': movie[u'id']})
                        continue
                elif movie.has_key('alternative_name'):
                    if self.removeCommonWords(movie['alternative_name']) == self.removeCommonWords(name):
                        if not year:
                            if movie.has_key('released'):
                                TMDB_movies.append({'name': "%s (%s)" % (movie['alternative_name'], movie['released'][:4]), u'sid': movie[u'id']})
                            else:
                                TMDB_movies.append({'name': "%s" % (movie['alternative_name'], ), u'sid': movie[u'id']})
                            continue
                        if movie.has_key(u'released'):
                            if movie['released'][:4] == year:
                                if rtnyear:
                                    return {'name': "%s (%s)" % (movie['alternative_name'], movie['released'][:4]), u'sid': movie[u'id']}
                                else:
                                    return movie['id']
                            TMDB_movies.append({'name': "%s (%s)" % (movie['alternative_name'], movie['released'][:4]), u'sid': movie[u'id']})
                            continue
                        else:
                            TMDB_movies.append({'name': "%s" % (movie['alternative_name'], ), u'sid': movie[u'id']})
                            continue

        # When there is only one match but NO year to confirm then it is OK to assume an exact match
        if len(TMDB_movies) == 1 and year == '':
            if rtnyear:
                return TMDB_movies[0]
            else:
                return TMDB_movies[0][u'sid']

        if imdb_lib:    # Can a imdb.com search be done?
            imdb_access = imdb.IMDb()
            movies_found = []
            try:
                movies_found = imdb_access.search_movie(tmp_title.encode("ascii", 'ignore'))
            except Exception:
                return False
            if not len(movies_found):
                return False
            tmp_movies={}
            for movie in movies_found: # Get rid of duplicates
                try: # Protect against bad data from IMDBpy
                    if movie.has_key('year'):
                        temp =  {imdb_access.get_imdbID(movie): u"%s (%s)" % (movie['title'], movie['year'])}
                    else:
                        temp =  {imdb_access.get_imdbID(movie): movie['title']}
                except Exception:
                    return False
                if tmp_movies.has_key(temp.keys()[0]):
                    continue
                tmp_movies[temp.keys()[0]] = temp[temp.keys()[0]]
            for movie in tmp_movies:
                if tmp_movies[movie][:-7].lower() == name or self.removeCommonWords(tmp_movies[movie][:-7]) == self.removeCommonWords(name):
                    if year:
                        if tmp_movies[movie][-5:-1] == year:
                            if rtnyear:
                                return {'name': tmp_movies[movie], u'sid': movie}
                            else:
                                return u"%07d" % int(movie) # Pad out IMDB# with leading zeroes
                IMDB_movies.append({'name': tmp_movies[movie], u'sid': movie})

        if len(IMDB_movies) == 1: # If this is the only choice and titles matched then auto pick it
            if self.removeCommonWords(IMDB_movies[0]['name'][:-7]) == self.removeCommonWords(name):
                if rtnyear:
                    return IMDB_movies[0]
                else:
                    return u"%07d" % int(IMDB_movies[0][u'sid'])

        # Does IMDB list this movie?
        if len(IMDB_movies) == 0:
            return False

        # Did the user want an interactive interface?
        if not self.config['interactive']:
            return False

        # Force only an IMDB look up for a movie
        movies = IMDB_movies
        video_type=u'IMDB'

        ui = jamu_ConsoleUI(config = self.config, log = self.config['log'])
        try:
            inetref = ui.selectSeries(movies)
        except tvdb_userabort:
            if video_type==u'IMDB' or len(IMDB_movies) == 0:
                self._displayMessage(u"1-No selection made for Movie(%s)" % title)
                return False
            movies = IMDB_movies
            video_type=u'IMDB'
            try:
                inetref = ui.selectSeries(movies)
            except tvdb_userabort:
                self._displayMessage(u"2-No selection made for Movie(%s)" % title)
                return False

        if inetref.has_key('sid'):
            if _can_int(inetref['sid']):
                if inetref['sid'] == '99999999':
                    return inetref['sid']
                if rtnyear:
                    if inetref['name'] == u'User input':
                        try:
                            data = imdb_access.get_movie(inetref['sid'])
                            if data.has_key('long imdb title'):
                                return {'name': data['long imdb title'], u'sid': inetref['sid']}
                            elif data.has_key('title'):
                                return {'name': data['title'], u'sid': inetref['sid']}
                            else:
                                return False
                        except imdb._exceptions.IMDbDataAccessError:
                            return False
                    else:
                        return inetref
                else:
                    return u"%07d" % int(inetref['sid']) # Pad out IMDB# with leading zeroes
            else:
                return False
        else:
            return False
    # end _getTmdbIMDB

    def _getTmdbGraphics(self, cfile, graphic_type, watched=False):
        '''Download either a movie Poster or Fanart
        return None
        return full qualified path and filename of downloaded graphic
        '''
        if graphic_type == u'-P':
            graphic_name = u'poster'
            key_type = u'coverart'
            rel_type = u'coverfile'
        else:
            graphic_name = u'fanart'
            key_type = u'fanart'
            rel_type = key_type

        self.config['series_name']=cfile['file_seriesname']
        try:
            if len(cfile['inetref']) == 7: # IMDB number
                results = self.config['tmdb_api'].searchIMDB(cfile['inetref'])
            else:
                results = self.config['tmdb_api'].searchTMDB(cfile['inetref'])
        except TmdbMovieOrPersonNotFound, e:
            self._displayMessage(u"0-tmdb %s for Movie not found(%s)(%s)" % (graphic_name, cfile['filename'], cfile['inetref']))
            return None
        except Exception, e:
            self._displayMessage(u"themoviedb.com error for Movie(%s) graphics(%s), error(%s)" % (cfile['file_seriesname'], graphic_name, e))
            return None

        if results != None:
            if not results.has_key(key_type):
                self._displayMessage(u"1-tmdb %s for Movie not found(%s)(%s)" % (graphic_name, cfile['filename'], cfile['inetref']))
                return None
        else:
            self._displayMessage(u"1b-tmdb %s for Movie not found(%s)(%s)" % (graphic_name, cfile['filename'], cfile['inetref']))
            return None

        graphic_file = (results[key_type].split(u','))[0].strip() # Only want the first image URL

        self.config['g_defaultname']=False
        self.config['toprated'] = True
        self.config['nokeys'] = False

        self.config['sid']=None
        if watched:
            if self.program_seriesid == None:
                self.config['g_series'] = self.sanitiseFileName(cfile['file_seriesname'])+u' Season 1'+self.graphic_suffix[rel_type]+u'.%(ext)s'
            else:
                self.config['g_series'] = self.sanitiseFileName(self.program_seriesid)+self.graphic_suffix[rel_type]+u'.%(ext)s'
        else:
            self.config['g_series'] = cfile['inetref']+self.graphic_suffix[rel_type]+u'.%(ext)s'
        if graphic_type == '-P':
            g_type = u'poster'
        else:
            g_type = u'fanart'

        self.config['season_num']= None    # Needed to get graphics named in 'g_series' format

        self.config['overwrite'] = True # Force overwriting any existing graphic file

        tmp_URL = graphic_file.replace(u"http://", u"")
        graphic_file = u"http://"+urllib.quote(tmp_URL.encode("utf-8"))
        value = self._downloadGraphics(u"%s:%s" % (g_type, graphic_file), mythtv=True)

        self.config['overwrite'] = False # Turn off overwriting

        if value == None:
            self._displayMessage(u"2-tmdb %s for Movie not found(%s)(%s)" % (graphic_name, cfile['filename'], cfile['inetref']))
            return None
        else:
            return self.rtnRelativePath(value, graphicsDirectories[rel_type])
    # end _getTmdbGraphics

    def _getSecondarySourceGraphics(self, cfile, graphic_type, watched=False):
        '''Download from secondary source such as movieposter.com
        return None
        return full qualified path and filename of downloaded graphic
        '''
        if not len(self.config['myth_secondary_sources']):
            return None

        if graphic_type == u'coverfile':
            graphic_type = u'poster'
            rel_type = u'coverfile'

        if cfile['seasno'] == 0 and cfile['epno'] == 0:
            if not self.config['myth_secondary_sources'].has_key('movies'):
                return None
            if self.config['myth_secondary_sources']['movies'].has_key(graphic_type):
                source = self.config['myth_secondary_sources']['movies'][graphic_type]
                if source.find(u'%(imdb)s') != -1:
                    if len(cfile['inetref']) != 7:
                        try:
                            results = self.config['tmdb_api'].searchTMDB(cfile['inetref'])
                        except TmdbMovieOrPersonNotFound, e:
                            self._displayMessage(u"\n! Warning: Secondary themoviedb.com error for Movie(%s) graphics(%s), error(%s)" % (cfile['file_seriesname'], graphic_type, e))
                            return None
                        except Exception, e:
                            self._displayMessage(u"\n! Warning: Secondary themoviedb.com error for Movie(%s) graphics(%s), error(%s)" % (cfile['file_seriesname'], graphic_type, e))
                            return None
                        if results == None:
                            return None
                        if not results.has_key('imdb'):
                            self._displayMessage(u"\n! Warning: themoviedb.com wiki does not have an IMDB number to search a secondary source (%s)\nfor the movie (%s) inetref (%s).\n" % (source , cfile['filename'], cfile['inetref']))
                            return None
                        cfile['imdb'] = results['imdb']
                    else:
                        cfile['imdb'] = cfile['inetref']
            else:
                return None
        else:
            if not self.config['myth_secondary_sources'].has_key('tv'):
                return None
            if self.config['myth_secondary_sources']['tv'].has_key(graphic_type):
                source = self.config['myth_secondary_sources']['tv'][graphic_type]
            else:
                return None

        self.config['series_name']=cfile['file_seriesname']

        if self.config['simulation']:
            sys.stdout.write(u"Simulating - downloading Secondary Source graphic (%s)\n" % cfile['file_seriesname'])
            return u"Simulated Secondary Source graphic filename place holder"

        # Test that the secondary's required data has been passed
        try:
            command = source % cfile
        except:
            self._displayMessage(u"Graphics Secondary source command:\n%s\nRequired information is not available. Here are the variables that are available:\n%s\n" % (source, cfile))
            return None

        tmp_files = callCommandLine(command)
        if tmp_files == '':
            self._displayMessage(u"\n! Warning: Source (%s)\n could not find (%s) for (%s)(%s)\n" % (source % cfile, graphic_type, cfile['filename'], cfile['inetref']))
            return None

        tmp_array=tmp_files.split('\n')
        if tmp_array[0].startswith(u'Failed'):
            self._displayMessage(u"\n! Warning: Source (%s)\nfailed to download (%s) for (%s)(%s)\n" % (source % cfile, graphic_type, cfile['filename'], cfile['inetref']))
            return None

        if tmp_array[0].startswith(u'file://'):
            tmp_files=tmp_array[0].replace(u'file://', u'')
            if not os.path.isfile(tmp_files):
                sys.stderr.write(u'\n! Error: The graphic file does not exist (%s)\n' % tmp_files)
                sys.exit(1)

            # Fix file extentions in all caps or 4 character JPEG extentions
            fileExtension = (_getExtention(tmp_files)).lower()
            if fileExtension == u'jpeg':
                fileExtension = u'jpg'
            if watched:
                if self.program_seriesid == None:
                    filename = u'%s/%s%s.%s' % (self.config['posterdir'][0], self.sanitiseFileName(cfile['file_seriesname']), self.graphic_suffix[rel_type], fileExtension)
                else:
                    filename = u'%s/%s%s.%s' % (self.config['posterdir'][0], self.sanitiseFileName(self.program_seriesid), self.graphic_suffix[rel_type], fileExtension)
            else:
                filename = u'%s/%s%s.%s' % (self.config['posterdir'][0], cfile['inetref'], self.graphic_suffix[rel_type], fileExtension)

            if os.path.isfile(filename): # This may be the same small file or worse then current
                try:
                    (width, height) = self.config['image_library'].open(filename).size
                    (width2, height2) = self.config['image_library'].open(tmp_files).size
                    if width >= width2:
                        os.remove(tmp_files)
                        return None
                except IOError:
                    return None

            # Verify that the downloaded file was NOT HTML instead of the intended file
            if self._checkValidGraphicFile(tmp_files, graphicstype=u'', vidintid=False) == False:
                os.remove(tmp_files) # Delete the useless HTML text
                return None
            shutil.copy2(tmp_files, filename)
            os.remove(tmp_files)
            self.num_secondary_source_graphics_downloaded+=1
            return self.rtnRelativePath(filename, graphicsDirectories[rel_type])
        else:
            graphic_file = tmp_array[0]

            self.config['g_defaultname']=False
            self.config['toprated'] = True
            self.config['nokeys'] = False

            self.config['sid']=None
            if watched:
                if self.program_seriesid == None:
                    self.config['g_series'] = self.sanitiseFileName(cfile['file_seriesname'])+self.graphic_suffix[rel_type]+'.%(ext)s'
                else:
                    self.config['g_series'] = self.sanitiseFileName(self.program_seriesid)+self.graphic_suffix[rel_type]+'.%(ext)s'
            else:
                self.config['g_series'] = self.sanitiseFileName(cfile['inetref'])+self.graphic_suffix[rel_type]+'.%(ext)s'
            g_type = graphic_type

            self.config['season_num']= None    # Needed to get graphics named in 'g_series' format

            self.config['overwrite'] = True # Force overwriting any existing graphic file

            tmp_URL = graphic_file.replace(u"http://", u"")
            graphic_file = u"http://"+urllib.quote(tmp_URL.encode("utf-8"))
            value = self._downloadGraphics(u"%s:%s" % (g_type, graphic_file), mythtv=True)

            self.config['overwrite'] = False # Turn off overwriting
            if value == None:
                self._displayMessage(u"Secondary source %s not found(%s)(%s)" % (graphic_file, cfile['filename'], cfile['inetref']))
                return None
            else:
                self.num_secondary_source_graphics_downloaded+=1
                return self.rtnRelativePath(value, graphicsDirectories[rel_type])
    # end _getSecondarySourceGraphics

    def combineMetaData(self, available_metadata, meta_dict, vid_type=False):
        ''' Combine the current data with new meta data from primary or secondary sources
        return combinted meta data dictionary
        '''
        # Combine meta data
        for key in meta_dict.keys():
            if key in self.config['metadata_exclude_as_update_trigger']:
                continue
            else:
                if key == 'inetref' and available_metadata[key] != meta_dict[key]:
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'releasedate' and available_metadata[key] != meta_dict[key]:
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'userrating' and available_metadata[key] == 0.0:
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'length' and available_metadata[key] == 0:
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'rating' and (available_metadata[key] == 'NR' or available_metadata[key] == 'Unknown'):
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'year' and available_metadata[key] == 1895:
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'category' and available_metadata[key] == 0:
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'inetref' and available_metadata[key] == '00000000':
                    available_metadata[key] = meta_dict[key]
                    continue
                if key == 'title':
                    available_metadata[key] = meta_dict[key]
                    continue
                if vid_type and key == 'subtitle': # There are no subtitles in movies
                    continue
                if key == 'plot': # Remove any line-feeds from the plot. Mythvideo does not expect them.
                    meta_dict[key] = meta_dict[key].replace('\n', ' ')
                if (vid_type and key == 'plot') and (meta_dict[key].find('@') != -1 or len(meta_dict[key].split(' ')) < 10):
                    continue
                if vid_type and key == 'plot':
                    if available_metadata[key] != None:
                        if len(available_metadata[key].split(' ')) < 10 and len(meta_dict[key].split(' ')) > 10:
                            available_metadata[key] = meta_dict[key]
                            continue
                if not available_metadata.has_key(key): # Mainly for Genre, Cast and Countries
                    available_metadata[key] = meta_dict[key]
                    continue
                if available_metadata[key] == None or available_metadata[key] == '' or available_metadata[key] == 'None' or available_metadata[key] == 'Unknown':
                    available_metadata[key] = meta_dict[key]
                    continue
        return available_metadata
    # end combineMetaData


    def _getSecondarySourceMetadata(self, cfile, available_metadata):
        '''Download meta data from secondary source
        return available_metadata (returns the current metadata unaltered)
        return dictionary of combined meta data
        '''
        if not len(self.config['myth_secondary_sources']):
            return available_metadata

        if cfile['seasno'] == 0 and cfile['epno'] == 0:
            if not self.config['myth_secondary_sources'].has_key('movies'):
                return available_metadata
            movie = True
            if self.config['myth_secondary_sources']['movies'].has_key('metadata'):
                source = self.config['myth_secondary_sources']['movies']['metadata']
                if source.find(u'%(imdb)s') != -1:
                    if len(cfile['inetref']) != 7:
                        try:
                            results = self.config['tmdb_api'].searchTMDB(cfile['inetref'])
                        except TmdbMovieOrPersonNotFound, e:
                            self._displayMessage(u"Secondary metadata themoviedb.com error for Movie(%s), error(%s)" % (cfile['file_seriesname'], e))
                            return available_metadata
                        except Exception, e:
                            self._displayMessage(u"Secondary metadata themoviedb.com error for Movie(%s), error(%s)" % (cfile['file_seriesname'], e))
                            return available_metadata
                        if results == None:
                            return available_metadata
                        if not results.has_key('imdb'):
                            self._displayMessage(u"No IMDB number for meta data secondary source (%s)\nfor the movie (%s) inetref (%s) in themoviedb.com wiki.\n" % (source, cfile['filename'], cfile['inetref']))
                            return available_metadata
                        cfile['imdb'] = results['imdb']
                    else:
                        cfile['imdb'] = cfile['inetref']
            else:
                return available_metadata
        else:
            if not self.config['myth_secondary_sources'].has_key('tv'):
                return available_metadata
            movie = False
            if self.config['myth_secondary_sources']['tv'].has_key('metadata'):
                source = self.config['myth_secondary_sources']['tv']['metadata']
            else:
                return available_metadata

        # Test that the secondary's required data has been passed
        try:
            command = source % cfile
        except:
            self._displayMessage(u"Metadata Secondary source command:\n%s\nRequired information is not available. Here are the variables that are available:\n%s\n" % (source, cfile))
            return available_metadata

        self.config['series_name']=cfile['file_seriesname']

        tmp_files=u''
        tmp_files = (callCommandLine(command)).decode("utf8")
        if tmp_files == '':
            self._displayMessage(u"1-Secondary source (%s)\ndid not find(%s)(%s) meta data dictionary cannot be returned" % (source % cfile, cfile['filename'], cfile['inetref']))
            return available_metadata

        meta_dict={}
        tmp_array=tmp_files.split('\n')
        for element in tmp_array:
            element = (element.rstrip('\n')).strip()
            if element == '' or element == None:
                continue
            try:
                index = element.index(':')
            except:
                continue
            key = element[:index].lower()
            data = element[index+1:]
            if data == None or data == '':
                continue
            if key == u'inetref' and len(cfile['inetref']) == 7:
                meta_dict[key] = cfile['inetref']
                continue
            data = self._changeAmp(data)
            data = self._changeToCommas(data)
            if key == 'year':
                try:
                    meta_dict[key] = int(data)
                except:
                    continue
                continue
            if key == 'userrating':
                try:
                    meta_dict[key] = float(data)
                except:
                    continue
                continue
            if key == 'runtime':
                try:
                    meta_dict['length'] = long(data)
                except:
                    continue
                continue
            if key == 'movierating':
                meta_dict['rating'] = data
                continue
            if key == 'plot':
                try:
                    if len(data.split(' ')) < 10: # Skip plots that are less than 10 words
                        continue
                except:
                    pass
            if key == 'trailer':
                continue
            if key == 'releasedate':
                try:
                    meta_dict[key] = datetime.datetime.strptime(data,'%Y-%m-%d').date()
                except ValueError:
                    pass
                continue
            meta_dict[key] = data
        if not len(meta_dict):
            self._displayMessage(u"2-Secondary source (%s)\n did not find(%s)(%s) meta data dictionary cannot be returned" % (source % cfile, cfile['filename'], cfile['inetref']))
            return available_metadata

        # Combine meta data
        available_metadata = self.combineMetaData(available_metadata, meta_dict, vid_type=movie)
        self.num_secondary_source_metadata_downloaded+=1
        return available_metadata
    # end _getSecondarySourceMetadata

    def _getTmdbMetadata(self, cfile, available_metadata):
        '''Download a movie's meta data and massage the genres string
        return results for secondary sources when no primary source meta data
        return dictionary of metadata combined with data from a secondary source
        '''
        try:
            if len(cfile['inetref']) == 7: # IMDB number
                meta_dict = self.config['tmdb_api'].searchIMDB(cfile['inetref'])
            else:
                meta_dict = self.config['tmdb_api'].searchTMDB(cfile['inetref'])
        except TmdbMovieOrPersonNotFound, e:
            self._displayMessage(u"0-tmdb Movie not found(%s)(%s) meta data dictionary cannot be returned" % (cfile['filename'], cfile['inetref']))
            return self._getSecondarySourceMetadata(cfile, available_metadata)
        except Exception, e:
            self._displayMessage(u"themoviedb.com error for Movie(%s)(%s) meta data dictionary cannot be returned, error(%s)" % (cfile['filename'], cfile['inetref'], e))
            return self._getSecondarySourceMetadata(cfile, available_metadata)

        if meta_dict == None:
            self._displayMessage(u"1-tmdb Movie not found(%s)(%s) meta data dictionary cannot be returned" % (cfile['filename'], cfile['inetref']))
            return self._getSecondarySourceMetadata(cfile, available_metadata)

        keys = meta_dict.keys()

        for key in keys:
            data = meta_dict[key]
            if not data:
                continue
            if key == 'homepage':
                continue
            data = self._changeAmp(data)
            data = self._changeToCommas(data)
            if key == 'genres':
                genres=''
                genre_array = data.split(',')
                for i in range(len(genre_array)):
                    genre_array[i] = (genre_array[i].strip()).lower()
                    if genre_array[i] in self.config['tmdb_genre_filter']:
                        genres+=genre_array[i].title()+','
                if genres == '':
                    meta_dict[key] = u''
                    continue
                else:
                    meta_dict[key] = genres[:-1]
            if key == 'trailer':
                continue
            if key == 'year':
                try:
                    meta_dict[key] = int(data)
                except:
                    pass
                continue
            if key == 'userrating':
                try:
                    meta_dict[key] = float(data)
                except:
                    pass
                continue
            if key == 'url':
                meta_dict['homepage'] = data
                continue
            if key == 'releasedate':
                try:
                    meta_dict[key] = datetime.datetime.strptime(data,'%Y-%m-%d').date()
                except ValueError:
                    del meta_dict[key]
                continue
            if key == 'runtime':
                try:
                    meta_dict['length'] = long(data)
                except:
                    pass
                continue
            if key == 'movierating':
                meta_dict['rating'] = data
                continue
        if meta_dict.has_key('rating'):
            if meta_dict['rating'] == '':
                meta_dict['rating'] = 'Unknown'

        if len(meta_dict):
            if available_metadata['hash'] == u'' or available_metadata['hash'] == None:
                filename = u'%s/%s.%s' % (cfile['filepath'], cfile['filename'], cfile['ext'])
                meta_dict['hash'] = self.hashFile(filename)
            available_metadata = self.combineMetaData(available_metadata, meta_dict, vid_type=True)
            return self._getSecondarySourceMetadata(cfile, available_metadata)
        else:
            self._displayMessage(u"2-tmdb Movie not found(%s)(%s) meta data dictionary cannot be returned" % (cfile['filename'], cfile['inetref']))
            return self._getSecondarySourceMetadata(cfile, available_metadata)
    # end _getTmdbMetadata

    def _getTvdbGraphics(self, cfile, graphic_type, toprated=False, watched=False):
        '''Download either a TV Series Poster, Banner, Fanart or Episode image
        return None
        return full qualified path and filename of downloaded graphic
        '''
        rel_type = graphic_type
        if graphic_type == u'coverfile':
            graphic_type = u'poster'
        elif graphic_type == u'poster':
            rel_type =u'coverfile'

        self.config['g_defaultname']=False
        self.config['toprated'] = toprated
        self.config['nokeys'] = False
        self.config['maximum'] = u'1'

        if watched:
            self.config['sid']=cfile['inetref']
        else:
            self.config['sid']=None
        self.config['episode_name'] = None
        self.config['series_name']=cfile['file_seriesname']
        if not watched:
            self.config['season_num']=u"%d" % cfile['seasno']
            self.config['episode_num']=u"%d" % cfile['epno']

        # Special logic must be used if the (-MG) guessing option has been requested
        if not self.config['sid'] and self.config['mythtv_guess']:
            try:
                allmatchingseries = self.config['tvdb_api']._getSeries(self.config['series_name'])
            except Exception, e:
                self._displayMessage(u"tvdb Series not found(%s) or connection issues with thetvdb.com web site.\nError:(%s)\n" % (cfile['filename'], e))
                return None
            if filter(is_not_punct_char, allmatchingseries['name'].lower()) == filter(is_not_punct_char,cfile['file_seriesname'].lower()):
                self.config['sid'] = allmatchingseries['sid']
                self.config['series_name'] = allmatchingseries['name']
                cfile['file_seriesname'] = allmatchingseries['name']
            else:
                sys.stderr.write(u"\nGuessing could not find an exact match on tvdb for Series (%s)\ntherefore a graphics cannot be downloaded\n\n" % cfile['filename'])
                return None
        else:
            if not self.verifySeriesExists():
                self._displayMessage(u"tvdb Series not found(%s)" % cfile['filename'])
                return None

        if watched:
            if self.program_seriesid == None:
                self.config['g_series'] = self.sanitiseFileName(cfile['file_seriesname'])+u' Season 1'+self.graphic_suffix[rel_type]+u'.%(ext)s'
                self.config['g_season'] = self.sanitiseFileName(cfile['file_seriesname'])+u' Season %(seasonnumber)d'+self.graphic_suffix[rel_type]+u'.%(ext)s'
            else:
                self.config['g_series'] = self.sanitiseFileName(self.program_seriesid)+self.graphic_suffix[rel_type]+u'.%(ext)s'
                self.config['g_season'] = self.sanitiseFileName(self.program_seriesid)+u' Season %(seasonnumber)d'+self.graphic_suffix[rel_type]+u'.%(ext)s'
        else:
        # TV Series ALWAYS need the ' Season' in the file name incase the show name could clobber a Movie image
        # Season X is used so that a real season image is not overritten. It will be renamed later.
            self.config['g_series'] = self.sanitiseFileName(self.config['series_name'])+u' Season X'+self.graphic_suffix[rel_type]+u'.%(ext)s'
            self.config['g_season'] = self.sanitiseFileName(self.config['series_name'])+u' Season %(seasonnumber)d'+self.graphic_suffix[rel_type]+u'.%(ext)s'
        if toprated:
            typegetGraphics=self.getTopRatedGraphics
            self.config['season_num']= None    # Needed to get toprated graphics named in 'g_series' format
        else:
            typegetGraphics=self.getGraphics

        self.config['overwrite'] = True # Force overwriting any existing graphic file
        value = self._downloadGraphics(typegetGraphics(graphic_type), mythtv=True)
        self.config['overwrite'] = False # Turn off overwriting
        if value == None:
            return None
        else:
            return self.rtnRelativePath(value, graphicsDirectories[rel_type])
    # end _getTvdbGraphics

    def _getTvdbMetadata(self, cfile, available_metadata):
        '''Download thetvdb.com meta data
        return what was input or results from a secondary source
        return dictionary of metadata
        '''
        global video_type, UI_title
        video_type=u'TV series'
        UI_title = cfile['file_seriesname']

        meta_dict={}
        self.config['nokeys'] = False
        self.config['sid']=None
        self.config['episode_name'] = None
        self.config['series_name']=cfile['file_seriesname']
        self.config['season_num']=u"%d" % cfile['seasno']
        self.config['episode_num']=u"%d" % cfile['epno']
        if self.config['series_name_override']:
            if self.config['series_name_override'].has_key(cfile['file_seriesname'].lower()):
                self.config['sid'] = (self.config['series_name_override'][cfile['file_seriesname'].lower()]).strip()

        # Special logic must be used if the (-MG) guessing option has been requested
        if not self.config['sid'] and self.config['mythtv_guess']:
            try:
                allmatchingseries = self.config['tvdb_api']._getSeries(self.config['series_name'])
            except Exception, e:
                self._displayMessage(u"tvdb Series not found(%s) or there are connection problems with thetvdb.com\nError(%s)" % (cfile['filename'], e))
                return None
            if filter(is_not_punct_char, allmatchingseries['name'].lower()) == filter(is_not_punct_char,cfile['file_seriesname'].lower()):
                self.config['sid'] = allmatchingseries['sid']
                self.config['series_name'] = allmatchingseries['name']
                cfile['file_seriesname'] = allmatchingseries['name']
            else:
                sys.stderr.write(u"\nGuessing could not find an exact match on tvdb for Series (%s)\ntherefore a meta data dictionary cannot be returned\n\n" % cfile['filename'])
                return False
        else:
            if not self.verifySeriesExists():
                self._displayMessage(u"tvdb Series not found(%s) meta data dictionary cannot be returned" % cfile['filename'])
                return self._getSecondarySourceMetadata(cfile, available_metadata)

        if self.config['sid'] == '99999999':
            if not self.config['interactive']:
                return self._getSecondarySourceMetadata(cfile, available_metadata)
            else:
                return {'sid': self.config['sid'], 'title': cfile['file_seriesname']}

        meta_dict={}
        tmp_array=(self.getSeriesEpisodeData()).split('\n')

        for element in tmp_array:
            element = (element.rstrip('\n')).strip()
            if element == '':
                continue
            index = element.index(':')
            key = element[:index].lower()
            data = element[index+1:]
            if data == None:
                continue
            if key == 'series':
                meta_dict['title'] = data
                continue
            if key == 'seasonnumber':
                try:
                    meta_dict['season'] = int(data)
                except:
                    pass
                continue
            if key == 'episodenumber':
                try:
                    meta_dict['episode'] = int(data)
                except:
                    pass
                continue
            if key == 'episodename':
                meta_dict['subtitle'] = data
                continue
            if key == u'overview':
                meta_dict['plot'] = data
                continue
            if key == u'director' and data == 'None':
                meta_dict['director'] = ''
                continue
            if key == u'firstaired' and len(data) > 4:
                try:
                    meta_dict['year'] = int(data[:4])
                except:
                    pass
                meta_dict['firstaired'] = data
                try:
                    meta_dict['releasedate'] = datetime.datetime.strptime(data,'%Y-%m-%d').date()
                except ValueError:
                    pass
                continue
            if key == 'year':
                try:
                    meta_dict['year'] = int(data)
                except:
                    pass
                continue
            if key == 'seriesid':
                meta_dict['inetref'] = data
                meta_dict[key] = data
                continue
            if key == 'rating':
                try:
                    meta_dict['userrating'] = float(data)
                except:
                    pass
                continue
            if key == 'filename':# This "episodeimage URL clashed with the video file name and ep image
                continue        #  is not used yet. So skip fixes the db video filename from being wiped.
            if key == 'runtime':
                try:
                    meta_dict['length'] = long(data)
                except:
                    pass
                continue
            meta_dict[key] = data

        if len(meta_dict):
            if not meta_dict.has_key('director'):
                meta_dict['director'] = u''
            meta_dict['rating'] = u'TV Show'
            # URL to TVDB web site episode web page for this series
            for url_data in [u'seriesid', u'seasonid', u'id']:
                if not url_data in meta_dict.keys():
                    break
            else:
                meta_dict['homepage'] = u'http://www.thetvdb.com/?tab=episode&seriesid=%s&seasonid=%s&id=%s' % (meta_dict['seriesid'], meta_dict['seasonid'], meta_dict['id'])
            if available_metadata['hash'] == u'' or available_metadata['hash'] == None:
                filename = u'%s/%s.%s' % (cfile['filepath'], cfile['filename'], cfile['ext'])
                meta_dict['hash'] = self.hashFile(filename)
            available_metadata = self.combineMetaData(available_metadata, meta_dict, vid_type=False)
            return self._getSecondarySourceMetadata(cfile, available_metadata)
        else:
            self._displayMessage(u"tvdb Series found (%s) but no meta data for dictionary" % cfile['filename'])
            return self._getSecondarySourceMetadata(cfile, available_metadata)
    # end _getTvdbMetadata

    def _make_db_ready(self, text):
        '''Prepare text for inclusion into a DB
        return None
        return data base ready text
        '''
        if not text: return text
        try:
            text = text.replace(u'\u2013', u"-")
            text = text.replace(u'\u2014', u"-")
            text = text.replace(u'\u2018', u"'")
            text = text.replace(u'\u2019', u"'")
            text = text.replace(u'\u2026', u"...")
            text = text.replace(u'\u201c', u'"')
            text = text.replace(u'\u201d', u'"')
        except UnicodeDecodeError:
            pass

        return text
    # end make_db_ready

    def _addCastGenreCountry(self, data_string, vim, cast_genres_type):
        '''From a comma delimited string of cast members, genres or countries add the ones
        not already in the myth db and update the video's meta data
        return True when successfull
        return False if failed
        '''
        if data_string == '':
            return True
        data = data_string.split(',')
        for i in range(len(data)):
            data[i]=data[i].strip()
        try:
            data.remove('')
        except:
            pass

        if cast_genres_type == 'genres':
            for item in data:
                vim.genre.add(item)
        elif cast_genres_type == 'cast':
            for item in data:
                vim.cast.add(item)
        elif cast_genres_type == 'countries':
            for item in data:
                vim.country.add(item)

        return True
    # end _addCastGenreCountry()

    # Local variables
    errors = []
    new_names = []

    def _moveDirectoryTree(self, src, dst, symlinks=False, ignore=None):
        '''Move a directory tree from a given source to a given destination. Subdirectories will be
        created and synbolic links will be recreated in the new destination.
        return an array of two arrays. Names of files/directories moved and Errors found
        '''
        wild_card = False
        org_src = src
        if src[-1:] == '*':
            wild_card = True
            (src, fileName) = os.path.split(src)
            try:
                names = os.listdir(unicode(src, 'utf8'))
            except (UnicodeEncodeError, TypeError):
                names = os.listdir(src)
        else:
            if os.path.isfile(src):
                (src, fileName) = os.path.split(src)
                names = [fileName]
            else:
                try:
                    names = os.listdir(unicode(src, 'utf8'))
                except (UnicodeEncodeError, TypeError):
                    names = os.listdir(src)

        if ignore is not None:
            ignored_names = ignore(src, names)
        else:
            ignored_names = set()

        try:
            if self.config['simulation']:
                sys.stdout.write(u"Simulation creating subdirectories for file move (%s)\n" % dst)
            else:
                self._displayMessage(u"Creating subdirectories for file move (%s)\n" % dst)
                os.makedirs(dst)        # Some of the subdirectories may already exist
        except OSError:
            pass

        for name in names:
            if name in ignored_names:
                continue
            srcname = os.path.join(src, name)
            dstname = os.path.join(dst, name)

            if not os.access(srcname, os.F_OK | os.R_OK | os.W_OK): # Skip any file that is not RW able
                sys.stderr.write(u"\n! Error: The Source video directory or file (%s) must have read and write permissions for be moved. File or directory has been skipped\n" % (srcname))
                continue
            try:
                if symlinks and os.path.islink(srcname):
                    linkto = os.readlink(srcname)
                    if self.config['simulation']:
                        sys.stdout.write(u"Simulation recreating symbolic link linkto:\n(%s) destination name:\n(%s)\n" % (linkto, dstname))
                    else:
                        os.symlink(linkto, dstname)
                        self._displayMessage(u"Recreating symbolic link linkto:\n(%s) destination name:\n(%s)\n" % (linkto, dstname))
                    self.num_symbolic_links+=1
                elif os.path.isdir(srcname):
                        if wild_card:
                            self._displayMessage(u"Wildcard skipping subdirectory (%s)\n" % srcname)
                            continue
                        self.num_created_video_subdirectories+=1
                        self._displayMessage(u"Move subdirectory (%s)\n" % srcname)
                        self._moveDirectoryTree(srcname, dstname, symlinks, ignore)
                else:
                    if self.config['simulation']:
                        if wild_card:
                            if srcname.startswith(org_src[:-1]):
                                sys.stdout.write(u"Simulation move wild card file from\n(%s) to\n(%s)\n" % (srcname, dstname))
                                self.num_moved_video_files+=1
                                self.new_names.append(dstname)
                            else:
                                self._displayMessage(u"Simulation of wildcard skipping file(%s)" % (srcname,))
                        else:
                            sys.stdout.write(u"Simulation move file from\n(%s) to\n(%s)\n" % (srcname, dstname))
                            self.num_moved_video_files+=1
                            self.new_names.append(dstname)
                    else:
                        if wild_card:
                            if srcname.startswith(org_src[:-1]):
                                self._displayMessage(u"Move wild card file from\n(%s) to\n(%s)\n" % (srcname, dstname))
                                shutil.move(srcname, dstname)
                                self.num_moved_video_files+=1
                                self.new_names.append(dstname)
                            else:
                                self._displayMessage(u"Wildcard skipping file(%s)" % (srcname,))
                        else:
                            self._displayMessage(u"Move file from\n(%s) to\n(%s)\n" % (srcname, dstname))
                            shutil.move(srcname, dstname)
                            self.num_moved_video_files+=1
                            self.new_names.append(dstname)
                # XXX What about devices, sockets etc.?
            except (IOError, os.error), why:
                self.errors.append([srcname, dstname, str(why)])
            # catch the Error from the recursive move tree so that we can
            # continue with other files
            except:
                self.errors.append([src, dst, u"Unknown error"])

        return [self.new_names, self.errors]
    # end _moveDirectoryTree

    # local variable for move stats
    num_moved_video_files=0
    num_created_video_subdirectories=0
    num_symbolic_links=0

    def _moveVideoFiles(self, target_destination_array):
        """Copy files or directories to a destination directory.
        If the -F filename option is set then rename TV series during the move process. The move will
        be interactive for identifying a movie's IMDB number or TV series if the -i option was also set.
        If there is a problem error message are displayed and the script exists. After processing
        print a statistics report.
        return a array of video file dictionaries to update in Mythvideo data base
        """
        global UI_selectedtitle
        # Validate that the targets and destinations actually exist.
        count=1
        for file_dir in target_destination_array:
            if os.access(file_dir, os.F_OK | os.R_OK):
                if count % 2 == 0:
                    # Destinations must all be directories
                    if not os.path.isdir(file_dir):
                        sys.stderr.write(u"\n! Error: Destinations must all be directories.\nThis destination is not a directory (%s)\n" % (file_dir,))
                        sys.exit(1)
                    else:
                        tmp_dir = file_dir
                        for directory in self.config['mythvideo']:
                            dummy_dir = file_dir.replace(directory, u'')
                            if dummy_dir != tmp_dir:
                                break
                        else:
                            sys.stderr.write(u"\n! Error: Destinations must all be a mythvideo directory or subdirectory.\nThis destination (%s) is not one of the Mythvideo directories(%s)\n" % (file_dir, self.config['mythvideo'], ))
                            sys.exit(1)
                # Verify that a target file is really a video file.
                if file_dir[-1:] != '*': # Skip wildcard file name targets
                    if os.access(file_dir, os.F_OK | os.R_OK):    # Confirm that the file actually exists
                        if not os.path.isdir(file_dir):
                            ext = _getExtention(file_dir)
                            for tmp_ext in self.config['video_file_exts']:
                                if ext.lower() == tmp_ext:
                                    break
                            else:
                                sys.stderr.write(u"\n! Error: Target files must be video files(%s).\nSupported video file extentions(%s)\n" % (file_dir, self.config['video_file_exts'],))
                                sys.exit(1)
                    count+=1

        # Stats counters
        num_renamed_files = 0
        num_mythdb_updates = 0

        i = 0
        video_files_to_process=[]
        cfile_array=[]
        while i < len(target_destination_array):
            src = target_destination_array[i]
            wild_card = False
            if src[-1:] == u'*':
                org_src = src
                wild_card = True
                (src, fileName) = os.path.split(src)
            dst = target_destination_array[i+1]
            self.errors = []
            self.new_names = []
            if wild_card:
                results = self._moveDirectoryTree(org_src, dst, symlinks=False, ignore=None)
            else:
                results = self._moveDirectoryTree(src, dst, symlinks=False, ignore=None)
            if len(results[1]):            # Check if there are any errors
                sys.stderr.write(u"\n! Warning: There were errors during moving, with these directories/files\n")
                for error in results[1]:
                    sys.stderr.write(u'\n! Warning: Source(%s), Destination(%s), Reason:(%s)\n' % (error[0], error[1], error[2]))
            tmp_cfile_array=[]
            for name in results[0]:
                file_name = os.path.join(dst, name)
                if os.path.isdir(file_name):
                    for dictionary in self._processNames(_getFileList([file_name]), verbose = self.config['debug_enabled'], movies=True):
                        tmp_cfile_array.append(dictionary)
                else:
                    for dictionary in self._processNames([file_name], verbose = self.config['debug_enabled'], movies=True):
                        tmp_cfile_array.append(dictionary)

            # Is the source directory within a mythvideo directory? If it is,
            # update existing mythdb records else add the record as you already have the inetref
            for directory in self.config['mythvideo']:
                if src.startswith(directory):
                    for cfile in tmp_cfile_array:
                        tmp_path = src+cfile['filepath'].replace(dst, u'')
                        video_file = self.rtnRelativePath(self.movie_file_format % (tmp_path, cfile['filename'], cfile['ext']), 'mythvideo')
                        tmp_filename = self.rtnRelativePath(self.movie_file_format % (cfile['filepath'], cfile['filename'], cfile['ext']), 'mythvideo')
                        try:
                            metadata = mythvideo.searchVideos(exactfile=video_file).next()
                        except StopIteration:
                            try:
                                metadata = mythvideo.searchVideos(exactfile=self.movie_file_format % (tmp_path, cfile['filename'], cfile['ext']), host=localhostname.lower()).next()
                            except StopIteration:
                                continue

                        if tmp_filename[0] == '/':
                            host = u''
                            self.absolutepath = True
                        else:
                            host = localhostname.lower()
                            self.absolutepath = False

                        if self.config['simulation']:
                            sys.stdout.write(u"Simulation Mythdb update for old file:\n(%s) new:\n(%s)\n" % (video_file, tmp_filename))
                        else:
                            self._displayMessage(u"Mythdb update for old file:\n(%s) new:\n(%s)\n" % (video_file, tmp_filename))
                            metadata.update({'filename': tmp_filename, 'host': host})
                        num_mythdb_updates += 1
                    break
            else:
                pass
            cfile_array.extend(tmp_cfile_array)
            i+=2        # Increment by 2 because array is int pairs of target and destination

        # Attempt to rename the video file
        if self.config['ret_filename']:
            for index in range(len(cfile_array)):
                cfile = cfile_array[index]
                if self.config['mythtv_inetref'] or self.config['mythtv_ref_num']:
                    sys.stdout.write(u"\nAttempting to rename video filename (%s)\n" % cfile['file_seriesname'])
                if  cfile['seasno'] == 0 and cfile['epno'] == 0: # File rename for a movie
                    sid = None
                    new_filename = u''
                    if self.config['series_name_override']:
                        if self.config['series_name_override'].has_key(cfile['file_seriesname'].lower()):
                            sid = self.config['series_name_override'][cfile['file_seriesname'].lower()]
                    if not sid:
                        data = self._getTmdbIMDB(cfile['file_seriesname'], rtnyear=True)
                        if data:
                            sid = data[u'sid']
                            if data[u'sid'] == '99999999': # The user chose to ignore this video
                                continue
                            new_filename = self.sanitiseFileName(data[u'name'])
                        else:
                            continue
                    else:
                        imdb_access = imdb.IMDb()
                        try:
                            data = imdb_access.get_movie(sid)
                            if data.has_key('long imdb title'):
                                new_filename = data['long imdb title']
                            elif data.has_key('title'):
                                new_filename = self.sanitiseFileName(namedata['title'])
                            else:
                                continue
                        except imdb._exceptions.IMDbDataAccessError:
                            continue

                    if not sid:    # Cannot find this movie skip the renaming
                        continue
                    inetref = sid
                    if not new_filename:
                        continue
                    else:
                        cfile_array[index]['file_seriesname'] = new_filename
                else:    # File rename for a TV Series Episode
                    UI_selectedtitle = u''
                    new_filename = u''
                    self.config['sid'] = None
                    self.config['series_name'] = cfile['file_seriesname']
                    if self.config['series_name_override']:
                        if self.config['series_name_override'].has_key(cfile['file_seriesname'].lower()):
                            self.config['sid'] = self.config['series_name_override'][cfile['file_seriesname'].lower()]
                            self.config['series_name'] = None
                    self.config['season_num'] = u"%d" % cfile['seasno']
                    self.config['episode_num'] = u"%d" % cfile['epno']
                    self.config['episode_name'] = None
                    new_filename = self.returnFilename()
                    inetref = self.config['sid']
                    if inetref == '99999999': # User chose to ignore this video
                        continue

                if new_filename:
                    if new_filename == cfile['filename']: # The file was already named to standard format
                        self._displayMessage(u"File is already the correct name(%s)\n" % cfile['filename'])
                        continue
                    video_file = self.movie_file_format % (cfile['filepath'], cfile['filename'], cfile['ext'])
                    tmp_filename = self.movie_file_format % (cfile['filepath'], new_filename, cfile['ext'])
                    if self.config['simulation']:
                        sys.stdout.write(u"Simulation file renamed from(%s) to(%s)\n" % (video_file, tmp_filename))
                    else:
                        if not os.access(video_file, os.F_OK | os.R_OK | os.W_OK):
                            sys.stdout.write(u"Cannot rename this file as it does not have read/write permissions set (%s)\n" % video_file)
                            continue
                        self._displayMessage(u"File renamed from(%s) to(%s)\n" % (video_file, tmp_filename))
                        os.rename(video_file, tmp_filename)
                    num_renamed_files+=1
                    video_file = self.rtnRelativePath(self.movie_file_format % (cfile['filepath'], cfile['filename'], cfile['ext']), 'mythvideo')
                    tmp_filename = self.rtnRelativePath(self.movie_file_format % (cfile['filepath'], new_filename, cfile['ext']), 'mythvideo')

                    try:
                        metadata = mythvideo.searchVideos(exactfile=video_file).next()
                    except StopIteration:
                        try:
                            metadata = mythvideo.searchVideos(exactfile=self.movie_file_format % (cfile['filepath'], cfile['filename'], cfile['ext']), host=localhostname.lower()).next()
                        except StopIteration:
                            metadata = Video(db=mythvideo)

                    if tmp_filename[0] == '/':
                        host = u''
                        self.absolutepath = True
                    else:
                        host = localhostname.lower()
                        self.absolutepath = False

                    if metadata.intid:
                        if self.config['simulation']:
                            sys.stdout.write(u"Simulation Mythdb update for renamed file(%s)\n" % (tmp_filename))
                        else:
                            self._displayMessage(u"Mythdb update for renamed file(%s)\n" % (tmp_filename))
                            metadata.update({'filename': tmp_filename, 'host': host})
                    else:
                        if self.config['simulation']:
                            sys.stdout.write(u"Simulation Mythdb add for renamed file(%s)\n" % (tmp_filename))
                        else:
                            self._displayMessage(u"Adding Mythdb record for file(%s)\n" % (tmp_filename))
                            metadata.title = cfile['file_seriesname']
                            metadata.filename = tmp_filename
                            metadata.host = host
                            metadata.inetref = inetref
                            metadata.create()

                    cfile_array[index]['filename'] = new_filename

        if self.config['simulation']:
            sys.stdout.write(u'\n---------Simulated Statistics---------------')
        sys.stdout.write('\n--------------Move Statistics---------------\nNumber of subdirectories ............(% 5d)\nNumber of files moved ...............(% 5d)\nNumber of symbolic links recreated...(% 5d)\nNumber of renamed TV-eps or movies.. (% 5d)\nNumber of Myth database updates .... (% 5d)\n--------------------------------------------\n\n' % (self.num_created_video_subdirectories, self.num_moved_video_files, self.num_symbolic_links, num_renamed_files, num_mythdb_updates))

        return cfile_array
    # end _moveVideoFiles

    def _displayMessage(self, message):
        """Displays messages through stdout. Usually used with MythTv metadata updates in -V
        verbose mode.
        returns nothing
        """
        if message[-1:] != '\n':
            message+='\n'
        if self.config['mythtv_verbose']:
            sys.stdout.write(message)
    # end _displayMessage

    def _findMissingInetref(self):
        '''Find any video file without a Mythdb record or without an inetref number
        return None if there are no new video files
        return a array of dictionary information on each video file that qualifies for processing
        '''
        directories=self.config['mythvideo']

        if not len(directories):
            sys.stderr.write(u"\n! Error: There must be a video directory specified in MythTv\n")
            sys.exit(1)

        allFiles = self._findFiles(directories, self.config['recursive'] , verbose = self.config['debug_enabled'])
        validFiles = self._processNames(allFiles, verbose = self.config['debug_enabled'], movies=True)
        if len(validFiles) == 0:    # Is there video files to process?
            return None

        missing_list=[]
        for cfile in validFiles:
            try:
                videopath = self.movie_file_format % (cfile['filepath'], cfile['filename'], cfile['ext'])
            except UnicodeDecodeError:
                videopath = os.path.join(unicode(cfile['filepath'],'utf8'), unicode(cfile['filename'],'utf8')+u'.'+cfile['ext'])

            # Find the MythTV meta data
            try:
                metadata = mythvideo.searchVideos(exactfile=videopath).next()
            except StopIteration:
                try:
                    if not self.config['video_dir']:
                        metadata = mythvideo.searchVideos(exactfile=self.rtnRelativePath(videopath, 'mythvideo'), host=localhostname.lower()).next()
                        if _can_int(metadata.inetref) and (metadata.inetref != u'00000000'):
                            continue
                except StopIteration:
                    pass

            missing_list.append(cfile)

        return missing_list
    # end _findMissingInetref

    def _checkValidGraphicFile(self, filename, graphicstype=u'', vidintid=False):
        '''Verify that a graphics file is not really an HTML file
        return True if it is a graphics file
        return False if it is an HTML file
        '''
        # Verify that the graphics file is NOT HTML instead of the intended graphics file
        try:
            p = subprocess.Popen(u'file "%s"' % filename, shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
        except:
            # There is something wrong with the file but do NOT say it is invalid just in case!
            return True
        data = p.stdout.readline()
        try:
            data = data.encode('utf8')
        except UnicodeDecodeError:
            data = unicode(data,'utf8')
        index = data.find(u'HTML document text')
        if index == -1:
            return True
        elif self.config['simulation']:
            sys.stdout.write(
                u"Simulation deleting bad graphics file (%s) as it is really HTML\n" % (filename, )
            )
            if vidintid:
                sys.stdout.write(
                    u"and the MythVideo record was corrected for the graphic reference.\n"
                )
            return False
        else:
            os.remove(filename) # Delete the useless HTML text
            sys.stderr.write( u"\n! Warning: The graphics file (%s) is actually HTML and not the intended file type.\nDuring the original file download the web site had issues. The bad downloaded file was removed.\n" % (filename))
            if vidintid:
                repair = {}
                if graphicstype == u'coverfile':
                    repair[graphicstype] = u'No Cover'
                else:
                    repair[graphicstype] = u''
                Video(vidintid, db=mythvideo).update(repair)
            return False
    # end _checkValidGraphicFile()


    def _graphicsCleanup(self):
        '''Match the graphics in the mythtv graphics directories with the ones specified by the
        mythvideometa records. Remove any graphics that are not referenced at least once. Print a
        report.
        '''
        global localhostname
        num_total = 0
        num_deleted = 1
        num_new_total = 2
        stats = {'coverfile': [0,0,0], 'banner': [0,0,0], 'fanart': [0,0,0]}

        graphics_file_dict={}
        all_graphics_file_list=[]
        for directory in graphicsDirectories.keys():
            if directory == 'screenshot':
                continue
            file_list = _getFileList(self.config[graphicsDirectories[directory]])
            if not len(file_list):
                graphics_file_dict[directory] = []
                continue
            for g_file in list(file_list):        # Cull the list removing dirs and non-graphics files
                if os.path.isdir(g_file):
                    file_list.remove(g_file)
                    continue
                g_ext = _getExtention(g_file)
                if not g_ext in self.image_extensions:
                    file_list.remove(g_file)
                    continue
            for filel in file_list:
                if not filel in all_graphics_file_list:
                    all_graphics_file_list.append(filel)
            graphics_file_dict[directory] = file_list

        for key in graphicsDirectories.keys():    # Set initial totals
            if key == 'screenshot':
                continue
            stats[key][num_total] = len(graphics_file_dict[key])

        # Start reading videometadata records to remove their graphics from the image orphan list
        try:
            records = mythvideo.searchVideos()
        except MythError, e:
            sys.stderr.write(u"\n! Error: Reading all videometadata records: %s\n" % e.args[0])
            return

        atleast_one_video_file = False
        if records:
            for record in records:
                atleast_one_video_file = True
                meta_dict = {'host': record.host, 'coverfile': record.coverfile, 'banner': record.banner, 'fanart': record.fanart, 'filename': record.filename, 'intid': record.intid, 'inetref': record.inetref, }
                # Skip any videometadata record that is not for this host
                if meta_dict['host'] != u'' and meta_dict['host'] != None:
                    if meta_dict['host'].lower() != localhostname.lower():
                        continue
                # Start removing any graphics in this videometadata record
                for key in meta_dict.keys():
                    if key in ['host','filename','intid', 'inetref']:
                        continue
                    if meta_dict[key] in [None, u'', u'None', u'No Cover', u'Unknown']:
                        continue

                    # Deal with videometadata record using storage groups
                    if meta_dict['filename'] != None:
                        if meta_dict['filename'][0] == u'/':
                            self.absolutepath = True
                        else:
                            self.absolutepath = False
                    if meta_dict[key][0] != '/':
                        meta_dict[key] = self.rtnAbsolutePath(meta_dict[key], graphicsDirectories[key])
                        if meta_dict[key][0] != '/': # There is not a storage group for this relative file name
                            continue

                    # Deal with TV series level graphics
                    (dirName, fileName) = os.path.split(meta_dict[key])
                    (fileBaseName, fileExtension)=os.path.splitext(fileName)
                    index = fileBaseName.find(u' Season ')
                    intid = meta_dict['intid']

                    if index != -1: # Is this a TV Series episode?
                        if meta_dict[key] in graphics_file_dict[key]:
                            if self._checkValidGraphicFile(meta_dict[key], graphicstype=key, vidintid=intid) == True:
                                graphics_file_dict[key].remove(meta_dict[key])
                                all_graphics_file_list.remove(meta_dict[key])
                    # This logic is specific to Movies and videos with a '99999999' inetref numbers
                    elif fileName.startswith(meta_dict['inetref']+u'_') or fileName.startswith(meta_dict['inetref']+u'.') or meta_dict['inetref'] == '99999999':
                        if meta_dict[key] in graphics_file_dict[key]:
                            if self._checkValidGraphicFile(meta_dict[key], graphicstype=key, vidintid=intid) == True:
                                graphics_file_dict[key].remove(meta_dict[key])
                                all_graphics_file_list.remove(meta_dict[key])

        if not atleast_one_video_file:
            sys.stderr.write(u"\n! Error: Janitor - did not find any video files to process so skipping\nimage clean up to protect your image files, in case this is a configuration or NFS error.\nIf you do not use MythVideo then the Janitor option (-MJ) is not of value to you on this MythTV back end.\n")
            return
        # end reading videometadata records to remove their graphics from the image orphan list

        # Get Scheduled and Recorded program list
        programs = self._getScheduledRecordedProgramList()

        # Remove Scheduled and Recorded program's graphics files from the delete list
        if programs:
            for field in graphicsDirectories.keys():
                if field == 'screenshot':
                    continue
                remove=[]
                for graphic in graphics_file_dict[field]:
                    (dirName, fileName) = os.path.split(graphic)
                    (fileBaseName, fileExtension)=os.path.splitext(fileName)
                    for program in programs:
                        if fileBaseName.lower().startswith(program['title'].lower()+u' '):
                            remove.append(graphic)
                            break
                        if not isValidPosixFilename(program['title']) and program['seriesid'] != u'':
                            if fileBaseName.lower().startswith(program['seriesid'].lower()):
                                remove.append(graphic)
                                break
                for rem in remove:
                    if self._checkValidGraphicFile(rem, graphicstype=u'', vidintid=False) == True:
                        graphics_file_dict[field].remove(rem)
                        try:
                            all_graphics_file_list.remove(rem)
                        except ValueError, e:
                            pass

        # Do not remove the MiroBridge default image files even if they are not currently being used
        for filel in list(all_graphics_file_list):
            if filel.endswith('mirobridge_coverart.jpg'):
                all_graphics_file_list.remove(filel)
                continue
            if filel.endswith('mirobridge_banner.jpg'):
                all_graphics_file_list.remove(filel)
                continue
            if filel.endswith('mirobridge_fanart.jpg'):
                all_graphics_file_list.remove(filel)
                continue

        for key in graphicsDirectories.keys():    # Set deleted files totals
            if key == 'screenshot':
                continue
            file_list = list(graphics_file_dict[key])
            for filel in file_list:
                if not filel in all_graphics_file_list:
                    graphics_file_dict[key].remove(filel)
            stats[key][num_deleted] = len(graphics_file_dict[key])

        # Delete all graphics files still on the delete list
        for filel in all_graphics_file_list:
            if self.config['simulation']:
                sys.stdout.write(
                    u"Simulation deleting (%s)\n" % (filel)
                )
            else:
                try:
                    os.remove(filel)
                except OSError:
                    pass
                self._displayMessage(u"(%s) Has been deleted\n" % (filel))

        for key in graphicsDirectories.keys():    # Set new files totals
            if key == 'screenshot':
                continue
            stats[key][num_new_total] = stats[key][num_total] - stats[key][num_deleted]

        if self.config['simulation']:
            sys.stdout.write(u'\n\n------------Simulated Statistics---------------')
        sys.stdout.write(u'\n--------------Janitor Statistics---------------\n')
        stat_type = ['total', 'deleted', 'new total']
        for index in range(len(stat_type)):
            for key in graphicsDirectories.keys():    # Print stats
                if key == 'screenshot':
                    continue
                if key == 'coverfile':
                    g_type = 'posters'
                else:
                    g_type = key+'s'
                sys.stdout.write(u'% 9s % 7s ......................(% 5d)\n' % (stat_type[index], g_type, stats[key][index], ))

        for key in graphicsDirectories.keys():    # Print stats
            if key == 'screenshot':
                continue
            if not len(graphics_file_dict[key]):
                continue
            if key == 'coverfile':
                g_type = 'poster'
            else:
                g_type = key
            sys.stdout.write(u'\n----------------Deleted %s files---------------\n' % g_type)
            for graphic in graphics_file_dict[key]:
                sys.stdout.write('%s\n' % graphic)
        return
    # end _graphicsCleanup

    def _getVideoLength(self, videofilename):
        '''Using ffmpeg (if it can be found) get the duration of the video
        return False if either ffmpeg cannot be found or the file is not a video
        return video lenght in minutes
        '''
        if not self.config['ffmpeg']:
            return False

        # Filter out specific file types due to potential negative processing overhead
        if _getExtention(videofilename) in [u'iso', u'img', u'VIDEO_TS', u'm2ts', u'vob']:
            return False

        p = subprocess.Popen(u'ffmpeg -i "%s"' % (videofilename), shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)

        ffmpeg_found = True
        while 1:
            data = p.stderr.readline()
            if data.endswith('not found\n'):
                ffmpeg_found = False
                break
            if data.startswith('  Duration:'):
                break
            if data == '' and p.poll() != None:
                break

        if ffmpeg_found == False:
            self.config['ffmpeg'] = False
            return False
        elif data:
            time = (data[data.index(':')+1: data.index('.')]).strip()
            return (60*(int(time[:2]))+(int(time[3:5])))
        else:
            return False
    # end _getVideoLength


    def _getMiroVideometadataRecords(self):
        """Fetches all videometadata records with an inetref of '99999999' and a category of 'Miro'. If the
        videometadata record has a host them it must match the lower-case of the locahostname.
        aborts if processing failed
        return and array of matching videometadata dictionary records
        """
        global localhostname
        intids = []
        try:
            records = mythvideo.searchVideos(category=u'Miro', custom=(('inetref=%s','99999999'),))
        except MythError, e:
            sys.stderr.write(u"\n! Error: Reading all Miro videometadata records: %s\n" % e.args[0])
            sys.exit(1)
        if records:
            for record in records:
                intids.append(record.intid)

        videometadatarecords=[]
        if len(intids):
            for intid in intids:
                vidrec = Video(intid, db=mythvideo)
                if vidrec[u'host'] != u'' and vidrec[u'host'] != None:
                    if vidrec[u'host'].lower() != localhostname.lower():
                        continue
                videometadatarecords.append(vidrec)

            return videometadatarecords
        else:
            return None
        # end _getMiroVideometadataRecords()

    def _getExtraMiroDetails(self, mythvideorec, vidtype):
        '''Find the extra details required for Miro MythVideo record processing
        return a dictionary of details required for processing
        '''
        extradata = {}
        extradata[u'intid'] = [mythvideorec[u'intid']]
        if vidtype == u'movies':
            extradata[u'tv'] = False
        else:
            extradata[u'tv'] = True

        for key in [u'coverfile', u'banner', u'fanart', ]:
            extradata[key] = True    # Set each graphics type as if it has already been downloaded
            if mythvideorec[key] == None or mythvideorec[key] == u'No Cover' or mythvideorec[key] == u'':
                extradata[key] = False
                continue
            elif key == u'coverfile': # Look for undersized coverart
                if mythvideorec[u'filename'][0] == u'/':
                    self.absolutepath = True
                else:
                    self.absolutepath = False
                filename = self.rtnAbsolutePath(mythvideorec[key], graphicsDirectories[key])
                try:
                    (width, height) = self.config['image_library'].open(filename).size
                    if width < self.config['min_poster_size']:
                        extradata[key] = False
                        continue
                except IOError:
                    extradata[key] = False
                    continue
                continue
            else: # Check if the default graphics are being used
                if mythvideorec[key].endswith(u'mirobridge_banner.jpg'):
                    extradata[key] = False
                if mythvideorec[key].endswith(u'mirobridge_fanart.jpg'):
                    extradata[key] = False
                continue

        if vidtype == u'movies': # Data specific to Movie Trailers
            if mythvideorec[u'filename'][0] == u'/':
                self.absolutepath = True
            else:
                self.absolutepath = False
            extradata[u'filename'] = mythvideorec[u'filename']
            extradata[u'pathfilename'] = self.rtnAbsolutePath(mythvideorec[u'filename'], u'mythvideo')
            if os.path.islink(extradata[u'pathfilename']):
                extradata[u'symlink'] = True
            else:
                extradata[u'symlink'] = False
            moviename = mythvideorec['subtitle']
            if not moviename:
                moviename = ''
            else:
                index = moviename.find(self.config[u'mb_movies'][filter(is_not_punct_char, mythvideorec[u'title'].lower())])
                if index != -1:
                    moviename = moviename[:index].strip()
            extradata[u'moviename'] = moviename
            extradata[u'inetref'] = False
            if not moviename == None and not moviename == '':
                lastyear = int(datetime.datetime.now().strftime(u"%Y"))
                years = []
                i = 0
                while i < 5: # Check for a Movie that will be released this year or the next four years
                    years.append(u"%d" % ((lastyear+i)))
                    i+=1
                imdb_access = imdb.IMDb()
                movies_found = []
                try:
                    movies_found = imdb_access.search_movie(moviename.encode("ascii", 'ignore'))
                except Exception:
                    pass
                tmp_movies={}
                for movie in movies_found: # Get rid of duplicates
                    if movie.has_key('year'):
                        temp =  {imdb_access.get_imdbID(movie): u"%s (%s)" % (movie['title'], movie['year'])}
                        if tmp_movies.has_key(temp.keys()[0]):
                            continue
                        tmp_movies[temp.keys()[0]] = temp[temp.keys()[0]]
                for year in years:
                    for movie in tmp_movies:
                        if filter(is_not_punct_char, tmp_movies[movie][:-7].lower()) == filter(is_not_punct_char, moviename.lower()) and tmp_movies[movie][-5:-1] == year:
                            extradata[u'inetref'] = u"%07d" % int(movie)
                            extradata[u'moviename'] = tmp_movies[movie]
                            extradata[u'year'] = year
                            break
                    if extradata[u'inetref']:
                        break
        return extradata
    # end _getExtraMiroDetails()

    def updateMiroVideo(self, program):
        '''Update the information in a Miro/MythVideo record
        return nothing
        '''
        global localhostname, graphicsDirectories

        mirodetails = program[u'miro']

        for intid in mirodetails[u'intid']:
            changed_fields = {}
            for key in graphicsDirectories.keys():
                if key == u'screenshot':
                    continue
                if mirodetails[key] != True and mirodetails[key] != False and mirodetails[key] != None and mirodetails[key] != u'Simulated Secondary Source graphic filename place holder':
                    # A graphics was downloaded
                    changed_fields[key] = mirodetails[key]

            if not mirodetails[u'tv'] and not mirodetails[u'symlink'] and os.access(mirodetails[u'pathfilename'], os.F_OK | os.R_OK | os.W_OK):
                changed_fields[u'inetref'] = mirodetails[u'inetref']
                changed_fields[u'subtitle'] = u''
                changed_fields[u'year'] = mirodetails[u'year']
                changed_fields[u'banner'] = u''
                (dirName, fileName) = os.path.split(mirodetails[u'pathfilename'])
                (fileBaseName, fileExtension) = os.path.splitext(fileName)
                try:
                    dir_list = os.listdir(unicode(dirName, 'utf8'))
                except (UnicodeEncodeError, TypeError):
                    dir_list = os.listdir(dirName)
                index = 1
                while index != 0:
                    filename = self.sanitiseFileName(u'%s - Trailer %d' % (mirodetails[u'moviename'], index))
                    fullfilename = u'%s/%s%s' % (dirName, filename, fileExtension)
                    for flenme in dir_list:
                        if fnmatch.fnmatch(flenme.lower(), u'%s.*' % filename.lower()):
                            break
                    else:
                        changed_fields[u'title'] = filename
                        if self.config['simulation']:
                            sys.stdout.write(
                            u"Simulation rename Miro-MythTV movie trailer from (%s) to (%s)\n" % (mirodetails[u'pathfilename'], fullfilename))
                        else:
                            os.rename(mirodetails[u'pathfilename'], fullfilename)
                        changed_fields[u'filename'] = self.rtnRelativePath(fullfilename, u'mythvideo')
                        if changed_fields[u'filename'][0] != u'/':
                            changed_fields[u'host'] = localhostname.lower()
                        else:    # Deal with the whole mixing Video SG and local with SG graphics mess
                            for key in graphicsDirectories.keys():
                                if key == u'screenshot' or not changed_fields.has_key(key):
                                    continue
                                if changed_fields[key][0] == u'/':
                                    continue
                                else:
                                    changed_fields.remove(key)
                        break
                    index+=1

            if len(changed_fields):
                if self.config['simulation']:
                    if program['subtitle']:
                        sys.stdout.write(
                        u"Simulation MythTV DB update for Miro video (%s - %s)\n" % (program['title'], program['subtitle']))
                    else:
                        sys.stdout.write(
                        u"Simulation MythTV DB update for Miro video (%s)\n" % (program['title'],))
                else:
                    Video(intid, db=mythvideo).update(changed_fields)
    # end updateMiroVideo()

    def _getScheduledRecordedProgramList(self):
        '''Find all Scheduled and Recorded programs
        return array of found programs, if none then empty array is returned
        '''
        global localhostname
        programs=[]

        # Get pending recordings
        try:
            progs = mythbeconn.getUpcomingRecordings()
        except MythError, e:
            sys.stderr.write(u"\n! Error: Getting Upcoming Recordings list: %s\n" % e.args[0])
            return programs

        for prog in progs:
            record={}
            if prog.title == None:
                continue
            record['title'] = prog.title
            record['subtitle'] = prog.subtitle
            record['seriesid'] = prog.seriesid

            if record['subtitle'] and prog.airdate != None:
                record['originalairdate'] = prog.airdate.year
            else:
                if prog.year != '0':
                    record['originalairdate'] = prog.year
                elif prog.airdate != None:
                    record['originalairdate'] = prog.airdate.year
            for program in programs:    # Skip duplicates
                if program['title'] == record['title']:
                    break
            else:
                programs.append(record)

        # Get recorded records
        try:
            recordedlist = list(mythdb.searchRecorded(hostname=localhostname))
        except MythError, e:
            sys.stderr.write(u"\n! Error: Getting recorded programs list: %s\n" % e.args[0])
            return programs

        if not recordedlist:
            return programs

        recordedprogram = {}
        for recordedRecord in recordedlist:
            if recordedRecord.recgroup == u'Deleted':
                continue
            recorded = {}
            if recordedRecord.title == None:
                continue
            if recordedRecord.chanid == 9999:
                recorded[u'miro_tv'] = True
            recorded[u'title'] = recordedRecord.title
            recorded[u'subtitle'] = recordedRecord.subtitle
            recorded[u'seriesid'] = recordedRecord.seriesid
            for program in programs:    # Skip duplicates
                if program['title'] == recorded['title']:
                    break
            else:
                programs.append(recorded)
                # Get Release year for recorded movies
                # Get Recorded videos recordedprogram / airdate
                try:
                    recordedDetails = dict(RecordedProgram.fromRecorded(recordedRecord))
                except MythError, e:
                    sys.stderr.write(u"\n! Error: Getting recordedprogram table record: %s\n" % e.args[0])
                    continue
                if not len(recordedDetails):
                    continue
                if not recordedDetails['subtitle']:
                    recordedprogram[recordedDetails['title']]= u'%d' % recordedDetails['airdate']

        # Add release year to recorded movies
        for program in programs:
            if recordedprogram.has_key(program['title']):
                program['originalairdate'] = recordedprogram[program['title']]


        # Add real names to mb_tv if they are among the recorded videos
        if len(self.config['mb_tv_channels']):
            for program in programs:
                programtitle = filter(is_not_punct_char, program[u'title'].lower())
                if programtitle in self.config['mb_tv_channels'].keys():
                    self.config['mb_tv_channels'][programtitle][1] = program[u'title']

        # Check that each program has an original airdate
        for program in programs:
            if not program.has_key('originalairdate'):
                program['originalairdate'] = u'0000' # Set the original airdate to zero (unknown)

        # If there are any Miro TV or movies to process then add them to the list
        if len(self.config['mb_tv_channels']) or len(self.config['mb_movies']):
            miromythvideorecs = self._getMiroVideometadataRecords()
            if miromythvideorecs:
                # Create array used to check for duplicates
                duplicatekeys = {}
                i = 0
                for program in programs:
                    programtitle = filter(is_not_punct_char, program[u'title'].lower())
                    if programtitle in self.config['mb_tv_channels'].keys():
                        if not program[u'title'] in duplicatekeys:
                            duplicatekeys[program[u'title']] = i
                    elif programtitle in self.config['mb_movies'].keys():
                        moviename = program['subtitle']
                        if not moviename:
                            moviename = ''
                        else:
                            index = moviename.find(self.config['mb_movies'][programtitle])
                            if index != -1:
                                moviename = moviename[:index].strip()
                        if not moviename in duplicatekeys:
                            duplicatekeys[moviename] = i
                    i+=1

                for record in miromythvideorecs:
                    program = {}
                    program[u'title'] = record[u'title']
                    program[u'subtitle'] = record[u'subtitle']
                    program[u'originalairdate'] = record[u'year']
                    recordtitle = filter(is_not_punct_char, record[u'title'].lower())
                    if recordtitle in self.config['mb_tv_channels'].keys():
                        if not record[u'title'] in duplicatekeys.keys():
                            program[u'miro'] = self._getExtraMiroDetails(record, u'tv')
                            duplicatekeys[program[u'title']] = len(programs)
                            programs.append(program)
                            self.config['mb_tv_channels'][recordtitle][1] = record[u'title']
                        elif programs[duplicatekeys[program[u'title']]].has_key(u'miro'):
                            programs[duplicatekeys[program[u'title']]][u'miro'][u'intid'].append(record[u'intid'])
                        else:
                            programs[duplicatekeys[program[u'title']]][u'miro'] = self._getExtraMiroDetails(record, u'tv')
                    elif recordtitle in self.config['mb_movies'].keys():
                        moviename = record['subtitle']
                        if not moviename:
                            moviename = ''
                        else:
                            index = moviename.find(self.config['mb_movies'][filter(is_not_punct_char, program[u'title'].lower())])
                            if index != -1:
                                moviename = moviename[:index].strip()
                        if not moviename in duplicatekeys.keys():
                            program[u'miro'] = self._getExtraMiroDetails(record, u'movies')
                            if program[u'miro'][u'inetref']:
                                duplicatekeys[moviename] = len(programs)
                                programs.append(program)
                        elif programs[duplicatekeys[moviename]].has_key(u'miro'):
                            programs[duplicatekeys[moviename]][u'miro'][u'intid'].append(record[u'intid'])
                        else:
                            program[u'miro'] = self._getExtraMiroDetails(record, u'movies')
                            if program[u'miro'][u'inetref']:
                                programs[duplicatekeys[moviename]][u'miro'] = self._getExtraMiroDetails(record, u'movies')

        # Check that each program has seriesid
        for program in programs:
            if not program.has_key('seriesid'):
                program['seriesid'] = u''     # Set an empty seriesid - Generall only for Miro Videos
            if program['seriesid'] == None:
                program['seriesid'] = u''     # Set an empty seriesid

        return programs
    # end _getScheduledRecordedProgramList


    def _getScheduledRecordedTVGraphics(self, program, graphics_type):
        '''Get TV show graphics for Scheduled and Recorded TV programs
        return None if no graphics found
        return fullpath and filename of downloaded graphics file
        '''
        if graphics_type == 'coverfile':
            graphics_type = 'poster'

        self.config['sid'] = None
        if self.config['series_name_override']:
            if self.config['series_name_override'].has_key(program['title'].lower()):
                self.config['sid'] = self.config['series_name_override'][program['title'].lower()]
        # Find out if there are any Series level graphics available
        self.config['toprated'] = True
        self.config['episode_name'] = None
        self.config['series_name'] = program['title']
        self.config['season_num'] = None
        self.config['episode_num'] = None

        series_graphics = self.getGraphics(graphics_type)

        if series_graphics != None:
            cfile = { 'file_seriesname': program['title'],
                    'inetref': self.config['sid'],
                    'seasno': self.config['season_num'],
                    'epno': self.config['episode_num'],
                    'filepath':u'',
                    'filename': program['title'],
                    'ext':u'',
                    'categories':u''
            }
            return self._getTvdbGraphics(cfile, graphics_type, toprated=True, watched=True)
        return None
    # end _getScheduledRecordedTVGraphics

    def _downloadScheduledRecordedGraphics(self):
        '''Get Scheduled and Recorded programs and Miro vidoes get their graphics if not already
        downloaded
        return (nothing is returned)
        '''
        global localhostname

        # Initialize reporting stats
        total_progs_checked = 0
        total_posters_found = 0
        total_banners_found = 0
        total_fanart_found = 0
        total_posters_downloaded = 0
        total_banners_downloaded = 0
        total_fanart_downloaded = 0
        total_miro_tv = 0
        total_miro_movies = 0

        programs = self._getScheduledRecordedProgramList()

        if not len(programs): # Is there any programs to process?
            return

        # Add any Miro Bridge mb_tv dictionary items to 'series_name_override' dictionary
        if not self.config['series_name_override'] and len(self.config['mb_tv_channels']):
            self.config['series_name_override'] = {}
        for miro_tv_key in self.config['mb_tv_channels'].keys():
            if self.config['mb_tv_channels'][miro_tv_key][0]:
                self.config['series_name_override'][self.config['mb_tv_channels'][miro_tv_key][1].lower()] = self.config['mb_tv_channels'][miro_tv_key][0]

        total_progs_checked = len(programs)

        # Get totals of Miro TV shows and movies that will be processed
        for program in programs:
            if program.has_key(u'miro'):
                if not program[u'miro'][u'tv']:
                    total_miro_movies+=1
                else:
                    total_miro_tv+=1
            elif program.has_key(u'miro_tv'):
                if filter(is_not_punct_char, program[u'title'].lower()) in self.config['mb_movies'].keys():
                    total_miro_movies+=1
                else:
                    total_miro_tv+=1

        # Prossess all TV shows and Movies
        for program in programs:
            program['need'] = False    # Initalize that this program does not need graphic(s) downloaded
            mirodetails = None
            program_override_tv = False
            # Check if a subtitle-less program is really a TV show with an override. This compensates for
            # poor EPG data sources (as has been reported from at least Australia)
            if not program['subtitle'] and program['title'].lower() in self.config['series_name_override']:
                try:
                    result = self._searchforSeries(program['title'])
                    program_override_tv = True
                except Exception, e:
                    pass

            # Even movies get the ' Season' added to the image names so that movie such as '1408' do not clash
            # with TMDB#ed image names
            pattern = u'%s Season*.*'
            if not program.has_key(u'miro'):
                if program['subtitle'] or program_override_tv:
                    graphics_name = program['title']
                else:
                    if not int(program['originalairdate']):
                        graphics_name = program['title']
                    else:
                        graphics_name = "%s (%s)" % (program['title'], program['originalairdate'])
            else:
                mirodetails = program[u'miro']
                if mirodetails[u'tv']:
                    graphics_name = program['title']
                else:
                    graphics_name = mirodetails[u'inetref']

            self.absolutepath = False        # All Scheduled Recorded and Miro videos start in the SG "Default"

            # Search for graphics that are already downloaded
            for directory in graphicsDirectories.keys():
                if directory == 'screenshot':    # There is no downloading of screenshots required
                    program[directory] = True
                    continue
                if directory == 'banner' and not program['subtitle']: # No banners for movies
                    program[directory] = True
                    continue
                elif mirodetails:
                    if not mirodetails[u'tv'] and directory == 'banner': # No banners for movies
                        program[directory] = True
                        continue
                if not mirodetails:
                    filename = program['title']
                elif mirodetails[u'tv']:
                    filename = program['title']
                else:
                    filename = mirodetails[u'inetref']

                # Deal with TV series names that would generate invalid file names for images TV and movies
                self.program_seriesid = None
                if not isValidPosixFilename(filename) and program['seriesid'] != u'':
                    filename = program['seriesid']
                    self.program_seriesid = program['seriesid']

                # Actual check for existing graphics
                for dirct in self.config[graphicsDirectories[directory]]:
                    try:
                        dir_list = os.listdir(unicode(dirct, 'utf8'))
                    except (UnicodeEncodeError, TypeError):
                        dir_list = os.listdir(dirct)
                    for flenme in dir_list:
                        if fnmatch.fnmatch(flenme.lower(), (pattern % filename).lower()):
                            program[directory] = True
                            if directory == 'coverfile':
                                total_posters_found +=1
                            elif directory == 'banner':
                                total_banners_found +=1
                            else:
                                total_fanart_found +=1
                            if mirodetails: # Update the Miro MythVideo records with any existing graphics
                                mirodetails[directory] = self.rtnRelativePath(u'%s/%s' % (dirct, flenme), directory)
                            break
                    else:
                        continue
                    break
                else:
                    program['need'] = True
                    program[directory] = False

            # Check if there are any graphics to download
            if not program['need']:
                if not mirodetails:
                    filename = program['title']
                elif mirodetails[u'tv']:
                    filename = program['title']
                else:
                    filename = mirodetails[u'moviename']
                self._displayMessage("All Graphics already downloaded for [%s]" % filename)
                if mirodetails: # Update the Miro MythVideo records with any new graphics
                    self.updateMiroVideo(program)
                continue

            if not mirodetails:
                # It is more efficient to find inetref of movie once
                if not program['subtitle'] and not program_override_tv:
                    if not program.has_key('inetref'): # Was the inetref number already found?
                        inetref = self._getTmdbIMDB(graphics_name, watched=True)
                        if not inetref:
                            self._displayMessage("No movie inetref [%s]" % graphics_name)
                            # Fake subtitle as this may be a TV series without a subtitle
                            program['subtitle']=' '
                        else:
                            self._displayMessage("Found movie inetref (%s),[%s]" % (inetref, graphics_name))
                            program['inetref'] = inetref
            elif not mirodetails[u'tv']:
                program['inetref'] = mirodetails[u'inetref']

            # Download missing graphics
            for key in graphicsDirectories.keys():
                if program[key]:    # Check if this type of graphic is already downloaded
                    continue
                miromovieflag = False
                if mirodetails:
                    if not mirodetails[u'tv']:
                        miromovieflag = True
                # This is a TV episode or Miro TV show
                if (program['subtitle'] or program_override_tv) and not miromovieflag:
                    results = self._getScheduledRecordedTVGraphics(program, key)
                    if results:
                        if not mirodetails:
                            filename = program['title']
                        elif mirodetails[u'tv']:
                            filename = program['title']
                        else:
                            filename = mirodetails[u'moviename']
                        if key == 'coverfile':
                            total_posters_downloaded +=1
                        elif key == 'banner':
                            total_banners_downloaded +=1
                        elif key == 'fanart':
                            total_fanart_downloaded +=1
                        if mirodetails:    # Save the filename for storing later
                            mirodetails[key] = results
                    else:
                        self._displayMessage("TV Series - No (%s) for [%s]" % (key, program['title']))
                else: # This is a movie
                    title = program['title']
                    filename = program['title']
                    if miromovieflag:
                        title = mirodetails[u'inetref']
                        filename = mirodetails[u'inetref']
                    cfile = { 'file_seriesname': title,
                            'inetref': program['inetref'],
                            'seasno': 0,
                            'epno': 0,
                            'filepath':u'',
                            'filename': filename,
                            'ext':u'',
                            'categories':u''
                    }
                    if key == 'coverfile':
                        g_type = '-P'
                    else:
                        g_type = '-B'
                    results = self._getTmdbGraphics(cfile, g_type, watched=True)
                    if not results:
                        results = self._getSecondarySourceGraphics(cfile, key, watched=True)
                    if results:
                        if key == 'coverfile':
                            total_posters_downloaded +=1
                        elif key == 'fanart':
                            total_fanart_downloaded +=1
                        if mirodetails:    # Save the filename for storing later
                            mirodetails[key] = results
                    else:
                        if not mirodetails:
                            filename = program['title']
                        else:
                            filename = mirodetails[u'moviename']
                        self._displayMessage("No (%s) for [%s]" % (key, filename))

            if mirodetails: # Update the Miro MythVideo records with any new graphics
                self.updateMiroVideo(program)

        # Print statistics
        sys.stdout.write(u'\n-----Scheduled & Recorded Statistics-------\nNumber of Scheduled & Recorded ......(% 5d)\nNumber of Fanart graphics found .....(% 5d)\nNumber of Poster graphics found .....(% 5d)\nNumber of Banner graphics found .....(% 5d)\nNumber of Fanart graphics downloaded (% 5d)\nNumber of Poster graphics downloaded (% 5d)\nNumber of Banner graphics downloaded (% 5d)\nNumber of Miro TV Shows ............ (% 5d)\nNumber of Miro Movie Trailers ...... (% 5d)\n' % (total_progs_checked, total_fanart_found, total_posters_found, total_banners_found, total_fanart_downloaded, total_posters_downloaded, total_banners_downloaded, total_miro_tv, total_miro_movies))

        if len(programs):
            sys.stdout.write(u'\n-------------Scheduled & Recorded----------\n')
            for program in programs:
                if not program.has_key(u'miro'):
                    if program.has_key(u'miro_tv'):
                        if filter(is_not_punct_char, program[u'title'].lower()) in self.config['mb_movies'].keys():
                            sys.stdout.write(u'Miro Movie Trailer: %s\n' % (program['title'], ))
                        else:
                            sys.stdout.write(u'Miro TV Show: %s\n' % (program['title'], ))
                    else:
                        if program['subtitle']:
                            sys.stdout.write(u'%s\n' % (program['title'], ))
                        else:
                            if program['originalairdate'] != u'0000':
                                sys.stdout.write(u'%s\n' % ("%s (%s)" % (program['title'], program['originalairdate'])))
                            else:
                                sys.stdout.write(u'%s\n' % (program['title'], ))
                elif program[u'miro'][u'tv']:
                    sys.stdout.write(u'Miro TV Show: %s\n' % (program['title'], ))
                else:
                    sys.stdout.write(u'Miro Movie Trailer: %s\n' % (program[u'miro'][u'moviename'], ))
        return
    # end _downloadScheduledRecordedGraphics()


    def findFileInDir(self, filename, directories, suffix=None, fuzzy_match=False):
        '''Find if a file is in any of the specified directories. An exact match or a variation.
        return False - File not found in directories
        return True - Absolute file name and path
        '''
        (dirName, fileName) = os.path.split(filename)
        (fileBaseName, fileExtension) = os.path.splitext(fileName)
        if fuzzy_match: # Match even when the names are not exactly the same by removing punctuation
            for dirct in directories:
                try:
                    dir_list = os.listdir(unicode(dirct, 'utf8'))
                except (UnicodeEncodeError, TypeError):
                    dir_list = os.listdir(dirct)
                match_list = []
                for file_name in dir_list:
                    match_list.append(filter(is_not_punct_char, file_name.lower()))
                if suffix:
                    if fileBaseName.find(suffix) == -1:
                        file_path = filter(is_not_punct_char, (u"%s%s%s" % (fileBaseName, suffix, fileExtension)).lower())
                        file_path2 = filter(is_not_punct_char, (u"%s%s" % (fileBaseName, fileExtension)).lower())
                    else:
                        file_path = filter(is_not_punct_char, (u"%s%s" % (fileBaseName, fileExtension)).lower())
                        file_path2 = filter(is_not_punct_char, (u"%s%s" % (fileBaseName.replace(suffix, u''), fileExtension)).lower())
                    if file_path in match_list:
                        return u'%s/%s' % (dirct, dir_list[match_list.index(file_path)])
                    if file_path2 in match_list:
                        return u'%s/%s' % (dirct, dir_list[match_list.index(file_path2)])
                    continue
                else:
                    file_path = filter(is_not_punct_char, (u"%s%s" % (fileBaseName, fileExtension)).lower())
                    if file_path in match_list:
                        return u'%s/%s' % (dirct, dir_list[match_list.index(file_path)])
            else:
                return False
        else: # Find an exact match
            for directory in directories:
                if filename[0] != u'/' and dirName != u'':
                    dir_name = u"%s/%s" % (directory, dirName)
                else:
                    dir_name = directory
                if suffix:
                    if fileBaseName.find(suffix) == -1:
                        file_path = u"%s/%s%s%s" % (dir_name, fileBaseName, suffix, fileExtension)
                        file_path2 = u'%s/%s' % (dir_name, fileName)
                    else:
                        file_path = u'%s/%s' % (dir_name, fileName)
                        file_path2 = u'%s/%s' % (dir_name, fileName.replace(suffix, u''))
                    if os.path.isfile(file_path):
                        return file_path
                    if os.path.isfile(file_path2):
                        return file_path2
                    continue
                else:
                    file_path = u'%s/%s' % (dir_name, fileName)
                    if os.path.isfile(file_path):
                        return file_path
            else:
                return False
    # end findFileInDir()


    # Local Variables
    num_secondary_source_graphics_downloaded=0
    num_secondary_source_metadata_downloaded=0

    def processMythTvMetaData(self):
        '''Check each video file in the mythvideo directories download graphics files and meta data then
        update MythTV data base meta data with any new information.
        '''
        # Verify that the proper fields are present
        db_version = mythdb.settings.NULL.DBSchemaVer
        field_names = mythvideo.tablefields['videometadata']
        for field in ['season', 'episode', 'coverfile', 'screenshot', 'banner', 'fanart']:
            if not field in field_names:
                sys.stderr.write(u"\n! Error: Your MythTv data base scheme version (%s) does not have the necessary fields at least (%s) is missing\n\n" % (db_version, field))
                sys.exit(1)

        # Initailize and instance to the TMDB api
        apikey = "c27cb71cff5bd76e1a7a009380562c62"
        if self.config['interactive']:
            # themoviedb.org api key given by Travis Bell for Mythtv
            self.config['tmdb_api'] = tmdb_api.MovieDb(apikey,
                mythtv = True,
                interactive = True,
                select_first = False,
                debug = self.config['debug_enabled'],
                custom_ui = None,
                language = self.config['local_language'],
                search_all_languages = True,)
        else:
            self.config['tmdb_api'] = tmdb_api.MovieDb(apikey,
                mythtv = True,
                interactive = False,
                select_first = False,
                debug = self.config['debug_enabled'],
                language = self.config['local_language'],
                search_all_languages = True,)

        # If there were directories specified move them and update the MythTV db meta data accordingly
        if self.config['video_dir']:
            if len(self.config['video_dir']) % 2 == 0:
                validFiles = self._moveVideoFiles(self.config['video_dir'])
                self.config[u'file_move_flag'] = False
            else:
                sys.stderr.write(u"\n! Error: When specifying target (file or directory) to move to a destination (directory) they must always be in pairs (target and destination directory).\nYou specified an uneven number of variables (%d) for target and destination pairs.\nVariable count (%s)\n" % (len(self.config['video_dir']), self.config['video_dir']))
                sys.exit(1)

        # Check if only missing inetref video's should be processed
        if self.config['mythtv_inetref'] or self.config['mythtv_ref_num']:
            validFiles = self._findMissingInetref()
            if validFiles == None:
                sys.stderr.write(u"\n! Warning: There were no missing interef video files found.\n\n")
                sys.exit(0)
            elif not len(validFiles):
                sys.stderr.write(u"\n! Warning: There were no missing interef video files found.\n\n")
                sys.exit(0)

        # Check if this is a Scheduled and Recorded graphics download request
        if self.config['mythtv_watched']:
            self._downloadScheduledRecordedGraphics()
            sys.exit(0)

        # Check if this is just a Janitor (clean up unused graphics files) request
        if self.config['mythtvjanitor']:
            self._graphicsCleanup()
            sys.exit(0)

        directories=self.config['mythvideo']

        if not len(directories):
            sys.stderr.write(u"\n! Error: There must be a video directory specified in MythTv\n")
            sys.exit(1)

        # Set statistics
        num_processed=0
        num_fanart_downloads=0
        num_posters_downloads=0
        num_banners_downloads=0
        num_episode_metadata_downloads=0
        num_movies_using_imdb_numbers=0
        num_symlinks_created=0
        num_mythdb_updates=0
        num_posters_below_min_size=0
        videos_with_small_posters=[]
        videos_using_imdb_numbers=[]
        videos_updated_metadata=[]
        missing_inetref=[]

        sys.stdout.write(u'Mythtv video database maintenance start: %s\n' % (datetime.datetime.now()).strftime("%Y-%m-%d %H:%M"))

        if not self.config['video_dir'] and not self.config['mythtv_inetref'] and not self.config['mythtv_ref_num']:
            allFiles = self._findFiles(directories, self.config['recursive'] , verbose = self.config['debug_enabled'])
            validFiles = self._processNames(allFiles, verbose = self.config['debug_enabled'], movies=True)

        if not len(validFiles):
            sys.stderr.write(u"\n! Error: No valid video files found\n")
            sys.exit(1)

        tv_series_season_format=u"%s/%s Season %d.%s"
        tv_series_format=u"%s/%s.%s"
        for cfile in validFiles:
            self._displayMessage(u"\nNow processing video file (%s)(%s)(%s)\n" % (cfile['filename'], cfile['seasno'], cfile['epno']))
            num_processed+=1

            # Find the MythTV meta data
            videopath = tv_series_format % (cfile['filepath'], cfile['filename'], cfile['ext'])
            try:
                metadata = mythvideo.searchVideos(exactfile=videopath).next()
            except StopIteration:
                try:
                    metadata = mythvideo.searchVideos(exactfile=self.rtnRelativePath(videopath, u'mythvideo'), host=localhostname.lower()).next()
                except StopIteration:
                    # create new empty entry
                    if not self.config['interactive'] and not self.config['mythtv_guess']:
                        continue
                    sys.stdout.write(u"\n\nEntry does not exist in MythDB.  Adding (%s).\n" % cfile['filename'])
                    metadata = Video().create({'title':cfile['file_seriesname'], 'filename':''})

            if (metadata.category == 'none') and (metadata.year == 1895):
                sys.stdout.write(u"\n\nEntry is set to default values.\nUpdating (%s).\n" % cfile['filename'])
                filename = self.rtnRelativePath(videopath, u'mythvideo')
                if filename.startswith('/'):
                    metadata.update({'filename': filename, u'host': u''})
                else:
                    metadata.update({'filename': filename, u'host': localhostname.lower()})

            if (cfile['seasno'] == 0) and (cfile['epno'] == 0):
                movie = True
            else:
                movie = False

            # Get a dictionary of the existing meta data plus a copy for update comparison
            meta_dict={}
            vim = Video(metadata.intid, db=mythvideo)
            for key in vim.keys():
                meta_dict[key] = vim[key]

            # Fix a metadata record that has an incorrectly initialized inetref number value
            if meta_dict['inetref'] == None:
                meta_dict['inetref'] = u'00000000'
            available_metadata = dict(meta_dict)

            available_metadata['season']=cfile['seasno']
            available_metadata['episode']=cfile['epno']

            if available_metadata['title'] == u'':
                available_metadata['title'] = cfile['file_seriesname']

            # Set whether a video file is stored in a Storage Group or not
            if available_metadata['filename'][0] == u'/':
                self.absolutepath = True
            else:
                self.absolutepath = False

            # There must be an Internet reference number. Get one for new records.
            if _can_int(meta_dict['inetref']) and not meta_dict['inetref'] == u'00000000' and not meta_dict['inetref'] == '':
                if meta_dict['inetref'] == '99999999': # Records that are not updated by Jamu
                    continue
                inetref = meta_dict['inetref']
                cfile['inetref'] = meta_dict['inetref']
            else:
                if movie:
                    if not self.config['interactive'] and not self.config['mythtv_guess']:
                        sys.stderr.write(u'\n! Warning: Skipping "%s" as there is no TMDB or IMDB number for this movie.\nUse interactive option (-I) or (-R) to select the TMDB or IMDB number.\n\n' % (cfile['file_seriesname']))
                        continue
                    inetref = self._getTmdbIMDB(available_metadata['title'])
                    cfile['inetref'] = inetref
                    if not inetref:
                        self._displayMessage(u"themoviedb.com does not recognize the movie (%s) - Cannot update metadata - skipping\n" % available_metadata['title'])
                        missing_inetref.append(available_metadata['title'])
                        continue
                    # Only update the reference number
                    if self.config['mythtv_ref_num'] or inetref == '99999999':
                        Video(metadata.intid, db=mythvideo).update({'inetref': inetref})
                        num_mythdb_updates+=1
                        videos_updated_metadata.append(cfile['filename'])
                        self._displayMessage(u"\nReference number (%s) added for (%s) \n" % (inetref, cfile['filename']))
                        continue
                else:
                    copy = {}
                    for key in available_metadata.keys():
                        copy[key] = available_metadata[key]
                    tmp_dict = self._getTvdbMetadata(cfile, copy)
                    if not tmp_dict:
                        self._displayMessage(u"thetvdb.com does not recognize the Season(%d) Episode(%d) for video file(%s) - skipping\n" % (cfile['seasno'], cfile['epno'], videopath))
                        missing_inetref.append(available_metadata['title'])
                        continue
                    inetref = tmp_dict['inetref']
                    available_metadata['title'] = tmp_dict['title']
                    cfile['file_seriesname'] = tmp_dict['title']
                    # Only update the reference number and title
                    if self.config['mythtv_ref_num'] or inetref == '99999999':
                        if inetref == u'99999999':
                            Video(metadata.intid, db=mythvideo).update({'inetref': inetref})
                        else:
                            Video(metadata.intid, db=mythvideo).update({'inetref': inetref, 'title': tmp_dict['title']})
                        num_mythdb_updates+=1
                        videos_updated_metadata.append(cfile['filename'])
                        self._displayMessage(u"\nReference number (%s) added for (%s) \n" % (inetref, cfile['filename']))
                        continue
                cfile['inetref'] = inetref
                available_metadata['inetref'] = inetref

            if (meta_dict['subtitle'] == None or meta_dict['subtitle'] == '') and not movie:
                tmp_subtitle = self._getSubtitle(cfile)
                if tmp_subtitle == None:
                    self._displayMessage(u"thetvdb.com does not recognize the Season(%d) Episode(%d) for video file(%s) - skipping\n" % (cfile['seasno'], cfile['epno'], videopath))
                    continue
                else:
                    available_metadata['subtitle'] = tmp_subtitle
                    available_metadata['title'] = self.config['series_name']
                    cfile['file_seriesname'] = self.config['series_name']

            # Check if current inetref is a IMDB#
            # If so then check it can be changed to tmdb#
            # If it can be changed then rename any graphics and update meta data
            if movie and len(inetref) == 7:
                self._displayMessage(u"%s has IMDB# (%s)" % (available_metadata['title'], inetref))
                num_movies_using_imdb_numbers+=1
                videos_using_imdb_numbers.append(u"%s has IMDB# (%s)" % (available_metadata['title'], inetref))
                movie_data = self._getTmdbMetadata(cfile, dict(available_metadata))
                if movie_data.has_key('inetref'):
                    if available_metadata['inetref'] != movie_data['inetref']:
                        available_metadata['inetref'] = movie_data['inetref']
                        inetref = movie_data['inetref']
                        cfile['inetref'] = movie_data['inetref']
                        for graphic_type in ['coverfile', 'banner', 'fanart']: # Rename graphics files
                            if available_metadata[graphic_type] == None or available_metadata[graphic_type] == '':
                                continue
                            graphic_file = self.rtnAbsolutePath(available_metadata[graphic_type], graphicsDirectories[graphic_type])
                            if os.path.isfile(graphic_file):
                                filepath, filename = os.path.split(graphic_file)
                                filename, ext = os.path.splitext( filename )
                                ext = ext[1:]
                                if self.config['simulation']:
                                    sys.stdout.write(
                                        u"Simulation renaming (%s) to (%s)\n" % (graphic_file, tv_series_format % (filepath, inetref+self.graphic_suffix[graphic_type], ext))
                                    )
                                else:
                                    dest = tv_series_format % (filepath, inetref+self.graphic_suffix[graphic_type], ext)
                                    try:
                                        if not os.path.isfile(dest):
                                            os.rename(graphic_file, dest)
                                    except IOError, e:
                                        sys.stderr.write(
                                            u"Renaming image file (%s) to (%s) failed, error(%s)\n" % (graphic_file, dest, e))

                                    self._displayMessage(u"Renamed (%s) to (%s)\n" % (graphic_file, tv_series_format % (filepath, inetref+self.graphic_suffix[graphic_type], ext)))
                                available_metadata[graphic_type]= self.rtnRelativePath(dest,  graphicsDirectories[graphic_type])

            ###############################################################################
            # START of metadata Graphics logic - Checking, downloading, renaming
            ###############################################################################
            for graphic_type in ['coverfile', 'banner', 'fanart']:
                ###############################################################################
                # START of MOVIE graphics updating
                ###############################################################################
                # Check that there are local graphics path for abs path video
                # An abs path video can only use the FE specified graphic directories
                if self.absolutepath:
                    if not len(self.config['localpaths'][graphicsDirectories[graphic_type]]):
                        continue
                    graphicsdirs = self.config['localpaths'][graphicsDirectories[graphic_type]]
                else:
                    graphicsdirs = self.config[graphicsDirectories[graphic_type]]
                if movie:
                    if graphic_type == 'banner':
                        continue
                    if graphic_type == 'coverfile':
                        g_type = '-P'
                    else:
                        g_type = '-B'
                    need_graphic = True
                    undersized_graphic = False
                    for ext in self.image_extensions:
                        for graphicsdir in graphicsdirs:
                            filename = self.findFileInDir(u"%s.%s" % (inetref, ext), [graphicsdir], suffix=self.graphic_suffix[graphic_type])

                            if filename:
                                available_metadata[graphic_type]=self.rtnRelativePath(filename,  graphicsDirectories[graphic_type])
                                if graphic_type == 'coverfile':
                                    try:
                                        (width, height) = self.config['image_library'].open(filename).size
                                        if width < self.config['min_poster_size']:
                                            num_posters_below_min_size+=1
                                            videos_with_small_posters.append(cfile['filename'])
                                            undersized_graphic = True
                                            break
                                    except IOError:
                                        undersized_graphic = True
                                        break
                                need_graphic = False
                                break
                        if not need_graphic:
                            break

                    if need_graphic == True:
                        dummy_graphic = self._getTmdbGraphics(cfile, g_type)

                        # Try secondary source if themoviedb.com did not have graphicrecord['title']
                        if dummy_graphic == None or undersized_graphic == True:
                            dummy_graphic = self._getSecondarySourceGraphics(cfile, graphic_type)

                        if dummy_graphic != None:
                            available_metadata[graphic_type] = self.rtnRelativePath(dummy_graphic,  graphicsDirectories[graphic_type])
                            if graphic_type == 'fanart':
                                self._displayMessage(u"Movie - Added fan art for(%s)" % cfile['filename'])
                                num_fanart_downloads+=1
                            else:
                                self._displayMessage(u"Movie - Added a poster for(%s)" % cfile['filename'])
                                num_posters_downloads+=1
                    continue
                    # END of Movie graphics updates ###############################################
                else:
                    ###############################################################################
                    # START of TV Series graphics updating
                    ###############################################################################
                    need_graphic = False
                    new_format = True # Initialize that a graphics file NEEDS a new format
                    # Check if an existing TV series graphic file is in the old naming format
                    if available_metadata[graphic_type] != None and available_metadata[graphic_type] != 'No Cover' and available_metadata[graphic_type] != '':
                        graphic_file = self.rtnAbsolutePath(available_metadata[graphic_type], graphicsDirectories[graphic_type])
                        filepath, filename = os.path.split(graphic_file)
                        filename, ext = os.path.splitext( filename )
                        if filename.find(u' Season ') != -1:
                            new_format = False
                    else:
                        need_graphic = True
                    if need_graphic or new_format: # Graphic does not exist or is in an old format
                        for ext in self.image_extensions:
                            for graphicsdir in graphicsdirs:
                                filename=self.findFileInDir(u"%s Season %d.%s" % (self.sanitiseFileName(available_metadata['title']), available_metadata['season'], ext), [graphicsdir], suffix=self.graphic_suffix[graphic_type], fuzzy_match=True)
                                if filename:
                                    available_metadata[graphic_type]=self.rtnRelativePath(filename,  graphicsDirectories[graphic_type])
                                    need_graphic = False
                                    if graphic_type == 'coverfile':
                                        try:
                                            (width, height) = self.config['image_library'].open(filename).size
                                            if width < self.config['min_poster_size']:
                                                num_posters_below_min_size+=1
                                                videos_with_small_posters.append(cfile['filename'])
                                                break
                                        except IOError:
                                            undersized_graphic = True
                                            break
                                    break
                            if not need_graphic:
                                break
                        else:
                            graphic_file = self.rtnAbsolutePath(available_metadata[graphic_type], graphicsDirectories[graphic_type])
                            if not graphic_file == None:
                                graphic_file = self.findFileInDir(graphic_file, [graphicsdir], suffix=self.graphic_suffix[graphic_type], fuzzy_match=True)
                            if graphic_file == None:
                                need_graphic = True
                            if not need_graphic: # Have graphic but may be using an old naming convention
                                must_rename = False
                                season_missing = False
                                suffix_missing = False
                                if graphic_file.find(u' Season ') == -1: # Check for Season
                                    must_rename = True
                                    season_missing = True
                                if graphic_file.find(self.graphic_suffix[graphic_type]) == -1:
                                    must_rename = True
                                    suffix_missing = True
                                if must_rename:
                                    filepath, filename = os.path.split(graphic_file)
                                    baseFilename, ext = os.path.splitext( filename )
                                    baseFilename = self.sanitiseFileName(baseFilename)
                                    if season_missing and suffix_missing:
                                        newFilename = u"%s/%s Season %d%s%s" % (filepath, baseFilename, available_metadata['season'], self.graphic_suffix[graphic_type], ext)
                                    elif suffix_missing:
                                        newFilename = u"%s/%s%s%s" % (filepath, baseFilename, self.graphic_suffix[graphic_type], ext)
                                    elif season_missing:
                                        baseFilename = baseFilename.replace(self.graphic_suffix[graphic_type], u'')
                                        newFilename = u"%s/%s Season %d%s%s" % (filepath, baseFilename.replace(u' Season X', u''), available_metadata['season'], self.graphic_suffix[graphic_type], ext)
                                    if self.config['simulation']:
                                        sys.stdout.write(
                                            u"Simulation renaming (%s) to (%s)\n" % (graphic_file, newFilename)
                                        )
                                    else:
                                        os.rename(graphic_file, newFilename)
                                    available_metadata[graphic_type]= self.rtnRelativePath(newFilename,  graphicsDirectories[graphic_type])
                                else:
                                    available_metadata[graphic_type]= self.rtnRelativePath(graphic_file,  graphicsDirectories[graphic_type])
                            else: # Must see if a graphic is on thetvdb wiki
                                if graphic_type == 'coverfile' or graphic_type == 'banner':
                                    available_metadata[graphic_type] = self.rtnRelativePath(self._getTvdbGraphics(cfile, graphic_type),  graphicsDirectories[graphic_type])
                                    if available_metadata[graphic_type] == None:
                                        tmp = self._getTvdbGraphics(cfile, graphic_type, toprated=True)
                                        if tmp!= None:
                                            tmp_fullfilename = self.rtnAbsolutePath(tmp, graphicsDirectories[graphic_type])
                                            filepath, filename = os.path.split(tmp_fullfilename)
                                            baseFilename, ext = os.path.splitext( filename )
                                            baseFilename = self.sanitiseFileName(baseFilename)
                                            baseFilename = baseFilename.replace(self.graphic_suffix[graphic_type], u'')
                                            newFilename = u"%s/%s Season %d%s%s" % (filepath, baseFilename.replace(u' Season X', u''), available_metadata['season'], self.graphic_suffix[graphic_type], ext)
                                            if self.config['simulation']:
                                                sys.stdout.write(
                                                    u"Simulation rename (%s) to (%s)\n" % (tmp_fullfilename,newFilename)
                                                )
                                            else:
                                                self._displayMessage(u"Rename existing graphic %s for series (%s)" % (graphic_type, available_metadata['title']))
                                                try:
                                                    os.rename(tmp_fullfilename, newFilename)
                                                    if graphic_type == 'coverfile':
                                                        self._displayMessage("1-Added a poster for(%s)" % cfile['filename'])
                                                        num_posters_downloads+=1
                                                    else:
                                                        self._displayMessage("1-Added a banner for(%s)" % cfile['filename'])
                                                        num_banners_downloads+=1
                                                    available_metadata[graphic_type] = self.rtnRelativePath(newFilename,  graphicsDirectories[graphic_type])
                                                except IOError, e:
                                                    sys.stderr.write(
                                                        u"IOError coping (%s) to (%s)\nError:(%s)\n" % (tmp_fullfilename, newFilename, e))
                                        else: # Try a secondary source
                                            dummy = self._getSecondarySourceGraphics(cfile, graphic_type)
                                            if dummy:
                                                if graphic_type == 'coverfile':
                                                    self._displayMessage(u"1-Secondary source poster for(%s)" % cfile['filename'])
                                                    num_posters_downloads+=1
                                                else:
                                                    self._displayMessage(u"1-Secondary source banner for(%s)" % cfile['filename'])
                                                    num_banners_downloads+=1
                                                available_metadata[graphic_type] = self.rtnRelativePath(dummy,  graphicsDirectories[graphic_type])
                                else: # download fanart
                                    tmp = self.rtnAbsolutePath(self._getTvdbGraphics(cfile, graphic_type, toprated=True), graphicsDirectories['fanart'])
                                    if tmp!= None:
                                        filepath, filename = os.path.split(tmp)
                                        baseFilename, ext = os.path.splitext( filename )
                                        baseFilename = self.sanitiseFileName(baseFilename)
                                        baseFilename = baseFilename.replace(self.graphic_suffix[graphic_type], u'')
                                        newFilename = u"%s/%s Season %d%s%s" % (filepath, baseFilename.replace(u' Season X', u''), available_metadata['season'], self.graphic_suffix[graphic_type], ext)
                                        if self.config['simulation']:
                                            sys.stdout.write(
                                                u"Simulation fanart rename (%s) to (%s)\n" % (tmp, newFilename)
                                            )
                                        else:
                                            try:
                                                os.rename(self.rtnAbsolutePath(tmp, graphicsDirectories[graphic_type]), newFilename)
                                                available_metadata['fanart'] = self.rtnRelativePath(newFilename,  graphicsDirectories['fanart'])
                                                num_fanart_downloads+=1
                                            except IOError, e:
                                                sys.stderr.write(
                                                    u"IOError coping (%s) to (%s)\nError:(%s)\n" % (self.rtnAbsolutePath(tmp, graphicsDirectories[graphic_type]), newFilename, e))
                                    else: # Try a secondary source
                                        dummy = self._getSecondarySourceGraphics(cfile, graphic_type)
                                        if dummy:
                                            available_metadata['fanart'] = self.rtnRelativePath(dummy,  graphicsDirectories['fanart'])
                                            num_fanart_downloads+=1
                    # END of TV Series graphics updating
            ###############################################################################
            # END of metadata Graphics logic - Checking, downloading, renaming
            ###############################################################################

             ###############################################################################
            # START of metadata text logic - Checking, downloading, renaming
            ###############################################################################
            # Clean up meta data code
            if movie:
                if available_metadata['rating'] == 'TV Show':
                    available_metadata['rating'] = 'NR'

            # Check if any meta data needs updating
            metadata_update = True
            for key in available_metadata.keys():
                if key in self.config['metadata_exclude_as_update_trigger']:
                    continue
                else:
                    if key == 'rating' and (available_metadata[key] == 'NR' or available_metadata[key] ==  '' or available_metadata[key] == 'Unknown'):
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if key == 'userrating' and available_metadata[key] == 0.0:
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if key == 'length' and available_metadata[key] == 0:
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if key == 'category' and available_metadata[key] == 0:
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if key == 'year' and (available_metadata[key] == 0 or available_metadata[key] == 1895):
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if movie and key == 'subtitle': # There are no subtitles in movies
                        continue
                    if movie and key == 'plot' and available_metadata[key] != None:
                        if len(available_metadata[key].split(' ')) < 10:
                            self._displayMessage(
                            u"The plot is less than 10 words check if a better plot exists\n")
                            break
                    if key == 'releasedate' and (available_metadata[key] == None or available_metadata[key] == date(1,1,1)):
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if key == 'hash' and (available_metadata['hash'] == u'' or available_metadata['hash'] == None):
                        if (os.path.getsize(u'%s/%s.%s' % (cfile['filepath'], cfile['filename'], cfile['ext'])) < 65536 * 2):
                            continue
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
                    if available_metadata[key] == None or available_metadata[key] == '' or available_metadata[key] == 'None' or available_metadata[key] == 'Unknown':
                        self._displayMessage(
                        u"At least (%s) needs updating\n" % (key))
                        break
            else:
                metadata_update = False
                if not movie and not len(available_metadata['inetref']) >= 5:
                    self._displayMessage(
                    u"At least (%s) needs updating\n" % ('inetref'))
                    metadata_update = True
                # Find the video file's real duration in minutes
                try:
                    length = self._getVideoLength(u'%s/%s.%s' % (cfile['filepath'], cfile['filename'], cfile['ext'], ))
                except:
                    length = False
                if length:
                    if length != available_metadata['length']:
                        self._displayMessage(u"Video file real length (%d) minutes needs updating\n" % (length))
                        metadata_update = True

            # Fetch meta data
            genres_cast={'genres': u'', 'cast': u''}
            if metadata_update:
                copy = dict(available_metadata)
                if movie:
                    tmp_dict = self._getTmdbMetadata(cfile, copy)
                else:
                    tmp_dict = self._getTvdbMetadata(cfile, copy)
                num_episode_metadata_downloads+=1
                # Update meta data
                if tmp_dict:
                    for key in ['genres', 'cast', 'countries']:
                        if tmp_dict.has_key(key):
                            genres_cast[key] = tmp_dict[key]
                    for key in available_metadata.keys():
                        if key in self.config['metadata_exclude_as_update_trigger']:
                            continue
                        else:
                            if not tmp_dict.has_key(key):
                                continue
                            if key == 'userrating' and available_metadata[key] == 0.0:
                                available_metadata[key] = tmp_dict[key]
                                continue
                            if key == 'length':
                                try:
                                    length = self._getVideoLength(u'%s/%s.%s' %(cfile['filepath'], cfile['filename'], cfile['ext'], ))
                                except:
                                    length = False
                                if length:
                                    available_metadata['length'] = length
                                else:
                                    available_metadata[key] = tmp_dict[key]
                                continue
                            available_metadata[key] = tmp_dict[key]

            # Fix fields that must be prepared for insertion into data base
            available_metadata['title'] = self._make_db_ready(available_metadata['title'])
            available_metadata['director'] = self._make_db_ready(available_metadata['director'])
            available_metadata['plot'] = self._make_db_ready(available_metadata['plot'])
            if available_metadata['year'] == 0:
                available_metadata['year'] = 1895
            if available_metadata['coverfile'] == None:
                available_metadata['coverfile'] = u'No Cover'
            if len(genres_cast['genres']) and available_metadata['category'] == 'none':
                try:
                    genres = genres_cast['genres'][:genres_cast['genres'].index(',')]
                except:
                    genres = genres_cast['genres']
                available_metadata['category'] = genres
                self._displayMessage(u"Category added for (%s)(%s)" % (available_metadata['title'], available_metadata['category']))

            # Make sure graphics relative/absolute paths are set PROPERLY based
            # on the 'filename' field being a relative or absolute path. A filename with an absolite path
            # CAN ONLY have graphics baed on absolute paths.
            # A filename with a relative path can have mixed absolute and relative path graphic files
            if available_metadata[u'filename'][0] == u'/':
                available_metadata[u'host'] = u''
                for key in [u'coverfile', u'banner', u'fanart']:
                    if available_metadata[key] != None and available_metadata[key] != u'No Cover' and available_metadata[key] != u'':
                        if available_metadata[key][0] != u'/':
                            tmp = self.rtnAbsolutePath(available_metadata[key], graphicsDirectories[key])
                            if tmp[0] != u'/':
                                if key == u'coverfile':
                                    available_metadata[key] = u'No Cover'
                                else:
                                    available_metadata[key] = u''
            else:
                available_metadata[u'host'] = localhostname.lower()

            ###############################################################################
            # END of metadata text logic - Checking, downloading, renaming
            ###############################################################################

            ###############################################################################
            # START of metadata updating the MythVideo record when graphics or text has changed
            ###############################################################################
            # Check if any new information was found
            if not self.config['overwrite']:
                for key in available_metadata.keys():
                    if available_metadata[key] != meta_dict[key]:
                        if available_metadata[key] == u'' and meta_dict[key] == None:
                            continue
                        if available_metadata[key] == u'' and meta_dict[key] == u'Unknown':
                            continue
                        try:
                            self._displayMessage(
                            u"1-At least (%s)'s value(%s) has changed new(%s)(%s) old(%s)(%s)\n" % (cfile['filename'], key, available_metadata[key], type(available_metadata[key]), meta_dict[key], type(meta_dict[key])))
                        except:
                            self._displayMessage(
                            u"2-At least (%s)'s value(%s) has changed new(%s) old(%s)\n" % (cfile['filename'], key, type(available_metadata[key]), type(meta_dict[key])))
                        break
                else:
                    self._displayMessage(
                        u"Nothing to update for video file(%s)\n" % cfile['filename']
                    )
                    continue

            if self.config['simulation']:
                sys.stdout.write(
                    u"Simulation MythTV DB update for video file(%s)\n" % cfile['filename']
                )
                for key in available_metadata.keys():
                    print key,"        ", available_metadata[key]
                for key in genres_cast.keys():
                    sys.stdout.write(u"Key(%s):(%s)\n" % (key, genres_cast[key]))
                else:
                    sys.stdout.write('\n')
            else:
                # Clean up a few fields before updating Mythdb
                if available_metadata['showlevel'] == 0:    # Allows mythvideo to display this video
                    available_metadata['showlevel'] = 1
                Video(metadata.intid, db=mythvideo).update(available_metadata)
                num_mythdb_updates+=1
                videos_updated_metadata.append(cfile['filename'])
                for key in ['genres', 'cast', 'countries']:
                    if key == 'genres' and len(cfile['categories']):
                        genres_cast[key]+=cfile['categories']
                    if genres_cast.has_key(key):
                        self._addCastGenreCountry( genres_cast[key], Video(metadata.intid, db=mythvideo), key)
                self._displayMessage(
                    u"Updated Mythdb for video file(%s)\n" % cfile['filename']
                )
            ###############################################################################
            # END of metadata updating the MythVideo record when graphics or text has changed
            ###############################################################################

        sys.stdout.write(u"\nMythtv video database maintenance ends at  : %s\n" % (datetime.datetime.now()).strftime("%Y-%m-%d %H:%M"))

        # Print statistics
        sys.stdout.write(u'\n------------------Statistics---------------\nNumber of video files processed .....(% 5d)\nNumber of Fanart graphics downloaded (% 5d)\nNumber of Poster graphics downloaded (% 5d)\nNumber of Banner graphics downloaded (% 5d)\nNumber of 2nd source graphics downld (% 5d)\nNumber of metadata downloads.........(% 5d)\nNumber of 2nd source metadata found .(% 5d)\nNumber of symbolic links created.....(% 5d)\nNumber of Myth database updates......(% 5d)\nNumber of undersized posters ........(% 5d)\nNumber of Movies using IMDB numbers .(% 5d)\n' % (num_processed, num_fanart_downloads, num_posters_downloads, num_banners_downloads, self.num_secondary_source_graphics_downloaded, num_episode_metadata_downloads, self.num_secondary_source_metadata_downloaded, num_symlinks_created, num_mythdb_updates, num_posters_below_min_size, num_movies_using_imdb_numbers))

        if len(videos_updated_metadata):
            sys.stdout.write(u'\n--------------Updated Video Files----------\n' )
            for videofile in videos_updated_metadata:
                sys.stdout.write(u'%s\n' % videofile)
        if len(missing_inetref):
            sys.stdout.write(u'\n----------------No Inetref Found-----------\n' )
            for videofile in missing_inetref:
                sys.stdout.write(u'%s\n' % videofile)
        if len(videos_with_small_posters):
            sys.stdout.write(u'\n---------------Under sized Poster----------\n' )
            for videofile in videos_with_small_posters:
                sys.stdout.write(u'%s\n' % videofile)
        if len(videos_using_imdb_numbers):
            sys.stdout.write(u'\n---------------Movies with IMDB#s----------\n' )
            for videofile in videos_using_imdb_numbers:
                sys.stdout.write(u'%s\n' % videofile)
        return None
    # end processMythTvMetaData

    def __repr__(self):    # Just a place holder
        return self.config
    # end __repr__

# end MythTvMetaData

def simple_example():
    """Simple example of using jamu
    Displays the poster graphics URL(s) and episode meta data for the TV series Sanctuary, season 1
    episode 3
    returns None if there was no data found for the request TV series
    returns False if there is no TV series as specified
    returns a string with poster URLs and episode meta data
    """
    # Get an instance of the variable configuration information set to default values
    configuration = Configuration(interactive = True, debug = False)

    # Set the type of data to be returned
    configuration.changeVariable('get_poster', True)
    configuration.changeVariable('get_ep_meta', True)

    # Validate specific variables and set the TV series information
    configuration.validate_setVariables(['Sanctuary', '1', '3'])

    # Get an instance of the tvdb process function and fetch the data
    process = Tvdatabase(configuration.config)
    results = process.processTVdatabaseRequests()

    if results != None and results != False:        # Print the returned data string to the stdout
        print process.processTVdatabaseRequests().encode('utf8')
# end simple_example


def main():
    """Support jamu from the command line
    returns True
    """
    parser = OptionParser(usage=u"%prog usage: jamu -hbueviflstdnmoCRFUDSGN [parameters]\n <series name/SID or 'series/SID and season number' or 'series/SID and season number and episode number' or 'series/SID and episode name' or video file/directory paired with destination directory'>")

    parser.add_option(  "-b", "--debug", action="store_true", default=False, dest="debug",
                        help=u"Show debugging info")
    parser.add_option(  "-u", "--usage", action="store_true", default=False, dest="usage",
                        help=u"Display the six main uses for this jamu")
    parser.add_option(  "-e", "--examples", action="store_true", default=False, dest="examples",
                        help=u"Display examples for executing the jamu script")
    parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                        help=u"Display version and author information")
    parser.add_option(  "-i", "--interactive", action="store_true", default=False, dest="interactive",
                        help=u"Interactive mode allows selection of a specific Series from a series list")
    parser.add_option(  "-f", "--flags_options", action="store_true", default=False,dest="flags_options",
                        help=u"Display all variables and settings then exit")
    parser.add_option(  "-l", "--language", metavar="LANGUAGE", default=u'en', dest="language",
                        help=u"Select data that matches the specified language fall back to english if nothing found (e.g. 'es' Español, 'de' Deutsch ... etc)")
    parser.add_option(  "-s", "--simulation", action="store_true", default=False, dest="simulation",
                        help=u"Simulation (dry run), no downloads are performed or data bases altered")
    parser.add_option(  "-t", "--toprated", action="store_true", default=False, dest="toprated",
                        help=u"Only display/download the top rated TV Series graphics")
    parser.add_option(  "-d", "--download", action="store_true", default=False, dest="download",
                        help=u"Download and save the graphics and/or meta data")
    parser.add_option(  "-n", "--nokeys", action="store_true", default=False, dest="nokeys",
                        help=u"Do not add data type keys to data values when displaying data")
    parser.add_option(  "-m", "--maximum", metavar="MAX", default=None, dest="maximum",
                        help=u"Limit the number of graphics per type downloaded.           e.g. --maximum=6")
    parser.add_option(  "-o", "--overwrite", action="store_true", default=False, dest="overwrite",
                        help=u"Overwrite any matching files already downloaded")
    parser.add_option(  "-C", "--user_config", metavar="FILE", default="", dest="user_config",
                        help=u"User specified configuration variables.                     e.g --user_config='~/.jamu/jamu.conf'")
    parser.add_option(  "-F", "--filename", action="store_true", default=False, dest="ret_filename",
                        help=u"Display a formated filename for an episode")
    parser.add_option(  "-U", "--update", action="store_true", default=False, dest="update",
                        help=u"Update a meta data file if local episode meta data is older than what is available on thetvdb.com")
    parser.add_option(  "-D", "--mythtvdir", action="store_true", default=False, dest="mythtvdir",
                        help=u"Store graphic files into the MythTV DB specified dirs")
    parser.add_option(  "-M", "--mythtvmeta", action="store_true", default=False, dest="mythtvmeta",
                        help=u"Add/update TV series episode or movie meta data in MythTV DB")
    parser.add_option(  "-V", "--mythtv_verbose", action="store_true", default=False, dest="mythtv_verbose",
                        help=u"Display verbose messages when performing MythTV metadata maintenance")
    parser.add_option(  "-J", "--mythtvjanitor", action="store_true", default=False, dest="mythtvjanitor",
                        help=u"Remove unused graphics (poster, fanart, banners) with the graphics janitor. Any graphics not associated with atleast one MythTV video file record is delected.")
    parser.add_option(  "-N", "--mythtvNFS", action="store_true", default=False, dest="mythtvNFS",
                        help=u"This option overrides Jamu's restrictions on processing NFS mounted Video and/or graphic files.")
    parser.add_option(  "-I", "--mythtv_inetref", action="store_true", default=False, dest="mythtv_inetref",
                        help=u"Find and interactively update any missing Interent reference numbers e.g. IMDB. This option is ONLY active if the -M option is also selected.")
    parser.add_option(  "-W", "--mythtv_watched", action="store_true", default=False, dest="mythtv_watched",
                        help=u"Download graphics for Scheduled and Recorded videos. This option is ONLY active if the -M option is also selected.")
    parser.add_option(  "-G", "--mythtv_guess", action="store_true", default=False, dest="mythtv_guess",
                        help=u"Guess at the inetref for a video. This option is ONLY active if the -M option is also selected.")
    parser.add_option(  "-S", "--selected_data", metavar="TYPES", default=None, dest="selected_data",
                        help=u"Select one of more data types to display or download, P-poster, B-Banner, F-Fanart, E-Episode data, I-Episode Image. e.g. --selected_data=PBFEI gets all types of data")
    parser.add_option(  "-R", "--mythtv_ref_num", action="store_true", default=False, dest="mythtv_ref_num",
                        help=u"Start an interactive session that ONLY adds the TVDB/TMDB reference numbers to when missing. No meta data or images will be concurrently downloaded.")

    opts, series_season_ep = parser.parse_args()

    if opts.debug:
        print "opts", opts
        print "\nargs", series_season_ep

    # Set the default configuration values
    if opts.mythtv_inetref or opts.mythtv_ref_num:
        opts.interactive = True
    configuration = Configuration(interactive = opts.interactive, debug = opts.debug)

    if opts.usage:                    # Display usage information
        sys.stdout.write(usage_txt+'\n')
        sys.exit(0)

    if opts.examples:                    # Display example information
        sys.stdout.write(examples_txt+'\n')
        sys.exit(0)

    if opts.version == True:        # Display program information
        sys.stdout.write(u"\nTitle: (%s); Version: (%s); Author: (%s)\n%s\n" % (
        __title__, __version__, __author__, __purpose__ ))
        sys.exit(0)

    # Verify that only one instance of the following options is running at any one time
    # Options (-M, -MW and -MG)
    options = u''
    if opts.mythtvmeta:
        options+=u'M'
    else:
        MythLog._setlevel('none') # There cannot be any logging messages with non -M options
    if opts.mythtvmeta and opts.mythtv_watched:
        options+=u'W'
    if opts.mythtvmeta and opts.mythtv_guess:
        options+=u'G'
    if opts.mythtvmeta and opts.mythtvjanitor:  # No instance check with the janitor option
        options+=u'J'
    if opts.mythtvmeta and opts.mythtv_inetref: # No instance check with the interactive mode option
        options+=u'I'
    if options in [u'M', u'MW', u'MG']:
        jamu_instance = singleinstance(u'/tmp/Jamu_%s_instance.pid' % options)
        #
        # check is another instance of Jamu is running
        #
        if jamu_instance.alreadyrunning():
            print u'\n! Error: An instance of Jamu (-%s) is already running only one instance can run at a time.\nOne of the meta data sources may be off-line or very slow.\n' % options
            sys.exit(0)

    # Message the user that they are using incompatible options with the -MW option
    if opts.mythtvmeta and opts.mythtv_watched and (opts.mythtv_inetref or opts.interactive):
        print u'\n! Error: There us no Interactive mode (-I or -i) for the Jamu (-MW) option.\nPlease change your options and try again.\n'
        sys.exit(1)

    # Message the user that they are using incompatible options -R and -I or -i
    if opts.mythtvmeta and opts.mythtv_ref_num and opts.mythtv_inetref:
        print u'\n! Error: The (-R) and (-I) options are mutually exclusive.\nPlease change your options and try again.\n'
        sys.exit(1)

    # Apply any command line switches
    configuration.changeVariable('local_language', opts.language)
    configuration.changeVariable('simulation', opts.simulation)
    configuration.changeVariable('toprated', opts.toprated)
    configuration.changeVariable('download', opts.download)
    configuration.changeVariable('nokeys', opts.nokeys)
    configuration.changeVariable('maximum', opts.maximum)
    configuration.changeVariable('overwrite', opts.overwrite)
    configuration.changeVariable('ret_filename', opts.ret_filename)
    configuration.changeVariable('update', opts.update)
    configuration.changeVariable('mythtvdir', opts.mythtvdir)
    configuration.changeVariable('mythtvmeta', opts.mythtvmeta)
    configuration.changeVariable('mythtv_inetref', opts.mythtv_inetref)
    configuration.changeVariable('mythtv_ref_num', opts.mythtv_ref_num)
    configuration.changeVariable('mythtv_watched', opts.mythtv_watched)
    configuration.changeVariable('mythtv_guess', opts.mythtv_guess)
    configuration.changeVariable('mythtv_verbose', opts.mythtv_verbose)
    configuration.changeVariable('mythtvjanitor', opts.mythtvjanitor)
    configuration.changeVariable('mythtvNFS', opts.mythtvNFS)
    configuration.changeVariable('data_flags', opts.selected_data)

    # Check if the user wants to change options via a configuration file
    if opts.user_config != '':    # Did the user want to override the default config file name/location
        configuration.setUseroptions(opts.user_config)
    else:
        default_config = u"%s/%s" % (os.path.expanduser(u"~"), u".mythtv/jamu.conf")
        if os.path.isfile(default_config):
            configuration.setUseroptions(default_config)
        else:
            print u"\nThere was no default Jamu configuration file found (%s)\n" % default_config

    if opts.flags_options:                # Display option variables
        if len(series_season_ep):
            configuration.validate_setVariables(series_season_ep)
        else:
            configuration.validate_setVariables(['FAKE SERIES NAME','FAKE EPISODE NAME'])
        configuration.displayOptions()
        sys.exit(0)

    # Validate specific variables
    configuration.validate_setVariables(series_season_ep)

    if configuration.config['mythtvmeta']:
        process = MythTvMetaData(configuration.config)
        process.processMythTvMetaData()
    elif configuration.config['video_dir']:
        process = VideoFiles(configuration.config)
        results = process.processFileOrDirectory()
        if results != None and results != False:
            print process.processFileOrDirectory().encode('utf8')
    else:
        process = Tvdatabase(configuration.config)
        results = process.processTVdatabaseRequests()
        if results != None and results != False:
            print process.processTVdatabaseRequests().encode('utf8')
    return True
# end main

if __name__ == "__main__":
    main()
