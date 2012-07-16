#------------------------------
# MythTV/utility/mixin.py
# Author: Raymond Wagner
# Description: Provides mixin's for assorted methods
#------------------------------

class CMPVideo( object ):
    """
    Utility class providing comparison operators between data objects
        containing video metadata.  
    """
    _lt__ = None
    _gt__ = None
    _eq__ = None

    def __le__(self, other): return (self<other) or (self==other)
    def __ge__(self, other): return (self>other) or (self==other)
    def __ne__(self, other): return not self==other

    def __lt__(self, other):
        """
        If season and episode are defined for both and inetref matches, use
            those. Otherwise, use title and subtitle
        """
        try:
            # assume table has already been populated for other type, and
            # perform comparison using stored lambda function
            return self._lt__[type(other)](self, other)
        except TypeError:
            # class has not yet been configured yet
            # create dictionary for storage of type comparisons
            self.__class__._lt__ = {}
        except KeyError:
            # key error means no stored comparison function for other type
            # pass through so it can be built
            # all other errors such as NotImplemented should continue upstream
            pass

        attrs = ('inetref', 'season', 'episode')
        null = ('00000000', '0', '')

        check = None
        if all(hasattr(self, a) for a in attrs) and \
                all(hasattr(other, a) for a in attrs):
            # do a match using all of inetref, season, and episode
            # use title/subtitle instead if inetref is undefined
            check = lambda s,o: (s.title, s.subtitle) < \
                                (o.title, o.subtitle) \
                                if \
                                    ((s.inetref in null) or \
                                     (o.inetref in null)) \
                                else \
                                    False \
                                    if \
                                        s.inetref != o.inetref \
                                    else \
                                        [s[a] for a in attrs] < \
                                        [o[a] for a in attrs]
        elif hasattr(self, 'inetref') and hasattr(other, 'inetref'):
            # match using inetref only
            check = lambda s,o: s.inetref < o.inetref \
                                if \
                                    ((s.inetref not in null) and \
                                     (o.inetref not in null)) \
                                else \
                                    (s.title, s.subtitle) < \
                                    (o.title, o.subtitle)
        elif hasattr(self, 'title') and hasattr(self, 'subtitle') and \
                hasattr(other, 'title') and hasattr(other, 'subtitle'):
            # fall through to title and subtitle
            check = lambda s,o: (s.title, s.subtitle) < (o.title, o.subtitle)
        else:
            # no matching options, use ids
            check = lambda s,o: id(s) < id(o)

        # store comparison and return result
        self._lt__[type(other)] = check
        return check(self, other)

    def __eq__(self, other):
        """
        If inetref is defined, following through to season and episode, use
            those.  Otherwise, fall back to title and subtitle.
        """
        try:
            return self._eq__[type(other)](self, other)
        except TypeError:
            self.__class__._eq__ = {}
        except KeyError:
            pass

        attrs = ('inetref', 'season', 'episode')
        null = ('00000000', '0', '')

        check = None
        if all(hasattr(self, a) for a in attrs) and \
                all(hasattr(other, a) for a in attrs):
            check = lambda s,o: [s[a] for a in attrs] == \
                                [o[a] for a in attrs] \
                                if \
                                    ((s.inetref not in null) and \
                                     (o.inetref not in null)) \
                                else \
                                    (s.title, s.subtitle) == \
                                    (o.title, o.subtitle)
        elif hasattr(self, 'inetref') and hasattr(other, 'inetref'):
            check = lambda s,o: s.inetref == o.inetref \
                                if \
                                    ((s.inetref not in null) and \
                                     (o.inetref not in null)) \
                                else \
                                    (s.title, s.subtitle) == \
                                    (o.title, o.subtitle)
        elif hasattr(self, 'title') and hasattr(self, 'subtitle') and \
                hasattr(other, 'title') and hasattr(other, 'subtitle'):
            # fall through to title and subtitle
            check = lambda s,o: (s.title, s.subtitle) == (o.title, o.subtitle)
        else:
            # no matching options, use ids
            check = lambda s,o: id(s) == id(o)

        # store comparison and return result
        self._eq__[type(other)] = check
        return check(self, other)

    def __gt__(self, other):
        """
        If season and episode are defined for both and inetref matches, use
            those. Otherwise, use title and subtitle
        """
        try:
            return self._gt__[type(other)](self, other)
        except TypeError:
            self.__class__._gt__ = {}
        except KeyError:
            pass

        attrs = ('inetref', 'season', 'episode')
        null = ('00000000', '0', '')

        check = None
        if all(hasattr(self, a) for a in attrs) and \
                all(hasattr(other, a) for a in attrs):
            check = lambda s,o: (s.title, s.subtitle) > \
                                (o.title, o.subtitle) \
                                if \
                                    ((s.inetref in null) or \
                                     (o.inetref in null)) \
                                else \
                                    False \
                                    if \
                                        s.inetref != o.inetref \
                                    else \
                                        [s[a] for a in attrs] > \
                                        [o[a] for a in attrs]
        elif hasattr(self, 'inetref') and hasattr(other, 'inetref'):
            check = lambda s,o: s.inetref > o.inetref \
                                if \
                                    ((s.inetref not in null) and \
                                     (o.inetref not in null)) \
                                else \
                                    (s.title, s.subtitle) > \
                                    (o.title, o.subtitle)
        elif hasattr(self, 'title') and hasattr(self, 'subtitle') and \
                hasattr(other, 'title') and hasattr(other, 'subtitle'):
            # fall through to title and subtitle
            check = lambda s,o: (s.title, s.subtitle) > (o.title, o.subtitle)
        else:
            # no matching options, use ids
            check = lambda s,o: id(s) > id(o)

        # store comparison and return result
        self._gt__[type(other)] = check
        return check(self, other)

