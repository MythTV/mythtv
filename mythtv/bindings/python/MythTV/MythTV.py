#!/usr/bin/python

"""
Provides classes for connecting to a MythTV backend.

The MythTV class to handle connection to and querying of a MythTV backend.
The Recorder class representing and storing details of a tuner card.
The Program class for storing details of a TV program.
"""
import os
import sys
import socket
import shlex
import socket
import code
import re
from datetime import datetime
from time import mktime

from MythDB import *
from MythLog import *

log = MythLog(CRITICAL, '#%(levelname)s - %(message)s', 'MythTV')

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
PROTO_VERSION = 47
PROGRAM_FIELDS = 47

class MythTV:
	"""
	A connection to a MythTV backend.
	"""
	def __init__(self, conn_type='Monitor'):
		self.db = MythDB(sys.argv[1:])
		self.master_host = self.db.getSetting('MasterServerIP')
		self.master_port = int(self.db.getSetting('MasterServerPort'))

		if not self.master_host:
			log.Msg(CRITICAL, 'Unable to find MasterServerIP in database')
			sys.exit(1)
		if not self.master_port:
			log.Msg(CRITICAL, 'Unable to find MasterServerPort in database')
			sys.exit(1)

		try:
			self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			self.socket.settimeout(10)
			self.socket.connect((self.master_host, self.master_port))
			res = self.backendCommand('MYTH_PROTO_VERSION %s' % PROTO_VERSION).split(BACKEND_SEP)
			if res[0] == 'REJECT':
				log.Msg(CRITICAL, 'Backend has version %s and we speak version %s', res[1], PROTO_VERSION)
				sys.exit(1)
			res = self.backendCommand('ANN %s %s 0' % (conn_type, socket.gethostname()))
			if res != 'OK':
				log.Msg(CRITICAL, 'Unexpected answer to ANN command: %s', res)
			else:
				log.Msg(INFO, 'Successfully connected mythbackend at %s:%d', self.master_host, self.master_port)
		except socket.error, e:
			log.Msg(CRITICAL, 'Couldn\'t connect to %s:%d (is the backend running)', self.master_host, self.master_port)
			sys.exit(1)

	def __del__(self):
		self.backendCommand('DONE')
		self.socket.shutdown(1)
		self.socket.close()

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
				return u''
			data = []
			while length > 0:
				chunk = self.socket.recv(length)
				length = length - len(chunk)
				data.append(chunk)
			try:
				return unicode(''.join(data),'utf8')
			except:
				return u''.join(data)

		command = u'%-8d%s' % (len(data), data)
		log.Msg(DEBUG, 'Sending command: %s', command)
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
		log.Msg(DEBUG, '%s pending recordings', num_progs)
		for i in range(num_progs):
			programs.append(Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS)
				+ PROGRAM_FIELDS]))
		return tuple(programs)

	def getScheduledRecordings(self):
		"""
		Returns a list of Program objects which are scheduled to be recorded.
		"""
		programs = []
		res = self.backendCommand('QUERY_GETALLSCHEDULED').split(BACKEND_SEP)
		num_progs = int(res.pop(0))
		log.Msg(DEBUG, '%s scheduled recordings', num_progs)
		for i in range(num_progs):
			programs.append(Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS)
				+ PROGRAM_FIELDS]))
		return tuple(programs)

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
		return tuple(programs)

	def getRecorderList(self):
		"""
		Returns a list of recorders, or an empty list if none.
		"""
		recorders = []
		pc = self.db.cursor()
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

	def getRecording(self, chanid, starttime):
		"""
		Returns a Program object matching the channel id and start time
		"""
		res = self.backendCommand('QUERY_RECORDING TIMESLOT %d %d' % (chanid, starttime)).split(BACKEND_SEP)
		if res[0] == 'ERROR':
			return None
		else:
			return Program(res[1:])

	def getRecordings(self):
		"""
		Returns a list of all Program objects which have already recorded
		"""
		programs = []
		res = self.backendCommand('QUERY_RECORDINGS Play').split('[]:[]')
		num_progs = int(res.pop(0))
		log.Msg(DEBUG, '%s total recordings', num_progs)
		for i in range(num_progs):
			programs.append(Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS)
				+ PROGRAM_FIELDS]))
		return tuple(programs)

	def getCheckfile(self,program):
		"""
		Returns location of recording in file system
		"""
		res = self.backendCommand('QUERY_CHECKFILE[]:[]1[]:[]%s' % program.toString()).split(BACKEND_SEP)
		if res[0] == 0:
			return None
		else:
			return res[1]

	def deleteRecording(self,program,force=False):
		"""
		Deletes recording, set 'force' true if file is not available for deletion
		Returns the number fewer recordings that exist afterwards
		"""
		command = 'DELETE_RECORDING'
		if force:
			command = 'FORCE_DELETE_RECORDING'
		return self.backendCommand('%s%s%s' % (command,BACKEND_SEP,program.toString()))

	def forgetRecording(self,program):
		"""
		Forgets old recording and allows it to be re-recorded
		"""
		self.backendCommand('FORGET_RECORDING%s%s' % (BACKEND_SEP,program.toString()))

	def deleteFile(self,file,sgroup):
		"""
		Deletes a file from specified storage group on the connected backend
		Takes a relative file path from the root of the storage group, and returns 1 on success
		"""
		return self.backendCommand('DELETE_FILE%s%s%s%s' % (BACKEND_SEP,file,BACKEND_SEP,sgroup))

	def getFreeSpace(self,all=False):
		"""
		Returns a tuple of tuples, in the form:
			str   hostname
			str   path
			bool  is_local
			int   drive number
			int   storage group ID
			int   total space (in KB)
			int   used space (in KB)
		"""
		command = 'QUERY_FREE_SPACE'
		if all:
			command = 'QUERY_FREE_SPACE_LIST'
		res = self.backendCommand(command).split(BACKEND_SEP)
		dirs = []
		for i in range(0,len(res)/9):
			line = [res[i*9]]
			line.append(res[i*9+1])
			line.append(bool(int(res[i*9+2])))
			line.append(int(res[i*9+3]))
			line.append(int(res[i*9+4]))
			line.append(self.joinInt(int(res[i*9+5]),int(res[i*9+6])))
			line.append(self.joinInt(int(res[i*9+7]),int(res[i*9+8])))
			dirs.append(tuple(line))
		return tuple(dirs)

	def getFreeSpaceSummary(self):
		"""
		Returns a tuple of total space (in KB) and used space (in KB)
		"""
		res = self.backendCommand('QUERY_FREE_SPACE_SUMMARY').split(BACKEND_SEP)
		return (self.joinInt(int(res[0]),int(res[1])),self.joinInt(int(res[2]),int(res[3])))

	def getLoad(self):
		"""
		Returns a tuple of the 1, 5, and 15 minute load averages
		"""
		res = self.backendCommand('QUERY_LOAD').split(BACKEND_SEP)
		return (float(res[0]),float(res[1]),float(res[2]))

	def getSGList(self,host,sg,path):
		"""
		Returns a tuple of directories and files
		"""
		res = self.backendCommand('QUERY_SG_GETFILELIST%s%s%s%s%s%s' % (BACKEND_SEP,host,BACKEND_SEP,sg,BACKEND_SEP,path)).split(BACKEND_SEP)
		if res[0] == 'EMPTY LIST':
			return -1
		if res[0] == 'SLAVE UNREACHABLE: ':
			return -2
		dirs = []
		files = []
		for entry in res:
			type,name = entry.split('::')
			if type == 'file':
				files.append(name)
			if type == 'dir':
				dirs.append(name)
		return (tuple(dirs),tuple(files))
		
	def getSGFile(self,host,sg,path):
		"""
		Returns a tuple of last modification time and file size
		"""
		res = self.backendCommand('QUERY_SG_FILEQUERY%s%s%s%s%s%s' % (BACKEND_SEP,host,BACKEND_SEP,sg,BACKEND_SEP,path)).split(BACKEND_SEP)
		if res[0] == 'EMPTY LIST':
			return -1
		if res[0] == 'SLAVE UNREACHABLE: ':
			return -2
		return tuple(res[1:3])

	def getFrontends(self):
		"""
		Returns a list of Frontend objects for accessible frontends
		"""
		cursor = self.db.db.cursor()
		cursor.execute("SELECT DISTINCT hostname FROM settings WHERE hostname IS NOT NULL and value='NetworkControlEnabled' and data=1")
		frontends = []
		for fehost in cursor.fetchall():
			try:
				frontend = self.getFrontend(fehost[0])
				frontends.append(frontend)
			except:
				print "%s is not a valid frontend" % fehost[0]
		cursor.close()
		return frontends

	def getFrontend(self,host):
		"""
		Returns a Frontend object for the specified host
		"""
		port = self.db.getSetting("NetworkControlPort",host)
		return Frontend(host,port)

	def joinInt(self,high,low):
		"""
		Returns a single long from a pair of signed integers
		"""
		return (high + (low<0))*2**32 + low

	def splitInt(self,integer):
		"""
		Returns a pair of signed integers from a single long
		"""
		return integer/(2**32),integer%2**32 - (integer%2**32 > 2**31)*2**32

