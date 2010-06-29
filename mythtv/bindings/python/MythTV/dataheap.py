# -*- coding: utf-8 -*-

"""
Provides data access classes for accessing and managing MythTV data
"""

from static import *
from exceptions import *
from altdict import DictData, DictInvertCI
from database import DBCache, DBData, DBDataWrite, DBDataRef, DBDataCRef
from system import Grabber, InternetMetadata, VideoMetadata
from mythproto import ftopen, FileOps, Program
from utility import CMPRecord, CMPVideo, MARKUPLIST

import re
import locale
import xml.etree.cElementTree as etree
from time import strftime, strptime
from datetime import date, time, datetime
from socket import gethostname

class Record( DBDataWrite, RECTYPE, CMPRecord ):
    """
    Record(id=None, db=None, raw=None) -> Record object
    """

    _table = 'record'
    _where = 'recordid=%s'
    _setwheredat = 'self.recordid,'
    _defaults = {'recordid':None,    'type':RECTYPE.kAllRecord,
                 'title':u'Unknown', 'subtitle':'',      'description':'',
                 'category':'',      'station':'',       'seriesid':'',
                 'search':0,         'last_record':datetime(1900,1,1),
                 'next_record':datetime(1900,1,1),
                 'last_delete':datetime(1900,1,1)}
    _logmodule = 'Python Record'

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Record Rule at %s>" % hex(id(self))
        return u"<Record Rule '%s', Type %d at %s>" \
                                    % (self.title, self.type, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, id, db=None):
        DBDataWrite.__init__(self, (id,), db)

    def create(self, data=None):
        """Record.create(data=None) -> Record object"""
        self._wheredat = (DBDataWrite.create(self, data),)
        self._pull()
        FileOps(db=self._db).reschedule(self.recordid)
        return self

    def delete(self):
        DBDataWrite.delete(self)
        FileOps(db=self._db).reschedule(self.recordid)

    def update(self, *args, **keywords):
        DBDataWrite.update(*args, **keywords)
        FileOps(db=self._db).reschedule(self.recordid)

    def getUpcoming(self, deactivated=False):
        recstatus = None
        if not deactivated:
            recstatus=Program.rsWillRecord
        return FileOps(db=self._db)._getSortedPrograms('QUERY_GETALLPENDING',
                    header=1, recordid=self.recordid, recstatus=recstatus)

    @classmethod
    def fromGuide(cls, guide, type=RECTYPE.kAllRecord):
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
        return rec.create()

    @classmethod
    def fromProgram(cls, program, type=RECTYPE.kAllRecord):
        if datetime.now() > program.endtime:
            raise MythError('Cannot create recording rule for past recording.')
        rec = cls(None, db=program._db)
        for key in ('chanid','title','subtitle','description','category',
                    'seriesid','programid'):
            rec[key] = program[key]

        rec.startdate = program.starttime.date()
        rec.starttime = program.starttime-datetime.combine(rec.startdate, time())
        rec.enddate = program.endtime.date()
        rec.endtime = program.endtime-datetime.combine(rec.enddate, time())

        rec.station = program.callsign
        rec.type = type
        return rec.create()

class Recorded( DBDataWrite, CMPRecord ):
    """
    Recorded(data=None, db=None, raw=None) -> Recorded object
            'data' is a tuple containing (chanid, storagegroup)
    """
    _table = 'recorded'
    _where = 'chanid=%s AND starttime=%s'
    _setwheredat = 'self.chanid,self.starttime'
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
    _logmodule = 'Python Recorded'

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
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def _postinit(self):
        DBDataWrite._postinit(self)
        if self._wheredat is not None:
            self.cast = self._Cast(self._wheredat, self._db)
            self.seek = self._Seek(self._wheredat, self._db)
            self.markup = self._Markup(self._wheredat, self._db)

    @classmethod
    def fromProgram(cls, program):
        return cls((program.chanid, program.recstartts), program._db)

    def _push(self):
        DBDataWrite._push(self)
        self.cast.commit()
        self.seek.commit()
        self.markup.commit()

    def create(self, data=None):
        """Recorded.create(data=None) -> Recorded object"""
        DBDataWrite.create(self, data)
        self._wheredat = (self.chanid,self.starttime)
        self._pull()
        self.cast = self._Cast(self._wheredat, self._db)
        self.seek = self._Seek(self._wheredat, self._db)
        self.markup = self._Markup(self._wheredat, self._db)
        return self

    def delete(self, force=False, rerecord=False):
        """
        Recorded.delete(force=False, rerecord=False) -> retcode
                Informs backend to delete recording and all relevent data.
                'force' forces a delete if the file cannot be found.
                'rerecord' sets the file as recordable in oldrecorded
        """
        return self.getProgram().delete(force, rerecord)

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
        for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                              ('j','%d'),('d','%d')):
            path = path.replace('%o'+tag,
                    self['originalairdate'].strftime(format))
        path = path.replace('%-','-')
        path = path.replace('%%','%')
        path += '.'+self['basename'].split('.')[-1]

        # clean up for windows
        if replace is not None:
            for char in ('\\',':','*','?','"','<','>','|'):
                path = path.replace(char, replace)
        return path

