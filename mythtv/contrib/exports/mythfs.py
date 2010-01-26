#!/usr/bin/env python

try:
    import _find_fuse_parts
except ImportError:
    pass
import fuse, re, errno, stat, os, sys
from fuse import Fuse
from time import time, mktime
from datetime import date
from MythTV import MythDB, MythVideo, ftopen, MythBE, Video, Recorded, MythLog

if not hasattr(fuse, '__version__'):
    raise RuntimeError, \
        "your fuse-py doesn't know of fuse.__version__, probably it's too old."

fuse.fuse_python_api = (0, 2)

TIMEOUT = 30
MythLog._setlevel('none')

class URI( object ):
    def __init__(self, uri):
        self.uri = uri
    def open(self, mode='r'):
        return ftopen(self.uri, mode)

class Attr(fuse.Stat):
    def __init__(self):
        self.st_mode = 0
        self.st_ino = 0
        self.st_dev = 0
        self.st_blksize = 0
        self.st_nlink = 1
        self.st_uid = os.getuid()
        self.st_gid = os.getgid()
        self.st_rdev = 0
        self.st_size = 0
        self.st_atime = 0
        self.st_mtime = 0
        self.st_ctime = 0

class File( object ):
    def __repr__(self):
        return str(self.path)

    def __init__(self, path='', data=None):
        self.db = None
        self.time = time()
        self.data = data
        self.children = {}
        self.attr = Attr()
        self.path = path
        self.attr.st_ino = 0
        if self.data:   # file
            self.attr.st_mode = stat.S_IFREG | 0444
            self.fillAttr()
        else:           # directory
            self.attr.st_mode = stat.S_IFDIR | 0555
            self.path += '/'

    def update(self): pass
        # refresh data and update attributes
    def fillAttr(self): pass
        # define attributes with existing data
    def getPath(self, data): return []
        # produce split path list from video object
    def getData(self, full=False, single=None):
        if full:
            # return zip(objects, ids)
            return (None, None)
        elif single:
            # return single video object
            return None
        else:
            # return current list of ids
            return None

    def populate(self):
        if len(self.children) == 0:
            # run first population
            self.pathlist = {}
            for data,id in self.getData(full=True):
                path = self.getPath(data)
                self.add(path, data)
                self.pathlist[id] = path
        else:
            if (time() - self.time) > 30:
                self.time = time()
                # run maintenance
                curlist = self.pathlist.keys()
                newlist = self.getData()
                for i in range(len(curlist)-1, -1, -1):
                    # filter unchanged entries
                    if curlist[i] in newlist:
                        del newlist[newlist.index(curlist[i])]
                        del curlist[i]
                for id in curlist:
                    self.delete(self.pathlist[id])
                    del self.pathlist[id]
                for id in newlist:
                    data = self.getData(single=id)
                    path = self.getPath(data)
                    self.add(path, data)
                    self.pathlist[id] = path
                self.time = time()

    def add(self, path, data):
        # walk down list of folders
        name = path[0]
        if len(path) == 1: # creating file
            if name not in self.children:
                self.children[name] = self.__class__(self.path+name, data)
            else:
                count = 0
                oldname = name.rsplit('.', 1)
                while True:
                    count += 1
                    name = '%s (%d).%s' % (oldname[0], count, oldname[1])
                    if name not in self.children:
                        self.children[name] = \
                                    self.__class__(self.path+name, data)
                        break
        else: # creating folder
            if name not in self.children:
                self.children[name] = self.__class__(self.path+name)
            self.children[name].add(path[1:], data)
        if self.children[name].attr.st_ctime > self.attr.st_ctime:
            self.attr.st_ctime = self.children[name].attr.st_ctime
        if self.children[name].attr.st_mtime > self.attr.st_mtime:
            self.attr.st_mtime = self.children[name].attr.st_mtime
        if self.children[name].attr.st_atime > self.attr.st_atime:
            self.attr.st_atime = self.children[name].attr.st_atime
        self.attr.st_size = len(self.children)

    def delete(self, path):
        name = str(path[0])
        if len(path) == 1:
            del self.children[name]
            self.attr.st_size += -1
        else:
            self.children[name].delete(path[1:])
            if self.children[name].attr.st_size == 0:
                del self.children[name]
                self.attr.st_size += -1

    def getObj(self, path):
        # returns object at end of list of folders 
        if path[0] == '': # root folder
            return self
        elif path[0] not in self.children: # no file
            return None
        elif len(path) == 1: # file object
            self.children[path[0]].update()
            return self.children[path[0]]
        else: # recurse through folder
            return self.children[path[0]].getObj(path[1:])

    def getAttr(self, path):
        f = self.getObj(path)
        if f is None:
            return None
        else:
            return f.attr

    def open(self, path):
        f = self.getObj(path)
        if f is None:
            return None
        else:
            return f.data.open()

    def addStr(self, path, data=None):
        self.add(path.lstrip('/').split('/'), data)

    def getObjStr(self, path):
        self.populate()
        return self.getObj(path.lstrip('/').split('/'))

    def getAttrStr(self, path):
        self.populate()
        return self.getAttr(path.lstrip('/').split('/'))

    def openStr(self, path):
        self.populate()
        return self.open(path.lstrip('/').split('/'))

