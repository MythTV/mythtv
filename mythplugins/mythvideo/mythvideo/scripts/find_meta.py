#!/usr/bin/python
# -*- coding: utf8 -*-
"""
This Python script is intended to find metadata for a video file or video
directory with multiple video files. Perform movie data lookups using the
imdbpy.py script.

Written by Pekka J√§√§skel√§inen (gmail: pekka.jaaskelainen) 2006.

The metadata is searched with following steps:
	
- search for a file which contains the IMDb ID:
	- look for imdb.nfo
	- look for imdb.url
	- look for [filenamebody].nfo
	- look for [filenamebody].imdb
	- if not found, look for any file in the directory with suffix .nfo 
	
- look for the title derived from the file name / directory. Should detect the following cases:
	- if it's a DVD directory (a directory containing a sub directory VIDEO_TS):
	 	- use the directory name as the search string	
	- otherwise remove suffix and possible known tags, etc. and use the resulting string
	as a search string (a directory with multiple videos)

Stores the fetched metadata (output from imdbpy.py -D imdbid) to [filenamebody].metadata.
In case it's a video directory (a directory with 2 or more video files for the same title,
or a DVD directory) the file name is video.metadata.

Includes an interactive mode which makes the script ask user for input in case there are
titles for which IMDb queries cannot be done unambiguously.

"""  

import sys
import optparse
import re
import os
import glob
import fileinput
import imdbpy
try:
	# If loaded, get neater input prompt for interactive questions.
	import readline
except:
	pass

from stat import *

verbose=False
overwrite=False
interactive=False
recursive=False

videoExtensions = ["avi", "mpg", "wmv"]

# The file name for storing metadata for videos that occupy the whole
# directory and might consist of multiple video files for the same
# title (basically, DVD backups in its several forms)
dirMetadataFileName = "video.metadata"

# Directories to skip in the recursive scan (matched to the rightmost
# part of the directory).
skipDirs = ["/Sample", "/Sub"]

def print_verbose(string):
	global verbose
	if verbose:
		print string
	return

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
	title = title.replace("_", " ").replace(".", " ")
	cut_point_strings = ["hdtv", "xvid", "dvd", "proper"]
	lowest_cutpoint = len(title)
	for string in cut_point_strings:
		pos = title.lower().rfind(string)
		if pos > 0 and pos < lowest_cutpoint:
			lowest_cutpoint = pos
			
	title = title[0:lowest_cutpoint]
	return title.strip()

def save_metadata(fileName, metadata):
		
	if os.path.exists(fileName) and not overwrite:
		print_verbose("Metadata already exists, not overwriting.")
	
	if metadata is not None:
		print_verbose("Writing metadata to '%s'" % fileName)
		f = open(fileName, 'w')
		f.write(metadata.encode("utf8"))
		f.close()
		print_verbose("Metadata:")
		print_verbose(metadata)


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
			
				
	
	# Add rest of the .nfos to the list of the scanned ones.
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
			
	if imdb_id is None:
		# A title search 
		title = cleanup_title(title)
		
		print_verbose("Title search '%s'" % title)		
		
		candidates = imdbpy.title_search(title.decode("utf8"))
		if candidates is None or len(candidates) == 0:			
			# Try with the dirname
			pass 
		
		if candidates is not None and len(candidates) > 0:
			
			index = 0
			if len(candidates) > 1:
				if interactive:
					print "Got multiple candidates for the title search '%s'. " % title
					print "Which one is the correct title?"
					for i in range(len(candidates)):
						print "%d) %s (%d)" % (i, candidates[i][1], candidates[i][2])
					print "N) None of the above"
					answer = raw_input("?)")
					if answer is None or len(answer) == 0:
						return [None, None]
					
					if answer.lower() == "n":
						return [None, None]
					else:
						try:
							ans = int(answer)
							if 0 <= ans < len(candidates):
								index = ans
							else:
								return [None, None]
						except:
							print_verbose("Illegal index.")
							return [None, None]
					print_verbose("Chose %d" % index)
				else:
					print "Multiple candidates found for the movie and not in interactive mode."
					return [None, None]
			
			imdb_id = candidates[index][0]
		else:
			print "Couldn't find IMDb ID for '%s'" % pathName
			return [None, None]
			
	print_verbose("Querying IMDb for meta data for ID %s..." % imdb_id)
	metadata = imdbpy.metadata_search(imdb_id)
	if metadata is not None:
		metadata += "IMDb:%s" % imdb_id + "\n"
	return [source_nfo, metadata]
	
	# Save the metadata 

def video_file_list_metadata(videoPaths):	
	videoPaths = [os.path.basename(v) for v in videoPaths]		
	videoPaths.sort()
				
	return "Files:%s" % (",".join(videoPaths)) + "\n"

