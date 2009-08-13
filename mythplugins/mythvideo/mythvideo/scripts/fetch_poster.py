#!/usr/bin/python
# -*- coding: utf8 -*-
"""
This Python script is intended to find the best possible poster/cover image
for a video. 

Uses the following www-sites for scraping for the poster image (in this
order):
	
	movieposter.com
	imdb.com
	
Picks the largest (in pixels) vertical poster.	
	
Written by Pekka Jääskeläinen (gmail: pekka.jaaskelainen) 2007
"""

import urllib
import re
import tempfile
import os
import optparse
import sys
import imdbpy

movie_poster_site = True
try:
	import BeautifulSoup
except:
	print """BeautifulSoup class is required for parsing the MoviePoster site.
	
In Debian/Ubuntu it is packaged as 'python-beautifulsoup'.

http://www.crummy.com/software/BeautifulSoup/#Download/"""
	movie_poster_site = False
	
imaging_library = True
try:
	import Image
except:
	print """Python Imaging Library is required for figuring out the sizes of 
the fetched poster images.

In Debian/Ubuntu it is packaged as 'python-imaging'.

http://www.pythonware.com/products/pil/"""
	imaging_library = False

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
				print 'Failed to retrieve data, retry in 5s'
				time.sleep(5)
			
	if attempts > retries:
		print 'Error retrieving data : No more attempts'
	return result

class PosterImage:
	"""
	Holds a single poster image.
	
	Contains information of the resolution, location of the file in
	the file system. etc.	
	"""
	width = 0
	height = 0
	file_name = None
	def __init__(self, file_name):
		self.file_name = file_name
		try:
			(self.width, self.height) = Image.open(file_name).size
		except:
			# The imaging lib is not installed or some other error.
			# Do not take the size in account.
			pass

	
	def is_vertical(self):
		return self.width < self.height
	
	def pixels(self):
		return self.width*self.height
	
class PosterFetcher:
	"""
	Base class for poster image fetchers.
	"""
	def fetch(self, title_string, imdb_id = None):
		"""
		Fetch and download to a local temporary filename movie posters
		for the given title.		

		Return empty list in case no images was found.
		"""
		pass
		
	def download_image(self, image_url, extension=None):
				
		(fid, local_filename) = tempfile.mkstemp(extension)	
		local_file = os.fdopen(fid, "wb")
		local_file.write(urllib.urlopen(image_url).read())
		local_file.close()						
		return PosterImage(local_filename)

class MoviePosterPosterFetcher(PosterFetcher):
	"""
	Fetches poster images from movieposter.com
	"""	
	def fetch(self, title_string, imdb_id = None):

		poster_urls = self.title_search(title_string)
		results = 0
		max_results = 4
		images = []
		
		if poster_urls:
			for url in poster_urls:
				image_url = self.find_poster_image_url(url)
				if image_url is not None:
					images.append(self.download_image(image_url, ".jpg"))
					results += 1
				if results >= max_results:
					break
		return images
				
	def find_poster_image_url(self, poster_page_url):
		"""
		Parses the given poster page and returns an URL pointing to the poster
		image.
		"""	
		#print "Getting",poster_page_url
				
		soup = BeautifulSoup.BeautifulSoup(urllib.urlopen(poster_page_url))
		
		imgs = soup.findAll('img', attrs={'src':re.compile('/posters/archive/main/.*')})
		
		if len(imgs) == 1:
			return "http://eu.movieposter.com/" + imgs[0]['src']
		return None			
			
				
	def title_search(self, title_string):
		"""
		Executes a title search on movieposter.com.
		
		Returns a list of URLs leading to the page for the poster
		for the given title_string.
		"""
		params = urllib.urlencode(\
			{'ti': title_string.encode("ascii", 'replace'), 
			 'pl': 'action', 
			 'th': 'y', 
			 'rs': '12', 
			 'size': 'any'})
		opener = urllib.URLopener()
		(filename, headers) = \
			opener.retrieve("http://eu.movieposter.com/cgi-bin/mpw8/search.pl",
			data=params)

		f = open(filename, 'r')
		results = f.read()
		f.close()
		
		return self.parse_title_search_results(results, title_string)	
		
	def parse_title_search_results(self, result_page, title_string):
		"""
		Parses the result page of a title search on movieposter.com.
		
		Returns a list of URLs leading to a page with poster for the given title.
		"""
		search = title_string.lower()
		soup = BeautifulSoup.BeautifulSoup(result_page)
		divs = soup.findAll('div', attrs={'class':'pid'})
		urls = []
		for div in divs:
			links = div.findAll('a')
			
			if len(links) > 0:
				for link in links:
					# Skip the mailto links.
					spl = link['href'].split(":")
					if len(spl) > 1:
						if spl[0].lower() == "mailto":
							continue
					title = link['title'].lower()
					if title.endswith("poster"):
						title = title[0:-len(" poster")]
					if title == search:
						urls.append(link['href'])
		return urls
				
		
