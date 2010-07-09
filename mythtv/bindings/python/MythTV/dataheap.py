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
from utility import CMPRecord, CMPVideo, MARKUPLIST

import re
import locale
import xml.etree.cElementTree as etree
from time import strftime, strptime
from datetime import date, time, datetime
from socket import gethostname
from urllib import urlopen

class Record( DBDataWriteAI, RECTYPE, CMPRecord ):
    """
    Record(id=None, db=None) -> Record object
    """

    _table = 'record'
    _key   = ['recordid']
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

    def create(self, data=None):
        """Record.create(data=None) -> Record object"""
        DBDataWriteAI.create(self, data)
        FileOps(db=self._db).reschedule(self.recordid)
        return self

    def delete(self):
        DBDataWriteAI.delete(self)
        FileOps(db=self._db).reschedule(self.recordid)

    def update(self, *args, **keywords):
        DBDataWriteAI.update(*args, **keywords)
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
    Recorded(data=None, db=None) -> Recorded object
            'data' is a tuple containing (chanid, storagegroup)
    """
    _table = 'recorded'
    _key   = ['chanid','starttime']
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

    def _evalwheredat(self, wheredat=None):
        DBDataWrite._evalwheredat(self, wheredat)
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

        if self._wheredat is None:
            return
        for tag in ('title', 'subtitle', 'description'):
            if metadata[tag] and _allow_change(self, tag, overwrite):
                self[tag] = metadata[tag]
        for tagf,tagt in (('userrating','stars'),):
            if metadata[tagf] and _allow_change(self, tagt, overwrite):
                self[tagt] = metadata[tagf]
        for cast in metadata.people:
            self.cast.append((unicode(cast.name),unicode(cast.role)))
        self.update()

class RecordedProgram( DBDataWrite, CMPRecord ):

    """
    RecordedProgram(data=None, db=None) -> RecordedProgram object
            'data' is a tuple containing (chanid, storagegroup)
    """
    _table = 'recordedprogram'
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

class OldRecorded( DBDataWrite, RECSTATUS, CMPRecord ):
    """
    OldRecorded(data=None, db=None) -> OldRecorded object
            'data' is a tuple containing (chanid, storagegroup)
    """

    _table = 'oldrecorded'
    _key   = ['chanid','starttime']
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

class Job( DBDataWriteAI, JOBTYPE, JOBCMD, JOBFLAG, JOBSTATUS ):
    """
    Job(id=None, db=None) -> Job object
    """

    _table = 'jobqueue'
    _key   = ['id']
    _logmodule = 'Python Jobqueue'
    _defaults = {'id': None}

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

class Channel( DBDataWriteAI ):
    """Channel(chanid=None, db=None) -> Channel object"""
    _table = 'channel'
    _key   = ['chanid']
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

class Guide( DBData, CMPRecord ):
    """
    Guide(data=None, db=None) -> Guide object
            Data is a tuple of (chanid, starttime).
    """
    _table = 'program'
    _key   = ['chanid','starttime']
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

class Video( VideoSchema, DBDataWriteAI, CMPVideo ):
    """Video(id=None, db=None, raw=None) -> Video object"""
    _table = 'videometadata'
    _key   = ['intid']
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

    def _pull(self):
        DBDataWriteAI._pull(self)
        self._fill_cm()
        self._cat_toname()

    def _push(self):
        self._cat_toid()
        DBDataWriteAI._push(self)
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

    def _evalwheredat(self, wheredat=None):
        DBDataWriteAI._evalwheredat(self, wheredat)
        self._fill_cm()
        self._cat_toname()
        self.cast = self._Cast((self.intid,), self._db)
        self.genre = self._Genre((self.intid,), self._db)
        self.country = self._Country((self.intid,), self._db)
        self.markup = self._Markup((self.filename,), self._db)

    def create(self, data=None):
        """Video.create(data=None) -> Video object"""
        if self.host is not None:
            # check for pre-existing entry
            if self.hash == '':
                self.hash = self.getHash()
            with self._db as cursor:
                if cursor.execute("""SELECT intid FROM videometadata
                                     WHERE hash=%s""", self.hash) > 0:
                    id = cursor.fetchone()[0]
                    self._setwheredat([id])
                    self._pull()
                    return self

        # create new entry
        self._import(data)
        self._cat_toid()
        return DBDataWriteAI.create(self)

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
        if type not in sgroup:
            raise MythFileError(MythError.FILE_ERROR,
                            'Invalid type passed to Video._open(): '+str(type))
        if self.host == '':
            raise MythFileError('File access only works with Storage Group content')
        return ftopen('myth://%s@%s/%s' % ( sgroup[type],
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
        DBDataWriteAI.delete(self)

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
                title = title.rsplit('/',1)[-1]
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

        if self._wheredat is None:
            return

        for tag in ('title', 'subtitle', 'tagline', 'season', 'episode',
                    'inetref', 'homepage', 'trailer', 'userrating', 'year'):
            if metadata[tag] and _allow_change(self, tag, overwrite):
                self[tag] = metadata[tag]

        for tagf,tagt in (('description','plot'), ('runtime','length')):
            if metadata[tagf] and _allow_change(self, tagt, overwrite):
                self[tagt] = metadata[tagf]

        try:
            if _allow_change(self, 'director', overwrite):
                self.director = [person.name for person in metadata.people \
                                            if person.job=='Director'].pop(0)
        except IndexError: pass

        for actor in [person for person in metadata.people \
                                  if person.job=='Actor']:
            self.cast.add(unicode(actor.name))

        for category in metadata.categories:
            self.genre.add(unicode(category))

        if not _allow_change(self, 'host', False):
            # only perform image grabs if 'host' is set, denoting SG use
            imgtrans = {'coverart':'coverfile', 'fanart':'fanart',
                        'banner':'banner', 'screenshot':'screenshot'}
            be = FileOps(self.host, db=self._db)
            for image in metadata.images:
                if not _allow_change(self, imgtrans[image.type], overwrite):
                    continue
                type = imgtrans[image.type]
                if metadata._type == 'TV':
                    if type == 'screenshot':
                        fname = "%s Season %dx%d_%s." % \
                             (self.title, self.season, self.episode, image.type)
                    else:
                        fname = "%s Season %d_%s." % \
                             (self.title, self.season, image.type)
                else:
                    fname = "%s_%s." % (self.title, image.type)
                fname += image.url.rsplit('.',1)[1]

                self[imgtrans[image.type]] = fname
                if be.fileExists(fname, image.type.capitalize()):
                    continue
                dst = self._open(type, 'w', True)
                src = urlopen(image.url)
                dst.write(src.read())
                dst.close()
                src.close()

        self.update()
        

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
    _dbvalue = {'TV':'mythvideo.TVGrabber', 'Movie':'mythvideo.MovieGrabber'}

    def __init__(self, mode, lang='en', db=None):
        self.mode = mode
        try:
            Grabber.__init__(self, setting=self._dbvalue[mode], db=db)
        except KeyError:
            raise MythError('Invalid MythVideo grabber')
        self._check_schema('mythvideo.DBSchemaVer',
                                MVSCHEMA_VERSION, 'MythVideo')
        self.append('-l',lang)

    def command(self, *args):
        for res in Grabber.command(self, *args):
            res._type = self.mode
            yield res

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


#### MYTHMUSIC ####

class Song( MusicSchema, DBDataWriteAI ):
    _table = 'music_songs'
    _where = 'song_id=%s'
    _setwheredat = 'self.song_id,'
    _defaults = {'song_id':None}
    _logmodule = 'Python Song'

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

class Album( MusicSchema, DBDataWriteAI ):
    _table = 'music_albums'
    _where = 'album_id=%s'
    _setwheredat = 'self.album_id,'
    _defaults = {'album_id':None}
    _logmodule = 'Python Album'

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

class Artist( MusicSchema, DBDataWriteAI ):
    _table = 'music_artists'
    _where = 'artist_id=%s'
    _setwheredat = 'self.artist_id,'
    _defaults = {'artist_id':None}
    _logmodule = 'Python Artist'

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

class MusicPlaylist( MusicSchema, DBDataWriteAI ):
    _table = 'music_playlists'
    _where = 'playlist_id=%s'
    _setwheredat = 'self.playlist_id,'
    _defaults = {'playlist_id':None}
    _logmodule = 'Python Music Playlist'

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
        DBDataWriteAI._pull(self)
        self._pl_tolist()

    def _push(self):
        self._pl_tostr()
        DBDataWriteAI._push(self)
        self._pl_tolist()

    def _evalwheredat(self, wheredat=None):
        DBDataWriteAI._evalwheredat(self, wheredat)
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

class MusicDirectory( MusicSchema, DBDataWriteAI ):
    _table = 'music_directories'
    _where = 'directory_id=%s'
    _setwheredat = 'self.directory_id,'
    _defaults = {'directory_id':None}
    _logmodule = 'Python Music Directory'

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
