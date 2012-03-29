#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: cache_file.py
# Python Library
# Author: Raymond Wagner
# Purpose: Persistant file-backed cache using /tmp/ to share data
#          using flock or msvcrt.locking to allow safe concurrent
#          access.
#-----------------------

import struct
import errno
import json
import time
import os

from cache_engine import CacheEngine

def _donothing(*args, **kwargs):
    pass

try:
    import fcntl
    class Flock( object ):
        """
        Context manager to flock file for the duration the object exists.
        Referenced file will be automatically unflocked as the interpreter
        exits the context.
        Supports an optional callback to process the error and optionally
        suppress it.
        """
        LOCK_EX = fcntl.LOCK_EX
        LOCK_SH = fcntl.LOCK_SH

        def __init__(self, fileobj, operation, callback=None):
            self.fileobj = fileobj
            self.operation = operation
            self.callback = callback
        def __enter__(self):
            fcntl.flock(self.fileobj, self.operation)
        def __exit__(self, exc_type, exc_value, exc_tb):
            suppress = False
            if callable(self.callback):
                suppress = self.callback(exc_type, exc_value, exc_tb)
            fcntl.flock(self.fileobj, fcntl.LOCK_UN)
            return suppress
except ImportError:
    import msvcrt
    class Flock( object ):
        LOCK_EX = msvcrt.NBLCK
        LOCK_SH = msvcrt.NBLCK

        def __init__(self, fileobj, operation, callback=None):
            self.fileobj = fileobj
            self.operation = operation
            self.callback = callback
        def __enter__(self):
            self.size = os.path.getsize(self.fileobj.name)
            msvcrt.locking(self.fileobj.fileno(), self.operation, self.size)
        def __exit__(self, exc_type, exc_value, exc_tb):
            suppress = False
            if callable(self.callback):
                suppress = self.callback(exc_type, exc_value, exc_tb)
            msvcrt.locking(self.fileobj.fileno(), msvcrt.LK_UNLCK, self.size)
            return suppress

class FileEngine( CacheEngine ):
    """Simple file-backed engine."""
    name = 'file'
    __time_struct = struct.Struct('d') # double placed at start of file to
                                       # check for updated data

    def __init__(self, parent):
        super(FileEngine, self).__init__(parent)
        self.configure(None)

    def configure(self, filename):
        self.cachefile = filename
        self.cacheage = 0
        self.cache = None

    def _init_cache(self):
        # only run this once
        self._init_cache = _donothing

        if self.cachefile is None:
            raise TMDBCacheError("No cache filename given.")
        if self.cachefile.startswith('~'):
            self.cachefile = os.path.expanduser(self.cachefile)
        elif not self.cachefile.startswith('/'):
            self.cachefile = '/tmp/' + self.cachefile

        try:
            # attempt to read existing cache at filename
            # handle any errors that occur
            self._read()
            # seems to have read fine, make sure we have write access
            if not os.access(self.cachefile, os.W_OK):
                raise TMDBCacheWriteError(self.cachefile)

        except IOError as e:
            if e.errno == errno.ENOENT:
                # file does not exist, create a new one
                self.cache = {}
                try:
                    self._write()
                except IOError as e:
                    if e.errno == errno.ENOENT:
                        # directory does not exist
                        raise TMDBCacheDirectoryError(self.cachefile)
                    elif e.errno == errno.EACCES:
                        # user does not have rights to create new file
                        raise TMDBCacheWriteError(self.cachefile)
                    else:
                        # let the unhandled error continue through
                        raise
            elif e.errno == errno.EACCESS:
                # file exists, but we do not have permission to access it
                raise TMDBCacheReadError(self.cachefile)
            else:
                # let the unhandled error continue through
                raise

    def _open(self, mode='r'):
        # enforce binary operation
        mode += 'b'
        try:
            if self.cachefd.mode == mode:
                # already opened in requested mode, nothing to do
                self.cachefd.seek(0)
                return
        except: pass # catch issue of no cachefile yet opened
        self.cachefd = open(self.cachefile, mode)


    def _read(self):
        self._init_cache()
        self._open('r')
        
        with Flock(self.cachefd, Flock.LOCK_SH): # lock for shared access
            try:
                age = self.__time_struct.unpack(self.cachefd.read(8))
            except:
                # failed to read age, ignore and we'll clean up when
                # it gets rewritten
                if self.cache is None:
                    self.cache = {}
                return 

            if self.cacheage >= age:
                # local copy is sufficiently new, no need to read
                return
            # read remaining data from file
            self.cacheage = age
            self.cache = json.load(self.cachefd)

    def _write(self):
        self._init_cache()
        # WARNING: this does no testing to ensure this instance has the newest
        # copy of the file cache
        self._open('w')
        # the slight delay between truncating the file with 'w' and flocking
        # could cause problems with another instance simultaneously trying to
        # read the timestamp
        with Flock(self.cachefd, Flock.LOCK_EX): # lock for exclusive access
            # filter expired data from cache
            # running this while flocked means the file is locked for additional
            # time, however we do not want anyone else writing to the file
            # before we write our stuff
            self.expire()
            self.cacheage = time.time()
            self.cachefd.write(self.__time_struct.pack(self.cacheage))
            json.dump(self.cache, self.cachefd)

    def get(self, key):
        self._read()
        if (key in self.cache) and (time.time() < self.cache[key][0]):
            return self.cache[key][1]
        return None

    def put(self, key, value, lifetime):
        self._read()
        self.cache[key] = (time.time()+lifetime, value)
        self._write()

    def expire(self, key=None):
        t = time.time()
        for k,v in self.cache.items():
            if v[0] < t: # expiration has passed
                del self.cache[k]

