# -*- coding: utf-8 -*-

"""
Provides data access classes for accessing and managing MythTV data
"""

from MythTV.static import *
from MythTV.exceptions import *
from MythTV.altdict import DictData, DictInvertCI
from MythTV.database import *
from MythTV.system import Grabber, InternetMetadata, VideoMetadata
from MythTV.mythproto import ftopen, FileOps, Program
from MythTV.utility import CMPRecord, CMPVideo, MARKUPLIST, datetime, ParseSet

import re
import locale
import xml.etree.cElementTree as etree
from datetime import date, time

from UserString import MutableString
class Artwork( MutableString ):
    _types = {'coverart':   'Coverart',
              'coverfile':  'Coverart',
              'fanart':     'Fanart',
              'banner':     'Banners',
              'screenshot': 'ScreenShots',
              'trailer':    'Trailers'}

    @property
    def data(self):
        try:
            return self.parent[self.attr]
        except:
            raise RuntimeError("Artwork property must be used through an " +\
                               "object, not independently.")
    @data.setter
    def data(self, value):
        try:
            self.parent[self.attr] = value
        except:
            raise RuntimeError("Artwork property must be used through an " +\
                               "object, not independently.")
    @data.deleter
    def data(self):
        try:
            self.parent[self.attr] = self.parent._defaults.get(self.attr, "")
        except:
            raise RuntimeError("Artwork property must be used through an " +\
                               "object, not independently.")

    def __init__(self, attr, parent=None, imagetype=None):
        # replace standard MutableString init to not overwrite self.data
        from warnings import warnpy3k
        warnpy3k('the class UserString.MutableString has been removed in '
                    'Python 3.0', stacklevel=2)

        self.attr = attr
        if imagetype is None:
            imagetype = self._types[attr]
        self.imagetype = imagetype
        self.parent = parent

        if parent:
            self.hostname = parent.get('hostname', parent.get('host', None))

    def __repr__(self):
        return u"<{0.imagetype} '{0.data}'>".format(self)

    def __get__(self, inst, owner):
        if inst is None:
            return self
        return Artwork(self.attr, inst, self.imagetype)

    def __set__(self, inst, value):
        inst[self.attr] = value

    def __delete__(self, inst):
        inst[self.attr] = inst._defaults.get(self.attr, "")

    @property
    def exists(self):
        be = FileOps(self.hostname, db = self.parent._db)
        return be.fileExists(unicode(self), self.imagetype)

    def downloadFrom(self, url):
        if self.parent is None:
            raise RuntimeError("Artwork.downloadFrom must be called from "+\
                               "object, not class.")
        be = FileOps(self.hostname, db=self.parent._db)
        be.downloadTo(url, self.imagetype, self)

    def open(self, mode='r'):
        return ftopen('myth://{0.imagetype}@{0.hostname}/{0}'.format(self), mode)

class Record( CMPRecord, DBDataWrite, RECTYPE ):
    """
    Record(id=None, db=None) -> Record object
    """
    _defaults = {'type':RECTYPE.kAllRecord,
                 'title':u'Unknown', 'subtitle':'',      'description':'',
                 'category':'',      'station':'',       'seriesid':'',
                 'search':0,         'last_record':datetime(1900,1,1),
                 'inetref':'',       'next_record':datetime(1900,1,1),
                 'season':0,         'last_delete':datetime(1900,1,1),
                 'episode':0}
    _artwork = None

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Record Rule at %s>" % hex(id(self))
        return u"<Record Rule '%s', Type %d at %s>" \
                                    % (self.title, self.type, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def create(self, data=None, wait=False):
        """Record.create(data=None) -> Record object"""
        DBDataWrite._create_autoincrement(self, data)
        FileOps(db=self._db).reschedule(self.recordid, wait)
        return self

    def delete(self, wait=False):
        DBDataWrite.delete(self)
        FileOps(db=self._db).reschedule(self.recordid, wait)

    def update(self, *args, **keywords):
        wait = keywords.get('wait',False)
        DBDataWrite.update(self, *args, **keywords)
        FileOps(db=self._db).reschedule(self.recordid, wait)

    def getUpcoming(self, deactivated=False):
        recstatus = None
        if not deactivated:
            recstatus=Program.rsWillRecord
        return FileOps(db=self._db)._getSortedPrograms('QUERY_GETALLPENDING',
                    header=1, recordid=self.recordid, recstatus=recstatus)

    @property
    def artwork(self):
        if self._artwork is None:
            if (self.inetref is None) or (self.inetref == ""):
                raise MythError("Record cannot have artwork without inetref")

            try:
                self._artwork = \
                    RecordedArtwork((self.inetref, self.season), self._db)
            except MythError:
                #artwork does not exist, create new
                self._artwork = RecordedArtwork(db=self._db)
                self._artwork.inetref = self.inetref
                self._artwork.season = self.season
                self._artwork.host = self._db.getMasterBackend()
                self._artwork.create()
        return self._artwork

    @classmethod
    def fromGuide(cls, guide, type=RECTYPE.kAllRecord, wait=False):
        if datetime.now() > guide.endtime:
            raise MythError('Cannot create recording rule for past recording.')
        rec = cls(None, db=guide._db)
        for key in ('chanid','title','subtitle','description', 'category',
                    'seriesid','programid'):
            rec[key] = guide[key]

        rec.startdate = guide.starttime.date()
        rec.starttime = guide.starttime-datetime.combine(rec.startdate, time())
        rec.enddate = guide.endtime.date()
        rec.endtime = guide.endtime-datetime.combine(rec.enddate, time())

        rec.station = Channel(guide.chanid, db=guide._db).callsign
        rec.type = type
        return rec.create(wait=wait)

    @classmethod
    def fromProgram(cls, program, type=RECTYPE.kAllRecord, wait=False):
        if datetime.now() > program.endtime:
            raise MythError('Cannot create recording rule for past recording.')
        rec = cls(None, db=program._db)
        for key in ('chanid','title','subtitle','description','category',
                    'seriesid','programid'):
            rec[key] = program[key]
        rec.station = program.callsign

        rec.startdate = program.starttime.date()
        rec.starttime = program.starttime-datetime.combine(rec.startdate, time())
        rec.enddate = program.endtime.date()
        rec.endtime = program.endtime-datetime.combine(rec.enddate, time())

        if program.recordid:
            rec.parentid = program.recordid
            if program.recstatus == RECTYPE.kNotRecording:
                rec.type = RECTYPE.kOverrideRecord
            else:
                rec.type = RECTYPE.kDontRecord
        else:
            rec.type = type
        return rec.create(wait=wait)

    @classmethod
    def fromPowerRule(cls, title='unnamed (Power Search)', where='', args=None, 
                           join='', db=None, type=RECTYPE.kAllRecord, 
                           searchtype=RECSEARCHTYPE.kPowerSearch, wait=False):

        if type not in (RECTYPE.kAllRecord,           RECTYPE.kFindDailyRecord,
                        RECTYPE.kFindWeeklyRecord,    RECTYPE.kFindOneRecord):
            raise MythDBError("Invalid 'type' set for power recording rule.")

        rec = cls(None, db=db)
        if args is not None:
            where = rec._db.literal(where, args)

        now = datetime.now()
        rec.starttime = now.time()
        rec.endtime = now.time()
        rec.startdate = now.date()
        rec.enddate = now.date()

        rec.title = title
        rec.description = where
        rec.subtitle = join
        rec.type = type
        rec.search = searchtype
        return rec.create(wait=wait)

class Recorded( CMPRecord, DBDataWrite ):
    """
    Recorded(data=None, db=None) -> Recorded object
            'data' is a tuple containing (chanid, storagegroup)
    """
    _defaults = {'title':u'Unknown', 'subtitle':'',          'description':'',
                 'category':'',      'hostname':'',          'bookmark':0,
                 'editing':0,        'cutlist':0,            'autoexpire':0,
                 'commflagged':0,    'recgroup':'Default',   'seriesid':'',
                 'programid':'',     'lastmodified':'CURRENT_TIMESTAMP',
                 'filesize':0,       'stars':0,              'previouslyshown':0,
                 'preserve':0,       'bookmarkupdate':0,
                 'findid':0,         'deletepending':0,      'transcoder':0,
                 'timestretch':1,    'recpriority':0,        'playgroup':'Default',
                 'profile':'No',     'duplicate':1,          'transcoded':0,
                 'watched':0,        'storagegroup':'Default',
                 'inetref':'',       'season':0,            'episode':0}
    _artwork = None

    class _Cast( DBDataCRef ):
        _table = ['recordedcredits','people']
        _ref = ['chanid','starttime']
        _cref = ['person']

    class _Seek( DBDataRef, MARKUP ):
        _table = 'recordedseek'
        _ref = ['chanid','starttime']

    class _Markup( DBDataRef, MARKUP, MARKUPLIST ):
        _table = 'recordedmarkup'
        _ref = ['chanid','starttime']
        def getskiplist(self):
            return self._buildlist(self.MARK_COMM_START, self.MARK_COMM_END)
        def getunskiplist(self):
            return self._buildlist(self.MARK_COMM_END, self.MARK_COMM_START)
        def getcutlist(self):
            return self._buildlist(self.MARK_CUT_START, self.MARK_CUT_END)
        def getuncutlist(self):
            return self._buildlist(self.MARK_CUT_END, self.MARK_CUT_START)

    class _Rating( DBDataRef ):
        _table = 'recordedrating'
        _ref = ['chanid','starttime']
        
    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Recorded at %s>" % hex(id(self))
        return u"<Recorded '%s','%s' at %s>" % (self.title,
                self.starttime.isoformat(' '), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, data=None, db=None):
        if data is not None:
            if None not in data:
                data = [data[0], datetime.duck(data[1])]
        DBDataWrite.__init__(self, data, db)

    def _postinit(self):
        self.seek = self._Seek(self._wheredat, self._db)
        self.markup = self._Markup(self._wheredat, self._db)
        wheredat = (self.chanid, self.progstart)
        self.cast = self._Cast(wheredat, self._db)
        self.rating = self._Rating(wheredat, self._db)

    @classmethod
    def fromProgram(cls, program):
        return cls((program.chanid, program.recstartts), program._db)

    @classmethod
    def fromJob(cls, job):
        return cls((job.chanid, job.starttime), job._db)

    def _push(self):
        DBDataWrite._push(self)
        self.cast.commit()
        self.seek.commit()
        self.markup.commit()
        self.rating.commit()

    def delete(self, force=False, rerecord=False):
        """
        Recorded.delete(force=False, rerecord=False) -> retcode
                Informs backend to delete recording and all relevent data.
                'force' forces a delete if the file cannot be found.
                'rerecord' sets the file as recordable in oldrecorded
        """
        try:
            return self.getProgram().delete(force, rerecord)
        except AttributeError:
            raise MythError("Program could not be found")

    def open(self, type='r'):
        """Recorded.open(type='r') -> file or FileTransfer object"""
        return ftopen("myth://%s@%s/%s" % ( self.storagegroup,
                                            self.hostname,
                                            self.basename),
                      type, db=self._db,
                      chanid=self.chanid, starttime=self.starttime)

    def getProgram(self):
        """Recorded.getProgram() -> Program object"""
        return Program.fromRecorded(self)

    def getRecordedProgram(self):
        """Recorded.getRecordedProgram() -> RecordedProgram object"""
        return RecordedProgram.fromRecorded(self)

    @property
    def artwork(self):
        if self._artwork is None:
            if (self.inetref is None) or (self.inetref == ""):
                raise MythError("Recorded cannot have artwork without inetref")

            try:
                self._artwork = \
                    RecordedArtwork((self.inetref, self.season), self._db)
            except MythError:
                #artwork does not exist, create new
                self._artwork = RecordedArtwork(db=self._db)
                self._artwork.inetref = self.inetref
                self._artwork.season = self.season
                self._artwork.host = self._db.getMasterBackend()
                self._artwork.create()
        return self._artwork


    def formatPath(self, path, replace=None):
        """
        Recorded.formatPath(path, replace=None) -> formatted path string
                'path' string is formatted as per mythrename.pl
        """
        for (tag, data) in (('T','title'), ('S','subtitle'),
                            ('R','description'), ('C','category'),
                            ('U','recgroup'), ('hn','hostname'),
                            ('c','chanid') ):
            tmp = unicode(self[data]).replace('/','-')
            path = path.replace('%'+tag, tmp)
        for (data, pre) in (   ('starttime','%'), ('endtime','%e'),
                               ('progstart','%p'),('progend','%pe') ):
            for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                                  ('j','%d'),('d','%d'),('g','%I'),('G','%H'),
                                  ('h','%I'),('H','%H'),('i','%M'),('s','%S'),
                                  ('a','%p'),('A','%p') ):
                path = path.replace(pre+tag, self[data].strftime(format))
        if self.originalairdate is None:
            airdate = date(1,1,1)
        else:
            airdate = self.originalairdate
        for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                              ('j','%d'),('d','%d')):
            path = path.replace('%o'+tag, airdate.strftime(format))
        path = path.replace('%-','-')
        path = path.replace('%%','%')
        path += '.'+self['basename'].split('.')[-1]

        # clean up for windows
        if replace is not None:
            for char in ('\\',':','*','?','"','<','>','|'):
                path = path.replace(char, replace)
        return path

    def importMetadata(self, metadata, overwrite=False):
        """Imports data from a VideoMetadata object."""
        def _allow_change(self, tag, overwrite):
            if overwrite: return True
            if self[tag] is None: return True
            if self[tag] == '': return True
            if tag in self._defaults:
                if self[tag] == self._defaults[tag]:
                    return True
            return False

        # only work on existing entries
        if self._wheredat is None:
            return

        # pull direct matches
        for tag in ('title', 'subtitle', 'description', 'season', 'episode',
                    'chanid', 'seriesid', 'programid', 'inetref',
                    'recgroup', 'playgroup', 'seriesid', 'programid',
                    'storagegroup'):
            if metadata[tag] and _allow_change(self, tag, overwrite):
                self[tag] = metadata[tag]

        # pull renamed matches
        for tagf,tagt in (('userrating','stars'), ('filename', 'basename'),
                          ('startts','progstart'),('endts','progend'),
                          ('recstartts','starttime'),('recendts','endtime')):
            if metadata[tagf] and _allow_change(self, tagt, overwrite):
                self[tagt] = metadata[tagf]

        # pull cast
        trans = {'Author':'writer'}
        for cast in metadata.people:
            self.cast.append(unicode(cast.name),
                             unicode(trans.get(cast.job,
                                        cast.job.lower().replace(' ','_'))))

        # pull images
        for image in metadata.images:
            if not hasattr(self.artwork, image.type):
                pass
            if getattr(self.artwork, image.type, ''):
                continue
            setattr(self.artwork, image.type, image.filename)
            getattr(self.artwork, image.type).downloadFrom(image.url)

        self.update()

    def exportMetadata(self):
        """Exports data to a VideoMetadata object."""
        # only work on existing entries
        if self._wheredat is None:
            return
        metadata = VideoMetadata()

        # pull direct matches
        for tag in ('title', 'subtitle', 'description', 'season', 'episode',
                    'chanid', 'seriesid', 'programid', 'inetref',
                    'recgroup', 'playgroup', 'seriesid', 'programid',
                    'storagegroup'):
            if self[tag]:
                metadata[tag] = self[tag]

        # pull translated matches
        for tagt,tagf in (('userrating','stars'), ('filename', 'basename'),
                          ('startts','progstart'),('endts','progend'),
                          ('recstartts','starttime'),('recendts','endtime'),):
            if self[tagf]:
                metadata[tagt] = self[tagf]

        # pull cast
        for member in self.cast:
            name = member.name
            role = ' '.join([word.capitalize() for word in member.role.split('_')])
            if role=='Writer': role = 'Author'
            metadata.people.append(OrdDict((('name',name), ('job',role))))

#        for arttype in ['coverart', 'fanart', 'banner']:
#            art = getattr(self.artwork, arttype)
#            if art:
#                metadata.images.append(OrdDict((('type',arttype), ('filename',art))))

        return metadata

    def __getstate__(self):
        data = DBDataWrite.__getstate__(self)
        data['cast'] = self.cast._picklelist()
        data['seek'] = self.seek._picklelist()
        data['markup'] = self.markup._picklelist()
        data['rating'] = self.rating._picklelist()
        return data

    def __setstate__(self, state):
        DBDataWrite.__setstate__(self, state)
        if self._wheredat is not None:
            self.cast._populate(data=state['cast'])
            self.seek._populate(data=state['seek'])
            self.markup._populate(data=state['markup'])
            self.rating._populate(data=state['rating'])

    def _playOnFe(self, fe):
        return fe.send('play','program %d %s' % \
                    (self.chanid, self.starttime.isoformat()))

class RecordedProgram( CMPRecord, DBDataWrite ):

    """
    RecordedProgram(data=None, db=None) -> RecordedProgram object
            'data' is a tuple containing (chanid, storagegroup)
    """
    _key   = ['chanid','starttime']
    _defaults = {'title':'',     'subtitle':'',
                 'category':'',  'category_type':'',     'airdate':0,
                 'stars':0,      'previouslyshown':0,    'title_pronounce':'',
                 'stereo':0,     'subtitled':0,          'hdtv':0,
                 'partnumber':0, 'closecaptioned':0,     'parttotal':0,
                 'seriesid':'',  'originalairdate':'',   'showtype':u'',
                 'colorcode':'', 'syndicatedepisodenumber':'',
                 'programid':'', 'manualid':0,           'generic':0,
                 'first':0,      'listingsource':0,      'last':0,
                 'audioprop':u'','videoprop':u'',        
                 'subtitletypes':u''}

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized RecordedProgram at %s>" % hex(id(self))
        return u"<RecordedProgram '%s','%s' at %s>" % (self.title,
                self.starttime.isoformat(' '), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, data=None, db=None):
        if data is not None:
            if None not in data:
                data = [data[0], datetime.duck(data[1])]
        DBDataWrite.__init__(self, data, db)

    def _postinit(self):
        self.AudioProp = ParseSet(self, 'audioprop')
        self.VideoProp = ParseSet(self, 'videoprop')
        self.SubtitleTypes = ParseSet(self, 'subtitletypes')

    @classmethod
    def fromRecorded(cls, recorded):
        return cls((recorded.chanid, recorded.progstart), recorded._db)

class OldRecorded( CMPRecord, DBDataWrite, RECSTATUS ):
    """
    OldRecorded(data=None, db=None) -> OldRecorded object
            'data' is a tuple containing (chanid, starttime)
    """

    _key   = ['chanid','starttime']
    _defaults = {'title':'',     'subtitle':'',      
                 'category':'',  'seriesid':'',      'programid':'',
                 'findid':0,     'recordid':0,       'station':'',
                 'rectype':0,    'duplicate':0,      'recstatus':-3,
                 'reactivate':0, 'generic':0,        'future':None,
                 'inetref':'',   'season':0,         'episode':0}

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized OldRecorded at %s>" % hex(id(self))
        return u"<OldRecorded '%s','%s' at %s>" % (self.title,
                self.starttime.isoformat(' '), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, data=None, db=None):
        if data is not None:
            if None not in data:
                data = [data[0], datetime.duck(data[1])]
        DBDataWrite.__init__(self, data, db)
        if self.future:
            raise MythDBError(MythError.DB_RESTRICT, "'future' OldRecorded " +\
                        "instances are not usable from the bindings.")

    def setDuplicate(self, record=False):
        """
        OldRecorded.setDuplicate(record=False) -> None
                Toggles re-recordability
        """
        with self._db.cursor(self._log) as cursor:
            cursor.execute("""UPDATE oldrecorded SET duplicate=%%s
                              WHERE %s""" % self._where, \
                        tuple([record]+list(self._wheredat)))
        FileOps(db=self._db).reschedule(0)

    def update(self, *args, **keywords):
        """OldRecorded entries cannot be altered"""
        return
    def delete(self):
        """OldRecorded entries cannot be deleted"""
        return

class RecordedArtwork( DBDataWrite ):
    """
    RecordedArtwork(data=None, db=None)
    """
    _key = ('inetref', 'season')
    _defaults = {'inetref':'',      'season':0,     'host':'',
                 'coverart':'',     'fanart':'',    'banner':''}

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Artwork at %s>" % hex(id(self))
        return u"<RecordedArtwork '%s','%d' at %s>" % \
                        (self.inetref, self.season, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    coverart = Artwork('coverart')
    fanart   = Artwork('fanart')
    banner   = Artwork('banner')

class Job( DBDataWrite, JOBTYPE, JOBCMD, JOBFLAG, JOBSTATUS ):
    """
    Job(id=None, db=None) -> Job object
    """
    _table = 'jobqueue'
    _logmodule = 'Python Jobqueue'
    _defaults = {'id':None,     'inserttime':datetime.now(),
                 'hostname':'', 'status':JOBSTATUS.QUEUED,
                 'comment':'',  'schedruntime':datetime.now()}

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Job at %s>" % hex(id(self))
        return u"<Job '%s' at %s>" % (self.id, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def setComment(self,comment):
        """Job.setComment(comment) -> None, updates comment"""
        self.comment = comment
        self.update()

    def setStatus(self,status):
        """Job.setStatus(Status) -> None, updates status"""
        self.status = status
        self.update()

    @classmethod
    def fromRecorded(cls, rec, type, status=None, schedruntime=None,
                               hostname=None, args=None, flags=None):
        job = cls(db=rec._db)
        job.type = type
        job.chanid = rec.chanid
        job.starttime = rec.starttime
        if status:
            job.status = status
        if schedruntime:
            job.schedruntime = schedruntime
        if hostname:
            job.hostname = hostname
        if args:
            job.args = args
        if flags:
            job.flags = flags
        job.create()

    @classmethod
    def fromProgram(cls, prog, type, status=None, schedruntime=None,
                                hostname=None, args=None, flags=None):
        if prog.rectype != prog.rsRecorded:
            raise MythError('Invalid recording type for Job.')
        job = cls(db=prog._db)
        job.type = type
        job.chanid = prog.chanid
        job.starttime = prog.recstartts
        if status:
            job.status = status
        if schedruntime:
            job.schedruntime = schedruntime
        if hostname:
            job.hostname = hostname
        if args:
            job.args = args
        if flags:
            job.flags = flags
        job.create()

class Channel( DBDataWrite ):
    """Channel(chanid=None, db=None) -> Channel object"""
    _defaults = {'icon':'none',          'videofilters':'',  'callsign':u'',
                 'xmltvid':'',           'recpriority':0,    'contrast':32768,
                 'brightness':32768,     'colour':32768,     'hue':32768,
                 'tvformat':u'Default',  'visible':1,        'outputfilters':'',
                 'useonairguide':0,      'atsc_major_chan':0,
                 'tmoffset':0,           'default_authority':'',
                 'commmethod':-1,        'atsc_minor_chan':0,
                 'last_record':datetime(1900,1,1)}

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Channel at %s>" % hex(id(self))
        return u"<Channel '%s','%s' at %s>" % \
                        (self.chanid, self.name, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

class Guide( CMPRecord, DBData ):
    """
    Guide(data=None, db=None) -> Guide object
            Data is a tuple of (chanid, starttime).
    """
    _table = 'program'
    _key   = ['chanid','starttime']
    
    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Guide at %s>" % hex(id(self))
        return u"<Guide '%s','%s' at %s>" % (self.title,
                self.starttime.isoformat(' '), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def getRecStatus(self):
        be = FileOps(db=self._db)
        for prog in be._getPrograms('QUERY_GETALLPENDING', header=1):
            if (prog.chanid == self.chanid) and \
                    (prog.starttime == self.starttime):
                return prog.recstatus
        return 0

    def _postinit(self):
        self.AudioProp = ParseSet(self, 'audioprop', False)
        self.VideoProp = ParseSet(self, 'videoprop', False)
        self.SubtitleTypes = ParseSet(self, 'subtitletypes', False)

    @classmethod
    def fromEtree(cls, etree, db=None):
        dat = {'chanid':etree[0]}
        attrib = etree[1].attrib
        for key in ('title','subTitle','category','seriesId',
                    'hostname','programId','airdate'):
            if key in attrib:
                dat[key.lower()] = attrib[key]
        if 'stars' in attrib:
            dat['stars'] = locale.atof(attrib['stars'])
        if etree[1].text:
            dat['description'] = etree[1].text.strip()
        for key in ('startTime','endTime','lastModified'):
            if key in attrib:
                dat[key.lower()] = datetime.fromIso(attrib[key])

        raw = []
        for key in db.tablefields[cls._table]:
            if key in dat:
                raw.append(dat[key])
            else:
                raw.append(None)
        return cls.fromRaw(raw, db)

    @classmethod
    def fromJSON(cls, prog, db=None):
        dat = {}
        for key in ('ChanId','Title','SubTitle','Category'):
            dat[key.lower()] = prog[key]
        for key,key2 in (('CatType', 'category_type'),):
            dat[key2] = prog[key]
        for key in ('StartTime', 'EndTime'):
            dat[key.lower()] = datetime.fromIso(prog[key])
        dat['airdate'] = dat['starttime'].year

        raw = []
        for key in db.tablefields[cls._table]:
            if key in dat:
                raw.append(dat[key])
            else:
                raw.append(None)
        return cls.fromRaw(raw, db)

#### MYTHVIDEO ####

class Video( CMPVideo, VideoSchema, DBDataWrite ):
    """Video(id=None, db=None, raw=None) -> Video object"""
    _table = 'videometadata'
    _defaults = {'subtitle':u'',             'director':u'Unknown',
                 'rating':u'NR',             'inetref':u'00000000',
                 'year':1895,                'userrating':0.0,
                 'length':0,                 'showlevel':1,
                 'coverfile':u'No Cover',    'host':u'',
                 'homepage':u'',             'insertdate': datetime.now(),
                 'watched':False,            'category':0,
                 'browse':True,              'hash':u'',
                 'season':0,                 'episode':0,
                 'releasedate':date(1,1,1),  'childid':-1}
    _cm_toid, _cm_toname = DictInvertCI.createPair({0:'none'})

    @classmethod
    def _setClassDefs(cls, db=None):
        db = DBCache(db)
        super(Video, cls)._setClassDefs(db)
        cls._fill_cm(db)

    @classmethod
    def _fill_cm(cls, db=None):
        db = DBCache(db)
        with db.cursor() as cursor:
            cursor.execute("""SELECT * FROM videocategory""")
            for row in cursor:
                cls._cm_toname[row[0]] = row[1]

    def _cat_toname(self):
        if self.category is not None:
            try:
                self.category = self._cm_toname[int(self.category)]
            except ValueError:
                # already a named category
                pass
            except KeyError:
                self._fill_cm(self._db)
                if int(self.category) in self._cm_toname:
                    self.category = self._cm_toname[int(self.category)]
                else:
                    raise MythDBError('Video defined with unknown category id')
        else:
            self.category = 'none'

    def _cat_toid(self):
        if self.category is not None:
            try:
                if self.category.lower() not in self._cm_toid:
                    self._fill_cm(self._db)
                if self.category.lower() not in self._cm_toid:
                    with self._db.cursor(self._log) as cursor:
                        cursor.execute("""INSERT INTO videocategory
                                          SET category=%s""",
                                      self.category)
                        self._cm_toid[self.category] = cursor.lastrowid
                self.category = self._cm_toid[self.category]
            except AttributeError:
                # already an integer category
                pass
        else:
            self.category = 0

    def _pull(self):
        DBDataWrite._pull(self)
        self._fill_cm()
        self._cat_toname()

    def _push(self):
        self._cat_toid()
        DBDataWrite._push(self)
        self._cat_toname()
        self.cast.commit()
        self.genre.commit()
        self.country.commit()

    def __repr__(self):
        if self._wheredat is None:
            return u"<Uninitialized Video at %s>" % hex(id(self))
        res = self.title
        if self.season and self.episode:
            res += u' - %dx%02d' % (self.season, self.episode)
        if self.subtitle:
            res += u' - '+self.subtitle
        return u"<Video '%s' at %s>" % (res, hex(id(self)))

    def _postinit(self):
        self._fill_cm()
        self._cat_toname()
        self.cast = self._Cast(self._wheredat, self._db)
        self.genre = self._Genre(self._wheredat, self._db)
        self.country = self._Country(self._wheredat, self._db)
        self.markup = self._Markup((self.filename,), self._db)

    def create(self, data=None):
        """Video.create(data=None) -> Video object"""
        if (self.host is not None) and (self.host != ''):
            # check for pre-existing entry
            if self.hash == '':
                self.hash = self.getHash()
            with self._db as cursor:
                if cursor.execute("""SELECT intid FROM videometadata
                                     WHERE hash=%s""", self.hash) > 0:
                    id = cursor.fetchone()[0]
                    self._evalwheredat([id])
                    self._pull()
                    return self

        # create new entry
        self._import(data)
        self._cat_toid()
        return DBDataWrite._create_autoincrement(self)

    class _Cast( DBDataCRef ):
        _table = ['videometadatacast','videocast']
        _ref = ['idvideo']
        _cref = ['idcast','intid']

    class _Genre( DBDataCRef ):
        _table = ['videometadatagenre','videogenre']
        _ref = ['idvideo']
        _cref = ['idgenre','intid']

    class _Country( DBDataCRef ):
        _table = ['videometadatacountry','videocountry']
        _ref = ['idvideo']
        _cref = ['idcountry','intid']

    class _Markup( DBDataRef, MARKUP ):
        _table = 'filemarkup'
        _ref = ['filename',]

    def delete(self):
        """Video.delete() -> None"""
        if (self._where is None) or \
                (self._wheredat is None):
            return
        self.cast.clean()
        self.genre.clean()
        self.country.clean()
        DBDataWrite.delete(self)

    banner               = Artwork('banner')
    coverfile = coverart = Artwork('coverfile')
    fanart               = Artwork('fanart')
    screenshot           = Artwork('screenshot')
    trailer              = Artwork('trailer')

    def open(self, mode='r', nooverwrite=False):
        return ftopen('myth://Videos@{0.host}/{0.filename}'.format(self),
                    mode, False, nooverwrite, self._db)

    def getHash(self):
        """Video.getHash() -> file hash"""
        if self.host is None:
            return None
        be = FileOps(db=self._db)
        hash = be.getHash(self.filename, 'Videos', self.host)
        return hash

    def parseFilename(self):
        filename = self.filename
        filename = filename[:filename.rindex('.')]
        for old in ('%20','_','.'):
            filename = filename.replace(old, ' ')

        sep = '(?:\s?(?:-|/)?\s?)?'
        regex1 = re.compile(
            sep.join(['^(.*[^s0-9])',
                      '(?:s|(?:Season))?',
                      '(\d{1,4})',
                      '(?:[ex/]|Episode)',
                      '(\d{1,3})',
                      '(.*)$']), re.I)

        regex2 = re.compile('(%s(?:Season%s\d*%s)*%s)$' \
                            % (sep, sep, sep, sep), re.I)

        match1 = regex1.search(filename)
        if match1:
            title = match1.group(1)
            season = int(match1.group(2))
            episode = int(match1.group(3))
            subtitle = match1.group(4)

            match2 = regex2.search(title)
            if match2:
                title = title[:match2.start()]
                title = title.rsplit('/',1)[-1]
        else:
            season = None
            episode = None
            subtitle = None
            title = filename.rsplit('/',1)[-1]
            for left,right in (('(',')'), ('[',']'), ('{','}')):
                while left in title:
                    lin = title.index(left)
                    rin = title.index(right,lin)
                    title = title[:lin]+title[rin+1:]
            title = title

        return (title, season, episode, subtitle)

    def importMetadata(self, metadata, overwrite=False):
        """Imports data from a VideoMetadata object."""
        def _allow_change(self, tag, overwrite):
            if overwrite: return True
            if self[tag] is None: return True
            if self[tag] == '': return True
            if tag in self._defaults:
                if self[tag] == self._defaults[tag]:
                    return True
            return False

        # only operate on existing entries
        if self._wheredat is None:
            return

        # pull direct tags
        for tag in ('title', 'subtitle', 'tagline', 'season', 'episode',
                    'inetref', 'homepage', 'trailer', 'userrating', 'year',
                    'releasedate'):
            if metadata[tag] and _allow_change(self, tag, overwrite):
                self[tag] = metadata[tag]

        # pull tags needing renaming
        for tagf,tagt in (('description','plot'), ('runtime','length')):
            if metadata[tagf] and _allow_change(self, tagt, overwrite):
                self[tagt] = metadata[tagf]

        # pull director
        try:
            if _allow_change(self, 'director', overwrite):
                self.director = [person.name for person in metadata.people \
                                            if person.job=='Director'].pop(0)
        except IndexError: pass

        # pull actors
        for actor in [person for person in metadata.people \
                                  if person.job=='Actor']:
            self.cast.add(unicode(actor.name))

        # pull genres
        for category in metadata.categories:
            self.genre.add(unicode(category))

        # pull images (SG content only)
        if bool(self.host):
            for image in metadata.images:
                if not hasattr(self, image.type):
                    continue
                current = getattr(self, image.type)
                if current and (current != 'No Cover') and not overwrite:
                    continue
                setattr(self, image.type, image.filename)
                getattr(self, image.type).downloadFrom(image.url)

        self.processed = True
        self.update()

    def exportMetadata(self):
        """Exports data to a VideoMetadata object."""
        # only work on entries from the database
        if self._wheredat is None:
            return
        metadata = VideoMetadata()

        # pull direct tags
        for tag in ('title', 'subtitle', 'tagline', 'season', 'episode',
                    'inetref', 'homepage', 'trailer', 'userrating', 'year',
                    'releasedate'):
            if self[tag]:
                metadata[tag] = self[tag]

        # pull translated tags
        for tagf, tagt in (('plot', 'description'), ('length', 'runtime')):
            if self[tagf]:
                metadata[tagt] = self[tagf]

        # pull director
        if self.director:
            metadata.people.append(OrdDict((('name',self.director), ('job','Director'))))

        # pull actors
        for actor in self.cast:
            metadata.people.append(OrdDict((('name',actor.cast), ('job','Actor'))))

        # pull genres
        for genre in self.genre:
            metadata.categories.append(genre.genre)

        # pull countries
        for country in self.country:
            metadata.countries.append(country.country)

        # pull images
#        for arttype in ['coverart', 'fanart', 'banner', 'screenshot']:
#            art = getattr(self, arttype)
#            if art:
#                metadata.images.append(OrdDict((('type',arttype), ('filename',art))))

        return metadata

    def __getstate__(self):
        data = DBDataWrite.__getstate__(self)
        data['cast'] = self.cast._picklelist()
        data['genre'] = self.genre._picklelist()
        data['markup'] = self.markup._picklelist()
        data['country'] = self.country._picklelist()
        return data

    def __setstate__(self, state):
        DBDataWrite.__setstate__(self, state)
        if self._wheredat is not None:
            self.cast._populate(data=state['cast'])
            self.genre._populate(data=state['genre'])
            self.markup._populate(data=state['markup'])
            self.country._populate(data=state['country'])

    @classmethod
    def fromFilename(cls, filename, db=None):
        vid = cls(db=db)
        vid.filename = filename
        vid.title, vid.season, vid.episode, vid.subtitle = \
                        vid.parseFilename()
        return vid

    def _playOnFe(self, fe):
        return fe.send('play','file myth://Videos@%s/%s' % 
                    (self.host, self.filename))

    #### LEGACY ####
    # of course this will likely all get scrapped for 0.26...
    def openBanner(self, mode='r', nooverwrite=False):
        return self.banner.open(mode)
    def openCoverart(self, mode='r', nooverwrite=False):
        return self.coverfile.open(mode)
    def openFanart(self, mode='r', nooverwrite=False):
        return self.fanart.open(mode)
    def openScreenshot(self, mode='r', nooverwrite=False):
        return self.screenshot.open(mode)
    def openTrailer(self, mode='r', nooverwrite=False):
        return self.trailer.open(mode)


class VideoGrabber( Grabber ):
    """
    VideoGrabber(mode, lang='en', db=None) -> VideoGrabber object
            'mode' can be of either 'TV' or 'Movie'
    """
    logmodule = 'Python MythVideo Grabber'
    cls = VideoMetadata

    def __init__(self, mode, lang='en', db=None):
        dbvalue = {'tv':'TelevisionGrabber', 'movie':'MovieGrabber'}
        path = {'tv':'metadata/Television/ttvdb.py',
                'movie':'metadata/Movie/tmdb.py'}
        self.mode = mode.lower()
        try:
            Grabber.__init__(self, setting=dbvalue[self.mode], db=db,
                        path=path[self.mode],
                        prefix=os.path.join(INSTALL_PREFIX, 'share/mythtv'))
        except KeyError:
            raise MythError('Invalid MythVideo grabber')
        self.append('-l',lang)

#### MYTHNETVISION ####

class InternetContent( DBData ):
    _key = ['name']

class InternetContentArticles( DBData ):
    _key = ['feedtitle','title','subtitle']

class InternetSource( DictData ):
    logmodule = 'Python Internet Video Source'
    _field_order = ['name','author','thumbnail','command','type','description','version','search','tree']
    _field_type = 'Pass'
    xmlconn = None

    @classmethod
    def fromEtree(cls, etree, xmlconn):
        dat = {}
        for item in etree.getchildren():
            dat[item.tag] = item.text

        raw = []
        for field in cls._field_order:
            if field in dat:
                raw.append(dat[field])
            else:
                raw.append('')
        source = cls(raw)
        source.xmlconn = xmlconn
        return source

    def searchContent(self, query, page=1):
        if (self.xmlconn is None) or (self.search=='false'):
            return
        xmldat = self.xmlconn._queryTree('GetInternetSearch',
                    Grabber=self.command, Query=query, Page=page)
        xmldat = xmldat.find('channel')
        self.count = xmldat.find('numresults').text
        self.returned = xmldat.find('returned').text
        self.start = xmldat.find('startindex').text
        for item in xmldat.findall('item'):
            yield InternetMetadata(item)


#### MYTHMUSIC ####

class Song( MusicSchema, DBDataWrite ):
    _table = 'music_songs'

    @classmethod
    def fromAlbum(cls, album, db=None):
        """Returns iterable of songs from given album."""
        try:
            db = album._db
            album = album.album_id
        except AttributeError: pass

        return cls._fromQuery("WHERE album_id=%s", [album], db)

    @classmethod
    def fromArtist(cls, artist, db=None):
        """Returns iterable of songs from given artist."""
        try:
            db = artist._db
            artist = artist.artist_id
        except AttributeError: pass

        return cls._fromQuery("WHERE artist_id=%s", [artist], db)

    @classmethod
    def fromPlaylist(cls, playlist, db=None):
        """Returns iterable of songs from given playlist."""
        try:
            songs = playlist._songstring()
            db = playlist._db
        except AttributeError:
            db = DBCache(db)
            songs = MusicPlaylist(playlist, db)._songstring()

        return cls._fromQuery("WHERE LOCATE(song_id, %s)", songs, db)

class Album( MusicSchema, DBDataWrite ):
    _table = 'music_albums'

    @classmethod
    def fromArtist(cls, artist, db=None):
        """Returns iterable of albums from given artist."""
        try:
            db = artist._db
            artist = artist.artist_id
        except AttributeError:
            pass
        return cls._fromQuery("WHERE artist_id=%s", [artist], db)

    @classmethod
    def fromSong(cls, song, db=None):
        """Returns the album for the given song."""
        try:
            album = song.album_id
            db = song._db
        except AttributeError:
            db = DBCache(db)
            album = Song(song, db).album_id
        return cls(album, db)

class Artist( MusicSchema, DBDataWrite ):
    _table = 'music_artists'

    @classmethod
    def fromName(cls, name, db=None):
        db = MythDB(db)
        c = db.cursor()
        count = c.execute("""SELECT * FROM %s WHERE artist_name=%s""", (cls._table, name))
        if count > 1:
            raise MythDBError('Non-unique music_artist entry')
        elif count == 1:
            return cls.fromRaw(c.fetchone(), db)
        else:
            artist = cls(db=db)
            artist.artist_name = name
            return artist.create()
   
    @classmethod
    def fromSong(cls, song, db=None):
        """Returns the artist for the given song."""
        try:
            artist = song.artist_id
            db = song._db
        except AttributeError:
            db = DBCache(db)
            artist = Song(song, db).artist_id
        return cls(artist, db)

    @classmethod
    def fromAlbum(cls, album, db=None):
        """Returns the artist for the given album."""
        try:
            artist = album.artist_id
            db = album._db
        except AttributeError:
            db = DBCache(db)
            artist = Album(album, db).artist_id
        return cls(artist, db)

class MusicPlaylist( MusicSchema, DBDataWrite ):
    _table = 'music_playlists'

    def _pl_tolist(self):
        try:
            self.playlist_songs = \
                    [int(id) for id in self.playlist_songs.split(',')]
        except: pass

    def _pl_tostr(self):
        try:
            self.playlist_songs = \
                    ','.join(['%d' % id for id in self.playlist_songs])
        except: pass

    def _pull(self):
        DBDataWrite._pull(self)
        self._pl_tolist()

    def _push(self):
        self._pl_tostr()
        DBDataWrite._push(self)
        self._pl_tolist()

    def _evalwheredat(self, wheredat=None):
        DBDataWrite._evalwheredat(self, wheredat)
        self._pl_tolist()

    @classmethod
    def fromSong(cls, song, db=None):
        """Returns an iterable of playlists containing the given song."""
        try:
            db = song._db
            song = song.song_id
        except AttributeError:
            db = DBCache(db)
            song = Song(song, db).song_id
        return cls._fromQuery("WHERE LOCATE(%s, playlist_songs)", song, db)

class MusicDirectory( MusicSchema, DBDataWrite ):
    _table = 'music_directories'

    @classmethod
    def fromPath(cls, path, db=None):
        db = MythDB(db)
        c = db.cursor()
        count = c.execute("""SELECT * FROM %s WHERE path=%s""" \
                                    % cls._table, path)
        if count > 1:
            raise MythDBError('Multiple matches for MusicDirectory.fromPath')
        elif count == 1:
            return cls.fromRaw(c.fetchone())
        else:
            directory = cls()
            directory.path = path
            if path.find('/') != -1:
                directory.parent_id = \
                        cls.fromPath(path.rsplit('/',1)[0]).directory_id
            return directory.create()
