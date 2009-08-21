#!/usr/bin/python
# -*- coding: utf8 -*-
"""
This Python script is intended to find metadata for a video file or video
directory with multiple video files. Perform movie data lookups using the
imdbpy.py script.

Written by Pekka Jääskeläinen (gmail: pekka.jaaskelainen) 2006-2007.

The metadata is searched with following steps:

- search for a file which contains the IMDb ID:
	- look for imdb.nfo
	- look for imdb.url
	- look for [filenamebody].nfo
	- look for [filenamebody].imdb
	- if not found, look for any file in the directory with suffix .nfo

- look for the title derived from the file name / directory. Should detect the
following cases:
	- if it's a DVD directory (a directory containing a sub directory VIDEO_TS):
		- use the directory name as the search string
	- otherwise remove suffix and possible known tags, etc. and use the
	resulting string as a search string (a directory with multiple videos)

Stores the fetched metadata (output from imdbpy.py -D imdbid) to
[filenamebody].metadata.

In case it's a video directory (a directory with 2 or more video files for the
same title, or a DVD directory) the file name is video.metadata.

A crontab entry for the script to insert automatically data of new video files
to MythDB every hour:

0 * * * * find_meta.py -r /videos

In which /videos is the root of your MythVideo files.
"""

import sys
import optparse
import re
import os
import glob
import fileinput
import imdbpy
import shlex
import socket
import urllib
import fetch_poster
import distutils.file_util

try:
	# If found, we can insert data directly to MythDB
	from MythTV import MythDB, MythVideo
	mythdb = MythDB()
	mythvideo = MythVideo()
except:
	print "MythTV module cannot be initialized, MythDB importing disabled."
	mythdb = None
	mythvideo = None

from stat import *

verbose=False

try:
	# For better line editing in interactive mode. Optional.
	import readline
except:
	pass

interactive = False
recursive = False
dbimport = False
poster_search = True

# The language of the title name to add to the movie's original title name.
aka_language = None

# Import metadata from .metadata files if found.
import_from_files = False

# Overwrite the old metadata if found.
overwrite = False

# Create the *.metadata files.
metafiles = False

videoExtensions = ["avi", "mpg", "wmv", "mkv"]

# The file name for storing metadata for videos that occupy the whole
# directory and might consist of multiple video files for the same
# title (basically, DVD backups in its several forms)
dirMetadataFileName = "video.metadata"

# Directories to skip in the recursive scan (matched to the rightmost
# part of the directory).
skipDirs = ["/Sample", "/Sub", "/VIDEO_TS"]

# The directory to store the poster files in (this will be fetched from the
# MythTV settings table).
poster_dir = "./"

#Number of default IMDb retry
import time
defaultretries=3

def functionretry(func, arg1, arg2=None, retries=None):
	global defaultretries
	
	if retries == None:
		retries = defaultretries
	
	attempts = 0
	stop = False
	while (not stop):
		try:
			if arg2:
				result = func(arg1, arg2)
			else:
				result = func(arg1)				
			stop = True
		except:
			result = None
		if not stop:
			attempts += 1
			if attempts > retries:
				stop = True
			if attempts <= retries:
				print_verbose ('Failed to retrieve data, retry in 5s')
				time.sleep(5)
			
	if attempts > retries:
		print_verbose ('Error retrieving data : No more attempts')
	return result

def print_verbose(string):
	global verbose
	if verbose:
		print string
	return

db = None

def find_imdb_id_from_text_file(textFile):

	if os.access(textFile, os.R_OK):
		print_verbose("Scanning %s for IMDb ID" % textFile)

		regexps = []
		# Look for the following strings:

		# http://www.imdb.com/title/tt0497116/
		regexps.append(re.compile(".*imdb.com/title/tt(?P<imdb_id>\d+)/?.*"))

		for line in fileinput.input(textFile):
			#print_verbose("Scanning line: %s" % line)
			for regexp in regexps:
				m = regexp.match(line)
				if m is not None:
					imdb_id = m.group("imdb_id")
					print_verbose("Found IMDb ID '%s'" % (imdb_id))
					fileinput.close()
					return imdb_id
		fileinput.close()
	else:
		return None

def strip_extension(path):
	suffix_pos = path.rfind(".")
	if suffix_pos != -1:
		return path[0: suffix_pos]
	else:
		return path

