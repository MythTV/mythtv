#!/usr/local/bin/python
# -*- coding: UTF-8 -*-
#---------------------------
# Name: mythvidexport.py
# Python Script
# Author: Raymond Wagner
# Purpose
#   This python script is intended to function as a user job, run through
#   mythjobqueue, capable of exporting recordings into MythVideo.
#---------------------------
__title__  = "MythVidExport"
__author__ = "Raymond Wagner"
__version__= "v0.6.0"

usage_txt = """
This script can be run from the command line, or called through the mythtv
jobqueue.  The input format will be:
  mythvidexport.py [options] <--chanid <channel id>> <--starttime <start time>>
                 --- or ---
  mythvidexport.py [options] %JOBID%

Options are:
        --mformat <format string>
        --tformat <format string>
        --gformat <format string>
            overrides the stored format string for a single run
        --listingonly
            use EPG data rather than grabbers for metadata
            will still try to grab episode and season information from ttvdb.py

Additional functions are available beyond exporting video
  mythvidexport.py <options>
        -h, --help             show this help message
        -p, --printformat      print existing format strings
        -f, --helpformat       lengthy description for formatting strings
        --mformat <string>     replace existing Movie format
        --tformat <string>     replace existing TV format
        --gformat <string>     replace existing Generic format
"""

from MythTV import MythDB, Job, Video, VideoGrabber, MythLog, MythError
from socket import gethostname
from urllib import urlopen
from optparse import OptionParser
from ConfigParser import SafeConfigParser

import sys, re, os, time


