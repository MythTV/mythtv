#!/usr/bin/env python
# -*- coding: UTF-8 -*-
#----------------------

from urllib import urlopen
from time import time
from thread import start_new_thread, allocate_lock
from time import sleep, time
from MythTV import OrdDict
import threading
import cPickle
import Queue
import lxml
import lxml.html
import os

BASEURL = 'http://mythtv.org/wiki'

def getScripts():
    return Script.getAll()

def getPage(**kwargs):
    url = "{0}?{1}".format(BASEURL,
            '&'.join(['{0}={1}'.format(k,v) for k,v in kwargs.items()]))
    return lxml.html.parse(url).getroot()

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
    _xp_names = lxml.etree.XPath("//div[@id='bodyContent']/div[@style='background: #EFEFEF; border: 1px dashed black; padding: 5px 5px 5px 5px;']/p/b/text()")
    _xp_code = lxml.etree.XPath("//div[@id='bodyContent']/div[@style='background: #EFEFEF; border: 1px dashed black; padding: 5px 5px 5px 5px;']/pre/text()")
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
        scripts.sort()
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
        for i in reversed(range(len(text))):
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
            for i in range(len(code)):
                names[i] = str(names[i])
                if size < len(code[i]):
                    size = len(code[i])
                    name = names[i]
                code[i] = code[i].lstrip()
                code[i] = code[i].replace(u'\xa0',' ')
            self.code = dict(zip(names,code))
            if self.info.name == 'unnamed':
                self.info.name = name

    def getCategory(self, etree):
        self.category = [str(c) for c in self._xp_cat(etree)]

    def saveScript(self, name, path):
        fd = open(path,'w')
        fd.write(self.code[name])
        fd.close()

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

        path = '/tmp/mythwikiscripts.pickle'
        if os.access(path, os.F_OK):
            try:
                fd = open(path,'r')
                cls._cache = cPickle.load(fd)
                fd.close()
            except:
                os.remove(path)
                cls._cache = {}
        else:
            cls._cache = {}

    @classmethod
    def _dumpCache(cls):
        path = '/tmp/mythwikiscripts.pickle'
        try:
            fd = open(path,'w')
            cls._cache = cPickle.dump(cls._cache,fd)
            fd.close()
        except:
            os.remove(path)
            raise

