#!/usr/bin/python
# -*- coding: utf8 -*-
"""
This Python script is intended to perform movie data lookups based on
the popular www.imdb.com website using the IMDbPy script.

The script is hosted at http://imdbpy.sourceforge.net/, you need to install
it to make this script work.

This wrapper script is written by
Pekka Jääskeläinen (gmail: pekka.jaaskelainen).
"""

import sys
import optparse
import re
import socket

try:
	import imdb
except ImportError:
	sys.stderr.write("You need to install the IMDbPy library "\
		"from (http://imdbpy.sourceforge.net/?page=download)\n")
	sys.exit(1)

try:
	from MythTV import MythTV
	mythtv = MythTV()
except:
	mythtv = None

def detect_series_title(search_string):
	"""
	Detects a series episode title.

	Currently the following formats are detected:
	"Sopranos Season 1 Episode 2"
	"Sopranos S1E2"
	"Sopranos S1 E2"
	"Sopranos 1x2"
	"Sopranos - 1x2"
	"Sopranos 612"
	"""
	regexps = [re.compile(r"((?P<title>.+?)(-)?)?(\s*)"\
		"(s|(season\s)|\s)(?P<season>\d+)"\
		"\s*(e|(episode\s)|x)(?P<episode>\d+)"),
		re.compile(\
		r"((?P<title>.+)(-)?)?\s+"\
		"(?P<season>\d)(?P<episode>\d\d)(?!\d)\s*")]

	for exp in regexps:
		m = exp.match(search_string.lower())
		if m is None or m.group('title') is None or m.group('season') is None \
			or m.group('episode') is None:
			continue

		# Found a regexp that matches the title string.
		return (m.group('title'), m.group('season'), m.group('episode'))

	return (None, None, None)


def episode_search(title, season, episode):
	"""
	Searches the IMDb for an exact TV-serie episode match.
	
	Returns a list of 3-tuples [imdb id, title, year] for possible matches.	
	Rest of the data need to be fetched separately with fetch_metadata().
	"""
	matches = []
	imdb_access = imdb.IMDb()
	series = imdb_access.search_movie(title.encode("ascii", 'replace'))
	season = int(season)
	episode = int(episode)

	for serie in series:
		if serie['kind'] == 'tv series':
			# Fetch episode info.
			imdb_access.update(serie, 'episodes')
			if serie.has_key('episodes'):
				try:
					ep = serie['episodes'][season][episode]
				except:
					# Probably indexing exception in case the episode/season
					# is not found.
					continue
				# Found an exact episode match, return that match only.
				matches = []
				series_title = ep['series title']
				year = 0
				try:
					year = int(ep['year'])
				except:
					pass
				
				matches.append([imdb_access.get_imdbID(ep),
					"%s Season %d Episode %d" % \
						(series_title, season, episode), year])
				return matches
			else:
				matches.append([imdb_access.get_imdbID(serie),
					serie['title'], int(serie['year'])])
	return matches

def title_search(search_string):
	"""
	Returns a list of 3-tuples [imdb id, title, year] for possible matches.
	"""

	(title, season, episode) = detect_series_title(search_string)

	#print title,"--", season,"--", episode
	#return

	if title is not None and season is not None and episode is not None:
		return episode_search(title, season, episode)

	imdb_access = imdb.IMDb()
	#print "Search:",search_string
	movies = imdb_access.search_movie(search_string.encode("ascii", 'ignore'))

	if movies is None or len(movies) == 0:
		return None

	exact_titled = []
	#print "Found %d movie(s)" % (len(movies))

	def removeArticles(string):
		articles = ["The", "An", "A"]
		for article in articles:
			if string.startswith(article + " "):
				string = string[len(article + " "):]
			if string.endswith(", " + article):
				string = string[:-len(article + " ")]
		return string

	# Find the exact name matches without taking articles in account.
	for imdb_movie in movies:
		#imdb_access.update(imdb_movie)
		title = imdb_movie['title']

		#print "%s vs. %s\n" % (removeArticles(title), removeArticles(search_string))
		if removeArticles(title).lower() == removeArticles(search_string).lower():
			exact_titled.append(imdb_movie)

	sorted_movies = []
	if len(exact_titled) == 1:
		sorted_movies = [exact_titled[0]]
	elif len(exact_titled) == 0:
		# No exact titles, just return, say max. 5 results
		sorted_movies = movies[0:4]
	elif len(exact_titled) > 1:
		# Sort the exact title matches by year.
		def cmp_years(a, b):
			return int(a['year']) > int(b['year'])
		exact_titled.sort(cmp_years)
		sorted_movies = exact_titled[0:4]

	movies = []
	for m in sorted_movies:
		try:
			item = [imdb_access.get_imdbID(m), m['title'], int(m['year'])]
		except KeyError:
			item = [imdb_access.get_imdbID(m), m['title'], 1901]
		movies.append(item)
	return movies

