# -*- coding: utf-8 -*-
# smolt - Fedora hardware profiler
#
# Copyright (C) 2011 Raymond Wagner <raymond@wagnerrp.com>
# Copyright (C) 2011 james meyer <james.meyer@operamail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.


import os
import re
import statvfs
from i18n import _
from datetime import timedelta

from orddict import OrdDict
from uuiddb import UuidDb

import MythTV
from user import home

_DB = MythTV.MythDB()
_BE = MythTV.MythBE(db=_DB)
_SETTINGS = _DB.settings[_DB.gethostname()]
prefix = 'mythtv'

class _Mythtv_data:
    def __init__(self, gate):
        self.get_data(gate)
        
    def isBackend(self):
        return (_SETTINGS.BackendServerIP is not None)

    def ProcessVersion(self):
        data = OrdDict()
        for name in ['version', 'branch', 'protocol', 'libapi', 'qtversion']:
            data[name] = 'Unknown'

        executables = ('mythutil', 'mythbackend', 'mythfrontend')

        for executable in executables:
            execpath = os.path.join(MythTV.static.INSTALL_PREFIX,
                                    'bin', executable)
            try:
                cmd = MythTV.System(execpath)
                res = cmd.command('--version')
                break
            except:
                continue
        else:
            return data

        names = {'MythTV Version'  : 'version',
                'MythTV Branch'   : 'branch',
                'Network Protocol': 'protocol',
                'Library API'     : 'libapi',
                'QT Version'      : 'qtversion'}

        for line in res.split('\n'):
            try:
                prop, val = line.split(':')
                data[names[prop.strip()]] = val.strip()
            except:
                pass
        return data

    def td_to_secs(self,td): return td.days*86400 + td.seconds

    def ProcessPrograms(self):
        rdata = OrdDict()
        edata = OrdDict()
        ldata = OrdDict()
        udata = OrdDict()

        data = {'scheduled': rdata,
                'expireable': edata,
                'livetv': ldata,
                'upcoming': udata}

        progs = list(MythTV.Recorded.getAllEntries())
        upcoming = list(_BE.getUpcomingRecordings())
        livetv = [prog for prog in progs if prog.recgroup == 'LiveTV']
        recs = [prog for prog in progs if prog.recgroup != 'LiveTV']
        expireable = [prog for prog in recs if prog.autoexpire]

        times = []
        for dataset in (recs, expireable, livetv, upcoming):
            if len(dataset):
                try:
                    deltas = [rec.endtime-rec.starttime for rec in dataset]
                except:
                    deltas = [rec.recendts-rec.recstartts for rec in dataset]
                secs = self.td_to_secs(deltas.pop())
                for delta in deltas:
                    secs += self.td_to_secs(delta)
                times.append(secs)
            else:
                times.append(0)

        rdata.count = len(recs)
        rdata.size  = sum([rec.filesize for rec in recs])
        rdata.time  = times[0]

        edata.count = len(expireable)
        edata.size  = sum([rec.filesize for rec in expireable])
        edata.time  = times[1]

        ldata.count = len(livetv)
        ldata.size  = sum([rec.filesize for rec in livetv])
        ldata.time  = times[2]

        udata.count = len(upcoming)
        udata.time  = times[3]

        return {'recordings': data}

    def ProcessHistorical(self):
        data = OrdDict()
        data.db_age     = 0
        data.rectime    = 0
        data.reccount   = 0
        data.showcount  = 0

        recs = list(MythTV.OldRecorded.getAllEntries())
        if len(recs) == 0:
            return data

        oldest = recs[0]
        shows = []

        maxage = MythTV.utility.datetime(2001,1,1,0,0)

        for rec in recs:
            if (rec.starttime < oldest.starttime) and (rec.starttime > maxage):
                oldest = rec
            data.rectime += self.td_to_secs(rec.endtime - rec.starttime)
            if rec.recstatus == -3:
                shows.append(rec.title)
                data.reccount += 1

        data.db_age = self.td_to_secs(MythTV.utility.datetime.now() - oldest.starttime)
        data.showcount = len(set(shows))
        return {'historical': data}

    def ProcessSource(self):
        class CardInput( MythTV.database.DBData ): pass
        class VideoSource( MythTV.database.DBData ): pass

        data = OrdDict()
        usedsources = list(set([inp.sourceid for inp in CardInput.getAllEntries()]))
        grabbers = list(set([VideoSource(id).xmltvgrabber for id in usedsources]))
        data.sourcecount = len(usedsources)
        data.grabbers = grabbers
        return data

    def ProcessTimeZone(self):
        data = OrdDict()
        data.timezone = 'Unknown'
        data.tzoffset = 0

        res = _BE.backendCommand('QUERY_TIME_ZONE').split('[]:[]')
        data.timezone = res[0]
        data.tzoffset = int(res[1])
        return data

    def ProcessStorage(self):
        def processdirs(paths):
            total = 0
            free  = 0

            for path in paths:
                stat = os.statvfs(path)
                bsize = stat[statvfs.F_FRSIZE]
                total += stat[statvfs.F_BLOCKS]*bsize
                free  += stat[statvfs.F_BFREE]*bsize

            return (total, free)

        data = OrdDict()
        data.rectotal = 0
        data.recfree  = 0
        data.videototal     = 0
        data.videofree      = 0

        if not self.isBackend():
            return data

        sgnames  = [rec.storagegroup for rec in MythTV.Recorded.getAllEntries()]
        sgnames += [rec.storagegroup for rec in MythTV.Record.getAllEntries()]
        sgnames  = list(set(sgnames))

        sgs = []
        for host in set([_DB.gethostname(), _BE.hostname]):
            for sgname in sgnames:
                for sg in _DB.getStorageGroup(sgname, host):
                    if sg.local:
                        sgs.append(sg.dirname)
        data.rectotal, data.recfree = processdirs(sgs)

        sgs = [sg.dirname for sg in _DB.getStorageGroup('Videos', _DB.gethostname()) if sg.local]
        data.videototal, data.videofree = processdirs(sgs)

        return {'storage': data}

    def ProcessAudio(self):
        def _bool(val):
            if val is None:
                return False
            return bool(int(val))

        def _read_file(filename):
            firstline=[]
            try:
                with open(filename,'r') as f:
                        line = f.readline()
                        firstline = line.split()
            except:
                pass
            return firstline
             
            
        def _oss_alsa():
            snd_type = "unknown"
            version = "unknown"
            alsasnd = "/proc/asound/version"
            osssnd = "/dev/sndstat"
            
            if os.path.exists(alsasnd):
                snd_type = "ALSA"
                version = _read_file(alsasnd)[-1].rstrip(".")
                
            elif os.path.exists(osssnd):
                version = _read_file(osssnd)[1]
                snd_type = "OSS"
                
            return snd_type , version

        def _process_search(processname):
            foundit = False
            for line in os.popen("ps xa"):
                fields = line.split()
                pid = fields[0]
                process = fields[4].split("/")
                if processname in process :
                    foundit = True
                    break
            return foundit

        def _jack():
            if _process_search("jackd") or _process_search("jackdbus"):
                foundit = 1
            else:
                foundit = 0
            return foundit

        def _pulse():
            if _process_search("pulseaudio"):
                foundit = 1
            else:
                foundit = 0
            return foundit


        data = OrdDict()
        data.device           = _SETTINGS.AudioOutputDevice
        data.passthrudevice   = _SETTINGS.PassThruOutputDevice
        data.passthruoverride = _bool(_SETTINGS.PassThruDeviceOverride)
        data.stereopcm        = _bool(_SETTINGS.StereoPCM)
        data.sr_override      = _bool(_SETTINGS.Audio48kOverride)
        data.maxchannels      = _SETTINGS.MaxChannels
        data.defaultupmix     = _bool(_SETTINGS.AudioDefaultUpmix)
        data.upmixtype        = _SETTINGS.AudioUpmixType
        p = []
        for k,v in (('AC3PassThru', 'ac3'),         ('DTSPassThru', 'dts'),
                    ('HBRPassthru', 'hbr'),         ('EAC3PassThru', 'eac3'),
                    ('TrueHDPassThru', 'truehd'),   ('DTSHDPassThru', 'dtshd')):
            if _bool(_SETTINGS[k]):
                p.append(v)
        data.passthru         = p
        data.volcontrol       = _bool(_SETTINGS.MythControlsVolume)
        data.mixerdevice      = _SETTINGS.MixerDevice
        data.mixercontrol     = _SETTINGS.MixerControl
        data.jack             = _jack()
        data.pulse            = _pulse()
        data.audio_sys, data.audio_sys_version    = _oss_alsa()
        
        return {'audio': data}

    def ProcessVideoProfile(self):
        class DisplayProfileGroups( MythTV.database.DBData ): pass
        class DisplayProfiles( OrdDict ):
            def __new__(cls, *args, **kwargs):
                inst = super(DisplayProfiles, cls).__new__(cls, *args, **kwargs)
                inst.__dict__['_profilegroupid'] = None
                inst.__dict__['_profileid'] = None
                inst.__dict__['_db'] = None
                return inst

            def __init__(self, profilegroupid, profileid, db=None):
                self._db             = MythTV.database.DBCache(db=db)
                self._profilegroupid = profilegroupid
                self._profileid      = profileid
                with db as cursor:
                    cursor.execute("""SELECT value,data FROM displayprofiles
                                    WHERE profilegroupid=%s
                                    AND profileid=%s""",
                                    (profilegroupid, profileid))
                    for k,v in cursor.fetchall():
                        self[k] = v

            @classmethod
            def fromProfileGroup(cls, profilegroupid, db=None):
                db = MythTV.database.DBCache(db=db)
                with db as cursor:
                    cursor.execute("""SELECT DISTINCT(profileid)
                                    FROM displayprofiles
                                    WHERE profilegroupid=%s""",
                                    profilegroupid)
                    for profileid in cursor.fetchall():
                        yield cls(profilegroupid, profileid[0], db)

        data = OrdDict()
        data.name = _SETTINGS.DefaultVideoPlaybackProfile
        data.profiles = []

        profilegroupid = DisplayProfileGroups((data.name, _DB.gethostname()), _DB)\
                                .profilegroupid
        for profile in DisplayProfiles.fromProfileGroup(profilegroupid, _DB):
            d = OrdDict()
            d.decoder   = profile.pref_decoder
            d.deint_pri = profile.pref_deint0
            d.deint_sec = profile.pref_deint1
            d.renderer  = profile.pref_videorenderer
            d.filters   = profile.pref_filters
            data.profiles.append(d)

        return {'playbackprofile':data}

    def ProcessMySQL(self):
        data = OrdDict()

        c = _DB.cursor()
        c.execute("""SELECT VERSION()""")
        data.version = c.fetchone()[0]

        c.execute("""SHOW ENGINES""")
        data.engines = [r[0] for r in c.fetchall()]

        c.execute("""SHOW TABLE STATUS WHERE NAME='settings'""")
        data.usedengine = c.fetchone()[1]

        data.schema = OrdDict()
        c.execute("""SELECT value,data FROM settings
                    WHERE value LIKE '%SchemaVer'""")
        for k,v in c.fetchall():
            data.schema[k] = v

        return {'database':data}

    def ProcessScheduler(self):
        def stddev(data):
            avg = sum(data)/len(data)
            return avg, (sum([(d-avg)**2 for d in data])/len(data))**.5

        data = OrdDict()
        data.count = 0
        data.match_avg = 0
        data.match_stddev = 0
        data.place_avg = 0
        data.place_stddev = 0

        r = re.compile('Scheduled ([0-9]*) items in [0-9.]* = ([0-9.]*) match \+ ([0-9.]*) place')
        data = OrdDict()

        c = _DB.cursor()
        c.execute("""SELECT details FROM mythlog
                    WHERE module='scheduler'
                    AND message='Scheduled items'""")

        runs = [r.match(d[0]).groups() for d in c.fetchall()]

        if len(runs) == 0:
            return {'scheduler': data}

        a,s = stddev([float(r[2]) for r in runs])
        for i,r in reversed(list(enumerate(runs))):
            if abs(float(r[2]) - a) > (3*s):
                runs.pop(i)

        data = OrdDict()

        count = [float(r[0]) for r in runs]
        match = [float(r[1]) for r in runs]
        place = [float(r[2]) for r in runs]

        data.count = int(sum(count)/len(count))
        data.match_avg, data.match_stddev = stddev(match)
        data.place_avg, data.place_stddev = stddev(place)

        return {'scheduler': data}


    def Processtuners(self):
        class CaptureCard( MythTV.database.DBData ): pass

        cardtypes = {}
        virtual = [0,0]
        virtualtypes = ('DVB', 'HDHOMERUN', 'ASI')

        for card in CaptureCard.getAllEntries(db=_DB):
            isvirt = (card.cardtype in virtualtypes)
            loc = card.videodevice+'@'+card.hostname
            if card.cardtype not in cardtypes:
                cardtypes[card.cardtype] = [loc]
                virtual[0] += isvirt
            else:
                if loc not in cardtypes[card.cardtype]:
                    cardtypes[card.cardtype].append(loc)
                    virtual[0] += isvirt
                else:
                    virtual[1] += isvirt

        data = {'tuners':dict([(k,len(v)) for k,v in cardtypes.items()])}
        if virtual[0]:
            data['vtpertuner'] = sum(virtual)/float(virtual[0])
        return data

    def ProcessSmoltInfo(self):
        smoltfile=home+"/.mythtv/smolt.info"
        config = {}
        try:
            config_file= open(smoltfile)
            for line in config_file:
                line = line.strip()
                if line and line[0] is not "#" and line[-1] is not "=":
                    var,val = line.rsplit("=",1)
                    config[var.strip()] = val.strip("\"")
        except:
            pass

        try:
            myth_systemrole = config["systemtype" ]
        except:
            myth_systemrole = '0'

        try:
            mythremote  = config["remote" ]
        except:
            mythremote = 'unknown'


        return myth_systemrole , mythremote

    def ProcessLogUrgency(self):
        c = _DB.cursor()
        c.execute("""SELECT level,count(level) FROM logging GROUP BY level""")
        levels = ('EMERG', 'ALERT', 'CRIT', 'ERR', 
                  'WARNING', 'NOTICE', 'INFO') # ignore debugging from totals
        counts = {}
        total = 0.
        for level,count in c.fetchall():
            if level in range(len(levels)):
                counts[levels[level]] = count
                total += count
        for k,v in counts.items():
            counts[k] = v/total
        return {'logurgency':counts}

    def get_data(self,gate):
        self._data = OrdDict()
        for func in (self.ProcessVersion,
                     self.ProcessPrograms,
                     self.ProcessHistorical,
                     self.ProcessSource,
                     self.ProcessTimeZone,
                     self.ProcessStorage,
                     self.ProcessAudio,
                     self.ProcessVideoProfile,
                     self.ProcessMySQL,
                     self.ProcessScheduler,
                     self.Processtuners,
                     self.ProcessLogUrgency):
            try:
                self._data.update(func())
            except:
                pass
        
        self._data.theme          = _SETTINGS.Theme
        self._data.country          = _SETTINGS.Country
        self._data.channel_count  = len([c for c in MythTV.Channel.getAllEntries() if c.visible])
        self._data.language       = _SETTINGS.Language.lower()
        self._data.mythtype, self._data.remote = self.ProcessSmoltInfo()

        if _DB.settings.NULL.SystemUUID is None:
            _DB.settings.NULL.SystemUUID = UuidDb().gen_uuid()
        self._data.uuid           = _DB.settings.NULL.SystemUUID

        
        

    def serialize(self):
        res = self._data
        return res

    def _dump_rst_section(self, lines, title, data, line_break=True):
        lines.append(title)
        for k,v in sorted(data.items()):
            lines.append('- %s:\n      %s \n' %(k,v))
            
        if line_break:
            lines.append('')

    def dump_rst(self, lines):
        serialized = self.serialize()
        lines.append('MythTV Features')
        lines.append('-----------------------------')
        self._dump_rst_section(lines, '', serialized)


    def _dump(self):
        lines = []
        self.dump_rst(lines)
        print '\n'.join(lines)
        print


def create_mythtv_data(gate):
    return _Mythtv_data(gate)
    


if __name__ == '__main__':
    import pprint
    pp = pprint.PrettyPrinter()
    pp.pprint(get_data())
