#!/usr/bin/python

import sys
import os
import socket
import MySQLdb
from optparse import OptionParser
from mythvideo_test import HTTP_PORT

IMAGE_URL_BASE = 'http://%s:%d/images'

SETTINGS_BACKUP = '~/.mythvideo_search.rc'

URL_PORT=HTTP_PORT
URL_HOST='localhost'

DB_USER = 'mythtv'
DB_PASSWORD = 'mythtv'
DB_HOST = 'localhost'
DB_DATABASE = 'mythconverg'

class ItemInfo:
	def __init__(self, id, title, year, release_date, director, plot,
			user_rating, movie_rating, runtime, writers, cast, genres,
			countries, poster = None, akas = None):
		self.m_id = id
		self.m_title = title
		self.m_year = year
		self.m_release_date = release_date
		self.m_director = director
		self.m_plot = plot
		self.m_user_rating = user_rating
		self.m_movie_rating = movie_rating
		self.m_runtime = runtime
		self.m_writers = writers
		self.m_cast = cast
		self.m_genres = genres
		self.m_countries = countries
		self.m_poster = poster
		self.m_akas = akas

	def __str__(self):
		return """Title:%(title)s
Year:%(year)s
ReleaseDate:%(releasedate)s
Director:%(director)s
Plot:%(plot)s
UserRating:%(userrating)s
MovieRating:%(movierating)s
Runtime:%(runtime)s
Writers:%(writers)s
Cast:%(cast)s
Genres:%(genres)s
Countries:%(countries)s
""" % {
		'title' : self.m_title,
		'year' : self.m_year,
		'releasedate' : self.m_release_date,
		'director' : self.m_director,
		'plot' : self.m_plot,
		'userrating' : self.m_user_rating,
		'movierating' : self.m_movie_rating,
		'runtime' : self.m_runtime,
		'writers' : self.m_writers,
		'cast' : self.m_cast,
		'genres' : self.m_genres,
		'countries' : self.m_countries
		}

items = [
		ItemInfo('1234567', 'Test Video 1', '2008', '2008-01-02', 'ADirector',
			'None.', '5.3', 'Rated PG-13', '103', 'Some Writter', 'Cast member',
			'Action', 'USA', 'poster1.png',
			['Test Video', 'Test Video One', 'all']),
		ItemInfo('1234568', 'Test Video One', '2008', '2008-01-02', 'ADirector',
			'None.', '5.3', 'Rated PG-13', '103', 'Some Writter', 'Cast member',
			'Action', 'USA', 'poster2.png',
			['Test Video', 'Test Video One', 'Test video 1', 'all']),
		ItemInfo('1234569', 'Test Video 2', '2008', '2008-01-02', 'ADirector',
			'None.', '5.3', 'Rated PG-13', '103', 'Some Writter', 'Cast member',
			'Action', 'USA', 'poster3.png', ['Test Video Two', 'all']),
		]

poster_map = dict()

def title_search(title):
	for video in items:
		search_list = [video.m_title]
		if (video.m_akas):
			search_list.extend(video.m_akas)
		for ts in search_list:
			if ts.lower() == title.lower():
				print('%s:%s' % (video.m_id, video.m_title))
				break

def data_search(uid):
	for video in items:
		if video.m_id.lower() == uid.lower():
				print(video)
				break

def poster_search(uid):
	item = poster_map[uid]
	if item:
		print('%s/%s' % (IMAGE_URL_BASE % (URL_HOST, URL_PORT), item));

def __open_db():
	conn = MySQLdb.connect(host = DB_HOST, user = DB_USER,
			passwd = DB_PASSWORD, db = DB_DATABASE)
	return conn

def get_db_setting(cur, name, hostname):
	cur.execute("SELECT data FROM settings WHERE "
			"value = %s AND hostname = %s", (name, hostname))
	row = cur.fetchone()
	if (row and len(row)):
		return row[0]
	return None

def set_db_setting(cur, name, value, hostname):
	cur.execute("DELETE FROM settings WHERE value = %s AND hostname = %s",
			(name, hostname))
	cur.execute("INSERT INTO settings (value, data, hostname) "
	            "VALUES (%s, %s, %s)", (name, value, hostname))

def get_video_startup_dir(cur, hostname):
	vsd = get_db_setting(cur, 'VideoStartupDir', hostname)
	return vsd.split(':')[0]

