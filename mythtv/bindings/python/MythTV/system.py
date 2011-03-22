# -*- coding: utf-8 -*-

"""
Provides base classes for managing system calls.
"""

from exceptions import MythError, MythDBError, MythFileError
from logging import MythLog
from altdict import DictData, OrdDict
from utility import levenshtein
from database import DBCache

from subprocess import Popen
from select import select
from lxml import etree
import shlex
import os

class System( DBCache ):
    """
    System(path=None, setting=None, db=None) -> System object

    'path' sets the object to use a path to an external command
    'setting' pulls the external command from a setting in the database
    """
    logmodule = 'Python system call handler'

    @classmethod
    def system(cls, command, db=None):
        path = command.split()[0]
        try:
            s = cls(path, db=db)
            res = s._runcmd(command)
            if len(res):
                s.log(MythLog.SYSTEM|MythLog.EXTRA, '---- Output ----', res)
            if len(s.stderr):
                s.log(MythLog.SYSTEM|MythLog.EXTRA,
                                               '---- Error  ----', s.stderr)
            return 0
        except (MythDBError,MythFileError):
            return -1
        except MythError:
            return s.returncode

    def __init__(self, path=None, setting=None, db=None, useshell=True):
        DBCache.__init__(self, db=db)
        self.log = MythLog(self.logmodule, db=self)
        self.path = None

        if setting is not None:
            # pull setting from database, substitute from argument if needed
            host = self.gethostname()
            self.path = self.settings[host][setting]
            if (self.path is None) and (path is None):
                raise MythDBError(MythError.DB_SETTING, setting, host)

        if self.path is None:
            # setting not given, use path from argument
            if path is None:
                raise MythError('Invalid input to System()')
            self.path = path

        cmd = self.path.split()[0]
        if self.path.startswith('/'):
            # test full given path
            if not os.access(cmd, os.F_OK):
                raise MythFileError('Defined executable path does not exist.')
        else:
            # search command from PATH
            for folder in os.environ['PATH'].split(':'):
                if os.access(os.path.join(folder,cmd), os.F_OK):
                    self.path = os.path.join(folder,self.path)
                    break
            else:
                raise MythFileError('Defined executable path does not exist.')
                
        self.returncode = 0
        self.stderr = ''
        self.useshell = useshell

    def __call__(self, *args): return self.command(*args)

    def __str__(self):
        return "<%s '%s' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1],
                    self.path, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def append(self, *args):
        """
        obj.append(*args) -> None

        Permenantly appends one or more strings to the command
            path, separated by spaces.
        """
        self.path += ' '+' '.join(['%s' % a for a in args])

    def command(self, *args):
        """
        obj.command(*args) -> output string

        Executes external command, adding one or more strings to the
            command during the call. If call exits with a code not
            equal to 0, a MythError will be raised. The error code and
            stderr will be available in the exception and this object
            as attributes 'returncode' and 'stderr'.
        """
        if self.path is '':
            return ''
        arg = ' '+' '.join(['%s' % a for a in args])
        return self._runcmd('%s %s' % (self.path, arg))

    def _runshell(self, cmd):
        self.log(MythLog.SYSTEM, 'Running external command', cmd)
        fd = Popen(cmd, stdout=-1, stderr=-1, shell=True)
        return self._runshared(fd)

    def _runnative(self, cmd):
        self.log(MythLog.SYSTEM, 'Running external command', cmd)
        args = shlex.split(cmd)
        fd = Popen(args, stdout=-1, stderr=-1)
        return self._runshared(fd)

    def _runshared(self, fd):
        pmap = {fd.stdout:'', fd.stderr:''}
        while fd.poll() is None:
            socks = select([fd.stdout, fd.stderr],[],[])
            for sock in socks[0]:
                pmap[sock] += sock.read()
        self.stderr = pmap[fd.stderr]

        self.returncode = fd.poll()
        if self.returncode:
            raise MythError(MythError.SYSTEM,self.returncode,cmd,self.stderr)
        return pmap[fd.stdout]

    def _runcmd(self, cmd):
        return self._runshell(cmd) if self.useshell else self._runnative(cmd)

