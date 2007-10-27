#!/usr/bin/python

import logging

log = logging.getLogger('mythtv')
log.setLevel(logging.WARNING)
ch = logging.StreamHandler()
ch.setFormatter(logging.Formatter('%(levelname)s - %(message)s'))
log.addHandler(ch)

import os
import sys
import socket
import shlex
import socket
import code

from datetime import datetime

try:
	import MySQLdb
except:
	log.critical("MySQLdb (python-mysqldb) is required but is not found.")
	sys.exit(1)

RECSTATUS = {
	'TunerBusy': -8,
	'LowDiskSpace': -7,
	'Cancelled': -6,
	'Deleted': -5,
	'Aborted': -4,
	'Recorded': -3,
	'Recording': -2,
	'WillRecord': -1,
	'Unknown': 0,
	'DontRecord': 1,
	'PreviousRecording': 2,
	'CurrentRecording': 3,
	'EarlierShowing': 4,
	'TooManyRecordings': 5,
	'NotListed': 6,
	'Conflict': 7,
	'LaterShowing': 8,
	'Repeat': 9,
	'Inactive': 10,
	'NeverRecord': 11,
}

BACKEND_SEP = '[]:[]'
PROTO_VERSION = 36
PROGRAM_FIELDS = 46

class MythDB:
	def __init__(self):
		"""
		A connection to the mythtv database.
		"""
		config_files = [
			'/usr/local/share/mythtv/mysql.txt',
			'/usr/share/mythtv/mysql.txt',
			'/usr/local/etc/mythtv/mysql.txt',
			'/etc/mythtv/mysql.txt',
			os.path.expanduser('~/.mythtv/mysql.txt'),
		]
		if 'MYTHCONFDIR' in os.environ:
			config_locations.append('%s/mysql.txt' % os.environ['MYTHCONFDIR'])
	
		found_config = False
		for config_file in config_files:
			try:
				config = shlex.shlex(open(config_file))
				config.wordchars += "."
			except:
				continue
	
			token = config.get_token()
			db_host = db_user = db_password = db_name = None
			while  token != config.eof and (db_host == None or db_user == None or db_password == None or db_name == None):
				if token == "DBHostName":
					if config.get_token() == "=":
						db_host = config.get_token()
				elif token == "DBUserName":
					if config.get_token() == "=":
						db_user = config.get_token()
				elif token == "DBPassword":
					if config.get_token() == "=":
						db_password = config.get_token()
				elif token == "DBName":
					if config.get_token() == "=":
						db_name = config.get_token()
	
				token = config.get_token()
			log.debug('Using config %s' % config_file)
			found_config = True
			break
	
		if not found_config:
			raise "Unable to find MythTV configuration file"
		self.db = MySQLdb.connect(user=db_user, host=db_host, passwd=db_password, db=db_name)
	
	def getSetting(self, value, hostname=None):
		"""
		Returns the value for the given MythTV setting.
		
		Returns None if the setting was not found. If multiple rows are
		found (multiple hostnames), returns the value of the first one.
		"""
		log.debug('Looking for setting %s for host %s', value, hostname)
		c = self.db.cursor()
  		if hostname is None:
			c.execute("""
				SELECT data
				FROM settings
				WHERE value LIKE(%s) AND hostname IS NULL LIMIT 1""",
				(value,))
		else:
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
	
	def cursor(self):
		return self.db.cursor()
	

