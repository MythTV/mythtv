#!/usr/bin/env python
# -*- coding: UTF-8 -*-
#----------------------

from __future__ import with_statement

from urllib import urlopen
from time import time
from thread import start_new_thread, allocate_lock
from time import sleep, time
from MythTV import QuickDictData
import cPickle
import os

try:
    import lxml.html, lxml.etree
except:
    raise Exception("The wikiscripts module requires the 'lxml' libraries"
                    " be available for use")

BASEURL = 'http://mythtv.org/wiki'

def getURL(**kwargs):
    return '?'.join([BASEURL,
                     '&'.join(['%s=%s' % (k,v) for k,v in kwargs.items()])])

def getWhatLinksHere(page):
    root = lxml.html.parse(getURL(title='Special:WhatLinksHere',
                                  limit='500', target=page))

    links = []
    for link in root.getroot().xpath("//ul[@id='mw-whatlinkshere-list']/li"):
        links.append('_'.join(link.find('a').text.split(' ')))
    return links

def getScripts():
    scripts = []
    for link in getWhatLinksHere('Template:Script_info'):
        try:
            scripts.append(Script(link))
        except:
            pass
    if len(scripts) > 0:
        Script.sema.wait()
        scripts[0].dumpCache()
        scripts.sort()
    return scripts

class WaitingSemaphore( object ):
    def __init__(self, count):
        self._lock = allocate_lock()
        self._initial_value = count
        self._value = count
    def __enter__(self):
        self.acquire()
    def __exit__(self, extype, exvalue, trace):
        self.release()
    def acquire(self):
        while True:
            with self._lock:
                if self._value > 0:
                    self._value -= 1
                    self._acquired = True
                    return
            sleep(0.1)
    def release(self):
        with self._lock:
            self._value += 1
    def wait(self):
        while self._value < self._initial_value:
            sleep(0.1)

class Script( object ):
    cache = {}
    tmax = 4
    sema = WaitingSemaphore(tmax)
    xp_info = lxml.etree.XPath("//span[@id='script-info']/text()")
    xp_names = lxml.etree.XPath("//div[@id='bodyContent']/div[@style='background: #EFEFEF; border: 1px dashed black; padding: 5px 5px 5px 5px;']/p/b/text()")
    xp_code = lxml.etree.XPath("//div[@id='bodyContent']/div[@style='background: #EFEFEF; border: 1px dashed black; padding: 5px 5px 5px 5px;']/pre/text()")

    def __init__(self, title):
        self.url = getURL(title=title)
        if not self.fromCache(title):
            start_new_thread(self.readPage,(title,))

    def __cmp__(self, other):
        if self.info.category < other.info.category:
            return -1
        elif self.info.category > other.info.category:
            return 1
        elif self.info.name < other.info.name:
            return -1
        elif self.info.name > other.info.name:
            return 1
        else:
            return 0

    def readPage(self, title):
        with self.sema:
            page = lxml.html.parse(self.url)
            etree = page.getroot()
            self.getInfo(etree)
            self.getScript(etree)
            self.toCache(title)

    def getInfo(self, etree):
        text = self.xp_info(etree)[0].strip().split('\n')
        self.info = QuickDictData([a.split('=') for a in text])
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
            names = self.xp_names(etree)
            code = self.xp_code(etree)
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

    def saveScript(self, name, path):
        fd = open(path,'w')
        fd.write(self.code[name])
        fd.close()

    def toCache(self, title):
        self.cache[title] = QuickDictData(())
        self.cache[title].info = self.info
        self.cache[title].code = self.code
        self.cache[title].time = time()

    def fromCache(self, title):
        if len(self.cache) == 0:
            self.loadCache()
        if title not in self.cache:
            # no cache for current script
            return False
        if time()-self.cache[title].time > 1800:
            # cache has expired
            return False
        self.info = self.cache[title].info
        self.code = self.cache[title].code
        return True

    def loadCache(self):
        path = '/tmp/mythwikiscripts.pickle'
        if os.access(path, os.F_OK):
            try:
                fd = open(path,'r')
                self.cache.update(cPickle.load(fd))
                fd.close()
            except:
                os.remove(path)
                self.cache = {}

    def dumpCache(self):
        path = '/tmp/mythwikiscripts.pickle'
        try:
            fd = open(path,'w')
            self.cache = cPickle.dump(self.cache,fd)
            fd.close()
        except:
            os.remove(path)
            raise

