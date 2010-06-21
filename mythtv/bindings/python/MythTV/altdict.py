# -*- coding: utf-8 -*-
"""Provides tweaked dict-type classes."""

from exceptions import MythError

from itertools import imap, izip
from datetime import datetime
from time import mktime
import locale

class OrdDict( dict ):
    """
    OrdData.__init__(raw) -> OrdData object

    A modified dictionary, that maintains the order of items.
        Data can be accessed as attributes or items.
    """

    _localvars = ['_field_order']
    def __getattr__(self, name):
        if name in self.__dict__:
            return self.__dict__[name]
        else:
            try:
                return self[name]
            except KeyError:
                raise AttributeError(str(name))

#    def __getitem__(self, key):

    def __setattr__(self, name, value):
        if (name in self.__dict__) or (name in self._localvars):
            self.__dict__[name] = value
        else:
            self[name] = value

    def __setitem__(self, key, value):
        if key not in self:
            self._field_order.append(key)
        dict.__setitem__(self, key, value)

    def __delattr__(self, name):
        if name in self.__dict__:
            del self.__dict__[name]
        else:
            del self[name]

    def __delitem__(self, key):
        dict.__delitem(self, key)
        self._field_order.remove(key)

    def __iter__(self):
        return self.itervalues()

    def __init__(self, data=()):
        dict.__init__(self)
        self._field_order = []
        for k,v in data:
            self[k] = v

    def keys(self):
        return list(self.iterkeys())

    def iterkeys(self):
        return iter(self._field_order)

    def values(self):
        return list(self.itervalues())        

    def itervalues(self):
        return imap(self.get, self.iterkeys())

    def items(self):
        return list(self.iteritems())

    def iteritems(self):
        return izip(self.iterkeys(), self.itervalues())

    def copy(self):
        c = self.__class__(self.iteritems())
        for attr in self._localvars:
            try:
                c.__setattr__(attr, self.__getattr__(attr))
            except AttributeError:
                pass
        return c

class DictData( OrdDict ):
    """
    DictData.__init__(raw) --> DictData object

    Modified OrdDict, with a pre-defined item order. Allows processing of
        raw input data.
    """
    def __setattr__(self, name, value):
        if name in self._localvars:
            self.__dict__[name] = value
        elif name not in self._field_order:
            self.__dict__[name] = value
        else:
            try:
                self[name] = value
            except KeyError:
                raise AttributeError(str(name))

    def __setitem__(self, key, value):
        if key not in self._field_order:
                raise KeyError(str(name))
        dict.__setitem__(self, key, value)

    def __delattr__(self, name):
        if name in self.__dict__:
            del self.__dict__[name]
        else:
            raise AttributeError(str(name))

    def __delitem__(self, name): raise NotImplementedError

    def __init__(self, data, _process=True):
        dict.__init__(self)
        if _process:
            data = self._process(data)
        dict.update(self, data)

    def _process(self, data):
        if self._field_type != 'Pass':
            if len(data) != len(self._field_type):
                raise MythError('Incorrect raw input length to DictData()')
            data = list(data)
            for i in xrange(len(data)):
                if data[i] == '':
                    data[i] = None
                elif self._field_type[i] == 0:
                    data[i] = int(data[i])
                elif self._field_type[i] == 1:
                    data[i] = locale.atof(data[i])
                elif self._field_type[i] == 2:
                    data[i] = bool(data[i])
                elif self._field_type[i] == 3:
                    data[i] = data[i]
                elif self._field_type[i] == 4:
                    data[i] = datetime.fromtimestamp(int(data[i]))
        return dict(zip(self._field_order,data))

    def _deprocess(self):
        data = self.values()
        if self._field_type != 'Pass':
            for i in xrange(len(data)):
                if data[i] is None:
                    data[i] = ''
                elif self._field_type == 'Pass':
                    pass
                elif self._field_type[i] == 0:
                    data[i] = str(data[i])
                elif self._field_type[i] == 1:
                    data[i] = locale.format("%0.6f", data[i])
                elif self._field_type[i] == 2:
                    data[i] = str(int(data[i]))
                elif self._field_type[i] == 3:
                    pass
                elif self._field_type[i] == 4:
                    data[i] = str(int(mktime(data[i].timetuple())))
        return data

    def _fillNone(self):
        """Fills out dictionary fields with empty data"""
        field_order = self._field_order
        dict.update(self, zip(field_order, [None]*len(field_order)))

    def copy(self):
        return self.__class__(zip(self.iteritems()), _process=False)

class DictInvert(dict):
    """
    DictInvert.__init__(other, mine=None) --> DictInvert object

    Creates a dictionary mirrored in reverse by another dictionary.
    """

    def __init__(self, other, mine=None):
        self.other = other
        if mine is None:
            mine = dict(zip(*reversed(zip(*other.items()))))
        dict.__init__(self, mine)

    @classmethod
    def createPair(cls, d):
        a = cls(d)
        b = cls(a, d)
        a.other = b
        return a,b

    def __setitem__(self, key, value):
        dict.__setitem__(self, key, value)
        dict.__setitem__(self.other, value, key)

    def __delitem__(self, key):
        dict.__delitem__(self.other, self[key])
        dict.__delitem__(self, key)

class DictInvertCI( DictInvert ):
    """
    Case insensitive version of DictInvert
    """

    def __getitem__(self, key):
        try:
            return dict.__getitem__(self, key.lower())
        except AttributeError:
            return dict.__getitem__(self, key)

    def __setitem__(self, key, value):
        try:
            dict.__setitem__(self, key.lower(), value)
        except AttributeError:
            dict.__setitem__(self, key, value)
        try:
            dict.__setitem__(self.other, value.lower(), key)
        except AttributeError:
            dict.__setitem__(self.other, value, key)

    def __delitem__(self, key):
        try:
            key = key.lower()
        except AttributeError:
            pass
        try:
            value = self[key].lower()
        except AttributeError:
            value = self[key]
        dict.__delitem__(self.other, value)
        dict.__delitem__(self, key)

    def __contains__(self, key):
        try:
            return dict.__contains__(self, key.lower())
        except AttributeError:
            return dict.__contains__(self, key)