def cleanup_title(title):
	title = title.replace("_", " ").replace(".", " ").replace("-", " ")
	cut_point_strings = [
		"hdtv", "xvid", "dvd", "proper", "720p", "limited",
		"dsr", "pdtv", "cd", "disk", "disc"]
	lowest_cutpoint = len(title)
	for string in cut_point_strings:
		pos = title.lower().rfind(string)
		if pos > 0 and pos < lowest_cutpoint:
			lowest_cutpoint = pos

	title = title[0:lowest_cutpoint]
	# Remove trailing year in the form of "name [year]" or "name (year)" if any
	m = re.compile(r"(.*)([\(\[]([0-9]+)[\]\)])$")
	found = m.match(title.strip())
	if found:
		title = found.group(1)
	return title.strip()

def parse_meta(variable, oldvalue, emptyvalue="", meta=""):
	"""
	Parses a single metadata from a metadata string (returned by the imdbpy.py, etc.).

	variable is the metadata to find
	oldvalue if we are replacing an old value for the metadata
	emptyvalue what is considered an empty value for the metadata (e.g. "None", "")
	meta the metadata string
	"""
	global overwrite
	if not overwrite and oldvalue is not None and oldvalue != emptyvalue:
		return oldvalue

	return imdbpy.parse_meta(meta, variable)

def detect_disc_number(allfiles, file):
	"""
	Detect the number of disc of the video file considering all the discs
	that are part of the title.

	Returns None if cannot detect the disc number.
	"""

	if len(allfiles) < 2 or file not in allfiles:
		return None

	# Ensure all filenames are of same length (assume there are no
	# more than 9 discs per title). And they are not all the same.

	size = len(allfiles[0])
	differing = False
	for f in allfiles:
		if len(f) != size:
			return None
		if f != file:
			differing = True

	if not differing:
		return None

	# Find the range of chars of the string which are not equal
	# for all the files.
	# For example:
	# "Rambo III disc 1.avi"
	# "Rambo III disc 2.avi"
	#  ===============!====

	startpos = 0
	for startpos in range(len(file)):
		c = allfiles[0][startpos]
		allequal = True
		for f in allfiles:
			if f[startpos] != c:
				allequal = False
				break
		if not allequal:
			# Found the starting index.
			break

	endpos = len(file) - 1
	for endpos in reversed(range(len(file))):
		c = allfiles[0][endpos]
		allequal = True
		for f in allfiles:
			if f[endpos] != c:
				allequal = False
				break
		if not allequal:
			# Found the ending index.
			break
	endpos = endpos + 1

	# Now the disc number can be found.
	disc_str = file[startpos:endpos].lower()

	disc = -1
	try:
		disc = int(disc_str)
	except:
		# It was not an integer. Maybe an alphabetic?
		alpha = ["a", "b", "c", "d", "e"]
		for i in range(len(alpha)):
			if alpha[i] == disc_str:
				return i + 1 # a == disc 1, b == disc 2, ...

	if disc == -1:
		return None
	else:
		return disc

def save_metadata_to_mythdb(videopath, metadata):
	"""
	Updates the given metadata for the given video path.

	Detects if the given title is a dvd-rip dir with multiple videos and
	adds metadata for all the videos separately and chains the videos
	together.
	"""
	files_str = parse_meta("Files", "", "", metadata)
	if files_str is not None:
		files = files_str.split(",")

		if len(files) > 1:
			# It's a dvd-rip dir.

			# Set the first video's childid to -1, which denotes no childs.
			child = -1
			# Add the last file in chain first so we can set it as a child
			# to the next one, etc.
			for file in reversed(files):
				child = save_video_metadata_to_mythdb(
					videopath + "/" + file, metadata,
					child, disc = detect_disc_number(files, file))
			return

	return save_video_metadata_to_mythdb(videopath, metadata)

def save_video_metadata_to_mythdb(videopath, metadata, child=-1, disc=None):
	"""
	Updates the given metadata for the given video file.

	child can be set to the id of the child video (video to be played after this one).
	disc can be set to the disc number in case of multifile title (the disc number
		is appended to the title string to enable differentiating the titles in
		file browse mode.

	Returns the id of the inserted metadata.
	"""
	global overwrite, db, poster_dir, poster_search, aka_language

	# Drop the trailing '/' from the path
	if videopath.endswith('/'):
		videopath = videopath[0:-1]
	videopath = os.path.abspath(videopath)

	print_verbose("Inserting metadata to MythDB for %s." % videopath)

	(intid, title, category, director, plot, rating, inetref, year,
	userrating, length, filename, coverfile, childid, playcommand) = \
		(None, None, 0, None, None, None, None,
		0.0, None, 0, None, None, child, "")

	intid = mythvideo.getMetadataId(videopath)
	has_metadata = mythvideo.hasMetadata(videopath)
	if intid is not None :
		if not overwrite and has_metadata:
			print_verbose("Metadata already exist in MythDB, not overwriting it.")
			return None
		else:
			print_verbose("Entry exists in MythDB with default metadata.  Updating.")
			mythvideo.setMetadata({'filename': videopath}, id=intid)
	else:
		print_verbose("No metadata in MythDB, creating a new one.")
		# Create a new empty entry at this point so we can use the common UPDATE code
		# to actually insert the data.
		intid = mythvideo.setMetadata({'filename': videopath})

	def parse_metadata(variable, oldvalue, emptyvalue="", meta=metadata):
		return parse_meta(variable, oldvalue, emptyvalue, meta)

	if title is None:
		title = parse_metadata('Title', title)

	if title is None:
		title = cleanup_title(os.path.basename(videopath))

	inetref = parse_metadata('IMDb', inetref, '00000000')

	if inetref == None:
		inetref = '00000000'

	if poster_search:
		print_verbose("Fetching a poster image...")
		coverfile = find_poster_image(title, inetref)
		if coverfile is not None:
			print_verbose("Found a poster.")
		else:
			print_verbose("Poster not found.")

	akas = parse_metadata('AKA', None)
	if aka_language is not None and akas is not None:
		for aka in akas.split(', '):
		# Grill Point::(International: English title)
		# Catastrofi d'amore::(Italy)::[it]
			akaRegexp = ".+::\[%s\].*" % aka_language
			m = re.match(akaRegexp, aka)
			if m is not None:
				aka_title = aka.split("::")[0]
				if aka_title != title:
					title = aka_title + " (" + title + ")"
					print_verbose("Found AKA: %s" % title)
				break
			# For IMDbPY >= 3.9
			# Grill Point::(International: English title)
			# Catastrofi d'amore::(Italy) [it]
			akaRegexp = ".+::\(.+\) \[%s\].*" % aka_language
			m = re.match(akaRegexp, aka)
			if m is not None:
				aka_title = aka.split("::")[0]
				if aka_title != title:
					title = aka_title + " (" + title + ")"
					print_verbose("Found AKA: %s" % title)
				break

	if disc is not None:
		title += " [disc" + unicode(disc) + "]"

	year = parse_metadata('Year', year, 0)

	if year is None:
		year = 0

	director = parse_metadata('Director', director, 'Unknown')

	if director == None:
		director = "Unknown"

	plot = parse_metadata('Plot', plot, "None")
	if plot == None:
		plot = ""

	userrating = parse_metadata('UserRating', userrating, 0.0)

	try:
		float(userrating)
	except:
		userrating = 0.0

	rating = parse_metadata('MovieRating', rating, "Unknown")

	if rating is None:
		rating = "Unknown"

	length = parse_metadata('Runtime', length, 0)
	try:
		length = length.split(",")[0]
		length = int(length)
	except:
		print_verbose("Chose runtime: %s" % length)
		try:
			length = length.split(":")[1]
			length = int(length)
		except:
			try:
				length = length.split("(")[0]
				length = int(length)
			except:
				length = 0

	filename = videopath

	## Process genres
	genrestring = parse_metadata('Genres', "", "")
	genres = []
	if genrestring is not None and len(genrestring) > 0:
		genres = genrestring.split(",")

	#Always set category to "Unknown", until we can identify what it really is
	category = mythvideo.getGenreId("Unknown")
	if len(genres) < 1:
		print_verbose("No genres.")
	else:
		#Remove previous genres
		mythvideo.cleanGenres(intid)
		#Set all the genres
		for genre in genres:
			mythvideo.setGenres(genre.strip(), intid)

	## Process countries
	countrystring = parse_metadata('Countries', "", "")
	countries = []
	if countrystring is not None and len(countrystring) > 0:
		countries = countrystring.split(",")

	if len(countries) < 1:
		print_verbose("No countries.")
	else:
		#Remove previous countries
		mythvideo.cleanCountry(intid)
		#Set all the countries
		for country in countries:
			mythvideo.setCountry(country.strip(), intid)

	## Process cast
	caststring = parse_metadata('Cast', "", "")
	cast = []
	if caststring is not None and len(caststring) > 0:
		cast = caststring.split(",")

	if len(cast) < 1:
		print_verbose("No cast.")
	else:
		#Remove previous cast
		mythvideo.cleanCast(intid)
		#Set all the cast
		for actor in cast:
			mythvideo.setCast(actor.strip(), intid)

	if coverfile == None:
		coverfile = "No cover"

	mythvideo.setMetadata({'showlevel': 1, 'browse': 1, 'childid': childid,
					'playcommand': playcommand, 'title': title.encode('utf8'),
					'director': director.encode('utf8'), 'plot': plot.encode('utf8'),
					'rating': rating, 'inetref': inetref, 'category': category,
					'year': year, 'userrating': userrating, 'length': length,
					'filename': filename, 'coverfile': coverfile}, intid)
	return intid