def install():
	backupfile = os.path.expanduser(SETTINGS_BACKUP)
	if os.path.exists(backupfile):
		print('Error: a backup already exists, will not install over.')
		return

	try:
		hostname = socket.gethostname()

		conn = __open_db()
		c = conn.cursor()
		old_movie_list = get_db_setting(c, 'MovieListCommandLine', hostname)
		old_poster = get_db_setting(c, 'MoviePosterCommandLine', hostname)
		old_data = get_db_setting(c, 'MovieDataCommandLine', hostname)

		if (old_movie_list and old_poster and old_data):
			f = open(backupfile, 'w')
			f.writelines(
			"""MovieListCommandLine=%(movie_list_cmd)s
MoviePosterCommandLine=%(movie_poster_cmd)s
MovieDataCommandLine=%(movie_data_cmd)s
""" % { 'movie_list_cmd' : old_movie_list,
	    'movie_poster_cmd' : old_poster,
		'movie_data_cmd' : old_data})
			f.close()

			thisscript = os.path.abspath(sys.argv[0])
			set_db_setting(c, 'MovieListCommandLine',
					'%s -M' % (thisscript,), hostname)
			set_db_setting(c, 'MoviePosterCommandLine',
					'%s -P' % (thisscript,), hostname)
			set_db_setting(c, 'MovieDataCommandLine',
					'%s -D' % (thisscript,), hostname)

			vsd = get_video_startup_dir(c, hostname)
			testdir = os.path.join(vsd, 'mythvideo_search')
			print('Creating dummy video files here: %s' % (testdir,))
			if os.path.isdir(testdir) or os.mkdir(testdir):
				for item in items:
					f = open(os.path.join(testdir, item.m_title + '.iso'), 'w')
					f.write('dummyfile')
					f.close()

			print('Installation complete, use --uninstall to revert.')
	except:
		raise

def uninstall():
	backupfile = os.path.expanduser(SETTINGS_BACKUP)
	if not os.path.exists(backupfile):
		print('Error: backup file %s not found, unable to restore.' %
				(backupfile,))
		return

	try:
		hostname = socket.gethostname()

		conn = __open_db()
		c = conn.cursor()

		f = open(backupfile, "r")
		for line in f:
			line = line.strip()
			if len(line):
				(value, data) = line.split('=', 1)
				set_db_setting(c, value, data, hostname)

		os.unlink(backupfile)
		vsd = get_video_startup_dir(c, hostname)
		testdir = os.path.join(vsd, 'mythvideo_search')
		print('Removing dummy video files from: %s' % (testdir,))
		if os.path.isdir(testdir):
			for item in items:
				tf = os.path.join(testdir, item.m_title + '.iso')
				if os.path.isfile(tf):
					unlink(tf)
				else:
					print('Dummy test file (%s) does not exist, ignoring' %
							(tf))
		else:
			print('No test directory found, ignoring.')
		print('Restore now complete.')
	except:
		raise

def main():
	for item in items:
		if item.m_poster:
			poster_map[item.m_id] = item.m_poster

	parser = OptionParser()
	parser.add_option("-M", "--title", type="string", dest="title_search",
			help="Searches for entries matching TITLE.", metavar="TITLE")
	parser.add_option("-D", "--data", type="string", dest="data_search",
			help="Searches for data associated with UID.", metavar="UID")
	parser.add_option("-P", "--poster", type="string", dest="poster_search",
			help="Searches for the poster associated with UID.", metavar="UID")
	parser.add_option("--install", action="store_true", dest="install",
			help="Installs this script as the MythVideo external script for "
			     "poster/data/search.")
	parser.add_option("--uninstall", action="store_true", dest="uninstall",
			help="Undoes what --install did.")
	parser.add_option("--db-user", type="string", dest="db_user",
			help="Database user.")
	parser.add_option("--db-host", type="string", dest="db_host",
			help="Database host.")
	parser.add_option("--db-password", type="string", dest="db_password",
			help="Database password.")
	parser.add_option("-p", "--port", type="int", dest="port",
			help="Port returned URLs point to.")
	parser.add_option("-u", "--url-host", type="string", dest="url_host",
			help="Host serving returned poster URLs.")
	(options, args) = parser.parse_args()

	global DB_USER, DB_PASSWORD, DB_HOST, URL_PORT, URL_HOST
	if options.db_user:
		DB_USER = options.db_user
	if options.db_host:
		DB_HOST = options.db_host
	if options.db_password:
		DB_PASSWORD = options.db_password
	if options.port:
		URL_PORT=options.port
	if options.url_host:
		URL_HOST=options.url_host

	if options.install:
		install()
	elif options.uninstall:
		uninstall()
	elif options.title_search:
		title_search(options.title_search)
	elif options.data_search:
		data_search(options.data_search)
	elif options.poster_search:
		poster_search(options.poster_search)

if __name__ == '__main__':
	main()