class FileTransfer:
	"""
	A connection to mythbackend intended for file transfers
	"""
	sockno = None
	mode = None
	datsock = None
	tsize = 2**15
	write = False

	def __init__(self, file, mode):
		regex = re.compile('myth://((?P<group>.*)@)?(?P<host>[0-9\.]*)(:(?P<port>[0-9]*))?/(?P<file>.*)')
		self.db = MythDB(sys.argv[1:])
		self.comsock = MythTV()
		self.mode = mode
		if mode == 'r':
			write = 0
		elif mode == 'w':
			write = 1
		else:
			log.Msg(CRITICAL, 'Invalid FileTransfer mode given')
			sys.exit(1)
		if isinstance(file, Program):
			match = regex.match(file.filename)
			self.host = match.group('host')
			self.port = int(match.group('port'))
			self.filename = match.group('file')
			self.sgroup = file.storagegroup
			if self.port is None:
				self.port = 6543
		elif isinstance(file, tuple):
			if len(file) != 3:
				log.Msg(CRITICAL, 'Incorrect FileTransfer() input size')
				sys.exit(1)
			else:
				self.host = file[0]
				self.port = int(self.db.getSetting('BackendServerPort',self.host))
				self.filename = file[1]
				self.sgroup = file[2]
		elif isinstance(file, str):
			match = regex.match(file)
			if match is None:
				log.Msg(CRITICAL, 'Incorrect FileTransfer() input string: %s' % file)
				sys.exit(1)
			self.sgroup = match.group('group')
			self.host = match.group('host')
			self.port = int(match.group('port'))
			self.filename = match.group('file')
			if self.sgroup is None:
				self.sgroup = ''
			if self.port is None:
				self.port = 6543
		else:
			log.Msg(CRITICAL, 'Improper input to FileTransfer()')
			sys.exit(1)

		try:
			self.datsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			self.datsock.settimeout(10)
			self.datsock.connect((self.host, self.port))
			res = self.send('MYTH_PROTO_VERSION %s' % PROTO_VERSION).split(BACKEND_SEP)
			if res[0] == 'REJECT':
				log.Msg(CRITICAL, 'Backend has version %s and we speak version %s', res[1], PROTO_VERSION)
				sys.exit(1)
			res = self.send('ANN FileTransfer %s %d %d %d%s%s%s%s' % (socket.gethostname(), write, False, -1, BACKEND_SEP, self.filename, BACKEND_SEP, self.sgroup))
			if res.split(BACKEND_SEP)[0] != 'OK':
				log.Msg(CRITICAL, 'Unexpected answer to ANN command: %s', res)
			else:
				log.Msg(INFO, 'Successfully connected mythbackend at %s:%d', self.host, self.port)
				sp = res.split(BACKEND_SEP)
				self.sockno = int(sp[1])
				self.pos = 0
				self.size = (int(sp[2]) + (int(sp[3])<0))*2**32 + int(sp[3])
				
		except socket.error, e:
			log.Msg(CRITICAL, 'Couldn\'t connect to %s:%d (is the backend running)', self.host, self.port)
			sys.exit(1)

	def __del__(self):
		if self.sockno:
			self.comsock.backendCommand('QUERY_FILETRANSFER %d%sDONE' % (self.sockno, BACKEND_SEP))
		if self.datsock:
			self.datsock.shutdown(1)
			self.datsock.close()

	def send(self,data):
		command = '%-8d%s' % (len(data), data)
		log.Msg(DEBUG, 'Sending command: %s', command)
		self.datsock.send(command)
		return self.recv()

	def recv(self):
		data = self.datsock.recv(8)
		try:
			length = int(data)
		except:
			return ''
		data = []
		while length > 0:
			chunk = self.datsock.recv(length)
			length = length - len(chunk)
			data.append(chunk)
		return ''.join(data)

	def tell(self):
		"""
		Return the current offset from the beginning of the file
		"""
		return self.pos

	def close(self):
		"""
		Close the data transfer socket
		"""
		self.__del__()

	def rewind(self):
		"""
		Seek back to the start of the file
		"""
		self.seek(0)

	def read(self, size):
		"""
		Read a block of data, requests over 64KB will be buffered internally
		"""
		if self.mode != 'r':
			print 'Error: attempting to read from a write-only socket'
			return
		if size == 0:
			return ''
		if size > self.size - self.pos:
			size = self.size - self.pos
		csize = size
		rsize = 0
		if csize > self.tsize:
			csize = self.tsize
			rsize = size - csize
			
		res = self.comsock.backendCommand('QUERY_FILETRANSFER %d%sREQUEST_BLOCK%s%d' % (self.sockno,BACKEND_SEP,BACKEND_SEP,csize))
		self.pos += int(res)
