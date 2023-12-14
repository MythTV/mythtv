# smolt - Fedora hardware profiler
#
# Copyright (C) 2011 Raymond Wagner <raymond@wagnerrp.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

__doc__="""This is an ordered dictionary implementation to be used to
store client data before transmission to the server."""



class OrdDict( dict ):
    """
    OrdData.__init__(raw) -> OrdData object

    A modified dictionary, that maintains the order of items.
        Data can be accessed as attributes or items.
    """

    def __new__(cls, *args, **kwargs):
        inst = super().__new__(cls, *args, **kwargs)
        inst.__dict__['_field_order'] = []
        return inst

    def __getattr__(self, name):
        try:
            return super().__getattr__(name)
        except AttributeError:
            try:
                return self[name]
            except KeyError:
                raise AttributeError(str(name))

    def __setattr__(self, name, value):
        if name in self.__dict__:
            super().__setattr__(name, value)
        else:
            self[name] = value

    def __delattr__(self, name):
        try:
            super().__delattr__(name)
        except AttributeError:
            del self[name]

    def __setitem__(self, name, value):
        if name not in self:
            self._field_order.append(name)
        super().__setitem__(name, value)

    def __delitem__(self, name):
        super().__delitem__(name)
        self._field_order.remove(key)

    def update(self, *data, **kwdata):
        if len(data) == 1:
            try:
                for k,v in data[0].items():
                    self[k] = v
            except AttributeError:
                for k,v in iter(data[0]):
                    self[k] = v
        if len(kwdata):
            for k,v in kwdata.items():
                self[k] = v

    def __iter__(self):
        return iter(self.keys())

    def iterkeys(self):
        return iter(self._field_order)

    def keys(self):
        return list(self.iterkeys())

    def itervalues(self):
        return map(self.get, iter(self.keys()))

    def values(self):
        return list(self.itervalues())

    def iteritems(self):
        return zip(iter(self.keys()), iter(self.values()))

    def items(self):
        return list(self.iteritems())

    def copy(self):
        c = self.__class__()
        for k,v in list(self.items()):
            try:
                c[k] = v.copy()
            except AttributeError:
                c[k] = v
        for k,v in list(self.__dict__.items()):
            try:
                c[k] = v.copy()
            except AttributeError:
                c.__dict__[k] = v
        return c

    def clear(self):
        super().clear()
        self._field_order = []

# This sets up a factory for urllib2.Request objects, automatically
# providing the base url, user agent, and proxy information.
# The object returned is slightly modified, with a shortcut to urlopen.