class IMDbPosterFetcher(PosterFetcher):
	"""
	Fetches poster images from imdb.com.
	"""
	def fetch(self, title_string, imdb_id = None):

		if imdb_id is None:
			return []
		poster_url = imdbpy.find_poster_url(imdb_id)
		if poster_url is not None:
			filename = poster_url.split("/")[-1]
			(name, extension) = os.path.splitext(filename)
			return [self.download_image(poster_url, extension)]
		return []
			
def find_best_posters(title, count=1, accept_horizontal=False, imdb_id=None, retries=None):
		
	fetchers = [MoviePosterPosterFetcher(), IMDbPosterFetcher()]
	#fetchers = [IMDbPosterFetcher()]	
	posters = []
	
	# If it's a series title 'Sopranos, S06E14' then use just the series
	# name for finding the poster. Strip the episode number.
	(series_title, season, episode) = imdbpy.detect_series_title(title)	
	if series_title is not None and season is not None and episode is not None:
		title = series_title.strip()
		if title.endswith(","):
			title = title[0:-1]

	# Drop 'The" etc.
	preps = ["the", "a" , "an", "die", "der"]
	for prep in preps:
		if title.lower().startswith(prep + " "):
			title = title[len(prep + " "):]
			break
		
	for fetcher in fetchers:
		new_posters = functionretry(fetcher.fetch, title, arg2=imdb_id, retries=retries)
		if new_posters:
			for poster in new_posters:
				if not accept_horizontal and not poster.is_vertical():
					os.remove(poster.file_name)
					continue
				posters.append(poster)
		
	def size_cmp(a, b):
		return cmp(a.pixels(), b.pixels())
	
	posters.sort(size_cmp)
	posters.reverse()
	
	for small_poster in posters[count:]:
		os.remove(small_poster.file_name)
		
	return posters[0:count]
		
def main():
	global defaultretries
	
	p = optparse.OptionParser()
	p.add_option('--number', '-n', action="store", type="int", default=1,
		help="the count of biggest posters to get")
	p.add_option('--all', '-a', action="store_true", default=False,
		help="accept all posters, even horizontal ones")
	p.add_option('--poster_search', '-P', metavar='IMDB_ID', default=None, dest="imdb_id",
		help="Displays a list of URL's to movie posters.  The lines are "\
		"ranked by descending value. For MythVideo.")		
	p.add_option('--retry', '-t', action="store", type="int", dest="retries",default=3,
		help="Number of retries, 0 means no retry [default 3]")
		
	options, arguments = p.parse_args()
	
	defaultretries = options.retries

	title = ""
	if len(arguments) != 1:
		if options.imdb_id:
			# TODO: Fetch the title from IMDb.
			metadata = functionretry(imdbpy.metadata_search,options.imdb_id)
			if metadata:
				title = imdbpy.parse_meta(metadata, "Title")
			else:
				print "Error can't retrieve title from IMDb"
				sys.exit(1)				
		else:
			print "Please give a video title as argument."
			sys.exit(1)				
	else:
		title = arguments[0]
		
	posters = find_best_posters(title, options.number, options.all,
				imdb_id=options.imdb_id, retries=defaultretries)	
	
	if options.imdb_id is not None:
		for poster in posters:
			print "file://%s" % poster.file_name			
	else:
		for poster in posters:
			print "%s [%dx%d] vertical: %s " % \
				(poster.file_name, poster.width, 
			 	 poster.height, poster.is_vertical())	
		
if __name__ == '__main__':
	main()		