class VideoFile( File ):
    def fillAttr(self):
        if self.data.insertdate is not None:
            ctime = mktime(self.data.insertdate.timetuple())
        else:
            ctime = time()
        self.attr.st_ctime = ctime
        self.attr.st_mtime = ctime
        self.attr.st_atime = ctime
        self.attr.st_size = self.data.filesize

    def getPath(self, data):
        return data.filename.encode('utf-8').lstrip('/').split('/')

    def getData(self, full=False, single=None):
        if self.db is None:
            self.db = MythVideo()
        if full:
            newlist = []
            vids = self.db.searchVideos()
            newlist = [vid.intid for vid in vids]
            files = self.walkSG('Videos')
            for vid in vids:
                if '/'+vid.filename in files:
                    vid.filesize = files['/'+vid.filename]
                else:
                    vid.filesize = 0
            return zip(vids, newlist)
        elif single:
            return Video(id=single, db=self.db)
        else:
            c = self.db.cursor()
            c.execute("""SELECT intid FROM videometadata""")
            newlist = [id[0] for id in c.fetchall()]
            c.close()
            return newlist

    def walkSG(self, group, myth=None, base=None, path=None):
        fdict = {}
        if myth is None:
            # walk through backends
            c = self.db.cursor()
            c.execute("""SELECT DISTINCT hostname 
                                FROM storagegroup 
                                WHERE groupname=%s""", group)
            for host in c.fetchall():
                fdict.update(self.walkSG(group, MythBE(host[0], db=self.db)))
            c.close()
            return fdict

        if base is None:
            # walk through base directories
            for base in myth.getSGList(myth.hostname, group, ''):
                fdict.update(self.walkSG(group, myth, base, ''))
            return fdict

        dirs, files, sizes = myth.getSGList(myth.hostname, group, base+'/'+path)
        for d in dirs:
            fdict.update(self.walkSG(group, myth, base, path+'/'+d))
        for f, s in zip(files, sizes):
            fdict[path+'/'+f] = int(s)
        return fdict

class RecFile( File ):
    def update(self):
        if (time() - self.time) > 5:
            self.time = time()
            self.data._pull()
            self.fillAttr()

    def fillAttr(self):
        ctime = mktime(self.data.lastmodified.timetuple())
        self.attr.st_ctime = ctime
        self.attr.st_mtime = ctime
        self.attr.st_atime = ctime
        self.attr.st_size = self.data.filesize
        #LOG.log(LOG.IMPORTANT, 'Set ATTR %s' % self.path, str(self.attr.__dict__))

    def getPath(self, data):
        return data.formatPath(self.fmt, '-').encode('utf-8').\
                        lstrip('/').split('/')

    def getData(self, full=False, single=None):
        def _processrec(rec):
            for field in ('title','subtitle'):
                if (rec[field] == '') or (rec[field] == None):
                    rec[field] = 'Untitled'
            if rec['originalairdate'] is None:
                rec['originalairdate'] = date(1900,1,1)

        if self.db is None:
            self.db = MythDB()
        if full:
            recs = self.db.searchRecorded()
            newlist = []
            for rec in recs:
                _processrec(rec)
                newlist.append((rec.chanid, rec.starttime))
            return zip(recs, newlist)
        elif single:
            rec = Recorded(data=single, db=self.db)
            _processrec(rec)
            return rec
        else:
            c = self.db.cursor()
            c.execute("""SELECT chanid,starttime FROM recorded
                            WHERE recgroup!='LiveTV'""")
            newlist = list(c.fetchall())
            c.close()
            return newlist

class URIFile( File ):
    def fillAttr(self):
        fp = self.data.open('r')
        fp.seek(0,2)
        self.attr.st_size = fp.tell()
        fp.close()
        ctime = time()
        self.attr.st_ctime = ctime
        self.attr.st_mtime = ctime
        self.attr.st_atime = ctime

    def getPath(self, data):
        return ['file.%s' % data.uri.split('.')[-1]]

    def getData(self, full=False, single=None):
        if full:
            return [(URI(self.uri), 0)]
        elif single:
            return URI(self.uri)
        else:
            return [0,]

class MythFS( Fuse ):
    def __init__(self, *args, **kw):
        Fuse.__init__(self, *args, **kw)
        self.open_files = {}

    def prep(self):
        fmt = self.parser.largs[0].split(',',1)
        if fmt[0] == 'Videos':
            self.files = VideoFile()
            self.files.populate()
        elif fmt[0] == 'Recordings':
            self.files = RecFile()
            self.files.fmt = fmt[1]
            self.files.populate()
        elif fmt[0] == 'Single':
            self.files = URIFile()
            self.files.uri = fmt[1]
            self.files.populate()

    def getattr(self, path):
        rec = self.files.getAttrStr(path)
        if rec is None:
            return -errno.ENOENT
        else:
            return rec

    def readdir(self, path, offset):
        d = self.files.getObjStr(path)
        if d is None:
            return -errno.ENOENT
        return tuple([fuse.Direntry(e) for e in d.children.keys()])

    def open(self, path, flags):
        accmode = os.O_RDONLY | os.O_WRONLY | os.O_RDWR
        if (flags & accmode) != os.O_RDONLY:
            return -errno.EACCES

        if path not in self.open_files:
            f = self.files.getObjStr(path)
            if f is None:
                return -errno.ENOENT
            if f.data is None:
                return -errno.ENOENT
            self.open_files[path] = [1, f.data.open()]
        else:
            self.open_files[path][0] += 1

    def read(self, path, length, offset, fh=None):
        if path not in self.open_files:
            return -errno.ENOENT
        if self.open_files[path][1].tell() != offset:
            self.open_files[path][1].seek(offset)
        return self.open_files[path][1].read(length)

    def release(self, path, fh=None):
        if path in self.open_files:
            if self.open_files[path][0] < 2:
                self.open_files[path][1].close()
                del self.open_files[path]
            else:
                self.open_files[path][0] += -1
        else:
            return -errno.ENOENT

def main():
    fs = MythFS(version='MythFS 0.23.0', usage='', dash_s_do='setsingle')
    fs.parse(errex=1)
    fs.flags = 0
    fs.multithreaded = False
    fs.prep()
    fs.main()

if __name__ == '__main__':
    main()