class RecordedProgram( DBDataWrite, CMPRecord ):

    """
    RecordedProgram(data=None, db=None, raw=None) -> RecordedProgram object
            'data' is a tuple containing (chanid, storagegroup)
    """
    _table = 'recordedprogram'
    _where = 'chanid=%s AND starttime=%s'
    _setwheredat = 'self.chanid,self.starttime'
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
    _logmodule = 'Python RecordedProgram'

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized RecordedProgram at %s>" % hex(id(self))
        return u"<RecordedProgram '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    @classmethod
    def fromRecorded(cls, recorded):
        return cls((recorded.chanid, recorded.progstart), recorded._db)

    def create(self, data=None):
        """RecordedProgram.create(data=None) -> RecordedProgram object"""
        DBDataWrite.create(self, data)
        self._wheredat = (self.chanid, self.starttime)
        self._pull()
        return self

class OldRecorded( DBDataWrite, RECSTATUS, CMPRecord ):
    """
    OldRecorded(data=None, db=None, raw=None) -> OldRecorded object
            'data' is a tuple containing (chanid, storagegroup)
    """

    _table = 'oldrecorded'
    _where = 'chanid=%s AND starttime=%s'
    _setwheredat = 'self.chanid,self.starttime'
    _defaults = {'title':'',     'subtitle':'',      
                 'category':'',  'seriesid':'',      'programid':'',
                 'findid':0,     'recordid':0,       'station':'',
                 'rectype':0,    'duplicate':0,      'recstatus':-3,
                 'reactivate':0, 'generic':0}
    _logmodule = 'Python OldRecorded'

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized OldRecorded at %s>" % hex(id(self))
        return u"<OldRecorded '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def create(self, data=None):
        """OldRecorded.create(data=None) -> OldRecorded object"""
        DBDataWrite.create(self, data)
        self._wheredat = (self.chanid, self.starttime)
        self._pull()
        return self

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
        """OldRecorded entries can not be altered"""
        return
    def delete(self):
        """OldRecorded entries cannot be deleted"""
        return

class Job( DBDataWrite, JOBTYPE, JOBCMD, JOBFLAG, JOBSTATUS ):
    """
    Job(id=None, db=None, raw=None) -> Job object
    """

    _table = 'jobqueue'
    _where = 'id=%s'
    _setwheredat = 'self.id,'
    _logmodule = 'Python Jobqueue'
    _defaults = {'id': None}

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Job at %s>" % hex(id(self))
        return u"<Job '%s' at %s>" % (self.id, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, id=None, chanid=None, starttime=None, db=None):
            DBDataWrite.__init__(self, (id,), db)

    def create(self, data=None):
        """Job.create(data=None) -> Job object"""
        id = DBDataWrite.create(self, data)
        self._where = 'id=%s'
        self._wheredat = (id,)
        return self

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
    _table = 'channel'
    _where = 'chanid=%s'
    _setwheredat = 'self.chanid,'
    _defaults = {'icon':'none',          'videofilters':'',  'callsign':u'',
                 'xmltvid':'',           'recpriority':0,    'contrast':32768,
                 'brightness':32768,     'colour':32768,     'hue':32768,
                 'tvformat':u'Default',  'visible':1,        'outputfilters':'',
                 'useonairguide':0,      'atsc_major_chan':0,
                 'tmoffset':0,           'default_authority':'',
                 'commmethod':-1,        'atsc_minor_chan':0,
                 'last_record':datetime(1900,1,1)}
    _logmodule = 'Python Channel'

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Channel at %s>" % hex(id(self))
        return u"<Channel '%s','%s' at %s>" % \
                        (self.chanid, self.name, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, chanid=None, db=None):
        DBDataWrite.__init__(self, (chanid,), db)

    def create(self, data=None):
        """Channel.create(data=None) -> Channel object"""
        DBDataWrite.create(self, data)
        self._wheredat = (self.chanid,)
        self._pull()
        return self