class MythTV:
	"""
	A connection to MythTV backend.
	"""
	def __init__(self, conn_type='Monitor'):
		self.db = MythDB()
		self.master_host = self.db.getSetting('MasterServerIP')
		self.master_port = int(self.db.getSetting('MasterServerPort'))
		
		if not self.master_host:
			log.critical('Unable to find MasterServerIP in database')
			sys.exit(1)
		if not self.master_port:
			log.critical('Unable to find MasterServerPort in database')
			sys.exit(1)
		
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.socket.settimeout(10)
		self.socket.connect((self.master_host, self.master_port))
		res = self.backendCommand('MYTH_PROTO_VERSION %s' % PROTO_VERSION).split(BACKEND_SEP)
		if res[0] == 'REJECT':
			log.critical('Backend has version %s and we speak version %s', res[1], PROTO_VERSION)
			sys.exit(1)
		res = self.backendCommand('ANN %s %s 0' % (conn_type, socket.gethostname()))
		if res != 'OK':
			log.critical('Unexpected answer to ANN command: %s', res)

	def backendCommand(self, data):
		"""
		Sends a formatted command via a socket to the mythbackend.

		Returns the result from the backend.
		"""
		def recv():
			"""
			Reads the data returned from the backend.
			"""
			# The first 8 bytes of the response gives us the length
			data = self.socket.recv(8)
			try:
				length = int(data)
			except:
				return ''
			data = []
			while length > 0:
				chunk = self.socket.recv(length)
				length = length - len(chunk)
				data.append(chunk)
			return ''.join(data)
		
		command = '%-8d%s' % (len(data), data)
		log.debug('Sending command: %s' % command)
		self.socket.send(command)
		return recv()

	def getPendingRecordings(self):
		"""
		Returns a list of Program objects which are scheduled to be recorded.
		"""
		programs = []
		res = self.backendCommand('QUERY_GETALLPENDING').split(BACKEND_SEP)
		has_conflict = int(res.pop(0))
		num_progs = int(res.pop(0))
		log.debug('%s pending recordings', num_progs)
		for i in range(num_progs):
			programs.append(
				Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS) + PROGRAM_FIELDS]))
		return programs

	def getScheduledRecordings(self):
		"""
		Returns a list of Program objects which are scheduled to be recorded.
		"""
		programs = []
		res = self.backendCommand('QUERY_GETALLSCHEDULED').split(BACKEND_SEP)
		num_progs = int(res.pop(0))
		log.debug('%s scheduled recordings', num_progs)
		for i in range(num_progs):
			programs.append(
				Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS) + PROGRAM_FIELDS]))
		return programs

	def getUpcomingRecordings(self):
		"""
		Returns a list of Program objects which are scheduled to be recorded.

		Sorts the list by recording start time and only returns those with
		record status of WillRecord.
		"""
		def sort_programs_by_starttime(x, y):
			if x.starttime > y.starttime:
				return 1
			elif x.starttime == y.starttime:
				return 0
			else:
				return -1
		programs = []
		res = self.getPendingRecordings()
		for p in res:
			if p.recstatus == RECSTATUS['WillRecord']:
				programs.append(p)
		programs.sort(sort_programs_by_starttime)
		return programs

	def getRecorderList(self):
		"""
		Returns a list of recorders, or an empty list if none.
		"""
		recorders = []
		c = self.db.cursor()
		c.execute('SELECT cardid FROM capturecard')
		row = c.fetchone()
		while row is not None:
			recorders.append(int(row[0]))
			row = c.fetchone()
		c.close()
		return recorders
	
	def getFreeRecorderList(self):
		"""
		Returns a list of free recorders, or an empty list if none.
		"""
		res = self.backendCommand('GET_FREE_RECORDER_LIST').split(BACKEND_SEP)
		recorders = [int(d) for d in res]
		return recorders

	def getRecorderDetails(self, recorder_id):
		"""
		Returns a Recorder object with details of the recorder.
		"""
		c = self.db.cursor()
		c.execute("""SELECT cardid, cardtype, videodevice, hostname
			FROM capturecard WHERE cardid = %s""", recorder_id)
		row = c.fetchone()
		if row:
			recorder = Recorder(row)
			return recorder
		else:
			return None

	def getCurrentRecording(self, recorder):
		"""
		Returns a Program object for the current recorders recording.
		"""
		res = self.backendCommand('QUERY_RECORDER %s[]:[]GET_CURRENT_RECORDING' % recorder)
		return Program(res.split(BACKEND_SEP))

	def isRecording(self, recorder):
		"""
		Returns a boolean as to whether the given recorder is recording.
		"""
		res = self.backendCommand('QUERY_RECORDER %s[]:[]IS_RECORDING' % recorder)
		if res == '1':
			return True
		else:
			return False

	def isActiveBackend(self, hostname):
		"""
		Returns a boolean as to whether the given host is an active backend
		"""
		res = self.backendCommand('QUERY_IS_ACTIVE_BACKEND[]:[]%s' % hostname)
		if res == 'TRUE':
			return True
		else:
			return False

