#!/usr/bin/env python
# -*- coding: UTF-8 -*-
#----------------------------
# Name: logrotate.py
# Python Script
# Author: Raymond Wagner
# Purpose
#   This python script is intended to help manage the new logging format
#   introduced to MythTV in version 0.25. Each instance of each application
#   run generates a new log file, meaning log rotation applications do not
#   know to treat them as a group.
#   This script treats each application individually, and will delete
#   anything beyond a minimum number of log sets that have aged beyond the
#   minimum number of days.  One log set is one instance of the application,
#   and however many rotated files have been generated for it.
#----------------------------

import os
import re
import sys
from datetime import datetime, timedelta
from optparse import OptionParser

class LogFile( object ):
    path = None
    application = None
    datetime = None
    pid = None
    compressed = False
    sequence = None

    @classmethod
    def filter(cls, path, filelist):
        r = re.compile("[a-z]*.[0-9]{14}.[0-9]{1,6}.log(.[0-9]+(.[a-zA-Z0-9]+)?)?")
        return [cls(path, f) for f in filelist if r.match(f)]

    def __init__(self, path, filename):
        self.path = path
        self.filename = filename
        self.lastmod = datetime.fromtimestamp(os.stat(
                            os.path.join(self.path, self.filename)).st_mtime)

        s = filename.split('.')
        self.application = s[0]
        self.datetime = datetime.strptime(s[1], "%Y%m%d%H%M%S")
        self.pid = int(s[2])
        if len(s) > 4:
            self.sequence = int(s[4])
            if len(s) > 5:
                self.compressed = True
        self.children = []

    def __repr__(self):
        return "<LogFile %s, %s%s%s>" % \
                (self.application, self.datetime.strftime("%b %d, %H:%M"),
                 " #%d" % self.sequence if self.sequence is not None else "",
                 " (compressed)" if self.compressed else "")

    def __cmp__(self, other):
        if self.application != other.application:
            return cmp(self.application, other.application)

        if self.datetime != other.datetime:
            return cmp(self.datetime, other.datetime)

        if self.pid != other.pid:
            return cmp(self.pid, other.pid)

        if self.sequence != other.sequence:
            if self.sequence is None:
                return -1
            if other.sequence is None:
                return 1
            return cmp(self.sequence, other.sequence)

        return 0

    def append(self, child):
        self.children.append(child)

    def delete(self):
        for child in self.children:
            child.delete()
        #print 'deleting %s' % os.path.join(self.path, self.filename)
        os.unlink(os.path.join(self.path, self.filename))

def deletelogs(instances, opts):
    while len(instances) > int(opts.minfiles):
        if instances[0].lastmod > (datetime.now() -\
                                   timedelta(hours=24*int(opts.minage))):
            return
        instances.pop(0).delete()

def main(opts):
    ls = sorted(LogFile.filter(opts.logpath, os.listdir(opts.logpath)))
    if len(ls) == 0:
        print "Warning: Empty log path!"
        sys.exit(1)

    cur = None
    while len(ls):
        f = ls.pop(0)
        if cur is None:
            instances = [f]
            cur = f
            continue

        if cur.application != f.application:
            ls.insert(0,f)
            cur = None
            deletelogs(instances, opts)
            continue

        if (cur.datetime != f.datetime) and (cur.pid != f.pid):
            cur = f
            instances.append(f)
            continue

        cur.append(f)

    deletelogs(instances, opts)

if __name__ == "__main__":
    parser = OptionParser()
    parser.add_option("-p", "--path", dest="logpath", default="/var/log/mythtv",
                      help="Path where log files are stored")
    parser.add_option("-n", "--min-files", dest="minfiles", default="5",
                      help="Minimum number of logs per application to keep")
    parser.add_option("-t", "--min-age", dest="minage", default="7",
                      help="Minimum time (days) to keep log files")

    (opts, args) = parser.parse_args()
    main(opts)