class VIDEO:
    def __init__(self, opts, jobid=None):
        if jobid:
            self.job = Job(jobid)
            self.chanid = self.job.chanid
            self.starttime = int("%04d%02d%02d%02d%02d%02d" \
                        % self.job.starttime.timetuple()[0:6])
            self.job.update(status=3)
        else:
            self.job = None
            self.chanid = opts.chanid
            self.rtime = opts.starttime

        self.opts = opts
        self.db = MythDB()
        self.log = MythLog(module='mythvidexport.py', db=self.db)

        # load setting strings
        self.get_grabbers()
        self.get_format()

        # process file
        self.cast = []
        self.genre = []
        self.country = []
        self.rec = self.db.getRecorded(chanid=self.chanid,\
                                    starttime=self.starttime)
        self.vid = Video()
        self.vid.host = gethostname()

        self.get_meta()
        self.get_dest()

        # save file
        self.copy()
        self.write_images()
        self.vid.create()
        self.write_cref()

    def get_grabbers(self):
        # TV Grabber
        self.TVgrab = VideoGrabber('TV')
        # if ttvdb.py, optionally add config file
        if 'ttvdb.py' in self.TVgrab.path:
            path = os.path.expanduser('~/.mythtv/ttvdb.conf')
            if os.access(path, os.F_OK):
                # apply title overrides
                cfg = SafeConfigParser()
                cfg.read(path)
                if 'series_name_override' in cfg.sections():
                    ovr = [(title, cfg.get('series_name_override',title)) \
                            for title in cfg.options('series_name_override')]
                    self.TVgrab.setOverride(ovr)
                    self.TVgrab.append(' -c '+path)

        # Movie Grabber
        self.Mgrab = VideoGrabber('Movie')

    def get_format(self):
        host = gethostname()
        # TV Format
        if self.opts.tformat:
            self.tfmt = self.opts.tformat
        elif self.db.settings[host]['mythvideo.TVexportfmt']:
            self.tfmt = self.db.settings[host]['mythvideo.TVexportfmt']
        else:
            self.tfmt = 'Television/%TITLE%/Season %SEASON%/'+\
                            '%TITLE% - S%SEASON%E%EPISODEPAD% - %SUBTITLE%'

        # Movie Format
        if self.opts.mformat:
            self.mfmt = self.opts.mformat
        elif self.db.settings[host]['mythvideo.MOVIEexportfmt']:
            self.mfmt = self.db.settings[host]['mythvideo.MOVIEexportfmt']
        else:
            self.mfmt = 'Movies/%TITLE%'

        # Generic Format
        if self.opts.gformat:
            self.gfmt = self.opts.gformat
        elif self.db.settings[host]['mythvideo.GENERICexportfmt']:
            self.gfmt = self.db.settings[host]['mythvideo.GENERICexportfmt']
        else:
            self.gfmt = 'Videos/%TITLE%'

    def get_meta(self):
        self.vid.hostname = gethostname()
        if self.rec.subtitle:  # subtitle exists, assume tv show
            self.get_tv()
        else:                   # assume movie
            self.get_movie()

    def get_tv(self):
        # grab season and episode number, run generic export if failed
        match = self.TVgrab.searchTitle(self.rec.title)
        if len(match) == 0:
            # no match found
            self.get_generic()
            return
        elif len(match) > 1:
            # multiple matches found
            raise MythError('Multiple TV metadata matches found: '\
                                                    +self.rec.title)
        inetref = match[0][0]

        season, episode = self.TVgrab.searchEpisode(self.rec.title, \
                                                    self.rec.subtitle)
        if season is None:
            # no match found
            self.get_generic()
            return
        self.vid.season, self.vid.episode = season, episode

        if self.opts.listingonly:
            self.get_generic()
        else:
            dat, self.cast, self.genre, self.country = \
                    self.TVgrab.getData(inetref,\
                                        self.vid.season,\
                                        self.vid.episode)
            self.vid.data.update(dat)
            self.vid.title = self.rec.title
            self.vid.season, self.vid.episode = season, episode
        self.type = 'TV'

    def get_movie(self):
        inetref = self.Mgrab.searchTitle(self.rec.title,\
                            self.rec.originalairdate.year)
        if len(inetref) == 1:
            inetref = inetref[0][0]
        else:
            self.get_generic()
            return

        if self.opts.listingonly:
            self.get_generic()
        else:
            dat, self.cast, self.genre, self.country = \
                    self.Mgrab.getData(inetref)
            self.vid.data.update(dat)
            self.vid.title = self.rec.title
        self.type = 'Movie'

    def get_generic(self):
        self.vid.title = self.rec.title
        if self.rec.subtitle:
            self.vid.subtitle = self.rec.subtitle
        if self.rec.description:
            self.vid.plot = self.rec.description
        if self.rec.originalairdate:
            self.vid.year = self.rec.originalairdate.year
            self.vid.releasedate = self.rec.originalairdate
        lsec = (self.rec.endtime-self.rec.starttime).seconds
        self.vid.length = str(lsec/60)
        for member in self.rec.cast:
            if member.role == 'director':
                self.vid.director = member.name
            elif member.role == 'actor':
                self.cast.append(member.name)
        self.type = 'GENERIC'

    def get_dest(self):
        if self.type == 'TV':
            self.vid.filename = self.process_fmt(self.tfmt)
        elif self.type == 'MOVIE':
            self.vid.filename = self.process_fmt(self.mfmt)
        elif self.type == 'GENERIC':
            self.vid.filename = self.process_fmt(self.gfmt)

    def process_fmt(self, fmt):
        # replace fields from viddata
        #print self.vid.data
        ext = '.'+self.rec.basename.rsplit('.',1)[1]
        rep = ( ('%TITLE%','title','%s'),   ('%SUBTITLE%','subtitle','%s'),
            ('%SEASON%','season','%d'),     ('%SEASONPAD%','season','%02d'),
            ('%EPISODE%','episode','%d'),   ('%EPISODEPAD%','episode','%02d'),
            ('%YEAR%','year','%s'),         ('%DIRECTOR%','director','%s'))
        for (tag, data, format) in rep:
            if self.vid[data]:
                fmt = fmt.replace(tag,format % self.vid[data])
            else:
                fmt = fmt.replace(tag,'')

        # replace fields from program data
        rep = ( ('%HOSTNAME','hostname','%s'),('%STORAGEGROUP%','storagegroup','%s'))
        for (tag, data, format) in rep:
            data = eval('self.rec.%s' % data)
            fmt = fmt.replace(tag,format % data)