class Guide( DBData, CMPRecord ):
    """
    Guide(data=None, db=None, raw=None) -> Guide object
            Data is a tuple of (chanid, starttime).
    """
    _table = 'program'
    _where = 'chanid=%s AND starttime=%s'
    _setwheredat = 'self.chanid,self.starttime'
    _logmodule = 'Python Guide'
    
    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized Guide at %s>" % hex(id(self))
        return u"<Guide '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

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
                dat[key.lower()] = datetime.strptime(
                                attrib[key],'%Y-%m-%dT%H:%M:%S')

        raw = []
        for key in db.tablefields[cls._table]:
            if key in dat:
                raw.append(dat[key])
            else:
                raw.append(None)
        return cls.fromRaw(raw, db)

#### MYTHVIDEO ####

class Video( DBDataWrite, CMPVideo ):
    """Video(id=None, db=None, raw=None) -> Video object"""
    _table = 'videometadata'
    _where = 'intid=%s'
    _setwheredat = 'self.intid,'
    _defaults = {'subtitle':u'',             'director':u'Unknown',
                 'rating':u'NR',             'inetref':u'00000000',
                 'year':1895,                'userrating':0.0,
                 'length':0,                 'showlevel':1,
                 'coverfile':u'No Cover',    'host':u'',
                 'intid':None,               'homepage':u'',
                 'watched':False,            'category':0,
                 'browse':True,              'hash':u'',
                 'season':0,                 'episode':0,
                 'releasedate':date(1,1,1),  'childid':-1,
                 'insertdate': datetime.now()}
    _logmodule = 'Python Video'
    _schema_value = 'mythvideo.DBSchemaVer'
    _schema_local = MVSCHEMA_VERSION
    _schema_name = 'MythVideo'
    _cm_toid, _cm_toname = DictInvertCI.createPair({0:'none'})

    def _fill_cm(self):
        if len(self._cm_toid) > 1:
            return
        with self._db.cursor(self._log) as cursor:
            cursor.execute("""SELECT * FROM videocategory""")
            for row in cursor:
                self._cm_toname[row[0]] = row[1]

    def _cat_toname(self):
        if self.category is not None:
            try:
                self.category = self._cm_toname[int(self.category)]
            except: pass
        else:
            self.category = 'none'

    def _cat_toid(self):
        if self.category is not None:
            try:
                if self.category.lower() not in self._cm_toid:
                    with self._db.cursor(self._log) as cursor:
                        cursor.execute("""INSERT INTO videocategory
                                          SET category=%s""",
                                      self.category)
                    self._cm_toid[self.category] = cursor.lastrowid
                self.category = self._cm_toid[self.category]
            except:
                pass
        else:
            self.category = 0

    def _setDefs(self):
        DBDataWrite._setDefs(self)

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
        DBDataWrite._postinit(self)
        self._fill_cm()
        if self._wheredat is not None:
            self._cat_toname()
            self.cast = self._Cast((self.intid,), self._db)
            self.genre = self._Genre((self.intid,), self._db)
            self.country = self._Country((self.intid,), self._db)
            self.markup = self._Markup((self.filename,), self._db)

    def __init__(self, id=None, db=None):
        DBDataWrite.__init__(self, (id,), db)

    def create(self, data=None):
        """Video.create(data=None) -> Video object"""
        c = self._db.cursor(self._log)
        fields = ' AND '.join(['%s=%%s' % f for f in \
                        ('title','subtitle','season','episode')])
        with self._db.cursor(self._log) as cursor:
            count = cursor.execute("""SELECT intid
                                      FROM videometadata
                                      WHERE %s""" % fields, 
                      (self.title, self.subtitle, self.season, self.episode))
            if count:
                id = cursor.fetchone()[0]
            else:
                self._import(data)
                self._cat_toid()
                id = DBDataWrite.create(self)
        self._wheredat = (id,)
        self._pull()
        self._postinit()
        return self

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

    def _open(self, type, mode='r',nooverwrite=False):
        """
        Open file pointer
        """
        sgroup = {  'filename':'Videos',        'banner':'Banners',
                    'coverfile':'Coverart',     'fanart':'Fanart',
                    'screenshot':'Screenshots', 'trailer':'Trailers'}
        if self._data is None:
            return None
        if type not in sgroup:
            raise MythFileError(MythError.FILE_ERROR,
                            'Invalid type passed to Video._open(): '+str(type))
        SG = self._db.getStorageGroup(sgroup[type], self.host)
        if len(SG) == 0:
            SG = self._db.getStorageGroup('Videos', self.host)
            if len(SG) == 0:
                raise MythFileError(MythError.FILE_ERROR,
                                    'Could not find MythVideo Storage Groups')
        return ftopen('myth://%s@%s/%s' % ( SG[0].groupname,
                                            self.host,
                                            self[type]),
                            mode, False, nooverwrite, self._db)

    def delete(self):
        """Video.delete() -> None"""
        if (self._where is None) or \
                (self._wheredat is None):
            return
        self.cast.clean()
        self.genre.clean()
        self.country.clean()
        DBDataWrite.delete(self)

    def open(self,mode='r',nooverwrite=False):
        """Video.open(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('filename',mode,nooverwrite)

    def openBanner(self,mode='r',nooverwrite=False):
        """Video.openBanner(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('banner',mode,nooverwrite)

    def openCoverart(self,mode='r',nooverwrite=False):
        """Video.openCoverart(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('coverfile',mode,nooverwrite)

    def openFanart(self,mode='r',nooverwrite=False):
        """Video.openFanart(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('fanart',mode,nooverwrite)

    def openScreenshot(self,mode='r',nooverwrite=False):
        """Video.openScreenshot(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('screenshot',mode,nooverwrite)

    def openTrailer(self,mode='r',nooverwrite=False):
        """Video.openTrailer(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('trailer',mode,nooverwrite)

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
        regex1 = re.compile('^(.*[^s0-9])'+sep \
                           +'(?:s|(?:Season))?'+sep \
                           +'(\d{1,4})'+sep \
                           +'(?:[ex/]|Episode)'+sep \
                           +'(\d{1,3})'+sep \
                           +'(.*)$', re.I)

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
                title = title[title.rindex('/')+1:]
        else:
            season = None
            episode = None
            subtitle = None
            title = filename[filename.rindex('/')+1:]
            for left,right in (('(',')'), ('[',']'), ('{','}')):
                while left in title:
                    lin = title.index(left)
                    rin = title.index(right,lin)
                    title = title[:lin]+title[rin+1:]
            title = title

        return (title, season, episode, subtitle)

    @classmethod
    def fromFilename(cls, filename, db=None):
        vid = cls(db=db)
        vid.filename = filename
        vid.title, vid.season, vid.episode, vid.subtitle = \
                        vid.parseFilename()
        return vid
        

class VideoGrabber( Grabber ):
    """
    VideoGrabber(mode, lang='en', db=None) -> VideoGrabber object
            'mode' can be of either 'TV' or 'Movie'
    """
    logmodule = 'Python MythVideo Grabber'
    cls = VideoMetadata

    def __init__(self, mode, lang='en', db=None):
        if mode == 'TV':
            Grabber.__init__(self, setting='mythvideo.TVGrabber', db=db)
        elif mode == 'Movie':
            Grabber.__init__(self, setting='mythvideo.MovieGrabber', db=db)
        else:
            raise MythError('Invalid MythVideo grabber')
        self._check_schema('mythvideo.DBSchemaVer',
                                MVSCHEMA_VERSION, 'MythVideo')
        self.append('-l',lang)
        self.append('-X')

#### MYTHNETVISION ####

class InternetContent( DBData ):
    _table = 'internetcontent'
    _where = 'name=%s'
    _setwheredat = 'self.name,'

class InternetContentArticles( DBData ):
    _table = 'internetcontentarticles'
    _where = 'feedtitle=%s AND title=%s AND subtitle=%s'
    _setwheredat = 'self.feedtitle,self.title,self.subtitle'

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