def find_poster_url(imdb_id):

	imdb_access = imdb.IMDb()
	movie = imdb_access.get_movie(imdb_id)
	imdb_access.update(movie)
	url = None
	if 'cover url' in movie.keys():
		url = movie['cover url']

	if url is None and movie['kind'] == 'episode':
		series = movie['episode of']
		imdb_access.update(series)
		if 'cover url' in series.keys():
			url = series['cover url']
	return url

def poster_search(imdb_id):
	url = find_poster_url(imdb_id)
	if url is not None:
		print url

class VideoMetadata:

	series_episode = False
	series_title = ""
	season = None
	episode = None

	title = ""
	runtime = None
	year = None
	directors = None
	plot = None
	cast = None
	rating = None
	mpaa_rating = None
	genres = None
	countries = None
	akas = None

	def __init__(self):
		self.episode_title_format = None
		if mythtv != None:
			self.episode_title_format = mythtv.db.getSetting(
					'VideoEpisodeTitleFormat', socket.gethostname())
		if self.episode_title_format == None:
			self.episode_title_format = '%(series_title)s S%(season)02d E%(episode)02d %(episode_title)s'

	def toMetadataString(self):

		def createMetadataLine(keyName, value):
			if value is not None:
				return keyName + ":" + value + "\n"
			else:
				return ""
		metadata = unicode("", "utf8")
		if self.series_episode == True and self.season is not None and \
			self.episode is not None:
			metadata += 'Title:' + self.episode_title_format % \
				{
					'series_title': self.series_title,
					'season': int(self.season),
					'episode': int(self.episode),
					'episode_title': self.episode_title,
				} + '\n'
		else:
			metadata += createMetadataLine("Title", unicode(self.title))
		metadata += createMetadataLine("Runtime", self.runtime)
		metadata += createMetadataLine('Year', str(self.year))
		if self.directors is not None and len(self.directors) > 0:
			metadata += createMetadataLine("Director", unicode(self.directors[0]))
		metadata += createMetadataLine("Plot", self.plot)
		metadata += createMetadataLine("Cast", unicode(self.cast))
		metadata += createMetadataLine('UserRating', self.rating)
		metadata += createMetadataLine('MovieRating', self.mpaa_rating)
		metadata += createMetadataLine('Genres', self.genres)
		metadata += createMetadataLine('Countries', self.countries)

		if self.akas is not None:
			metadata += createMetadataLine('AKA', ", ".join(self.akas))

		return unicode(metadata)