#       fmt = fmt.replace('%CARDID%',self.rec.cardid)
#       fmt = fmt.replace('%CARDNAME%',self.rec.cardid)
#       fmt = fmt.replace('%SOURCEID%',self.rec.cardid)
#       fmt = fmt.replace('%SOURCENAME%',self.rec.cardid)
#       fmt = fmt.replace('%CHANNUM%',self.rec.channum)
#       fmt = fmt.replace('%CHANNAME%',self.rec.cardid)

        if len(self.genre):
            fmt = fmt.replace('%GENRE%',self.genre[0])
        else:
            fmt = fmt.replace('%GENRE%','')
#       if len(self.country):
#           fmt = fmt.replace('%COUNTRY%',self.country[0])
#       else:
#           fmt = fmt.replace('%COUNTRY%','')
        return fmt+ext

    def copy(self):
        if self.opts.skip:
            self.vid.hash = self.vid.getHash()
            return
        if self.opts.sim:
            return

        #print self.vid.filename

        stime = time.time()
        srcsize = self.rec.filesize
        htime = [stime,stime,stime,stime]

        self.log.log(MythLog.IMPORTANT|MythLog.FILE, "Copying myth://%s@%s/%s"\
               % (self.rec.storagegroup, self.rec.hostname, self.rec.basename)\
                                                    +" to myth://Videos@%s/%s"\
                                          % (self.vid.host, self.vid.filename))
        srcfp = self.rec.open('r')
        dstfp = self.vid.open('w')

        if self.job:
            self.job.setStatus(4)
        tsize = 2**24
        while tsize == 2**24:
            if (srcsize - dstfp.tell()) < tsize:
                tsize = srcsize - dstfp.tell()
            dstfp.write(srcfp.read(tsize))
            htime.append(time.time())
            rate = float(tsize*4)/(time.time()-htime.pop(0))
            remt = (srcsize-dstfp.tell())/rate
            if self.job:
                self.job.setComment("%02d%% complete - %d seconds remaining" %\
                            (dstfp.tell()*100/srcsize, remt))
        srcfp.close()
        dstfp.close()

        self.vid.hash = self.vid.getHash()

        self.log.log(MythLog.IMPORTANT|MythLog.FILE, "Transfer Complete",
                            "% seconds elapsed" % int(time.time()-stime))
        if self.job:
            self.job.setComment("Complete - %d seconds elapsed" % \
                            (int(time.time()-stime)))
            self.job.setStatus(256)

    def write_images(self):
        for type in ('coverfile', 'screenshot', 'banner', 'fanart'):
            if self.vid[type] in ('No Cover','',None):
                continue
            if type == 'coverfile': name = 'coverart'
            else: name = type
            url = self.vid[type]
            if len(url.split(',')) > 1:
                url = url.split(',')[0]

            if self.type == 'TV':
                if type == 'screenshot':
                    self.vid[type] = '%s Season %dx%d_%s.%s' % \
                                (self.vid.title, self.vid.season, 
                                self.vid.episode, name, url.rsplit('.',1)[1])
                else:
                    self.vid[type] = '%s Season %d_%s.%s' % \
                                (self.vid.title, self.vid.season,
                                 name, url.rsplit('.',1)[1])
            else:
                self.vid[type] = '%s_%s.%s' % \
                            (self.vid.title, name, url.rsplit('.',1)[1])

            try:
                dstfp = self.vid._open(type, 'w', True)
                srcfp = urlopen(url)
                dstfp.write(srcfp.read())
                srcfp.close()
                dstfp.close()
            except:
                #print 'existing images: ' + self.vid[type]
                pass

    def write_cref(self):
        for member in self.cast:
            self.vid.cast.add(member)
        for member in self.genre:
            self.vid.genre.add(member)
        for member in self.country:
            self.vid.country.add(member)


def usage_format():
    usagestr = """The default strings are:
    Television: Television/%TITLE%/Season %SEASON%/%TITLE% - S%SEASON%E%EPISODEPAD% - %SUBTITLE%
    Movie:      Movies/%TITLE%
    Generic:    Videos/%TITLE%

Available strings:
    %TITLE%:         series title
    %SUBTITLE%:      episode title
    %SEASON%:        season number
    %SEASONPAD%:     season number, padded to 2 digits
    %EPISODE%:       episode number
    %EPISODEPAD%:    episode number, padded to 2 digits
    %YEAR%:          year
    %DIRECTOR%:      director
    %HOSTNAME%:      backend used to record show
    %STORAGEGROUP%:  storage group containing recorded show
    %GENRE%:         first genre listed for recording
"""
#    %CARDID%:        ID of tuner card used to record show
#    %CARDNAME%:      name of tuner card used to record show
#    %SOURCEID%:      ID of video source used to record show
#    %SOURCENAME%:    name of video source used to record show
#    %CHANNUM%:       ID of channel used to record show
#    %CHANNAME%:      name of channel used to record show
#    %COUNTRY%:       first country listed for recording
    print usagestr

