#!/usr/bin/python

"""
Provides a class giving access to the MythTV database.
"""
import os
import sys
import xml.dom.minidom as minidom
import code
import getopt
from datetime import datetime

from MythLog import *

# create logging object
log = MythLog(CRITICAL, '#%(levelname)s - %(message)s', 'MythDB')

# check for dependency
try:
	import MySQLdb
except:
	log.Msg(CRITICAL, "MySQLdb (python-mysqldb) is required but is not found.")
	sys.exit(1)

class MythDB:
	"""
	A connection to the mythtv database.
	"""
	def __init__(self, args=None):
		# Setup connection variables
		dbconn = {
				'host' : None,
				'name' : None,
				'user' : None,
				'pass' : None,
				'USN'  : None,
				'PIN'  : None
				}

		# Try to read the config.xml file used by MythTV.
		config_files = [ os.path.expanduser('~/.mythtv/config.xml') ]
		if 'MYTHCONFDIR' in os.environ:
			config_locations.append('%s/config.xml' % os.environ['MYTHCONFDIR'])

		found_config = False
		for config_file in config_files:
			try:
				config = minidom.parse(config_file)
			except:
				continue

			dbconn['host'] = None
			dbconn['name'] = None
			dbconn['user'] = None
			dbconn['pass'] = None
			for token in config.getElementsByTagName('Configuration')[0].getElementsByTagName('UPnP')[0].getElementsByTagName('MythFrontend')[0].getElementsByTagName('DefaultBackend')[0].childNodes:
				if token.nodeType == token.TEXT_NODE:
					continue
				try:
					if token.tagName == "DBHostName":
						dbconn['host'] = token.childNodes[0].data
					elif token.tagName == "DBName":
						dbconn['name'] = token.childNodes[0].data
					elif token.tagName == "DBUserName":
						dbconn['user'] = token.childNodes[0].data
					elif token.tagName == "DBPassword":
						dbconn['pass'] = token.childNodes[0].data
					elif token.tagName == "USN":
						dbconn['USN'] = token.childNodes[0].data
					elif token.tagName == "SecurityPin":
						dbconn['PIN'] = token.childNodes[0].data
				except:
					pass

			if dbconn['host'] != None and dbconn['name'] != None and dbconn['user'] != None and dbconn['pass'] != None:
				log.Msg(INFO, 'Using config %s', config_file)
				found_config = True
				break

		# Overrides from command line parameters
		try:
			opts, args = getopt.getopt(args, '', ['dbhost=', 'user=', 'pass=', 'database='])
			for o, a in opts:
				if o == '--dbhost':
					dbconn['host'] = a
				if o == '--user':
					dbconn['user'] = a
				if o == '--pass':
					dbconn['pass'] = a
				if o == '--database':
					dbconn['name'] = a
		except:
			pass

		if not dbconn['host'] and not found_config:
			raise MythError('Unable to find MythTV configuration file')

		try:
			self.db = MySQLdb.connect(user=dbconn['user'], host=dbconn['host'], passwd=dbconn['pass'], db=dbconn['name'], use_unicode=True, charset='utf8')
			log.Msg(INFO, 'DB Connection info (host:%s, name:%s, user:%s, pass:%s)', dbconn['host'], dbconn['name'], dbconn['user'], dbconn['pass'])
		except:
			raise MythError('Connection failed for \'%s\'@\'%s\' to database %s using password %s' % (dbconn['user'], dbconn['host'], dbconn['name'], dbconn['pass']))

	def getAllSettings(self, hostname=None):
		"""
		Returns values for all settings.

		Returns None if there are no settings. If multiple rows are
		found (multiple hostnames), returns the value of the first one.
		"""
		log.Msg(DEBUG, 'Retrieving all setting for host %s', hostname)
		c = self.db.cursor()
		if hostname is None:
			c.execute("""
					SELECT value, data
					FROM settings
					WHERE hostname IS NULL""")
		else:
			c.execute("""
					SELECT value, data
					FROM settings
					WHERE hostname LIKE('%s%%')""" %
					(hostname))
		rows = c.fetchall()
		c.close()

		if rows:
			return rows
		else:
			return None

	def getSetting(self, value, hostname=None):
		"""
		Returns the value for the given MythTV setting.

		Returns None if the setting was not found. If multiple rows are
		found (multiple hostnames), returns the value of the first one.
		"""
		log.Msg(DEBUG, 'Looking for setting %s for host %s', value, hostname)
		c = self.db.cursor()
		if hostname is None:
			c.execute("""
					SELECT data
					FROM settings
					WHERE value LIKE('%s') AND hostname IS NULL LIMIT 1""" %
					(value))
		else:
			c.execute("""
					SELECT data
					FROM settings
					WHERE value LIKE('%s') AND hostname LIKE('%s%%') LIMIT 1""" %
					(value, hostname))
		row = c.fetchone()
		c.close()

		if row:
			return row[0]
		else:
			return None

	def setSetting(self, value, data, hostname=None):
		"""
		Sets the value for the given MythTV setting.
		"""
		log.Msg(DEBUG, 'Setting %s for host %s to %s', value, hostname, data)
		c = self.db.cursor()
		ws = None
		ss = None

		if hostname is None:
			ws = "WHERE value LIKE ('%s') AND hostname IS NULL" % (value)
			ss = "(value,data) VALUES ('%s','%s')" % (value, data)
		else:
			ws = "WHERE value LIKE ('%s') AND hostname LIKE ('%s%%')" % (value, hostname)
			ss = "(value,data,hostname) VALUES ('%s','%s','%s')" % (value, data, hostname)

		if c.execute("""UPDATE settings SET data %s LIMIT 1""" % ws) == 0:
			c.execute("""INSERT INTO settings %s""" % ss)
		c.close()

	def getCast(self, chanid, starttime, roles=None):
		"""
		Returns cast members for a recording
		A string for 'roles' will return a tuple of members for that role
		A tuple of strings will return a touple containing all listed roles
		No 'roles' will return a dictionary of tuples
		"""
		if roles is None:
			c = self.db.cursor()
			length = c.execute("SELECT name,role FROM people,credits WHERE people.person=credits.person AND chanid=%d AND starttime=%d ORDER BY role" % (chanid, starttime))
			if length == 0:
				return ()
			crole = None
			clist = []
			dict = {}
			for name,role in c.fetchall():
				if crole is None:
					crole = role
				if crole == role:
					clist.append(name)
				else:
					dict[crole] = tuple(clist)
					clist = []
					clist.append(name)
					crole = role
			dict[crole] = tuple(clist)
			c.close()
			return dict
		elif isinstance(roles,str):
			c = self.db.cursor()
			length = c.execute("SELECT name FROM people,credits WHERE people.person=credits.person AND chanid=%d AND starttime=%d AND role='%s'" % (chanid, starttime, roles))
			if length == 0:
				return ()
			names = []
			for name in c.fetchall():
				names.append(name[0])
			return tuple(names)
		elif isinstance(roles,tuple):
			c = self.db.cursor()
			length = c.execute("SELECT name FROM people,credits WHERE people.person=credits.person AND chanid=%d AND starttime=%d AND role IN %s" % (chanid, starttime, roles))
			if length == 0:
				return ()
			names = []
			for name in c.fetchall():
				names.append(name[0])
			return tuple(names)

	def cursor(self):
		return self.db.cursor()

