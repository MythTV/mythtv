# -*- coding: utf-8 -*-
"""Provides tweaked dict-type classes."""

from MythTV.exceptions import MythError
from MythTV.utility import datetime

from datetime import date
import locale


class OrdDict( dict ):
    """
    OrdDict.__init__(raw) -> OrdDict object

    A modified dictionary, that maintains the order of items.
    Data can be accessed as attributes or items.
    Allows adding of local variables, which are not visible in the dictionary.
    In contrast to dict, default iteration is done over values, not keys.
    Supports pickling and unpickling.
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

    def __setattr__(self, name, value):
        if (name in self.__dict__) or (name in self._localvars):
            self.__dict__[name] = value
        else:
            self[name] = value

    def __delattr__(self, name):
        if name in self.__dict__:
            del self.__dict__[name]
        else:
            del self[name]

    def __iter__(self):
        return iter(self.values())

    @property
    def _field_order(self):
        return list(self.keys())

    def copy(self):
        c = self.__class__(iter(self.items()))
        for attr in self._localvars:
            try:
                c.__setattr__(attr, self.__getattr__(attr))
            except AttributeError:
                pass
        return c

    # legacy implementations, deprecated
    def iteritems(self):
        return iter(self.items())

    def iterkeys(self):
        return iter(self.keys())

    def itervalues(self):
        return iter(self.values())


class DictData( OrdDict ):
    """
    DictData.__init__(raw) --> DictData object

    Modified OrdDict, with a pre-defined item order. Allows processing of
        raw input data.
    """
    _field_order = None
    _field_type = None
    _trans = [  int,
                locale.atof,
                lambda x: bool(int(x)),
                lambda x: x,
                lambda x: datetime.fromtimestamp(x if x != '4294967295' else '0', datetime.UTCTZ())\
                                  .astimezone(datetime.localTZ()),
                lambda x: date(*[int(y) for y in x.split('-')]),
                lambda x: datetime.fromRfc(x, datetime.UTCTZ())\
                                  .astimezone(datetime.localTZ())]
    _inv_trans = [  str,
                    lambda x: locale.format_string("%0.6f", x),
                    lambda x: str(int(x)),
                    lambda x: x,
                    lambda x: str(int(x.timestamp())),
                    lambda x: x.isoformat(),
                    lambda x: x.utcrfcformat()]

    def __setattr__(self, name, value):
        if name in self._localvars:
            self.__dict__[name] = value
        elif name not in self._field_order:
            object.__setattr__(self, name, value)
        else:
            try:
                self[name] = value
            except KeyError:
                raise AttributeError(str(name))

    def __setitem__(self, key, value):
        if key not in self._field_order:
                raise KeyError(str(key))
        if self._field_type != 'Pass':
            ind = self._field_order.index(key)
            if self._field_type[ind] in (4,6):
                value = datetime.duck(value)
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
        """
        Accepts a list of data, processes according to specified types,
            and returns a dictionary
        """
        if self._field_type != 'Pass':
            if len(data) != len(self._field_type):
                raise MythError('Incorrect raw input length to DictData()')
            data = list(data)
            for i,v in enumerate(data):
                if v == '':
                    data[i] = None
                else:
                    data[i] = self._trans[self._field_type[i]](v)
        return dict(zip(self._field_order,data))

    def _deprocess(self):
        """
        Returns the internal data back out in the format
            of the original raw list.
        """
        data = list(self.values())
        if self._field_type != 'Pass':
            for i,v in enumerate(data):
                if v is None:
                    data[i] = ''
                else:
                    data[i] = self._inv_trans[self._field_type[i]](v)
        return data

    def _fillNone(self):
        """Fills out dictionary fields with empty data."""
        field_order = self._field_order
        dict.update(self, zip(field_order, [None]*len(field_order)))

    def copy(self):
        """Returns a deep copy of itself."""
        return self.__class__(iter(self.items()), _process=False)

    def __getstate__(self):
        return dict(self)

    def __setstate__(self, state):
        for k,v in iter(state.items()):
            self[k] = v


class DictInvert(dict):
    """
    DictInvert.__init__(other, mine=None) --> DictInvert object

    Creates a dictionary mirrored in reverse by another dictionary.
    """

    def __init__(self, other, mine=None):
        self.other = other
        if mine is None:
            mine = dict(zip(*reversed(list(zip(*other.items())))))
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