def print_format():
    db = MythDB()
    host = gethostname()
    tfmt = db.settings[host]['mythvideo.TVexportfmt']
    if not tfmt:
        tfmt = 'Television/%TITLE%/Season %SEASON%/%TITLE% - S%SEASON%E%EPISODEPAD% - %SUBTITLE%'
    mfmt = db.settings[host]['mythvideo.MOVIEexportfmt']
    if not mfmt:
        mfmt = 'Movies/%TITLE%'
    gfmt = db.settings[host]['mythvideo.GENERICexportfmt']
    if not gfmt:
        gfmt = 'Videos/%TITLE%'
    print "Current output formats:"
    print "    TV:      "+tfmt
    print "    Movies:  "+mfmt
    print "    Generic: "+gfmt

def main():
    parser = OptionParser(usage="usage: %prog [options] [jobid]")

    parser.add_option("-f", "--helpformat", action="store_true", default=False, dest="fmthelp",
            help="Print explination of file format string.")
    parser.add_option("-p", "--printformat", action="store_true", default=False, dest="fmtprint",
            help="Print current file format string.")
    parser.add_option("--tformat", action="store", type="string", dest="tformat",
            help="Use TV format for current task. If no task, store in database.")
    parser.add_option("--mformat", action="store", type="string", dest="mformat",
            help="Use Movie format for current task. If no task, store in database.")
    parser.add_option("--gformat", action="store", type="string", dest="gformat",
            help="Use Generic format for current task. If no task, store in database.")
    parser.add_option("--chanid", action="store", type="int", dest="chanid",
            help="Use chanid for manual operation")
    parser.add_option("--starttime", action="store", type="int", dest="starttime",
            help="Use starttime for manual operation")
    parser.add_option("--listingonly", action="store_true", default=False, dest="listingonly",
            help="Use data from listing provider, rather than grabber")
    parser.add_option("-s", "--simulation", action="store_true", default=False, dest="sim",
            help="Simulation (dry run), no files are copied or new entries made")
    parser.add_option("--skip", action="store_true", default=False, dest="skip") # debugging use only
    parser.add_option('-v', '--verbose', action='store', type='string', dest='verbose',
            help='Verbosity level')

    opts, args = parser.parse_args()

    if opts.verbose:
        if opts.verbose == 'help':
            print MythLog.helptext
            sys.exit(0)
        MythLog._setlevel(opts.verbose)

    if opts.fmthelp:
        usage_format()
        sys.exit(0)

    if opts.fmtprint:
        print_format()
        sys.exit(0)

    if opts.chanid and opts.starttime:
        export = VIDEO(opts)
    elif len(args) == 1:
        try:
            export = VIDEO(opts,int(args[0]))
        except Exception, e:
            Job(int(args[0])).update({'status':304,
                                      'comment':'ERROR: '+e.args[0]})
            sys.exit(1)
    else:
        if opts.tformat or opts.mformat or opts.gformat:
            db = MythDB()
            host = gethostname()
            if opts.tformat:
                print "Changing TV format to: "+opts.tformat
                db.setting[host]['mythvideo.TVexportfmt'] = opts.tformat
            if opts.mformat:
                print "Changing Movie format to: "+opts.mformat
                db.setting[host]['mythvideo.MOVIEexportfmt'] = opts.mformat
            if opts.gformat:
                print "Changing Generic format to: "+opts.gformat
                db.setting[hosts]['mythvideo.GENERICexportfmt'] = opts.gformat
            sys.exit(0)
        else:
            parser.print_help()
            sys.exit(2)

if __name__ == "__main__":
    main()