class Metadata( DictData ):
    _global_type = {'title':3,      'subtitle':3,       'tagline':3,
                    'description':3,'season':0,         'episode':0,
                    'dvdseason':3,  'dvdepisode':3,     'albumtitle':3,
                    'system':3,     'inetref':3,        'imdb':3,
                    'tmsref':3,     'homepage':3,       'trailer':3,
                    'language':3,   'releasedate':5,    'lastupdated':6,
                    'userrating':1, 'tracnum':0,        'popularity':0,
                    'budget':0,     'revenue':0,        'year':0,
                    'runtime':0,    'runtimesecs':0}

    class _subgroup_name( list ):
        def __init__(self, xml):
            list.__init__(self)
            if xml is None: return
            for item in xml.getchildren():
                self.append(item.attrib['name'])
        def toXML(self):
            eroot = etree.Element(self.__class__.__name__.lower())
            for item in self:
                e = etree.Element(self._subname)
                e.set('name', item)
                eroot.append(e)
            return eroot

    class _subgroup_all( list ):
        def __init__(self, xml):
            list.__init__(self)
            if xml is None: return
            for item in xml.getchildren():
                self.append(OrdDict(item.attrib.items()))
        def toXML(self):
            eroot = etree.Element(self.__class__.__name__.lower())
            for item in self:
                e = etree.Element(self._subname)
                for k,v in item.items():
                    e.set(k,v)
                eroot.append(e)
            return eroot

    class Certifications( OrdDict ):
        def __init__(self, xml):
            OrdDict.__init__(self)
            if xml is None: return
            for cert in xml.getchildren():
                self[cert.attrib['locale']] = cert.attrib['name']
        def toXML(self):
            eroot = etree.Element('certifications')
            for k,v in self.items():
                e = etree.Element('certification')
                e.set('locale', k)
                e.set('name', v)
                eroot.append(e)
            return eroot

    class Categories( _subgroup_name ): _subname = 'category'
    class Countries( _subgroup_name ): _subname = 'country'
    class Studios( _subgroup_name ): _subname = 'studio'
    class People( _subgroup_all ): _subname = 'person'
    class Images( _subgroup_all ): _subname = 'image'

    def __init__(self, xml=None):
        dict.__init__(self)
        self._fillNone()
        if xml is not None:
            self._process(xml)

    def _fillNone(self):
        DictData._fillNone(self)
        for subgroup in self._groups:
            self.__dict__[subgroup] = \
                getattr(self, subgroup.capitalize())(None)

    def _process(self, xml):
        for element in xml.getchildren():
            if element.tag in self:
                if (element.text == '') or (element.text is None):
                    self[element.tag] = None
                else:
                    self[element.tag] = \
                            self._trans[self._global_type[element.tag]]\
                                (element.text)
            if element.tag in self._groups:
                self.__dict__[element.tag] = \
                    getattr(self, element.tag.capitalize())(element)

    def toXML(self):
        eroot = etree.Element('item')
        for k,v in self.items():
            if v is None:
                continue
            v = self._inv_trans[self._global_type[k]](v)
            e = etree.Element(k)
            e.text = v
            eroot.append(e)
        for group in self._groups:
            group = self.__getattr__(group)
            if len(group):
                eroot.append(group.toXML())
        return eroot

class VideoMetadata( Metadata ):
    _field_order = ['title','subtitle','tagline','description','season',
                    'episode','dvdseason','dvdepisode','inetref','imdb',
                    'tmsref','homepage','trailer','language', 'releasedate',
                    'lastupdated','userrating','popularity', 'budget',
                    'revenue','year','runtime']
    _groups = ['certifications','categories','countries',
               'studios','people','images']
    def _process(self, xml):
        Metadata._process(self, xml)
        isMovie = not (bool(self.episode) or bool(self.season))
        for image in self.images:
            if isMovie:
                image.filename = "%s_%s." % (self.title, image.type)
            else:
                if image.type == 'screenshot':
                    image.filename = "%s Season %dx%d_%s." % \
                         (self.title, self.season, self.episode, image.type)
                else:
                    image.filename = "%s Season %d_%s." % \
                         (self.title, self.season, image.type)
            image.filename += image.url.rsplit('.',1)[-1]

class MusicMetadata( Metadata ):
    _field_order = ['title','description','albumtitie','inetref','language',
                    'releasedate','lastupdated','userrating','tracnum',
                    'popularity','year','runtimesecs']
    _groups = ['certifications','categories','countries',
               'studios','people','images']

