#!/usr/bin/python

import sys
import os
import socket
import MySQLdb
from optparse import OptionParser

DB_USER = 'mythtv'
DB_PASSWORD = 'mythtv'
DB_HOST = 'localhost'
DB_DATABASE = 'mythconverg'

BACKUP = '~/.mythvideo_script.rc'

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

def install(script):
	backupfile = os.path.expanduser(BACKUP)
	if os.path.exists(backupfile):
		print('Error: a backup already exists, will not install over.')
		return

	try:
		hostname = socket.gethostname()
		print("hostname = %s" % (hostname,))
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

			set_db_setting(c, 'MovieListCommandLine',
					'%s -M' % (script,), hostname)
			set_db_setting(c, 'MoviePosterCommandLine',
					'%s -P' % (script,), hostname)
			set_db_setting(c, 'MovieDataCommandLine',
					'%s -D' % (script,), hostname)
		conn.commit()
		print('Installation complete, use --uninstall to revert.')
	except:
		raise

def uninstall():
	backupfile = os.path.expanduser(BACKUP)
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
		conn.commit()
		print('Restore now complete.')
	except:
		raise

def main():
	parser = OptionParser()

	parser.add_option("--install", type="string", dest="install",
			metavar="SCRIPT",
			help="Installs SCRIPT as the MythVideo external script for "
			"poster/data/search.")
	parser.add_option("--uninstall", action="store_true", dest="uninstall",
			help="Undoes what --install did.")

	parser.add_option("--db-user", type="string", dest="db_user",
			help="Database user.")
	parser.add_option("--db-host", type="string", dest="db_host",
			help="Database host.")
	parser.add_option("--db-password", type="string", dest="db_password",
			help="Database password.")
	parser.add_option("--db-database", type="string", dest="db_database",
			help="Database.")

	parser.add_option("--script-dir", type="string", dest="script_dir",
			default="/usr/local/share/mythtv/mythvideo/scripts",
			help="Script dir.")
	(options, args) = parser.parse_args()

	global DB_USER, DB_PASSWORD, DB_HOST, DB_DATABASE
	if options.db_user:
		DB_USER = options.db_user
	if options.db_host:
		DB_HOST = options.db_host
	if options.db_password:
		DB_PASSWORD = options.db_password
	if options.db_password:
		DB_DATABASE = options.db_database

	if options.install:
		inst_script = options.install
		if not os.path.exists(inst_script):
			inst_script = os.path.join(options.script_dir, options.install)

		if not os.path.exists(inst_script):
			print("Error: script not found '%s'" % (inst_script,))
			sys.exit(1)

		inst_script = os.path.abspath(inst_script)
		install(inst_script)
	elif options.uninstall:
		uninstall()
	else:
		parser.print_help()

if __name__ == '__main__':
	main()
