#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: cache_engine.py
# Python Library
# Author: Raymond Wagner
# Purpose: Base cache engine class for collecting registered engines
#-----------------------

class Engines( object ):
    def __init__(self):
        self._engines = {}
    def register(self, engine):
        self._engines[engine.__name__] = engine
        self._engines[engine.name] = engine
    def __getitem__(self, key):
        return self._engines[key]
Engines = Engines()

class CacheEngineType( type ):
    """
    Cache Engine Metaclass that registers new engines against the cache
    for named selection and use.
    """
    def __init__(mcs, name, bases, attrs):
        super(CacheEngineType, mcs).__init__(name, bases, attrs)
        if name != 'CacheEngine':
            # skip base class
            Engines.register(mcs)

class CacheEngine( object ):
    __metaclass__ = CacheEngineType

    name = 'unspecified'
    def __init__(self, parent):
        self.parent = parent
    def configure(self):
        raise RuntimeError
    def get(self, key):
        raise RuntimeError
    def put(self, key, value, lifetime):
        raise RuntimeError
    def expire(self, key):
        raise RuntimeError