class CMPRecord( CMPVideo ):
    """
    Utility class providing comparison operators between data objects
        containing video metadata.
    """
    @staticmethod
    def __choose_comparison(self, other):
        l = None
        r = None
        if (hasattr(self, 'recstartts') or hasattr(self, 'progstart')) and \
                (hasattr(other, 'recstartts') or hasattr(other, 'progstart')):
            if hasattr(self, 'recstartts'):
                l = 'recstartts'
            else:
                l = 'starttime'
            if hasattr(other, 'recstartts'):
                r = 'recstartts'
            else:
                r = 'starttime'
        # try to run comparison on program start second
        elif (hasattr(self, 'progstart') or hasattr(self, 'starttime')) and \
                (hasattr(other, 'progstart') or hasattr(other, 'starttime')):
            if hasattr(self, 'progstart'):
                l = 'progstart'
            else:
                l = 'starttime'
            if hasattr(other, 'progstart'):
                r = 'progstart'
            else:
                r = 'starttime'
        return l,r

    def __lt__(self, other):
        try:
            # assume table has already been populated for other type, and
            # perform comparison using stored lambda function
            return self._lt__[type(other)](self, other)
        except TypeError:
            # class has not yet been configured yet
            # create dictionary for storage of type comparisons
            self.__class__._lt__ = {}
        except KeyError:
            # key error means no stored comparison function for other type
            # pass through so it can be built
            # all other errors such as NotImplemented should continue upstream
            pass

        if not (hasattr(self, 'chanid') or hasattr(other, 'chanid')):
            # either object is not a recording-related object
            # fall through to video matching
            return super(CMPRecord, self).__lt__(other)

        # select attributes to use for comparison
        l,r = self.__choose_comparison(self, other)
        if None in (l,r):
            return super(CMPRecord, self).__lt__(other)

        # generate comparison function, and return result
        check = lambda s,o: (s.chanid, s[l]) < (o.chanid, o[l])
        self._lt__[type(other)] = check
        return check(self, other)

    def __eq__(self, other):
        try:
            return self._eq__[type(other)](self, other)
        except TypeError:
            self.__class__._eq__ = {}
        except KeyError:
            pass

        if not (hasattr(self, 'chanid') or hasattr(other, 'chanid')):
            return super(CMPRecord, self).__eq__(other)

        l,r = self.__choose_comparison(self, other)
        if None in (l,r):
            return super(CMPRecord, self).__eq__(other)

        check = lambda s,o: (s.chanid, s[l]) == (o.chanid, o[l])
        self._eq__[type(other)] = check
        return check(self, other)

    def __gt__(self, other):
        try:
            return self._gt__[type(other)](self, other)
        except TypeError:
            self.__class__._gt__ = {}
        except KeyError:
            pass

        if not (hasattr(self, 'chanid') or hasattr(other, 'chanid')):
            return super(CMPRecord, self).__gt__(other)

        l,r = self.__choose_comparison(self, other)
        if None in (l,r):
            return super(CMPRecord, self).__gt__(other)

        check = lambda s,o: (s.chanid, s[l]) > (o.chanid, o[l])
        self._gt__[type(other)] = check
        return check(self, other)