#		if int(res) == csize:
#			if csize < size:
#				self.tsize += 8192
#		else:
#			self.tsize -= 8192
#			rsize = size - int(res)
#		print 'resizing buffer to %d' % self.tsize

		return self.datsock.recv(int(res)) + self.read(rsize)

	def write(self, data):
		"""
		Write a block of data, requests over 64KB will be buffered internally
		"""
		if self.mode != 'w':
			print 'Error: attempting to write to a read-only socket'
			return
		size = len(data)
		buff = ''
		if size == 0:
			return
		if size > self.tsize:
			size = self.tsize
			buff = data[size:]
			data = data[:size]
		self.pos += int(self.datsock.send(data))
		self.comsock.backendCommand('QUERY_FILETRANSFER %d%sWRITE_BLOCK%s%d' % (self.sockno,BACKEND_SEP,BACKEND_SEP,size))
		self.write(buff)
		return
			

	def seek(self, offset, whence=0):
		"""
		Seek 'offset' number of bytes
		   whence==0 - from start of file
                   whence==1 - from current position
		"""
		if whence == 0:
			if offset < 0:
				offset = 0
			if offset > self.size:
				offset = self.size
		elif whence == 1:
			if offset + self.pos < 0:
				offset = -self.pos
			if offset + self.pos > self.size:
				offset = self.size - self.pos
		elif whence == 2:
			if offset > 0:
				offset = 0
			if offset < -self.size:
				offset = -self.size
		else:
			log.Msg(CRITICAL, 'Whence can only be 0, 1, or 2')

		curhigh,curlow = self.comsock.splitInt(self.pos)
		offhigh,offlow = self.comsock.splitInt(offset)

		res = self.comsock.backendCommand('QUERY_FILETRANSFER %d%sSEEK%s%d%s%d%s%d%s%d%s%d' % (self.sockno, BACKEND_SEP,BACKEND_SEP,offhigh,BACKEND_SEP,offlow,BACKEND_SEP,whence,BACKEND_SEP,curhigh,BACKEND_SEP,curlow)).split(BACKEND_SEP)
		self.pos = (int(res[0]) + (int(res[1])<0))*2**32 + int(res[1])


