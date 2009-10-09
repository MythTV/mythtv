#!/usr/bin/env python
from MythTV import MythTV, MythError
from curses import wrapper, ascii
from time   import sleep
import sys, socket, curses, re

#note for ticket, on-screen-keyboard remotely does not have focus

myth = MythTV()
frontend = None

keys = {		9:'tab',		10:'enter',	
27:'escape',		32:'space',		35:'poundsign', 
36:'dollar',		37:'percent',		38:'ampersand',
40:'parenleft',		41:'parenright',	42:'asterisk',
43:'plus',		44:'comma',		45:'minus',
46:'period',		47:'/',			58:'colon',
59:'semicolon',		60:'less',		61:'equal',
62:'greater',		63:'question',		91:'bracketleft',
92:'backslash',		93:'bracketright',	95:'underscore',
124:'pipe',		127:'backspace',	258:'down',
259:'up',		260:'left',		261:'right',
269:'f5',		270:'f6',		271:'f7',
272:'f8',		273:'f9',		274:'f10',
275:'f11',		276:'f12',		330:'delete',
331:'insert',		338:'pagedown',		339:'pageup'}

rplaytype = re.compile('Playback ([a-zA-Z]+)')
rrecorded = re.compile('Playback ([a-zA-Z]+) ([\d:]+) of ([\d:]+) ([-0-9\.]+x) (\d*) ([0-9-T:]+)')
rlivetv   = rrecorded
rvideo    = re.compile('Playback [a-zA-Z]+ ([\d:]+) ([-0-9\.]+x) .*/(.*) \d+ [\.\d]+')
rrname    = re.compile('\d+ [0-9-T:]+ (.*)')
rlname    = re.compile(' \d+ [0-9-T: ]+ (.*)')

def align(side, window, y, string, flush=0):
	w = window.getmaxyx()[1]-1
	if len(string) > w:
		string = string[:w]
	if side == 0:
		x = 1
	elif side == 1:
		x = (w-len(string))/2+1
	elif side == 2:
		x = w-len(string)
	window.addstr(y,x,string)

def query_time(w):
	time = frontend.sendQuery('time').split('T')[1]
	w.erase()
	w.border()
	align(1,w,1,time)
	w.noutrefresh()

def query_load(w):
	loads = frontend.sendQuery('load').split(' ')
	w.erase()
	w.border()
	align(0,w,2,'loads')
	align(2,w,1,'1:     ')
	align(2,w,2,'5:     ')
	align(2,w,3,'15:     ')
	align(2,w,1,loads[0],5)
	align(2,w,2,loads[1],5)
	align(2,w,3,loads[2],5)
	w.noutrefresh()

def query_loc(w):
	loc = frontend.sendQuery('location')
	pb = rplaytype.match(loc)
	w.erase()
	w.border()
	if pb:
		if pb.group(1) == 'Video':
			pb = rvideo.match(loc)
			align(0,w,1,'  Playback: %s' % pb.group(3))
			align(0,w,2,'     %s @ %s' % (pb.group(1),pb.group(2)))
		else:
			pb = rrecorded.match(loc)
			if pb.group(1) == 'Recorded':
				show = frontend.sendQuery('recording %s %s' % (pb.group(5),pb.group(6)))
				name = rrname.match(show).group(1)
				align(0,w,1,'  Playback: %s' % name)
				
			elif pb.group(1) == 'LiveTV':
				show = frontend.sendQuery('liveTV %s' % pb.group(5))
				name = rlname.match(show).group(1)
				align(0,w,1,'  LiveTV: %s - %s' % (pb.group(5),name))
			align(0,w,2,'      %s of %s @ %s' % (pb.group(2),pb.group(3),pb.group(4)))
	else:
		align(0,w,1,'  '+loc)
	w.noutrefresh()

def query_mem(w):
	mem = frontend.sendQuery('memstats').split(' ')
	w.erase()
	w.border()
	align(0,w,1,'phy:')
	align(0,w,2,'swp:')
	align(2,w,1,"%sM/%sM" % (mem[1],mem[0]))
	align(2,w,2,"%sM/%sM" % (mem[3],mem[2]))
	w.noutrefresh()

def main(w):
	curses.halfdelay(10)
	frontend.connect()
	y,x = w.getmaxyx()

	mem = w.derwin(4,20,9,0)
	query_mem(mem)

	load = w.derwin(5,20,5,0)
	query_load(load)

	conn = w.derwin(4,20,2,0)
	align(2,conn,1,frontend.host)
	align(2,conn,2,"%s:%d" % frontend.socket.getpeername())
	conn.border()

	time = w.derwin(3,20,0,0)
	query_time(time)

	loc = w.derwin(13,x-20,0,19)
	loc.timeout(1)
	while True:
		a = None
		s = None
		try:
			query_time(time)
			query_load(load)
			query_loc(loc)
			query_mem(mem)
			align(1,w,0,' MythFrontend Remote Socket Interface ')
			curses.doupdate()

			a = w.getch()
			curses.flushinp()
			if a == -1:
				continue
			if a in keys:
				s = keys[a]
			elif ascii.isalnum(a):
				s = chr(a)
			else:
				continue
			frontend.sendKey(s)
		except KeyboardInterrupt:
			break
		except MythError:
			print "Remote side closed connection..."
			break
		except:
			raise







if __name__ == '__main__':
	frontends = None
	if len(sys.argv) == 2:
		try:
			frontend = myth.getFrontend(sys.argv[1])
		except socket.timeout:
			print "Could not connect to "+sys.argv[1]
			pass
		except TypeError:
			print sys.argv[1]+" does not exist"
			pass
		except KeyboardInterrupt:
			sys.exit()
		except:
			raise
	while frontend is None:
		if frontends is None:
			frontends = myth.getFrontends()
		print "Please choose from the following available frontends:"
		for i in range(0,len(frontends)):
			print "%d. %s" % (i+1, frontends[i])
		try:
			i = int(raw_input('> '))-1
			if i in range(0,len(frontends)):
				frontend = frontends[i]
		except KeyboardInterrupt:
			sys.exit()
		except:
			raise
	wrapper(main)