def find_poster_image(title, imdb_id):
	"""
	Tries to find a poster image for the given IMDb id.

	First looks if the image already exist, if not, tries to fetch it using
	the fetch_poster.py. Returns None in case a poster image couldn't be found,
	otherwise returns the base name of the poster image file.
	"""
	global poster_dir,overwrite
	global defaultretries
	image_extensions = ["png", "jpg", "bmp"]

	poster_files = []
	for ext in image_extensions:
		poster_files += glob.glob("%s/%s.%s" % (poster_dir, imdb_id, ext))

	if len(poster_files) == 0 or overwrite:
		# Try to fetch the poster image from the web.
		posters = fetch_poster.find_best_posters(\
			title, count=1, accept_horizontal=True, imdb_id=imdb_id,retries=defaultretries)

		if len(posters) == 0:
			return None

		poster = posters[0]

		filename = os.path.basename(poster.file_name)
		(name, extension) = os.path.splitext(filename)
		local_filename = poster_dir + "/" + imdb_id + extension
		if os.path.exists(local_filename):
			if overwrite:
				os.remove(local_filename)
			else:
				return local_filename
		distutils.file_util.move_file(poster.file_name, local_filename)
		# Set enough read bits so Apache can read the cover for the MythWeb interface.
		os.chmod(local_filename, S_IREAD | S_IRUSR | S_IRGRP | S_IROTH)
		return local_filename
	else:
		print_verbose("Found existing cover image.")
		return poster_files[0]
	return None

def save_metadata_to_file(fileName, metadata):
	global overwrite

	if os.path.exists(fileName) and not overwrite:
		print_verbose("Metadata already exists, not overwriting.")
		return

	if metadata is not None:
		print_verbose("Writing metadata to '%s'" % fileName)
		f = open(fileName, 'w')
		f.write(metadata.encode("utf8"))
		f.close()

def save_metadata(videopath, metadata_filename, metadata):
	"""
	Saves metadata for the given video path to the given metadata_filename.

	Metadata should be a single string.
	"""
	global dbimport, metafiles

	print_verbose("Metadata:")
	print_verbose(metadata)

	if metafiles:
		save_metadata_to_file(metadata_filename, metadata)
	if dbimport:
		save_metadata_to_mythdb(videopath, metadata)