class Frontend:
	isConnected = False
	socket = None
	host = None
	port = None

	def __init__(self, host, port):
		self.host = host
		self.port = int(port)
		self.connect()
		self.disconnect()

	def __del__(self):
		if self.isConnected:
			self.disconnect()

	def __repr__(self):
		return "%s@%d" % (self.host, self.port)

	def __str__(self):
		return "%s@%d" % (self.host, self.port)

	def connect(self):
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.socket.settimeout(10)
		self.socket.connect((self.host, self.port))
		if self.recv()[:28] != "MythFrontend Network Control":
			self.socket.close()
			self.socket = None
			raise Exception('FrontendConnect','Connected socket does not belong to a mythfrontend')
		self.isConnected = True

	def disconnect(self):
		self.send("exit")
		self.socket.close()
		self.socket = None
		self.isConnected = False

	def send(self,command):
		if not self.isConnected:
			self.connect()
		self.socket.send("%s\n" % command)

	def recv(self,curstr=""):
		def subrecv(self,curstr=""):
			try:
				curstr += self.socket.recv(100)
			except:
				return None
			if curstr[-4:] != '\r\n# ':
				curstr = subrecv(self,curstr)
			return curstr
		return subrecv(self)[:-4]

	def sendJump(self,jumppoint):
		"""
		Sends jumppoint to frontend
		"""
		self.send("jump %s" % jumppoint)
		if self.recv() == 'OK':
			return 0
		else:
			return 1

	def getJump(self):
		"""
		Returns a tuple containing available jumppoints
		"""
		self.send("help jump")
		res = self.recv().split('\r\n')[3:-1]
		points = []
		for point in res:
			spoint = point.split(' - ')
			points.append((spoint[0].rstrip(),spoint[1]))
		return tuple(points)

	def sendKey(self,key):
		"""
		Sends keycode to connected frontend
		"""
		self.send("key %s" % key)
		if self.recv() == 'OK':
			return 0
		else:
			return 1

	def getKey(self):
		"""
		Returns a tuple containing available special keys
		"""
		self.send("help key")
		res = self.recv().split('\r\n')[4]
		keys = []
		for key in res.split(','):
			keys.append(key.strip())
		return tuple(keys)

	def sendQuery(self,query):
		"""
		Returns query from connected frontend
		"""
		self.send("query %s" % query)
		return self.recv()

	def getQuery(self):
		"""
		Returns a tuple containing available queries
		"""
		self.send("help query")
		res = self.recv().split('\r\n')[:-1]
		queries = []
		tmpstr = ""
		for query in res:
			tmpstr += query
			squery = tmpstr.split(' - ')
			if len(squery) == 2:
				tmpstr = ""
				queries.append((squery[0].rstrip().lstrip('query '),squery[1]))
		return tuple(queries)

	def sendPlay(self,play):
		"""
		Send playback command to connected frontend
		"""
		self.send("play %s" % play)
		if self.recv() == 'OK':
			return 0
		else:
			return 1

	def getPlay(self):
		"""
		Returns a tuple containing available playback commands
		"""
		self.send("help play")
		res = self.recv().split('\r\n')[:-1]
		plays = []
		tmpstr = ""
		for play in res:
			tmpstr += play
			splay = tmpstr.split(' - ')
			if len(splay) == 2:
				tmpstr = ""
				plays.append((splay[0].rstrip().lstrip('play '),splay[1]))
		return tuple(plays)
			
		

