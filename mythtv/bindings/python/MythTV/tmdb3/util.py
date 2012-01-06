#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: util.py    Assorted utilities used in tmdb_api
# Python Library
# Author: Raymond Wagner
#-----------------------

from copy import copy

class PagedList( list ):
    """
    List-like object, with support for automatically grabbing additional
    pages from a data source.
    """
    def __init__(self, iterable=None, count=None, perpage=20):
        super(PagedList, self).__init__()
        if iterable:
            super(PagedList, self).extend(iterable)
        super(PagedList, self).extend([None]*(count-len(self)))

        self._perpage = perpage

    def _getpage(self, page):
        raise NotImplementedError("PagedList._getpage() must be provided "+\
                                  "by subclass")

    def _populateindex(self, index):
        self._populatepage(int(index/self._perpage)+1)

    def _populatepage(self, page):
        offset = (page-1)*self._perpage
        for i,data in enumerate(self._getpage(page)):
            super(PagedList, self).__setitem__(offset+i, data)

    def __getitem__(self, index):
        item = super(PagedList, self).__getitem__(index)
        if item is None:
            self._populateindex(index)
        item = super(PagedList, self).__getitem__(index)
        return item

    def __iter__(self):
        for i in range(len(self)):
            yield self[i]

class Poller( object ):
    def __init__(self, func, lookup, inst=None):
        self.func = func
        self.lookup = lookup
        self.inst = inst
        if func:
            self.__doc__ = func.__doc__
            self.__name__ = func.__name__
            self.__module__ = func.__module__
        else:
            self.__name__ = '_populate'

    def __get__(self, inst, owner):
        if inst is None:
            return self
        func = None
        if self.func:
            func = self.func.__get__(inst, owner)
        return self.__class__(func, self.lookup, inst)

    def __call__(self):
        data = None
        if self.func:
            data = self.func()
        self.apply(data)

    def apply(self, data, set_nones=True):
        for k,v in self.lookup.items():
            if k in data:
                setattr(self.inst, v, data[k])
            elif set_nones:
                setattr(self.inst, v, None)

class Data( object ):
    def __init__(self, field, initarg=None, handler=None, poller=None, raw=True):
        self.field = field
        self.initarg = initarg
        self.poller = poller
        self.raw = raw
        self.sethandler(handler)

    def __get__(self, inst, owner):
        if inst is None:
            return self
        if self.field not in inst._data:
            if self.poller is None:
                return None
            self.poller.__get__(inst, owner)()
        return inst._data[self.field]

    def __set__(self, inst, value):
        if value:
            value = self.handler(value)
        inst._data[self.field] = value

    def sethandler(self, handler):
        if handler is None:
            self.handler = lambda x: x
        elif isinstance(handler, ElementType) and self.raw:
            self.handler = lambda x: handler(raw=x)
        else:
            self.handler = handler

class Datapoint( Data ):
    pass

class Datalist( Data ):
    def __init__(self, field, handler=None, poller=None, sort=None, raw=True):
        super(Datalist, self).__init__(field, None, handler, poller, raw)
        self.sort = sort
    def __set__(self, inst, value):
        data = []
        if value:
            for val in value:
                data.append(self.handler(val))
            if self.sort:
                data.sort(key=lambda x: getattr(x, self.sort))
        inst._data[self.field] = data

class Datadict( Data ):
    def __init__(self, field, handler=None, poller=None, raw=True, key=None, attr=None):
        if key and attr:
            raise TypeError("`key` and `attr` cannot both be defined")
        super(Datadict, self).__init__(field, None, handler, poller, raw)
        if key:
            self.getkey = lambda x: x[key]
        elif attr:
            self.getkey = lambda x: getattr(x, attr)
        else:
            raise TypeError("Datadict requires `key` or `attr` be defined "+\
                            "for populating the dictionary")
    def __set__(self, inst, value):
        data = {}
        if value:
            for val in value:
                val = self.handler(val)
                data[self.getkey(val)] = val
        inst._data[self.field] = data

class ElementType( type ):
    def __new__(mcs, name, bases, attrs):
        for base in bases:
            if isinstance(base, mcs):
                for k, attr in base.__dict__.items():
                    if (k not in attrs) and isinstance(attr, Data):
                        attr = copy(attr)
                        attr.poller = attr.poller.func
                        attrs[k] = attr

        _populate = []
        pollers = {attrs.get('_populate',None):_populate}
        args = []
        for n,attr in attrs.items():
            if isinstance(attr, Data):
                attr.name = n
                if attr.initarg:
                    args.append(attr)
                if attr.poller:
                    if attr.poller not in pollers:
                        pollers[attr.poller] = []
                    pollers[attr.poller].append(attr)
                else:
                    _populate.append(attr)
        for poller, atrs in pollers.items():
            lookup = dict([(a.field, a.name) for a in atrs])
            poller = Poller(poller, lookup)
            attrs[poller.__name__] = poller
            for a in atrs:
                a.poller = poller

        cls = type.__new__(mcs, name, bases, attrs)
        cls._InitArgs = tuple([a.name for a in sorted(args, key=lambda x: x.initarg)])
        return cls

    def __call__(cls, *args, **kwargs):
        obj = cls.__new__(cls)
        obj._data = {}
        if 'raw' in kwargs:
            if len(args) != 0:
                raise TypeError('__init__() takes exactly 2 arguments (1 given)')
            obj._populate.apply(kwargs['raw'], False)
        else:
            if len(args) != len(cls._InitArgs):
                raise TypeError('__init__() takes exactly {0} arguments ({1} given)'\
                            .format(len(cls._InitArgs)+1), len(args)+1)
            for a,v in zip(cls._InitArgs, args):
                setattr(obj, a, v)

        return obj

class Element( object ):
    __metaclass__ = ElementType
#    def __new__(cls, *args, **kwargs):
        
