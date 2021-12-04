#------------------------------
# MythTV/utility/altdict.py
# Author: Raymond Wagner
# Description: Provides various custom dict-like classes
#------------------------------

# Important Note: The classes provided by this module are considered deprecated and will be
#                 removed after the release of MythTV v32!
#                 Please use the class 'OrderedDict' from the module 'collections' provided
#                 by python3.
#                 For the classes 'DictInvert' and 'DictInvertCI', please use the imports
#                 'from MythTV.altdict import DictInvert, DictInvertCI'

from builtins import map, zip
from warnings import warn

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
        dict.__delitem__(self, key)
        self._field_order.remove(key)

    def __iter__(self):
        return self.itervalues()

    def __init__(self, data=()):
        warn("Class 'OrdDict' from this module  will be removed after MythTV v32 release, "
             "use the class 'OrderedDict' from the module 'collections' provided by python3.",
             DeprecationWarning, 2)
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
        return map(self.get, self.iterkeys())

    def items(self):
        return list(self.iteritems())

    def iteritems(self):
        return zip(self.iterkeys(), self.itervalues())

    def copy(self):
        c = self.__class__(self.iteritems())
        for attr in self._localvars:
            try:
                c.__setattr__(attr, self.__getattr__(attr))
            except AttributeError:
                pass
        return c

    def clear(self):
        dict.clear(self)
        self._field_order = []

class DictInvert(dict):
    """
    DictInvert.__init__(other, mine=None) --> DictInvert object

    Creates a dictionary mirrored in reverse by another dictionary.
    """

    def __init__(self, other, mine=None):
        warn("Classes 'DictInvert' and 'DictInvertCI' from this module  will be removed "
             "after MythTV v32 release, "
             "use the imports 'from MythTV.altdict import DictInvert, DictInvertCI.",
             DeprecationWarning, 2)
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

