# -*- coding: utf-8 -*-

"""
Provides base classes for managing system calls.
"""

from exceptions import MythError, MythDBError
from logging import MythLog
from altdict import DictData, OrdDict
from database import DBCache

from subprocess import Popen
from socket import gethostname
import xml.etree.cElementTree as etree

class System( DBCache ):
    """
    System(path=None, setting=None, db=None) -> System object

    'path' sets the object to use a path to an external command
    'setting' pulls the external command from a setting in the database
    """
    logmodule = 'Python system call handler'

    def __init__(self, path=None, setting=None, db=None):
        DBCache.__init__(self, db=db)
        self.log = MythLog(self.logmodule, db=self)
        self.path = ''
        if path is not None:
            self.path = path
        elif setting is not None:
            host = gethostname()
            self.path = self.settings[host][setting]
            if self.path is None:
                raise MythDBError(MythError.DB_SETTING, setting, host)
        else:
            raise MythError('Invalid input to System()')
        self.returncode = 0
        self.stderr = ''

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

    def _runcmd(self, cmd):
        fd = Popen(cmd, stdout=-1, stderr=-1, shell=True)
        self.returncode = fd.wait()
        stdout,self.stderr = fd.communicate()

        if self.returncode:
            raise MythError(MythError.SYSTEM,self.returncode,cmd,self.stderr)
        return stdout

class Metadata( DictData ):
    class _subgroup_name( list ):
        def __init__(self, xml):
            list.__init__(self)
            if xml is None: return
            for item in xml.getchildren():
                self.append(item.attrib['name'])
    class _subgroup_all( list ):
        def __init__(self, xml):
            list.__init__(self)
            if xml is None: return
            for item in xml.getchildren():
                self.append(item.attrib.copy())
    class Certifications( OrdDict ):
        def __init__(self, xml):
            OrdDict.__init__(self)
            if xml is None: return
            for cert in xml.getchildren():
                self[cert.attrib['locale']] = cert.attrib['name']
    class Categories( _subgroup_name ): pass
    class Countries( _subgroup_name ): pass
    class Studios( _subgroup_name ): pass
    class People( _subgroup_all ): pass
    class Images( _subgroup_all ): pass

    def __init__(self, xml=None):
        dict.__init__(self)
        self._fillNone()
        if xml is not None:
            self._process(xml)

    def _fillNone(self):
        DictData._fillNone(self)
        for subgroup in self._groups:
            self.__dict__[subgroup] = \
                eval('self.%s(None)' % subgroup.capitalize())

    def _process(self, xml):
        for element in xml.getchildren():
            if element.tag in self:
                self[element.tag] = element.text
            if element.tag in self._groups:
                self.__dict__[element.tag] = \
                    eval('self.%s(element)' % element.tag.capitalize())

class VideoMetadata( Metadata ):
    _field_order = ['title','subtitle','tagline','description','season',
                    'episode','dvdseason','dvdepisode','inetref','imdb',
                    'tmsref','homepage','trailer','language', 'releasedate',
                    'lastupdated','userrating','popularity', 'budget',
                    'revenue','year','runtime']
    _groups = ['certifications','categories','countries',
               'studios','people','images']

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
        for item in etree.fromstring(xml).getiterator('item'):
            yield self.cls(item)
 
    def command(self, *args):
        return self._processMetadata(System.command(self, *args))

    def search(self, phrase):
        return self.command('-M', '"%s"' %phrase)

    def grabInetref(self, inetref, season=None, episode=None):
        if season and episode:
            return self.command('-D', inetref, season, episode)
        else:
            return self.command('-D', inetref)

    def grabTitle(self, title, subtitle):
        return self.command('-N', '"%s" "%s"' % (title, subtitle))

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
