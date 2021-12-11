# -*- coding: UTF-8 -*-
#----------------------

try:
    from urllib import urlopen
except ImportError:
    from urllib.request import urlopen
from time import time
try:
    from thread import start_new_thread, allocate_lock
except ImportError:
    from _thread import start_new_thread, allocate_lock
from time import sleep, time
import threading
try:
    import cPickle as pickle
except:
    import pickle
from MythTV import OrdDict
try:
    import Queue
except ImportError:
    import queue as Queue
import lxml
import lxml.html
import os
import sys
import stat

from builtins import input

BASEURL = 'https://www.mythtv.org/wiki'

def getScripts():
    return Script.getAll()

def getPage(**kwargs):
    url = "{0}?{1}".format(BASEURL,
            '&'.join(['{0}={1}'.format(k,v) for k,v in list(kwargs.items())]))
    return lxml.html.parse(urlopen(url)).getroot()

def getWhatLinksHere(page):
    root = getPage(title='Special:WhatLinksHere',
                   limit='500', target=page)
    links = []
    for link in root.xpath("//ul[@id='mw-whatlinkshere-list']/li"):
        links.append('_'.join(link.find('a').text.split(' ')))
    return links

class Script( object ):
    _cache   = None
    _queue   = Queue.Queue()
    _pool    = []
    _running = True


    _valid = False
    _xp_info = lxml.etree.XPath("//span[@id='script-info']/text()")
    _xp_names = lxml.etree.XPath("//div[@id='bodyContent']/div/div[@style='background: #EFEFEF; border: 1px dashed black; padding: 5px 5px 5px 5px;']/p/b/text()")
    _xp_code = lxml.etree.XPath("//div[@id='bodyContent']/div/div[@style='background: #EFEFEF; border: 1px dashed black; padding: 5px 5px 5px 5px;']/pre/text()")
    _xp_cat = lxml.etree.XPath("//div[@id='mw-normal-catlinks']/ul/li/a/text()")

    @classmethod
    def getAll(cls, refresh=False):
        cls._running = True
        scripts = []
        try:
            for link in getWhatLinksHere('Template:Script_info'):
                scripts.append(cls(link))
            cls._wait()
        except KeyboardInterrupt:
            cls._running = False
            return []
        cls._dumpCache()
        scripts = [s for s in scripts if s.isValid()]
        #scripts.sort()   ### does not work in python3
        scripts.sort(key=lambda s: s.url)
        return scripts

    @classmethod
    def processQueue(cls):
        while cls._running:
            try:
                script = cls._queue.get(False, 0.1)
            except Queue.Empty:
                sleep(0.1)
                continue

            script.processPage()
            cls._queue.task_done()

    @classmethod
    def _wait(cls):
        cls._queue.join()
        cls._running = False
        cls._pool = []

    def __cmp__(self, other):
        if self.info.name < other.info.name:
            return -1
        elif self.info.name > other.info.name:
            return 1
        else:
            return 0

    def __repr__(self):
        return '<Script {0} at {1}>'.format(self.url, hex(id(self)))

    def __init__(self, url, refresh=False, pool=4):
        self.url = url
        if self._cache is None:
            self._loadCache(refresh)

        if (url in self._cache) and not refresh:
            self._fromCache(url)
            return

        self._queue.put(self)
        if len(self._pool) == 0:
            while pool:
                pool -= 1
                t = threading.Thread(target=self.processQueue)
                t.start()
                self._pool.append(t)

    def isValid(self): return self._valid

    def processPage(self):
        try:
            etree = getPage(title=self.url)
            self.getInfo(etree)
            self.getScript(etree)
            self.getCategory(etree)
            self._toCache(self.url)
            self._valid = True
        except:
            pass

    def getInfo(self, etree):
        text = self._xp_info(etree)[0].strip().split('\n')
        for i in reversed(list(range(len(text)))):
            if '=' not in text[i]:
                text[i-1] += text.pop(i)
        self.info = OrdDict([a.split('=') for a in text])
        if self.info.webpage == 'none':
            self.info.webpage = self.url

    def getScript(self, etree):
        if 'http://' in self.info.file:
            fp = urlopen(self.info.file)
            code = fp.read()
            fp.close()
            name = self.info.file.rsplit('/',1)[1].split('?',1)[0]
            self.code = {name: code}
            if self.info.name == 'unnamed':
                self.info.name = name
        else:
            names = self._xp_names(etree)
            code = self._xp_code(etree)
            name = ''
            size = 0
            for i in list(range(len(code))):
                names[i] = str(names[i])
                if size < len(code[i]):
                    size = len(code[i])
                    name = names[i]
                code[i] = code[i].lstrip()
                code[i] = code[i].replace('\xa0',' ')
            self.code = dict(list(zip(names,code)))
            if self.info.name == 'unnamed':
                self.info.name = name

    def getCategory(self, etree):
        self.category = [str(c) for c in self._xp_cat(etree)]

    def saveScript(self, name, path):
        fd = open(path,'w')
        fd.write(self.code[name])
        fd.close()
        st = os.stat(path)
        os.chmod(path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    def _toCache(self, title):
        self._cache[title] = OrdDict(())
        self._cache[title].info     = self.info
        self._cache[title].code     = self.code
        self._cache[title].category = self.category
        self._cache[title].time     = time()

    def _fromCache(self, title):
        if time()-self._cache[title].time > 1800:
            # cache has expired
            return False
        self.info     = self._cache[title].info
        self.code     = self._cache[title].code
        self.category = self._cache[title].category
        self._valid = True
        return True

    @classmethod
    def _loadCache(cls, refresh):
        if refresh:
            cls._cache = {}
            return
        if (sys.version_info[0] == 2):
            path = '/tmp/mythwikiscripts.pickle'
            fmode = 'r'
        else:
            path = '/tmp/mythwikiscripts.pickle3'
            fmode = 'rb'
        if os.access(path, os.F_OK):
            try:
                fd = open(path, fmode)
                cls._cache = pickle.load(fd)
                fd.close()
            except:
                os.remove(path)
                cls._cache = {}
        else:
            cls._cache = {}

    @classmethod
    def _dumpCache(cls):
        ### XXX ToDo allign pickle protocol versions
        try:
            if (sys.version_info[0] == 2):
                path = '/tmp/mythwikiscripts.pickle'
                fd = open(path,'w')
                cls._cache = pickle.dump(cls._cache,fd)
            else:
                path = '/tmp/mythwikiscripts.pickle3'
                fd = open(path,'wb')
                cls._cache = pickle.dump(cls._cache,fd,  protocol=1)
            fd.close()
        except:
            os.remove(path)
            raise