class Job:
	jobid = None
	chanid = None
	starttime = None
	host = None
	mythdb = None
	def __init__(self, *inp):
		if len(inp) == 1:
			self.jobid = inp[0]
			self.getProgram()
		elif len(inp) == 2:
			self.chanid = inp[0]
			self.starttime = inp[1]
			self.getJobID()
		else:
			print("improper input length")
			return None
		self.getHost()

	def getProgram(self):
		if self.mythdb is None:
			self.mythdb = MythDB()
		c = self.mythdb.cursor()
		c.execute("SELECT chanid,starttime FROM jobqueue WHERE id=%d" % self.jobid)
		self.chanid, self.starttime = c.fetchone()
		c.close()

	def getJobID(self):
		if self.mythdb is None:
			self.mythdb = MythDB()
		if self.jobid is None:
			c = self.mythdb.cursor()
			c.execute("SELECT id FROM jobqueue WHERE chanid=%d AND starttime=%d" % (self.chanid, self.starttime))
			self.jobid = c.fetchone()[0]
			c.close()
		return self.jobid

	def getHost(self):
		if self.mythdb is None:
			self.mythdb = MythDB()
		if self.host is None:
			c = self.mythdb.cursor()
			c.execute("SELECT hostname FROM jobqueue WHERE id=%d" % self.jobid)
			self.host = c.fetchone()[0]
			c.close()
		return self.host

	def setComment(self,comment):
		if self.mythdb is None:
			self.mythdb = MythDB()
		c = self.mythdb.cursor()
		c.execute("UPDATE jobqueue SET comment='%s' WHERE id=%d" % (comment,self.jobid))
		c.close()

	def setStatus(self,status):
		if self.mythdb is None:
			self.mythdb = MythDB()
		c = self.mythdb.cursor()
		c.execute("UPDATE jobqueue SET status=%d WHERE id=%d" % (status,self.jobid))
		c.close()

if __name__ == '__main__':
	banner = "'mdb' is a MythDB instance."
	try:
		import readline, rlcompleter
	except:
		pass
	else:
		readline.parse_and_bind("tab: complete")
		banner = banner + " TAB completion is available."
	mdb = MythDB(sys.argv[1:])
	namespace = globals().copy()
	namespace.update(locals())
	code.InteractiveConsole(namespace).interact(banner)

# vim: ts=4 sw=4:
