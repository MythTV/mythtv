# -*- coding: utf-8 -*-

"""
Provides data access classes for accessing and managing MythTV data
"""

from __future__ import with_statement

from static import *
from exceptions import *
from altdict import DictData, DictInvertCI
from database import *
from system import Grabber, InternetMetadata, VideoMetadata
from mythproto import ftopen, FileOps, Program
from utility import CMPRecord, CMPVideo, MARKUPLIST, datetime

import re
import locale
import xml.etree.cElementTree as etree
from datetime import date, time

class Record( DBDataWrite, RECTYPE, CMPRecord ):
    """
    Record(id=None, db=None) -> Record object
    """
    _defaults = {'type':RECTYPE.kAllRecord,
                 'title':u'Unknown', 'subtitle':'',      'description':'',
                 'category':'',      'station':'',       'seriesid':'',
                 'search':0,         'last_record':datetime(1900,1,1),
                 'next_record':datetime(1900,1,1),
                 'last_delete':datetime(1900,1,1)}

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

class Recorded( DBDataWrite, CMPRecord ):
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
                 'watched':0,        'storagegroup':'Default'}

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
        except NoneType:
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
        for tag in ('title', 'subtitle', 'description'):
            if metadata[tag] and _allow_change(self, tag, overwrite):
                self[tag] = metadata[tag]

        # pull renamed matches
        for tagf,tagt in (('userrating','stars'),):
            if metadata[tagf] and _allow_change(self, tagt, overwrite):
                self[tagt] = metadata[tagf]

        # pull cast
        trans = {'Author':'writer'}
        for cast in metadata.people:
            self.cast.append(unicode(cast.name),
                             unicode(trans.get(cast.job,
                                        cast.job.lower().replace(' ','_'))))

        # pull images
        exists = {'coverart':False,     'fanart':False,
                  'banner':False,       'screenshot':False}
        be = FileOps(self.hostname, db=self._db)
        for image in metadata.images:
            if exists[image.type]:
                continue
            group = Video._getGroup(self.hostname, image.type, self._db)
            if not be.fileExists(image.filename, group):
                be.downloadTo(image.url, group, image.filename)
            exists[image.type] = True

        self.update()

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

class RecordedProgram( DBDataWrite, CMPRecord ):

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

    @classmethod
    def fromRecorded(cls, recorded):
        return cls((recorded.chanid, recorded.progstart), recorded._db)

class OldRecorded( DBDataWrite, RECSTATUS, CMPRecord ):
    """
    OldRecorded(data=None, db=None) -> OldRecorded object
            'data' is a tuple containing (chanid, starttime)
    """

    _key   = ['chanid','starttime']
    _defaults = {'title':'',     'subtitle':'',      
                 'category':'',  'seriesid':'',      'programid':'',
                 'findid':0,     'recordid':0,       'station':'',
                 'rectype':0,    'duplicate':0,      'recstatus':-3,
                 'reactivate':0, 'generic':0}

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

class Job( DBDataWrite, JOBTYPE, JOBCMD, JOBFLAG, JOBSTATUS ):
    """
    Job(id=None, db=None) -> Job object
    """
    _table = 'jobqueue'
    _logmodule = 'Python Jobqueue'
    _defaults =  {'id':None,     'inserttime':datetime.now(),
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

class Guide( DBData, CMPRecord ):
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

#### MYTHVIDEO ####

class Video( VideoSchema, DBDataWrite, CMPVideo ):
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

    class _open(object):
        def __init__(self, func):
            self.__name__ = func.__name__
            self.__module__ = func.__module__
            self.__doc__ = """Video.%s(mode='r', nooverwrite=False)
                        -> file or FileTransfer object""" % self.__name__
            self.type, self.sgroup = \
                        {'':('filename','Videos'),
                         'Banner':('banner','Banners'),
                         'Coverart':('coverfile','Coverart'),
                         'Fanart':('fanart','Fanart'),
                         'Screenshot':('screenshot','Screenshots'),
                         'Trailer':('trailer','Trailers')}[self.__name__[4:]]
        def __get__(self, inst, own):
            self.inst = inst
            return self

        def __call__(self, mode='r', nooverwrite=False):
            if self.inst.host == '':
                raise MythFileError('File access only works '
                                    'with Storage Group content')
            return ftopen('myth://%s@%s/%s' % ( self.sgroup,
                                                self.inst.host,
                                                self.inst[self.type]),
                                    mode, False, nooverwrite, self.inst._db)

    @classmethod
    def _setClassDefs(cls, db=None):
        db = DBCache(db)
        super(Video, cls)._setClassDefs(db)
        cls._fill_cm(db)

    @classmethod
    def _getGroup(cls, host, groupname=None, db=None):
        db = DBCache(db)
        metadata = ['coverart','fanart','banner','screenshot']
        fields = ['coverfile','fanart','banner','screenshot']
        groups = ['Coverart','Fanart','Banners','Screenshots']

        if (groupname is None) or (groupname == 'Videos'):
            if len(list(db.getStorageGroup('Videos', host))) == 0:
                raise MythError('MythVideo not set up for this host.')
            else:
                return 'Videos'
        elif groupname in groups:
            if len(list(db.getStorageGroup(groupname, host))) == 0:
                return cls._getGroup(host, 'Videos', db)
            else:
                return groupname
        elif groupname in fields:
            return cls._getGroup(host, groups[fields.index(groupname)])
        elif groupname in metadata:
            return cls._getGroup(host, groups[metadata.index(groupname)])
        else:
            raise MythError('Invalid Video StorageGroup name.')

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
        return str(self).encode('utf-8')

    def __str__(self):
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

    @_open
    def open(self,mode='r',nooverwrite=False): pass
    @_open
    def openBanner(self,mode='r',nooverwrite=False): pass
    @_open
    def openCoverart(self,mode='r',nooverwrite=False): pass
    @_open
    def openFanart(self,mode='r',nooverwrite=False): pass
    @_open
    def openScreenshot(self,mode='r',nooverwrite=False): pass
    @_open
    def openTrailer(self,mode='r',nooverwrite=False): pass

    def getHash(self):
        """Video.getHash() -> file hash"""
        if self.host is None:
            return None
        be = FileOps(self.host)
        hash = be.getHash(self.filename, 'Videos')
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
                    'inetref', 'homepage', 'trailer', 'userrating', 'year'):
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
            # only perform image grabs if 'host' is set, denoting SG use
            t1 = ['coverart','fanart','banner','screenshot']
            t2 = ['coverfile','fanart','banner','screenshot']
            mdtype = dict(zip(t1,t2))
            exists = dict(zip(t1,[False,False,False,False]))

            be = FileOps(self.host, db=self._db)
            for image in metadata.images:
                if exists[image.type]:
                    continue
                if not _allow_change(self, mdtype[image.type], overwrite):
                    continue
                self[mdtype[image.type]] = image.filename
                group = self._getGroup(self.host, image.type, self._db)
                if not be.fileExists(image.filename, group):
                    be.downloadTo(image.url, group, image.filename)
                exists[image.type] = True

        self.processed = True
        self.update()

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

class VideoGrabber( Grabber ):
    """
    VideoGrabber(mode, lang='en', db=None) -> VideoGrabber object
            'mode' can be of either 'TV' or 'Movie'
    """
    logmodule = 'Python MythVideo Grabber'
    cls = VideoMetadata

    def __init__(self, mode, lang='en', db=None):
        dbvalue = {'tv':'TelevisionGrabber', 'movie':'MovieGrabber'}
        path = {'tv':'Television/ttvdb.py', 'movie':'Movie/tmdb.py'}
        self.mode = mode.lower()
        try:
            Grabber.__init__(self, setting=dbvalue[self.mode], db=db,
                path=os.path.join(INSTALL_PREFIX, 'share/mythtv/metadata', 
                                  path[self.mode]))
        except KeyError:
            raise MythError('Invalid MythVideo grabber')
        self._check_schema('mythvideo.DBSchemaVer',
                                MVSCHEMA_VERSION, 'MythVideo')
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
        count = c.execute("""SELECT * FROM %s WHERE path=%%s""" \
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