class Recorder:
	"""
	Represents a MythTV capture card.
	"""
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
	"""
	Represents a program with all the detail known.
	"""
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
		self.fs_high = int(data[9])
		self.fs_low = int(data[10])
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
		self.year = data[46]
		
		self.filesize = (self.fs_high + (self.fs_low<0))*2**32 + self.fs_low

	def toString(self):
		string =            self.title
		string += BACKEND_SEP + self.subtitle
		string += BACKEND_SEP + self.description
		string += BACKEND_SEP + self.category
		if self.chanid:
			string += BACKEND_SEP + str(self.chanid)
		else:
			string += BACKEND_SEP
		string += BACKEND_SEP + self.channum
		string += BACKEND_SEP + self.callsign
		string += BACKEND_SEP + self.channame
		string += BACKEND_SEP + self.filename
		string += BACKEND_SEP + str(self.fs_high)
		string += BACKEND_SEP + str(self.fs_low)
		string += BACKEND_SEP + str(int(mktime(self.starttime.timetuple())))
		string += BACKEND_SEP + str(int(mktime(self.endtime.timetuple())))
		string += BACKEND_SEP + str(self.duplicate)
		string += BACKEND_SEP + str(self.shareable)
		string += BACKEND_SEP + str(self.findid)
		string += BACKEND_SEP + self.hostname
		string += BACKEND_SEP + str(self.sourceid)
		string += BACKEND_SEP + str(self.cardid)
		string += BACKEND_SEP + str(self.inputid)
		string += BACKEND_SEP + str(self.recpriority)
		string += BACKEND_SEP + str(self.recstatus)
		string += BACKEND_SEP + str(self.recordid)
		string += BACKEND_SEP + self.rectype
		string += BACKEND_SEP + self.dupin
		string += BACKEND_SEP + self.dupmethod
		string += BACKEND_SEP + str(int(mktime(self.recstartts.timetuple())))
		string += BACKEND_SEP + str(int(mktime(self.recendts.timetuple())))
		string += BACKEND_SEP + str(self.repeat)
		string += BACKEND_SEP + self.programflags
		string += BACKEND_SEP + self.recgroup
		string += BACKEND_SEP + str(self.commfree)
		string += BACKEND_SEP + self.outputfilters
		string += BACKEND_SEP + self.seriesid
		string += BACKEND_SEP + self.programid
		string += BACKEND_SEP + self.lastmodified
		string += BACKEND_SEP + str(self.stars)
		string += BACKEND_SEP + self.airdate
		string += BACKEND_SEP + str(self.hasairdate)
		string += BACKEND_SEP + self.playgroup
		string += BACKEND_SEP + str(self.recpriority2)
		string += BACKEND_SEP + self.parentid
		string += BACKEND_SEP + self.storagegroup
		string += BACKEND_SEP + self.audio_props
		string += BACKEND_SEP + self.video_props
		string += BACKEND_SEP + self.subtitle_type
		string += BACKEND_SEP + self.year

		return string

if __name__ == '__main__':
	banner = '\'m\' is a MythTV instance.'
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

# vim: ts=4 sw=4:
