#!/usr/bin/python
# -*- coding: utf8 -*-
"""
This Python script is intended to perform movie data lookups based on 
the popular www.imdb.com website using the IMDbPy script.

The script is hosted at http://imdbpy.sourceforge.net/, you need to install
it to make this script work.

This wrapper script is written by Pekka Jääskeläinen (pekka jaaskelainen gmail).

Changes:
2006-11-26:[PJ] Modified some of the functions to be suitable for using as a Python API (from find_meta.py).
2006-10-05:[PJ] Improved poster searching and Runtime metadata finding for TV-series episodes.
                Better detection for a episode search.
2006-10-04:[PJ] The first version.
"""  

import sys
import optparse
import re

try:
	import imdb
except ImportError:
	print "You need to install the IMDbPy library from http://imdbpy.sourceforge.net/"
	sys.exit(1)
	
def detect_series_query(search_string):
	"""
	Detects a series episode query.
	
	Currently the following formats are detected:
	"Sopranos Season 1 Episode 2"
	"Sopranos S1E2"
	"Sopranos S1 E2"
	"Sopranos 1x2"
	"""
	m = re.match(r"(.+)\s*(s|(season)|\s)\s*(\d+)\s*(e|(episode)|x)\s*(\d+)", search_string.lower())
	if m is None or m.group(1) is None or m.group(4) is None or m.group(7) is None:
		return (None, None, None)
	
	return (m.group(1), m.group(4), m.group(7))

def episode_search(title, season, episode):
	
	matches = []
	imdb_access = imdb.IMDb()
	series = imdb_access.search_movie(title.encode("ascii", 'replace'))
	  	
	for serie in series:
		if serie['kind'] == 'tv series':
			# Fetch episode info.
			imdb_access.update(serie, 'episodes')
			if serie.has_key('episodes'):
				try:
					ep = serie['episodes'][int(season)][int(episode)]
				except:
					# Probably indexing exception in case the episode/season is not found.
					continue
				matches.append([imdb_access.get_imdbID(ep), 
						title.title().strip() + ", S" + season + " E" + episode, int(serie['year'])])
			else:
				
				matches.append([imdb_access.get_imdbID(serie), 
					serie['title'], int(serie['year'])])
	return matches
	
def title_search(search_string):
	"""
	Returns a list of 3-tuples [imdb id, title, year] for possible matches.
	"""
	
	(title, season, episode) = detect_series_query(search_string)	
	
	#print title,"--", season,"--", episode
	#return
	
	if title is not None and season is not None and episode is not None:
		return episode_search(title, season, episode)
	
	imdb_access = imdb.IMDb()
	#print "Search:",search_string
	movies = imdb_access.search_movie(search_string.encode("ascii", 'replace'))	
	
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
		movies.append([imdb_access.get_imdbID(m), m['title'], int(m['year'])])
	return movies

def poster_search(imdb_id):
	imdb_access = imdb.IMDb()
	movie = imdb_access.get_movie(imdb_id)
	imdb_access.update(movie)
	url = None
	if 'cover url' in movie.keys():
		url = movie['cover url']
	if url is not None:
		print url
	elif movie['kind'] == 'episode':
		series = movie['episode of']
		imdb_access.update(series)
		if 'cover url' in series.keys():
			url = series['cover url']
		if url is not None:
			print url
	
def metadata_search(imdb_id):
	
	metadata = unicode()
	imdb_access = imdb.IMDb()
	movie = imdb_access.get_movie(imdb_id)
	imdb_access.update(movie)
	
	def createMetadataFromField(key, searchKey=None, m=movie):
		if searchKey == None:
			searchKey = key.lower()
		if searchKey not in m.keys(): return ""
		value = unicode(m[searchKey])
		try:
			value = value.encode("utf8")
		except AttributeError:
			pass
		if value is not None:
			return "%s:%s" % (key, value) + "\n"
		else:
			return ""
					
	def createMetadataFromFirst(key, searchKey=None, m=movie):
		if searchKey == None:
			searchKey = key.lower()
		if searchKey not in m.keys(): return ""
		value = m[searchKey]
		if value is not None and len(value) > 0:				
			return "%s:%s" % (key, value[0].encode("utf8")) + "\n"
		else:
			return ""
						
	if movie['kind'] == 'episode':
		# print "TV Series episode detected"
		metadata += 'Title:%s, S%d E%d: \"%s\"' % (movie['series title'], int(movie['season']),
		int(movie['episode']), movie['title']) + "\n"
		series = movie['episode of']
		imdb_access.update(series)
		metadata += createMetadataFromFirst('Runtime', 'runtimes', m=series)
	else:
		metadata += createMetadataFromField('Title').decode("utf8")
	metadata += createMetadataFromField('Year')
	
	if 'director' in movie.keys():
		directors = movie['director']
		if directors is not None:
			metadata += "Director:%s" % directors[0] + "\n"
			
	if 'plot' in movie.keys():
		plots = movie['plot']
		if plots is not None:
			# Find the shortest plot.
			shortest_found = None
			#print "%d plots found" % len(plots)
			for plot in plots:
				text = plot.split("::")[1]
				if shortest_found == None or len(text) < len(shortest_found):				
					shortest_found = text
			metadata += "Plot:%s" % shortest_found + "\n"

	metadata += createMetadataFromField('UserRating', 'rating')
	metadata += createMetadataFromField('MovieRating', 'mpaa')
	metadata += createMetadataFromFirst('Runtime', 'runtimes')				
	return metadata

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
		print "Uses the IMDbPy package to fetch the data, thus externalizes the actual "\
			"parsing of IMDb data to another project, hopefully reducing the maintenance burden "\
			"in the future, in addition supports fetching data for TV-series episodes."
		sys.exit(0)
		
	if options.movie_search is not None:
		results = title_search(options.movie_search.decode("utf8"))
		for result in results:
			print "%s:%s (%d)" % (result[0], result[1], result[2])
	elif options.poster_search is not None:
		poster_search(options.poster_search)
	elif options.metadata_search is not None:
		print metadata_search(options.metadata_search)
	else:
		p.print_help()
	sys.exit(0)
	
		
    	
	

if __name__ == '__main__':
	main()
    