def fetch_metadata(imdb_id):
	"""
	Fetches metadata for the given IMDb id.
	
	Returns a VideoMetadata object.
	"""
	metadata = VideoMetadata()

	imdb_access = imdb.IMDb()
	movie = imdb_access.get_movie(imdb_id)
	imdb_access.update(movie)

	def metadataFromField(key, default=None, m=movie):

		searchKey = key.lower()
		if searchKey not in m.keys():
			return default
		value = unicode(m[searchKey])
		try:
			value = value.encode("utf8")
		except AttributeError:
			pass
		if value is not None:
			return value
		else:
			return default

	def metadataFromFirst(key, default=None, m=movie):

		searchKey = key.lower()
		if searchKey not in m.keys():
			return default

		value = m[searchKey]
		if value is not None and len(value) > 0:
			if len(value) > 1:
				return ','.join(value).encode("utf8")
			else:
				return value[0].encode("utf8")
		else:
			return default

	if movie['kind'] == 'episode':
		# print "TV Series episode detected"
		metadata.series_episode = True
		if 'series title' in movie.keys():
			metadata.series_title = movie['series title']
		if 'season' in movie.keys():
			metadata.season = movie['season']
		if 'episode' in movie.keys():
			metadata.episode = movie['episode']
		if 'title' in movie.keys():
			metadata.episode_title = unicode(movie['title'])

		if 'episode of' in movie.keys():
			series = movie['episode of']
			imdb_access.update(series)
			metadata.runtime = metadataFromFirst('runtimes', metadata.runtime, series)
	else:
		metadata.title = metadataFromField('title').decode("utf8")

	metadata.year = metadataFromField('year')

	if 'director' in movie.keys():
		directors = movie['director']
		if directors is not None:
			metadata.directors = directors

	plots = []
	if 'plot' in movie.keys():
		plots = movie['plot']
	if movie.has_key('plot outline') and len(movie['plot outline']):
		plots.append("Outline::" + movie['plot outline'])

	if plots is not None:
		# Find the shortest plot.
		shortest_found = None
		#print "%d plots found" % len(plots)
		for plot in plots:
			text = None
			#IMDbPY 3.9 and later use 'author::plot_content'
			#  earlier versions are 'plot_content::author'
			if float(imdb.__version__) > 3.8:
				text = plot.split("::")[1]
			else:
				text = plot.split("::")[0]
			text = plot.split("::")[1]
			if text.find('@') != -1 or len(text.split(' ')) < 10: # Skip plots of less than 5 words
				continue
			# Use word count as the method to identify the smallest plot
			if shortest_found == None or len(text.split(' ')) < len(shortest_found.split(' ')):
					shortest_found = text
		metadata.plot = shortest_found

	cast = movie.get('cast') 
	cast_str = "" 
	if cast: 
		cl = []
		for name in cast: 
			cl.append(name['name'])

		cast_str = ", ".join(cl)

	metadata.cast = cast_str 

	metadata.rating = metadataFromField('rating', metadata.rating)
	metadata.mpaa_rating = metadataFromField('mpaa', metadata.mpaa_rating)
	metadata.runtime = metadataFromFirst('runtimes', metadata.runtime)
	metadata.genres = metadataFromFirst('genres', metadata.genres)
	metadata.countries = metadataFromFirst('countries', metadata.countries)
	if movie.has_key('akas'):
		metadata.akas = movie['akas']

	return metadata

def metadata_search(imdb_id):
	meta = fetch_metadata(imdb_id)
	if meta is not None:
		return meta.toMetadataString()

def parse_meta(meta, key):
	for line in meta.split("\n"):
		beginning = key + ":"
		if line.startswith(beginning):
			return line[len(beginning):].strip()
	return None

def main():
	p = optparse.OptionParser()
	p.add_option('--version', '-v', action="store_true", default=False,
		help="display 1-line describing name, version, author etc")
	p.add_option('--info', '-i', action="store_true", default=False,
		help="display 1-line of info describing what makes this script unique")
	p.add_option('--movie_search', '-M', metavar='QUERY_STRING',
		help="displays a list of 'movieid:Movie Title' lines that may be "\
			"possible matches for the query.  The lines are ranked "\
			"by descending priority.")
	p.add_option('--poster_search', '-P', metavar='IMDB_ID',
		help="displays a list of URL's to movie posters.  The lines are "\
			"ranked by descending value.")
	p.add_option('--metadata_search', '-D', metavar='IMDB_ID',
		help="displays a list of 'name:value' pairs describing metadata "\
			"for the given movie at the IMDb id.")
	options, arguments = p.parse_args()

	if options.version:
		print "MythVideo IMDbPy wrapper v1.0 (c) Pekka Jääskeläinen 2006"
		sys.exit(0)

	if options.info:
		print """
Uses the IMDbPy package to fetch the data, thus externalizes the actual
parsing of IMDb data to another project, hopefully reducing the maintenance
burden in the future, in addition supports fetching data for TV-series
episodes."""
		sys.exit(0)

	if options.movie_search is not None:
		results = title_search(options.movie_search.decode("utf8"))
		if results:
			for result in results:
				print "%s:%s (%d)" % (result[0], result[1], result[2])
	elif options.poster_search is not None:
		poster_search(options.poster_search)
	elif options.metadata_search is not None:
		print metadata_search(options.metadata_search).encode("utf8")
	else:
		p.print_help()
	sys.exit(0)

if __name__ == '__main__':
	main()
