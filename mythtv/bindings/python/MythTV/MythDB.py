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

SCHEMA_VERSION = 1244

class MythDB(object):
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
			config_files.append('%s/config.xml' % os.environ['MYTHCONFDIR'])

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
			self.db = MySQLdb.connect(user=dbconn['user'], host=dbconn['host'],
					passwd=dbconn['pass'], db=dbconn['name'], use_unicode=True,
					charset='utf8')
			log.Msg(INFO, 'DB Connection info (host:%s, name:%s, user:%s, pass:%s)',
					dbconn['host'], dbconn['name'], dbconn['user'], dbconn['pass'])
		except:
			raise MythError('Connection failed for \'%s\'@\'%s\' to database %s using password %s' %
				(dbconn['user'], dbconn['host'], dbconn['name'], dbconn['pass']))

		# check schema version
		sver = int(self.getSetting('DBSchemaVer'))
		if SCHEMA_VERSION != sver:
			log.Msg(CRITICAL, 'DB speaks schema version %d, but we speak version %d', 
					sver, SCHEMA_VERSION)
			raise MythError('Mismatched schema version')

	def __del__(self):
		self.db.close()
			

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
					WHERE hostname LIKE(%s)""",
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
					WHERE value LIKE(%s) AND hostname IS NULL LIMIT 1""",
					(value))
		else:
			hostname += '%'
			c.execute("""
					SELECT data
					FROM settings
					WHERE value LIKE(%s) AND hostname LIKE(%s) LIMIT 1""",
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
		t = None

		if hostname is None:
			ws = "WHERE value LIKE (%s) AND hostname IS NULL"
			ss = "(data,value) VALUES (%s,%s)"
			t = (data,value)
		else:
			hostname += '%'
			ws = "WHERE value LIKE (%s) AND hostname LIKE (%s)"
			ss = "(data,value,hostname) VALUES (%s,%s,%s)"
			t = (data,value,hostname)

		if c.execute("""UPDATE settings SET data=%%s %s LIMIT 1""" % ws, t) == 0:
			c.execute("""INSERT INTO settings %s""" % ss, t)
		c.close()

	def getCast(self, chanid, starttime, roles=None):
		"""
		Returns cast members for a recording
		A string for 'roles' will return a tuple of members for that role
		A tuple of strings will return a touple containing all listed roles
		No 'roles' will return a dictionary of tuples
		"""
		c = self.db.cursor()
		if c.execute("""SELECT * FROM recorded WHERE 
				chanid=%s AND starttime=%s""",
				(chanid,starttime)) == 0:
			raise MythError('recording does not exist')
		if roles is None:
			length = c.execute("""SELECT name,role
					FROM people,credits
					WHERE people.person=credits.person
					AND chanid=%s AND starttime=%s
					ORDER BY role""", (chanid, starttime))
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
			length = c.execute("""SELECT name
					FROM people,credits
					WHERE people.person=credits.person
					AND chanid=%s AND starttime=%s
					AND role=%s""", (chanid, starttime, roles))
			if length == 0:
				return ()
			names = []
			for name in c.fetchall():
				names.append(name[0])
			return tuple(names)
		elif isinstance(roles,tuple):
			length = c.execute("""SELECT name
					FROM people,credits
					WHERE people.person=credits.person
					AND chanid=%d AND starttime=%d
					AND role IN %s""" % (chanid, starttime, roles))
			if length == 0:
				return ()
			names = []
			for name in c.fetchall():
				names.append(name[0])
			return tuple(names)
		else:
			raise MythError('invalid input format')

	def getStorageGroup(self, group=None, host=None):
		"""
		Returns tuple of dictionaries containing storage group directories
			with the fields 'id', 'group', 'host', and 'dirname'
		Takes an optional group and host for filtering
		"""
		c = self.db.cursor()
		q1 = 'SELECT * FROM storagegroup'
		q2 = 'ORDER BY id'
		if host:
			host += '%'
		if group and host:
			c.execute("""%s 
				WHERE groupname=%%s
				AND hostname like %%s
				%s""" % (q1,q2), (group, host))
		elif group:
			c.execute("""%s
				WHERE groupname=%%s
				%s""" % (q1,q2), (group,))
		elif host:
			c.execute("""%s
				WHERE hostname like %%s
				%s""" % (q1,q2), (host,))
		else:
			c.execute("""%s %s""" % (q1,q2))
		ret = []
		for i in c.fetchall():
			if not i[3][-1] == '/':
				i[3] += '/'
			ret.append({'id':i[0], 'group':i[1], 'host':i[2], 'dirname':i[3]})
		return tuple(ret)

	def getChannels(self):
		"""
		Returns a tuple of channel object defined in the database
		"""
		channels = []
		c = self.db.cursor()
		c.execute("""SELECT * FROM channel""")
		for row in c.fetchall():
			channels.append(Channel(row))
		c.close()
		return tuple(channels)

	def getChannel(self,chanid):
		"""
		Returns a single channel object for the given chanid
		"""
		c = self.db.cursor()
		if c.execute("""SELECT * FROM channel
				WHERE chanid=%s""", (chanid,)):
			return Channel(c.fetchone())
		else:
			return None

	def getGuideData(self, chanid, date):
		"""
		Returns tuple of guide data for one channel on one date
		"""
		guide = []
		c = self.db.cursor()
		c.execute("""SELECT * FROM program
				WHERE chanid=%s AND DATE(starttime)=%s""",
				(chanid,date))
		for show in c.fetchall():
			guide.append(Guide(show))
		c.close()
		return tuple(guide)

	def cursor(self):
		return self.db.cursor()

class Job(object):
	"""
	Class for managing tasks within mythtv's jobqueue
		Job(jobid)  or  Job(chanid,starttime)
			-- manage existing task
		Job(dictionary)
			-- create new task with values in the dictionary
	"""

	NONE         = 0x0000
	SYSTEMJOB    = 0x00ff
	TRANSCODE    = 0x0001
	COMMFLAG     = 0x0002
	USERJOB      = 0xff00
	USERJOB1     = 0x0100
	USERJOB2     = 0x0200
	USERJOB3     = 0x0300
	USERJOB4     = 0x0400

	RUN          = 0x0000
	PAUSE        = 0x0001
	RESUME       = 0x0002
	STOP         = 0x0004
	RESTART      = 0x0008

	NO_FLAGS     = 0x0000
	USE_CUTLIST  = 0x0001
	LIVE_REC     = 0x0002
	EXTERNAL     = 0x0004

	UNKNOWN      = 0x0000
	QUEUED       = 0x0001
	PENDING      = 0x0002
	STARTING     = 0x0003
	RUNNING      = 0x0004
	STOPPING     = 0x0005
	PAUSED       = 0x0006
	RETRY        = 0x0007
	ERRORING     = 0x0008
	ABORTING     = 0x0008
	DONE         = 0x0100
	FINISHED     = 0x0110
	ABORTED      = 0x0120
	ERRORED      = 0x0130
	CANCELLED    = 0x0140

	jobid = None
	host = None
	db = None
	def __init__(self, *inp):
		if isinstance(inp[0],dict):
			if self.insert(inp[0]) is None:
				return none
		elif len(inp) == 1:
			self.jobid = inp[0]
			self.getProgram()
		elif len(inp) == 2:
			self.chanid = inp[0]
			self.starttime = inp[1]
			self.getJobID()
		else:
			raise MythError('improper input length')
		self.getInfo()

	def processfields(self,inp):
		"""
		Sanitize lists of fields for SQL queries
		"""
		fields = ['id','chanid','starttime','inserttime','type','cmds',
				'flags','status','statustime','hostname','args',
				'comment','schedruntime']
		if isinstance(inp,list):
			for i in range(len(inp)-1,-1,-1):
				if inp[i] not in fields:
					del inp[i]
		elif isinstance(inp,dict):
			for i in inp.keys():
				if i not in fields:
					del inp[i]
		return inp

	def get(self,fields,where):
		if self.db is None:
			self.db = MythDB()
		fields = self.processfields(fields)
		where = self.processfields(where)
		if (len(fields)==0) or (len(where)==0):
			raise MythError('no valid database fields given')

		f = ','.join(fields)
		w = 'WHERE '
		data = []
		if isinstance(where,dict):
			wf = []
			for k,v in where.items():
				wf.append('%s=%%s' % k)
				data.append(v)
			w += ' AND '.join(wf)
		else:
			raise MythError("'where' argument must be given as a dictionary")

		c = self.db.cursor()
		c.execute("""SELECT %s FROM jobqueue %s""" % (f,w), data)

		res = c.fetchone()
		if len(fields) == 1:
			return res[0]
		jdict = {}
		for i in range(len(fields)):
			jdict[fields[i]] = res[i]

		return jdict

	def set(self,data):
		if self.db is None:
			self.db = MythDB()
		data = self.processfields(data)
		if len(data)==0:
			raise MythError('no valid database fields given')

		ustr = ', '.join(['%s = %%s' % d for d in data])
		values = data.values()
		values.append(self.jobid)

		c = self.db.cursor()
		c.execute("""UPDATE jobqueue SET %s WHERE id=%%s""" % ustr, values)
		c.close()

	def insert(self,data):
		if self.db is None:
			self.db = MythDB()
		data = self.processfields(data)
		if 'id' in data.keys():
			del data['id']
		if len(data)==0:
			return None

		# require these three fields for any job creation
		for field in ['chanid','starttime','type']:
			if field not in data.keys():
				raise MythError("missing fields in JOB creation")

		if 'status' not in data.keys():
			data['status'] = 0x0001          # default to queued
		if 'schedruntime' not in data.keys():
			data['schedruntime'] = 'NOW()'   # default to now
		data['inserttime'] = 'NOW()'             # override to now

		fields = ', '.join(data.keys())
		istr = ', '.join(['%s' for d in data])

		c = self.db.cursor()
		c.execute("""INSERT INTO jobqueue(%s) VALUES(%s)""" % (fields,istr), data.values())
		self.jobid = c.lastrowid
		c.close()
		return self.jobid

	def getProgram(self):
		"""
		Get chanid and starttime from existing job id
		"""
		res = self.get(['chanid','starttime'],{'id':self.jobid})
		self.chanid = res['chanid']
		self.starttime = res['starttime']

	def getJobID(self):
		"""
		Get job id from existing chanid and starttime
		"""
		self.jobid = self.get(['id'],{'chanid':self.chanid,'starttime':self.starttime})
		return self.jobid

	def getInfo(self):
		"""
		Get remaining data fields from existing job id
		"""
		res = self.get(['inserttime','type','cmds','flags','hostname','args'],{'id':self.jobid})
		self.inserttime = res['inserttime']
		self.type = res['type']
		self.cmds = res['cmds']
		self.flags = res['flags']
		self.host = res['hostname']
		self.args = res['args']

	def getComment(self):
		"""Get comment field from database"""
		return self.get(['comment'],{'id':self.jobid})

	def setComment(self,comment):
		"""Set comment field in database"""
		self.set({'comment':comment})

	def getStatus(self):
		"""Get status field from database"""
		return self.get(['status'],{'id':self.jobid})

	def setStatus(self,status):
		"""Set status field in database"""
		self.set({'status':status})

	def getArgs(self):
		"""Get argument field from database"""
		self.args = self.get(['args'],{'id':self.jobid})
		return self.args

	def setArgs(self,args):
		"""Set argument field in database"""
		self.set({'args':args})

class Channel(object):
	"""
	Represents a single channel from the channel table
	"""
	def __str__(self):
		return "%s (%s)" % (self.chanid, self.name)

	def __repr__(self):
		return "%s (%s)" % (self.chanid, self.name)

	def __init__(self,data):
		"""
		Load data into object
		"""
		self.chanid = data[0]
		self.channum = data[1]
		self.freqid = data[2]
		self.sourceid = data[3]
		self.callsign = data[4]
		self.name = data[5]
		self.icon = data[6]
		self.finetune = data[7]
		self.videofilters = data[8]
		self.xmltvid = data[9]
		self.recpriority = data[10]
		self.contrast = data[11]
		self.brightness = data[12]
		self.colour = data[13]
		self.hue = data[14]
		self.tvformat = data[15]
		self.visible = data[16]
		self.outputfiters = data[17]
		self.useonairguide = data[18]
		self.mplexid = data[19]
		self.serviceid = data[20]
		self.tmoffset = data[21]
		self.atsc_major_chan = data[22]
		self.atsc_minor_chan = data[23]
		self.last_record = data[24]
		self.default_authority = data[25]
		self.commmethod = data[26]

class Guide(object):
	"""
	Represents a single program from the program guide
	"""
	def __str__(self):
		return "%s (%s)" % (self.title, self.starttime.strftime('%Y-%m-%d %H:%M:%S'))

	def __repr__(self):
		return "%s (%s)" % (self.title, self.starttime.strftime('%Y-%m-%d %H:%M:%S'))

	def __init__(self,data):
		"""
		Load data into the object
		"""
		self.chanid = data[0]
		self.starttime = data[1]
		self.endtime = data[2]
		self.title = data[3]
		self.subtitle = data[4]
		self.description = data[5]
		self.category = data[6]
		self.category_type = data[7]
		self.airdate = data[8]
		self.stars = data[9]
		self.previouslyshown = data[10]
		self.title_pronounce = data[11]
		self.stereo = data[12]
		self.subtitled = data[13]
		self.hdtv = data[14]
		self.closecaptioned = data[15]
		self.partnumber = data[16]
		self.parttotal = data[17]
		self.seriesid = data[18]
		self.originalairdate = data[19]
		self.showtype = data[20]
		self.colorcode = data[21]
		self.syndicatedepisodenumber = data[22]
		self.programid = data[23]
		self.manualid = data[24]
		self.generic = data[25]
		self.listingsource = data[26]
		self.first = data[27]
		self.last = data[28]
		self.audioprop = data[29]
		self.subtitletypes = data[30]
		self.videoprop = data[31]

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