def find_metadata_for_video_path(pathName):
	global interactive

	fileName = os.path.basename(pathName)
	dirName = os.path.dirname(pathName)
	print_verbose("Scanning file %s..." % fileName)

	if os.path.isdir(pathName):
		title = os.path.basename(dirName)
		nfos = [dirName + "/imdb.nfo",
			dirName + "/imdb.url"]
	else:
		file_body = strip_extension(fileName)
		title = file_body

		nfos = [dirName + "/imdb.nfo",
			dirName + "/imdb.url",
			dirName + "/" + file_body + ".nfo",
			dirName + "/" + file_body + ".imdb"]

	# Add rest of the .nfos to the end of the list.
	for nfo in glob.glob(dirName + "/*.nfo"):
		if nfo not in nfos:
			nfos.append(nfo)

	imdb_id = None
	source_nfo = None
	for nfo in nfos:
		imdb_id = find_imdb_id_from_text_file(nfo)
		if imdb_id is not None:
			source_nfo = nfo
			break

	title = unicode(cleanup_title(title), "utf8", "ignore")
	if imdb_id is None:
		# A title search

		print_verbose("Title search '%s'" % title)

		candidates = functionretry(imdbpy.title_search, title)
			
		if candidates is None or len(candidates) == 0:
			# TODO: Try with the dirname
			pass
		if candidates is not None and len(candidates) > 0:
			index = 0
			if (len(candidates) == 2) and (candidates[0][0] == candidates[1][0]):
				imdb_id = candidates[0][0]
			elif len(candidates) > 1:
				print "Got multiple candidates for title search '%s'. " % title
				if not interactive:
					print "Use the '-a' or '-i' switch to choose the correct one."
				for candidate in candidates:
					print "%s) %s (%d)" % (candidate[0], candidate[1].encode("utf8"), candidate[2])

				if interactive:
					answer = raw_input("?)")
					if answer is None or len(answer) == 0:
						return None

					print_verbose("Chose %s" % answer)
					imdb_id = answer
				else:
					return None
			else:
				imdb_id = candidates[0][0]
		else:
			print "Couldn't find IMDb ID for '%s'" % pathName
			return None

	print_verbose("Querying IMDb for meta data for ID %s..." % imdb_id)
	try:
		meta = functionretry(imdbpy.fetch_metadata,imdb_id)
		if meta is not None:
			if meta.series_episode:
				title, season, episode = imdbpy.detect_series_title(title)
				if meta.season is None:
					meta.season = season
				if meta.episode is None:
					meta.episode = episode
			metadata = meta.toMetadataString()
			metadata += "IMDb:%s" % imdb_id + "\n"
		return metadata
	except:
		print_verbose("Problem occured fetching metadata for ID %s..." % imdb_id)
		return None

def video_file_list_metadata(videoPaths):
	videoPaths = [os.path.basename(v) for v in videoPaths]
	videoPaths.sort()

	return "Files:%s" % (",".join(videoPaths)) + "\n"

def load_metadata_file(metadata_filename):
	"""
	Loads a metadata file if found, returns None otherwise.
	"""
	metadata = None
	try:
		f = open(metadata_filename)
		metadata = unicode("", "utf8").join(f.readlines())
		f.close()
	except:
		pass
	return metadata

def detect_dvd_backup(dirName):
	"""
	If the given directory is detected as a directory with a dvd backup, meta data is
	searched for the directory title name (in addition to the directory-wide imdb files).
	"""
	global import_from_files
	videoTs = dirName + "/VIDEO_TS"
	if not (os.path.exists(videoTs) and os.path.isdir(videoTs)):
		return False

	print_verbose('A DVD backup directory with DVD directory structure detected.')

	metadata_target = dirName + "/" + dirMetadataFileName

	metadata = None
	if import_from_files:
		metadata = load_metadata_file(metadata_target)

	if metadata is None:
		if should_be_skipped(dirName, metadata_target):
			return True
		metadata = find_metadata_for_video_path(dirName + "/VIDEO_TS")

	if metadata is not None:
		save_metadata(dirName, metadata_target, metadata)
	return True

def detect_compressed_dvd_backup_dir(dirName):
	"""
	If the given directory is detected as a directory with one or more files of
	a single title,	fetches the meta data and returns true.
	"""
	global videoExtensions, import_from_files
	maxFilesPerTitle = 3

	foundVideos = None
	# In case the directory holds 1 to maxFilesPerTitle videos of the same extension,
	# it's a candidate for a compressed dvd backup directory. Otherwise it's probably
	# a collection of videos in a directory (e.g., a backup of a tv-series).
	for ext in videoExtensions:
		videos = glob.glob(dirName + "/*." + ext)

		if 1 < len(videos) <= maxFilesPerTitle:
			if foundVideos is not None:
				# Already found a set of videos, this cannot be a dvdrip dir
				return False

			# Detect that the file names don't look like series episodes
			filename_length = len(videos[0])
			for video in videos:
				title, season, episode = \
					imdbpy.detect_series_title(cleanup_title(video))
				if title is not None and season is not None and episode is not None:
					print_verbose("'%s' looks like a TV-series episode." % video)
					print_verbose("title: %s season: %s episode: %s" % \
					(title, season, episode))
					return False

			# Detect the disc numbers from file names.
			# If cannot detect, assume it's not a dvd rip dir.
			for video in videos:
				disc = detect_disc_number(videos, video)
				if disc is None:
					print_verbose("Did not detect disc number for %s." % video)
					return False
				else:
					print_verbose("Found disc %d." % disc)

			foundVideos = videos
		elif len(videos) > maxFilesPerTitle:
			return False			# Too many videos for a dvdrip dir.

	metadata_target = dirName + "/" + dirMetadataFileName
	if foundVideos is not None:
		print_verbose('DVD rip directory detected.')

		# Check if the video search should be skipped (because of existing data).
		first_file = foundVideos[0]
		if should_be_skipped(first_file, metadata_target):
			print_verbose("Skipping '%s'." % dirName)
			return True # DVD rip was detected, but no data should be inserted

		# Scan for the IMDb ID as usual, but store the
		# metadata to video.metadata file instead, i.e., do not create .metadata for
		# all videos in the directory.
		metadata = None
		if import_from_files:
			metadata = load_metadata_file(metadata_target)

		if metadata is None:
			metadata = find_metadata_for_video_path(foundVideos[0])

		if metadata is not None:
			# Add the Files: metadata which lists the videos in correct playing order
			metadata += video_file_list_metadata(foundVideos)
			save_metadata(dirName, metadata_target, metadata)
		return True

	return False

def scan_file(pathName, imdb_id = None):
	global import_from_files

	metadata_target = strip_extension(pathName) + ".metadata";

	if should_be_skipped(pathName, metadata_target):
		print_verbose("Skipping '%s'." % pathName)
		return

	metadata = None
	if import_from_files:
		metadata = load_metadata_file(metadata_target)

	if imdb_id is not None:
		meta = functionretry(imdbpy.fetch_metadata,imdb_id)
		if meta.series_episode:
			fileName = os.path.basename(pathName)
			t, season, episode = imdbpy.detect_series_title(fileName)
			if meta.season is None:
				meta.season = season
			if meta.episode is None:
				meta.episode = episode
		metadata = meta.toMetadataString()
		metadata += "IMDb:%s" % imdb_id + "\n"

	if metadata is None:
		metadata = find_metadata_for_video_path(pathName)

	if metadata is not None:
		save_metadata(pathName, metadata_target, metadata)

def scan_directory(dirName, imdb_id = None):
	global videoExtensions
	dirName = dirName.replace("[", "?").replace("]", "?")
	print_verbose("Scanning directory %s..." % dirName)

	if detect_compressed_dvd_backup_dir(dirName):
		return

	if imdb_id is not None:
		metadata = functionretry(imdbpy.metadata_search,imdb_id)

		if metadata is not None:
			metadata += "IMDb:%s" % imdb_id + "\n"
			save_metadata(dirName, dirName + "/video.metadata", metadata)
			return


	if detect_dvd_backup(dirName):
		return

	# It's just a directory with a bunch of videos. Scan each video individually.
	foundVideos = []
	for ext in videoExtensions:
		videos = glob.glob(dirName + "/*." + ext)
		if videos is not None and len(videos) > 0:
			foundVideos += videos

	if len(foundVideos) == 0:
		return

	print_verbose("Found %d videos." % len(foundVideos))

	for video in foundVideos:
		scan_file(video)

def should_be_skipped(path, meta_file = None):
	"""
	Returns true in case the given path should be skipped in the scan.
	"""
	global skipDirs, overwrite, dbimport, metafiles

	if path.endswith("/"):
		path = path[0:-1]

	# Check the ignored filename patterns.
	for skip in skipDirs:
		if path.lower().endswith(skip.lower()):
			return True

	if os.path.isfile(os.path.join(path, ".mythvideo_skip_directory")):
		print_verbose("Skipping due to .mythvideo_skip_directory")
		return True

	# Check if we are not in overwrite mode and there is existing data
	# for the wanted targets (metadata files and/or MythDB).
	if not overwrite:
		need_mythdb_data = dbimport and not mythvideo.hasMetadata(path)
		need_metadata_file = metafiles
		if metafiles and meta_file is not None:
			need_metadata_file = not os.path.exists(meta_file)
		if not need_mythdb_data and not need_metadata_file:
			return True # No need for data, skip this path.

	return False

def scan(pathName, imdb_id = None):
	global recursive
	metadata = None
	metadata_target = None
	if os.path.isdir(pathName):
		if recursive:
			for root, dirs, files in os.walk(pathName):
				if should_be_skipped(root):
					print_verbose("Skipping '%s'." % root)
					continue
				scan_directory(root)
		else:
			if should_be_skipped(pathName):
				print_verbose("Skipping '%s'." % pathName)
				return
			scan_directory(pathName, imdb_id)
	elif os.path.isfile(pathName):
		if should_be_skipped(pathName):
			print_verbose("Skipping '%s'." % pathName)
			return
		scan_file(pathName, imdb_id)
	else:
		raise IOError("File not found")
	return

def main():
	global verbose, overwrite, interactive, recursive, dbimport
	global import_from_files, metafiles, poster_dir, poster_search
	global aka_language
	global defaultretries

	usage = "usage: %prog [options] videopath1 [videopath2 videopath3...]"

	p = optparse.OptionParser(usage=usage)
	p.add_option('--version', '-v', action="store_true", default=False,
		help="Display 1-line describing name, version, author etc.")
	p.add_option('--overwrite', '-o', action="store_true", default=False,
		help="Overwrite existing metadata.")
	p.add_option('--wordy', '-w', action="store_true", default=False,
		help="Verbose mode, be wordy while scanning for the info.")
	p.add_option('--interactive', '-i', action="store_true", default=False,
		help="Allow the script to ask questions from the user to find the meta data.")
	p.add_option('--recursive', '-r', action="store_true", default=False,
		help="Traverse sub directories of the given directory recursively.")

	p.add_option('--no_dbimport', '-n', action="store_true", default=False,
		help="Do not import metadata directly to MythDB.")
	p.add_option('--fromfiles', '-f', action="store_true", default=False,
		help="Import data to MythDB from .metadata files if found. Requires -d.")
	p.add_option('--metafiles', '-m', action="store_true", default=False,
		help="Write metadata to *.metadata ascii files.")

	p.add_option('--answer', '-a', action="store", type="string", dest="imdb_id",
		help="Fetch metadata with the given IMDb ID for the path (must be a single path).")

	p.add_option('--prune', '-p', action="store_true", default=False,
		help="Prune metadata of deleted files from MythDB.")
	p.add_option('--skip_poster_search', '-s', action="store_true", default=False,
		help="Skip poster search.")
	p.add_option('--lang_code', '-l', action="store", type="string", dest="lang_code",
		default=None,
		help="Add the title name in the given country (two letter code, e.g., 'fi') "\
		"to the movie title.")
	p.add_option('--retry', '-t', action="store", type="int", dest="retries",default=3,
		help="Number of retries, 0 means no retry [default 3]")

	options, arguments = p.parse_args()

	if options.version:
		print "MythVideo Metadata Finder (c) Pekka Jääskeläinen 2006-2007"
		sys.exit(0)

	verbose = options.wordy
	overwrite = options.overwrite
	interactive = options.interactive
	recursive = options.recursive
	dbimport = not options.no_dbimport
	import_from_files = options.fromfiles and dbimport
	metafiles = options.metafiles
	prune = options.prune
	poster_search = not options.skip_poster_search
	aka_language = options.lang_code
	
	defaultretries = options.retries

	if not (metafiles or dbimport):
		print "You must define writing to either MythDB import (-d) or metadata files (-m)."
		sys.exit(1)

	if not prune and len(arguments) < 1:
		print "Please give the paths to be scanned as argument."
		sys.exit(1)
	paths = arguments

	if options.imdb_id is not None:
		if recursive:
			print "Manual IMDb ID must be given in recursive mode."
			sys.exit(1)
		if len(arguments) > 1:
			print "Manual IMDb ID must be given for a single path only (%d given)." % len(arguments)
			sys.exit(1)
		print_verbose("IMDb ID %s given manually." % options.imdb_id)

	if dbimport or prune:
		if not mythdb:
			print "You must have the MythTV module to make direct DB importing to work"
			sys.exit(1)
		poster_dir = mythdb.getSetting("VideoArtworkDir", socket.gethostname())
		if not mythdb:
			print "Could not get VideoArtworkDir setting for the current host."
			sys.exit(1)

	if prune:
		mythvideo.pruneMetadata()

	for path in paths:

		if not os.path.exists(path):
			print "'%s' does not exist." % path
			sys.exit(1)

		scan(path, options.imdb_id)

	sys.exit(0)

if __name__ == '__main__':
	main()
