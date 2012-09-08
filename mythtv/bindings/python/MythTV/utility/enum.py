#------------------------------
# MythTV/utility/enum.py
# Author: Raymond Wagner
# Description: Provides a base type and class to emulate
#              standard ENUM behavior, and proper bitwise
#              operation.
#------------------------------

from abc import ABCMeta
class number( object ):
    __metaclass__ = ABCMeta
number.register(int)
number.register(long)

class EnumValue( object ):
    _next = 0
    _storage = []
    def __init__(self, name, value=None, friendly_name=None):
        self.name = self.friendly = name
        if friendly_name:
            self.friendly = friendly_name
        self.value = self._incr(value)
        self._storage.append(self)

    @classmethod
    def _incr(cls, value):
        if value is not None:
            if cls._next:
                # this should be zero
                raise RuntimeError(("Cannot mix specified and auto-increment "
                                    "values in same Enum"))
            return value
        # increment and continue
        self._next += 1
        return self._next-1

class EnumType( type ):
    def __new__(mcs, name, bases, attrs):
        for k,v in attrs.items():
            if isinstance(v, number):
                EnumValue(k, v)
                del attrs[k]
        values = {}
        values.update([(v.name, v) for v in EnumValue._storage])
        values.update([(v.value, v) for v in EnumValue._storage])
        EnumValue._storage = []
        EnumValue._next = 0

        cls = type.__new__(mcs, name, bases, attrs)
        cls._values = values
        return cls

    def __getattr__(cls, key):
        if key in cls._values:
            return cls(cls._values[key].value)
        raise AttributeError(key)

class BaseEnum( object ):
    __metaclass__ = EnumType

    def __init__(self, mode):
        self.mode = mode

    def __int__(self): return self.mode
    def __eq__(self, other): return (self.mode == int(other))
    def __ne__(self, other): return (self.mode != int(other))

class Enum( BaseEnum ):
    def __str__(self):
        return self._values[self.mode].name

    def __repr__(self):
        return "<{0.__class__.__name__} '{0.friendly}'>".format(self)

    def __lt__(self, other): return (self.mode < int(other))
    def __le__(self, other): return (self.mode <= int(other))
    def __gt__(self, other): return (self.mode > int(other))
    def __ge__(self, other): return (self.mode >= int(other))

    @property
    def friendly(self):
        return self._values[self.mode].friendly

class BitwiseEnum( BaseEnum ):
    def __or__(self, other): return self.__class__(self.mode | int(other))
    def __ror__(self, other): return self|other
    def __ior__(self, other): return self._set_(self|other)

    def __and__(self, other): return self.__class__(self.mode & int(other))
    def __rand__(self, other): return self&other
    def __iand__(self, other): return self._set_(self&other)

    def __xor__(self, other): return self.__class__(self.mode ^ int(other))
    def __rxor__(self, other): return self^other
    def __ixor__(self, other): return self._set_(self^other)

    def _set_(self, other):
        self.mode = other.mode
        return self

    def __iter__(self):
        if self.mode == 0:
            yield self.__class__(0)
        else:
            mode = self.mode
            for m in self._values.values():
                if m.value == 0: continue
                if (m.value&mode) == m.value:
                    mode -= m.value
                    yield self.__class__(m.value)

    def __str__(self):
        modes = []
        for mode in self:
            modes.append(self._values[mode.mode].friendly)
        return '|'.join(modes)

    def __repr__(self):
        return "<{0.__class__.__name__} {0}>".format(self)