def detect_dvd_backup(dirName):
	"""
	If the given directory is detected as a directory with a dvd backup, meta data is
	searched for the directory title name (in addition to the directory-wide imdb files).
	"""
	videoTs = dirName + "/VIDEO_TS"
	if not (os.path.exists(videoTs) and os.path.isdir(videoTs)):
		return False
	
	print_verbose('A DVD backup directory (with DVD directory structure) detected.')
	
	metadata_target = dirName + "/" + dirMetadataFileName
	
	if os.path.exists(metadata_target) and not overwrite:
		print_verbose("Metadata already exists, not overwriting.")
		return True		
	
	[src_nfo, metadata] = find_metadata_for_video_path(dirName + "/VIDEO_TS")
	if metadata is not None: 
		save_metadata(metadata_target, metadata)		
	return True
	
	

def detect_compressed_dvd_backup_dir(dirName):
	"""
	If the given directory is detected as a directory with one or more files of 
	a single title,	fetched the meta data and returns true.
	"""
	global videoExtensions
	maxFilesPerTitle = 3
	
	foundVideos = None
	# In case the directory holds 1 to maxFilesPerTitle videos of the same extension,
	# it's a candidate for a compressed dvd backup directory. Otherwise it's probably
	# a collection of videos in a directory (e.g., a backup of a tv-series).
	for ext in videoExtensions:
		videos = glob.glob(dirName + "/*." + ext)
		# TODO: ensure that the filenames are almost exactly the same (CD1, CD2, etc.)
		if 1 < len(videos) <= maxFilesPerTitle:
			if foundVideos is not None:
				# Already found a set of videos, this cannot be a dvdrip dir
				return False
			
			# Ensure that file names are of the same length:
			# moviename.cd1.avi moviename.cd2.avi, etc.
			# in addition, detect that the file names don't look like series episodes
			filename_length = len(videos[0])
			for video in videos:
				if imdbpy.detect_series_query(cleanup_title(video)) != (None, None, None):
					print_verbose("'%s' looks like a TV-series episode." % video)
					return False
				if len(video) != filename_length:
					return False
			foundVideos = videos
		elif len(videos) > maxFilesPerTitle:
			return False			# Too many videos for a dvdrip dir.
	
	metadata_target = dirName + "/" + dirMetadataFileName
	if foundVideos is not None:
		print_verbose('Compressed DVD backup directory (a.k.a. DVD rip) detected.')
				
		if os.path.exists(metadata_target) and not overwrite:
			print_verbose("Metadata already exists, not overwriting.")
			return True		
		
		# Scan for the IMDb ID as usual, but store the
		# metadata to video.metadata file instead, i.e., do not create .metadata for
		# all videos in the directory.
		[src_nfo, metadata] = find_metadata_for_video_path(foundVideos[0])
		if metadata is not None: 
			# Add the Files: metadata which lists the videos in correct playing order
			metadata += video_file_list_metadata(foundVideos)
			save_metadata(metadata_target, metadata)		
		return True
	
	return False

def scan_file(pathName):
	metadata_target = strip_extension(pathName) + ".metadata";
	if os.path.exists(metadata_target) and not overwrite:
		print_verbose("Metadata already exists, not overwriting.")
	else:
		[src_nfo, metadata] = find_metadata_for_video_path(pathName)
		save_metadata(metadata_target, metadata)


def scan_directory(dirName):
	global videoExtensions
	print_verbose("Scanning directory %s..." % dirName)
	if detect_compressed_dvd_backup_dir(dirName):
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
	
def scan(pathName):
	global recursive
	metadata = None
	metadata_target = None
	if os.path.isdir(pathName):
		if recursive:
			for root, dirs, files in os.walk(pathName):
				if should_be_skipped(root): continue
				scan_directory(root)
		else:
			scan_directory(pathName)
        elif os.path.isfile(pathName):
		scan_file(pathName)
        else:
		raise IOError("File not found")
	return

def main():
	global verbose,overwrite,interactive,recursive
	
	p = optparse.OptionParser()
	p.add_option('--version', '-v', action="store_true", default=False,
		help="display 1-line describing name, version, author etc")
	p.add_option('--overwrite', '-o', action="store_true", default=False,
		help="overwrite existing metadata file(s)")		
	p.add_option('--wordy', '-w', action="store_true", default=False,
		help="verbose mode, be wordy while scanning for the info")
	p.add_option('--interactive', '-i', action="store_true", default=False,
		help="allow the script to ask questions from the user to find the meta data")		
	p.add_option('--recursive', '-r', action="store_true", default=False,
		help="traverse sub directories of the given directory recursively")		
		
		
	options, arguments = p.parse_args()
	
	if options.version: 
		print "MythVideo Metadata Finder (c) Pekka J‰‰skel‰inen 2006-2007"
		sys.exit(0)
			
	verbose = options.wordy
	overwrite = options.overwrite
	interactive = options.interactive
	recursive = options.recursive
	
	if len(arguments) < 1:
		print "Please give the file/directory to be scanned as argument."
		sys.exit(1)
	
	pathName = " ".join(arguments)
	
	if not os.path.exists(pathName):
		print "Given file does not exist."
		sys.exit(1)
		
	scan(pathName)		
			
	sys.exit(0)
	
if __name__ == '__main__':
	main()
		
		