class GameMetadata( Metadata ):
    _field_order = ['title','description','system','inetref','lastupdated',
                    'popularity','year']
    _groups = ['certifications','categories','countries','studios','images']

class InternetMetadata( Metadata ):
    _field_order = ['title','subtitle','author','pubDate','description',
                    'link','player','playerargs','download','downloadargs',
                    'thumbnail','content','url','length','duration','width',
                    'height','lang','rating','country','season','episode']
    def _fillNone(self): DictData._fillNone(self)
    def _process(self, xml):
        for element in xml.getiterator():
            if element.tag in self:
                self[element.tag] = element.text
            elif element.tag.split(':')[-1] in self:
                self[element.tag.split(':')[-1]] = element.text

class Grabber( System ):
    def _processMetadata(self, xml):
        try:
            xml = etree.fromstring(xml)
        except:
            raise StopIteration

        for item in xml.getiterator('item'):
            yield self.cls(item)
 
    def command(self, *args):
        return self._processMetadata(System.command(self, *args))

    def search(self, phrase, subtitle=None, tolerance=None, func=None):
        """
        obj.search(phrase, subtitle=None, tolerance=None) -> result generator

            Returns a generator of results matching the given search
                phrase.  A secondary phrase can be given through the 
                'subtitle' parameter, and an optional levenshtein
                tolerance value can be given for filtering results.
        """
        if not func:
            if subtitle is not None:
                func = lambda p,s,r: levenshtein(r.subtitle, s)
            else:
                func = lambda p,s,r: levenshtein('%s : %s' % \
                                            (r.title,r.subtitle), p) \
                                        if r.subtitle is not None else \
                                            levenshtein(r.title, p)

        if tolerance is None:
            tolerance = int(self.db.settings.NULL.\
                                        get('MetadataLookupTolerance', 5))
        if subtitle is not None:
            res = self.command('-N', '"%s" "%s"' % (phrase, subtitle))
        else:
            res = self.command('-M', '"%s"' % phrase)

        for r in res:
            r.levenshtein = func(phrase, subtitle, r)
            if r.levenshtein > tolerance:
                continue
            yield r

    def sortedSearch(self, phrase, subtitle=None, tolerance=None):
        """
        Behaves like obj.search(), but sorts results based off 
            levenshtein distance.
        """
        return sorted(self.search(phrase, subtitle, tolerance), \
                        key=lambda r: r.levenshtein)

    def grabInetref(self, inetref, season=None, episode=None):
        """
        obj.grabInetref(inetref, season=None, episode=None) -> metadata object

            Returns a direct search for a specific movie or episode.
            'inetref' can be an existing VideoMetadata object, and
                this method will return a fully populated object.
        """
        try:
            if inetref.season and inetref.episode:
                args = (inetref.inetref, inetref.season, inetref.episode)
            else:
                args = (inetref.inetref,)
        except:
            if season and episode:
                args = (inetref, season, episode)
            else:
                args = (inetref,)
        return self.command('-D', *args).next()

    def grabTitle(self, title, subtitle):
        """legacy - do not use"""
        return self.search(title, subtitle)

class SystemEvent( System ):
    """
    SystemEvent(eventname, db=None) -> SystemEvent object

    External function handler for system event messages.
        'eventname' is the event name sent in the BACKEND_MESSAGE message.
    """
    def __init__(self, event, db=None):
        setting = 'EventCmd'+''.join(\
                            [e.capitalize() for e in event.split('_')])
        try:
            System.__init__(self, setting=setting, db=db)
        except MythError:
            # no event handler registered
            self.path = ''
        except:
            raise

    def command(self, eventdata):
        """
        obj.command(eventdata) -> output string

        Executes external command, substituting event information into the
            command string. If call exits with a code not
            equal to 0, a MythError will be raised. The error code and
            stderr will be available in the exception and this object
            as attributes 'returncode' and 'stderr'.
        """
        if self.path is '':
            return
        cmd = self.path
        if 'program' in eventdata:
            cmd = eventdata['program'].formatJob(cmd)
        for a in ('sender','cardid','secs'):
            if a in eventdata:
                cmd = cmd.replace('%%%s%%' % a, eventdata[a])
        return self._runcmd(cmd)