class MythVideo:
	def __init__(self):
		self.db = MythDB()

	def pruneMetadata(self):
		"""
		Removes metadata from the database for files that no longer exist.
		"""
		c = self.db.cursor()
		c.execute("""
			SELECT intid, filename
			FROM videometadata""")
		
		row = c.fetchone()
		while row is not None:
			intid = row[0]
			filename = row[1]
			if not os.path.exists(filename):
				log.info("%s not exist, removing metadata..." % filename)
				c2 = self.db.cursor()
				c2.execute("""DELETE FROM videometadata WHERE intid = %s""", (intid,))
				c2.close()
			row = c.fetchone()
		c.close()

	def getGenreId(self, genre_name):
		"""
		Find the id of the given genre from MythDB.
		
		If the genre does not exist, insert it and return its id.
		"""
		c = self.db.cursor()
		c.execute("SELECT intid FROM videocategory WHERE lower(category) = %s", (genre_name,))
		row = c.fetchone()
		c.close()
		
		if row is not None:
			return row[0]
		
		# Insert a new genre.
		c = self.db.cursor()
		c.execute("INSERT INTO videocategory(category) VALUES (%s)", (genre_name.capitalize(),))
		newid = c.lastrowid
		c.close()
		
		return newid

	def getMetadataId(self, videopath):
		"""
		Finds the MythVideo metadata id for the given video path from the MythDB, if any.
		
		Returns None if no metadata was found.
		"""
		c = self.db.cursor()
		c.execute("""
			SELECT intid
			FROM videometadata
			WHERE filename = %s""", (videopath,))
		row = c.fetchone()
		c.close()
		
		if row is not None:
			return row[0]
		else:
			return None

	def hasMetadata(self, videopath):
		"""
		Determines if the given videopath has any metadata in the DB
		
		Returns False if no metadata was found.
		"""
		c = self.db.cursor()
		c.execute("""
			SELECT category, year 
			FROM videometadata
			WHERE filename = %s""", (videopath,))
		row = c.fetchone()
		c.close()
		
		if row is not None:
			# If category is 0 and year is 1895, we can safely assume no metadata
			if (row[0] == 0) and (row[1] == 1895):
				return False
			else:
				return True
		else:
			return False
	def getMetadata(self, id):
		"""
		Finds the MythVideo metadata for the given id from the MythDB, if any.
		
		Returns None if no metadata was found.
		"""
		c = self.db.cursor()
		c.execute("""
			SELECT *
			FROM videometadata
			WHERE intid = %s""", (id,))
		row = c.fetchone()
		c.close()
		
		if row is not None:
			return row
		else:
			return None

	def setMetadata(self, data, id=None):
		"""
		Adds or updates the metadata in the database for a video item.
		"""
		c = self.db.cursor()
		if id is None:
			fields = ', '.join(data.keys())
			format_string = ', '.join(['%s' for d in data.values()])
			sql = "INSERT INTO videometadata(%s) VALUES(%s)" % (fields, format_string)
			c.execute(sql, data.values())
			intid = c.lastrowid
			c.close()
			return intid
		else:
			log.debug('Updating metadata for %s' % id)
			format_string = ', '.join(['%s = %%s' % d for d in data])
			sql = "UPDATE videometadata SET %s WHERE intid = %%s" % format_string
			sql_values = data.values()
			sql_values.append(id)
			c.execute(sql, sql_values)
			c.close()

class Recorder:
	def __str__(self):
		return "Recorder %s (%s)" % (self.cardid, self.cardtype)
	
	def __repr__(self):
		return "Recorder %s (%s)" % (self.cardid, self.cardtype)

	def __init__(self, data):
		"""
		Load the list of data into the object.
		"""
		self.cardid = data[0]
		self.cardtype = data[1]
		self.videodevice = data[2]
		self.hostname = data[3]

class Program:
	def __str__(self):
		return "%s (%s)" % (self.title, self.starttime.strftime('%Y-%m-%d %H:%M:%S'))
	
	def __repr__(self):
		return "%s (%s)" % (self.title, self.starttime.strftime('%Y-%m-%d %H:%M:%S'))
	
	def __init__(self, data):
		"""
		Load the list of data into the object.
		"""
		self.title = data[0]
		self.subtitle = data[1]
		self.description = data[2]
		self.category = data[3]
		try:
			self.chanid = int(data[4])
		except ValueError:
			self.chanid = None
		self.channum = data[5] #chanstr
		self.callsign = data[6] #chansign
		self.channame = data[7]
		self.filename = data[8] #pathname
		self.fs_high = data[9]
		self.fs_low = data[10]
		self.starttime = datetime.fromtimestamp(int(data[11])) # startts
		self.endtime = datetime.fromtimestamp(int(data[12])) #endts
		self.duplicate = int(data[13])
		self.shareable = int(data[14])
		self.findid = int(data[15])
		self.hostname = data[16]
		self.sourceid = int(data[17])
		self.cardid = int(data[18])
		self.inputid = int(data[19])
		self.recpriority = int(data[20])
		self.recstatus = int(data[21])
		self.recordid = int(data[22])
		self.rectype = data[23]
		self.dupin = data[24]
		self.dupmethod = data[25]
		self.recstartts = datetime.fromtimestamp(int(data[26]))
		self.recendts = datetime.fromtimestamp(int(data[27]))
		self.repeat = int(data[28])
		self.programflags = data[29]
		self.recgroup = data[30]
		self.commfree = int(data[31])
		self.outputfilters = data[32]
		self.seriesid = data[33]
		self.programid = data[34]
		self.lastmodified = data[35]
		self.stars = float(data[36])
		self.airdate = data[37]
		self.hasairdate = int(data[38])
		self.playgroup = data[39]
		self.recpriority2 = int(data[40])
		self.parentid = data[41]
		self.storagegroup = data[42]
		self.audio_props = data[43]
		self.video_props = data[44]
		self.subtitle_type = data[45]

if __name__ == '__main__':
	banner = "'m' is a MythTV instance."
	try:
		import readline, rlcompleter
	except:
		pass
	else:
		readline.parse_and_bind("tab: complete")
		banner = banner + " TAB completion is available."
	m = MythTV()
	namespace = globals().copy()
	namespace.update(locals())
	code.InteractiveConsole(namespace).interact(banner)
