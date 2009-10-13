# Miro - an RSS based video player application
# Copyright (C) 2005-2009 Participatory Culture Foundation
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
# In addition, as a special exception, the copyright holders give
# permission to link the code of portions of this program with the OpenSSL
# library.
#
# You must obey the GNU General Public License in all respects for all of
# the code used other than OpenSSL. If you modify file(s) with this
# exception, you may extend this exception to your version of the file(s),
# but you are not obligated to do so. If you do not wish to do so, delete
# this exception statement from your version. If you delete this exception
# statement from all source files in the program, then also delete it here.
###########################################################################
# The original source "...miro/frontends/cli/interpreter.py" 
# was modified for the purposes of the MythTV script mirobridge.py 
###########################################################################

import cmd
import threading
import time
import Queue

from miro import app
from miro import dialogs
from miro import eventloop
from miro import folder
from miro import indexes
from miro import util
from miro import views
from miro.frontends.cli import clidialog
from miro.plat import resources
from miro import fileutil
from miro import feed

## mirobridge.py import additions - All to get feed updates and auto downloads working
import os, sys, subprocess, re, fnmatch, string
import logging
from miro.singleclick import parse_command_line_args
from miro import moviedata
from miro import autodler
from miro import downloader
from miro import iconcache
from miro.clock import clock
from miro import filetypes


def run_in_event_loop(func):
    def decorated(*args, **kwargs):
        return_hack = []
        event = threading.Event()
        def runThenSet():
            try:
                return_hack.append(func(*args, **kwargs))
            finally:
                event.set()
        eventloop.addUrgentCall(runThenSet, 'run in event loop')
        event.wait()
        if return_hack:
            return return_hack[0]
    decorated.__doc__ = func.__doc__
    return decorated

class FakeTab:
    def __init__(self, tab_type, tabTemplateBase):
        self.type = tab_type
        self.tabTemplateBase = tabTemplateBase

class MiroInterpreter(cmd.Cmd):
    def __init__(self):
        cmd.Cmd.__init__(self)
        self.quit_flag = False
        self.tab = None
        self.init_database_objects()

    @run_in_event_loop
    def init_database_objects(self):
        self.channelTabs = util.getSingletonDDBObject(views.channelTabOrder)
        self.playlistTabs = util.getSingletonDDBObject(views.playlistTabOrder)
        self.tab_changed()

    def tab_changed(self):
        """Calculate the current prompt.  This method access database objects,
        so it should only be called from the backend event loop
        """
        if self.tab is None:
            self.prompt = "> "
            self.selection_type = None
        elif self.tab.type == 'feed':
            if isinstance(self.tab.obj, folder.ChannelFolder):
                self.prompt = "channel folder: %s > " % self.tab.obj.get_title()
                self.selection_type = 'channel-folder'
            else:
                self.prompt = "channel: %s > " % self.tab.obj.get_title()
                self.selection_type = 'feed'
        elif self.tab.type == 'playlist':
            self.prompt = "playlist: %s > " % self.tab.obj.get_title()
            self.selection_type = 'playlist'
        elif (self.tab.type == 'statictab' and
                self.tab.tabTemplateBase == 'downloadtab'):
            self.prompt = "downloads > "
            self.selection_type = 'downloads'
        else:
            raise ValueError("Unknown tab type")

    def postcmd(self, stop, line):
        # HACK
        # If the last command results in a dialog, give it a little time to
        # pop up
        time.sleep(0.1)
        while True:
            try:
                dialog = app.cli_events.dialog_queue.get_nowait()
            except Queue.Empty:
                break
            clidialog.handle_dialog(dialog)

        return self.quit_flag

    def do_help(self, line):
        """help -- Lists commands and help."""
        commands = [m for m in dir(self) if m.startswith("do_")]
        for mem in commands:
            docstring = getattr(self, mem).__doc__
            print "    ", docstring

    def do_quit(self, line):
        """quit -- Quits Miro cli."""
        self.quit_flag = True

    @run_in_event_loop
    def do_feed(self, line):
        """feed <name> -- Selects a feed by name."""
        for tab in self.channelTabs.getView():
            if tab.obj.get_title() == line:
                self.tab = tab
                self.tab_changed()
                return
        print "Error: %s not found" % line

    @run_in_event_loop
    def do_rmfeed(self, line):
        """rmfeed <name> -- Deletes a feed."""
        for tab in self.channelTabs.getView():
            if tab.obj.get_title() == line:
                tab.obj.remove()
                return
        print "Error: %s not found" % line

    @run_in_event_loop
    def complete_feed(self, text, line, begidx, endidx):
        return self.handle_tab_complete(text, self.channelTabs.getView())

    @run_in_event_loop
    def complete_rmfeed(self, text, line, begidx, endidx):
        return self.handle_tab_complete(text, self.channelTabs.getView())

    @run_in_event_loop
    def complete_playlist(self, text, line, begidx, endidx):
        return self.handle_tab_complete(text, self.playlistTabs.getView())

    def handle_tab_complete(self, text, view):
        text = text.lower()
        matches = []
        for tab in view:
            if tab.obj.get_title().lower().startswith(text):
                matches.append(tab.obj.get_title())
        return matches

    def handle_item_complete(self, text, view, filterFunc=lambda i: True):
        text = text.lower()
        matches = []
        for item in view:
            if (item.get_title().lower().startswith(text) and
                    filterFunc(item)):
                matches.append(item.get_title())
        return matches


    ###################################################################################
    #
    # Start of mythbridge specific routines
    #
    ###################################################################################
    @run_in_event_loop
    def do_mythtv_update_autodownload(self, line):
        """Update feeds and auto-download"""
        logging.info("Starting auto downloader...")
        autodler.start_downloader()
        feed.expire_items()
        starttime = clock()
        logging.timing("Icon clear: %.3f", clock() - starttime)
        logging.info("Starting video updates")
        moviedata.movieDataUpdater.startThread()
        parse_command_line_args()
        # autoupdate.check_for_updates()
        # Wait a bit before starting the downloader daemon.  It can cause a bunch
        # of disk/CPU load, so try to avoid it slowing other stuff down.
        eventloop.addTimeout(5, downloader.startupDownloader,
                "start downloader daemon")
        # ditto for feed updates
        eventloop.addTimeout(30, feed.start_updates, "start feed updates")
        # ditto for clearing stale icon cache files, except it's the very lowest
        # priority
        eventloop.addTimeout(10, iconcache.clear_orphans, "clear orphans")

    def movie_data_program_info(self, movie_path, thumbnail_path):
        extractor_path = os.path.join(os.path.split(__file__)[0], "gst_extractor.py")
        return ((sys.executable, extractor_path, movie_path, thumbnail_path), None)

    @run_in_event_loop
    def do_mythtv_check_downloading(self, line):
        """Check if any items are being downloaded. Set True or False"""
        self.downloading = False
        downloadingItems = views.downloadingItems
        count = len(downloadingItems)
        for item in downloadingItems:
            logging.info(u"(%s - %s) video is downloading with (%0.0f%%) complete" % (item.get_channel_title(True).replace(u'/',u'-'), item.get_title().replace(u'/',u'-'), item.download_progress()))
        if not count:
            logging.info(u"No items downloading")
        if count:
            self.downloading = True

    @run_in_event_loop
    def do_mythtv_updatewatched(self, line):
        """Process MythTV update watched videos"""
        items = views.watchableItems
        for video in self.videofiles:
            for item in items:
                if item.get_filename() == video:
                     break
            else:
                logging.info(u"Item for Miro video (%s) not found, skipping" % video)
                continue
            if self.simulation:
                logging.info(u"Simulation: Item (%s - %s) marked as seen and watched" % (item.get_channel_title(True), item.get_title()))
            else:
                item.markItemSeen(markOtherItems=False)
                self.statistics[u'Miro_marked_watch_seen']+=1
                logging.info(u"Item (%s - %s) marked as seen and watched" % (item.get_channel_title(True), item.get_title()))

    @run_in_event_loop
    def do_mythtv_getunwatched(self, line):
        """Process MythTV get all un-watched video details"""
        if self.verbose:
             print
             print u"Getting details on un-watched Miro videos"

        self.videofiles = []
        if len(views.watchableItems):
            if self.verbose:
                print u"%-20s %-10s %s" % (u"State", u"Size", u"Name")
                print u"-" * 70
            for item in views.watchableItems:
                # Skip any audio file as MythTV Internal player may abort the MythTV Frontend on a MP3
                if not item.isVideo: 
                    continue
                state = item.get_state()
                if not state == u'newly-downloaded':
                    continue
                # Skip any bittorrent video downloads for legal concerns
                if filetypes.is_torrent_filename(item.getURL()):
                    continue
                self.printItems(item)
                self.videofiles.append(self._get_item_dict(item))
            if self.verbose:
                print
        if not len(self.videofiles):
             logging.info(u"No un-watched Miro videos")

    @run_in_event_loop
    def do_mythtv_getwatched(self, line):
        """Process MythTV get all watched/saved video details"""
        if self.verbose:
           print
           print u"Getting details on watched/saved Miro videos"
        self.videofiles = []
        if len(views.watchableItems):
            if self.verbose:
                print u"%-20s %-10s %s" % (u"State", u"Size", u"Name")
                print "-" * 70
            for item in views.watchableItems:
                # Skip any audio file as MythTV Internal player may abort the MythTV Frontend on a MP3
                if not item.isVideo: 
                    continue
                state = item.get_state()
                if state == u'newly-downloaded':
                    continue
                # Skip any bittorrent video downloads for legal concerns
                if filetypes.is_torrent_filename(item.getURL()):
                    continue
                self.printItems(item)
                self.videofiles.append(self._get_item_dict(item))
            if self.verbose:
                print
        if not len(self.videofiles):
             logging.info(u"No watched/saved Miro videos")

    def printItems(self, item):
        if not self.verbose:
            return
        state = item.get_state()
        if state == u'downloading':
            state += u' (%0.0f%%)' % item.download_progress()
        print u"%-20s %-10s %s" % (state, item.get_size_for_display(),
            item.get_title())
    # end printItems()

    @run_in_event_loop
    def do_mythtv_item_remove(self, args):
        """Removes an item from Miro by file name or Channel and title"""
        for item in views.watchableItems:
             if isinstance(args, list):
                  if filter(self.is_not_punct_char, item.get_channel_title(True).lower()) == filter(self.is_not_punct_char, args[0].lower()) and (filter(self.is_not_punct_char, item.get_title().lower())).startswith(filter(self.is_not_punct_char, args[1].lower())):
                     break
             elif filter(self.is_not_punct_char, item.get_filename().lower()) == filter(self.is_not_punct_char, args.lower()):
                 break
        else:
            logging.info(u"No item named %s" % args)
            return
        if item.is_downloaded():
            if self.simulation:
                logging.info(u"Simulation: Item (%s - %s) has been removed from Miro" % (item.get_channel_title(True), item.get_title()))
            else:
                item.expire()
                self.statistics[u'Miro_videos_deleted']+=1
                logging.info(u'%s has been removed from Miro' % item.get_title())
        else:
            logging.info(u'%s is not downloaded' % item.get_title())


    def _get_item_dict(self, item):
        """Take an item and convert all elements into a dictionary
        return a dictionary of item elements
        """
        def compatibleGraphics(filename):
            if filename:
                (dirName, fileName) = os.path.split(filename)
                (fileBaseName, fileExtension)=os.path.splitext(fileName)
                if not fileExtension[1:] in [u"png", u"jpg", u"bmp", u"gif"]:
                    return u''
                else:
                    return filename
            else:
                return u''

        def useImageMagick(screenshot):
            """ Using ImageMagick's utility 'identify'. Decide whether the screen shot is worth using.
            >>> useImageMagick('identify screenshot.jpg')
            >>> Example returned information "rose.jpg JPEG 640x480 DirectClass 87kb 0.050u 0:01"
            >>> u'' if the screenshot quality is too low 
            >>> screenshot if the quality is good enough to use
            """
            if not self.imagemagick: # If imagemagick is not installed do not bother checking
               return u''

            width_height = re.compile(u'''^(.+?)[ ]\[?([0-9]+)x([0-9]+)[^\\/]*$''', re.UNICODE)
            p = subprocess.Popen(u'identify "%s"' % (screenshot), shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)

            response = p.stdout.readline()
            if response:
                match = width_height.match(response)
                if match:
                    dummy, width, height = match.groups()
                    width, height = int(width), int(height)
                    if width >= 320:
                        return screenshot
                return u''
            else:
                return u''
            return screenshot
        # end useImageMagick()

        item_icon_filename = None
        channel_icon = None
        if item.getFeed():
            channel_icon = fileutil.expand_filename(item.getFeed().iconCache.get_filename())

        if item.iconCache and item.iconCache.filename:
            item_icon_filename = item.iconCache.filename

        # Conform to maximum length for MythTV database fields title and subtitle
        maximum_length = 128
        channel_title = item.get_channel_title(True).replace(u'/',u'-')
        if channel_title:
            if len(channel_title) > maximum_length:
                channel_title = channel_title[:maximum_length]
            channel_title = channel_title.replace(u'"', u'')	# These characters mess with filenames
        title = item.get_title().replace(u'/',u'-')
        if title:
            if len(title) > maximum_length:
                title = title[:maximum_length]
            title = title.replace(u'"', u'')	# These characters mess with filenames
            title = title.replace(u"'", u'')	# These characters mess with filenames

        item_dict =  {u'feed_id': item.feed_id, u'parent_id': item.parent_id, u'isContainerItem': item.isContainerItem, u'isVideo': item.isVideo, u'seen': item.seen, u'autoDownloaded': item.autoDownloaded, u'pendingManualDL': item.pendingManualDL, u'downloadedTime': item.downloadedTime, u'watchedTime': item.watchedTime, u'pendingReason': item.pendingReason, u'title': title,  u'expired': item.expired, u'keep': item.keep, u'videoFilename': item.get_filename(), u'eligibleForAutoDownload': item.eligibleForAutoDownload, u'duration': item.duration, u'screenshot': item.screenshot, u'resized_screenshots': item.resized_screenshots, u'resumeTime': item.resumeTime, u'channelTitle': channel_title, u'description': item.get_description(), u'size': item._get_size(), u'releasedate': item.get_release_date_obj(), u'length': item.get_duration_value(), u'channel_icon': channel_icon, u'item_icon': item_icon_filename}

        if not item_dict[u'screenshot']:
            if item_dict[u'item_icon']:
                item_dict[u'screenshot'] = useImageMagick(item_dict[u'item_icon'])

        for key in [u'screenshot', u'channel_icon', u'item_icon']:
            if item_dict[key]:
                item_dict[key] = compatibleGraphics(item_dict[key])
            #if self.verbose:
            #    if item_dict[key]:
            #        print "item (%s - %s) %s (%s)" % (channel_title, title, key, item_dict[key])
            #    else:
            #        print "item (%s - %s) does NOT have a %s" % (channel_title, title, key)

        #print item_dict
        return item_dict


    # Two routines used for Channel title search and matching
    def is_punct_char(self, char):
        '''check if char is punctuation char
        return True if char is punctuation
        return False if char is not punctuation
        '''
        return char in string.punctuation

    def is_not_punct_char(self, char):
        '''check if char is not punctuation char
        return True if char is not punctuation
        return False if chaar is punctuation
        '''
        return not self.is_punct_char(char)

    ###################################################################################
    #
    # End of mythbridge specific routines
    #
    ###################################################################################

    @run_in_event_loop
    def do_feeds(self, line):
        """feeds -- Lists all feeds."""
        current_folder = None
        for tab in self.channelTabs.getView():
            if isinstance(tab.obj, folder.ChannelFolder):
                current_folder = tab.obj
            elif tab.obj.getFolder() is not current_folder:
                current_folder = None
            if current_folder is None:
                print tab.obj.get_title()
            elif current_folder is tab.obj:
                print "[Folder] %s" % tab.obj.get_title()
            else:
                print " - %s" % tab.obj.get_title()

    @run_in_event_loop
    def do_play(self, line):
        """play <name> -- Plays an item by name in an external player."""
        if self.selection_type is None:
            print "Error: No feed/playlist selected"
            return
        item = self._find_item(line)
        if item is None:
            print "No item named %r" % line
            return
        if item.is_downloaded():
            resources.open_file(item.get_video_filename())
        else:
            print '%s is not downloaded' % item.get_title()

    @run_in_event_loop
    def do_playlists(self, line):
        """playlists -- Lists all playlists."""
        for tab in self.playlistTabs.getView():
            print tab.obj.get_title()

    @run_in_event_loop
    def do_playlist(self, line):
        """playlist <name> -- Selects a playlist."""
        for tab in self.playlistTabs.getView():
            if tab.obj.get_title() == line:
                self.tab = tab
                self.tab_changed()
                return
        print "Error: %s not found" % line

    @run_in_event_loop
    def do_items(self, line):
        """items -- Lists the items in the feed/playlist/tab selected."""
        if self.selection_type is None:
            print "Error: No tab/feed/playlist selected"
            return
        elif self.selection_type == 'feed':
            feed = self.tab.obj
            view = feed.items.sort(feed.itemSort.sort)
            self.printout_item_list(view)
            view.unlink()
        elif self.selection_type == 'playlist':
            playlist = self.tab.obj
            self.printout_item_list(playlist.getView())
        elif self.selection_type == 'downloads':
            self.printout_item_list(views.downloadingItems, views.pausedItems)
        elif self.selection_type == 'channel-folder':
            folder = self.tab.obj
            allItems = views.items.filterWithIndex(
                    indexes.itemsByChannelFolder, folder)
            allItemsSorted = allItems.sort(folder.itemSort.sort)
            self.printout_item_list(allItemsSorted)
            allItemsSorted.unlink()
        else:
            raise ValueError("Unknown tab type")

    @run_in_event_loop
    def do_downloads(self, line):
        """downloads -- Selects the downloads tab."""
        self.tab = FakeTab("statictab", "downloadtab")
        self.tab_changed()

    def printout_item_list(self, *views):
        totalItems = 0
        for view in views:
            totalItems += len(view)
        if totalItems > 0:
            print "%-20s %-10s %s" % ("State", "Size", "Name")
            print "-" * 70
            for view in views:
                for item in view:
                    state = item.get_state()
                    if state == 'downloading':
                        state += ' (%0.0f%%)' % item.download_progress()
                    print "%-20s %-10s %s" % (state, item.get_size_for_display(),
                            item.get_title())
            print
        else:
            print "No items"

    def _get_item_view(self):
        if self.selection_type == 'feed':
            feed = self.tab.obj
            return feed.items
        elif self.selection_type == 'playlist':
            playlist = self.tab.obj
            return playlist.getView()
        elif self.selection_type == 'downloads':
            return views.downloadingItems
        elif self.selection_type == 'channel-folder':
            folder = self.tab.obj
            return views.items.filterWithIndex(indexes.itemsByChannelFolder,
                    folder)
        else:
            raise ValueError("Unknown selection type")


    def _find_item(self, line):
        line = line.lower()
        for item in self._get_item_view():
            if item.get_title().lower() == line:
                return item

    @run_in_event_loop
    def do_stop(self, line):
        """stop <name> -- Stops download by name."""
        if self.selection_type is None:
            print "Error: No feed/playlist selected"
            return
        item = self._find_item(line)
        if item is None:
            print "No item named %r" % line
            return
        if item.get_state() in ('downloading', 'paused'):
            item.expire()
        else:
            print '%s is not being downloaded' % item.get_title()

    @run_in_event_loop
    def complete_stop(self, text, line, begidx, endidx):
        return self.handle_item_complete(text, self._get_item_view(),
                lambda i: i.get_state() in ('downloading', 'paused'))

    @run_in_event_loop
    def do_download(self, line):
        """download <name> -- Downloads an item by name in the feed/playlist selected."""
        if self.selection_type is None:
            print "Error: No feed/playlist selected"
            return
        item = self._find_item(line)
        if item is None:
            print "No item named %r" % line
            return
        if item.get_state() == 'downloading':
            print '%s is currently being downloaded' % item.get_title()
        elif item.is_downloaded():
            print '%s is already downloaded' % item.get_title()
        else:
            item.download()

    @run_in_event_loop
    def complete_download(self, text, line, begidx, endidx):
        return self.handle_item_complete(text, self._get_item_view(),
                lambda i: i.is_downloadable())

    @run_in_event_loop
    def do_pause(self, line):
        """pause <name> -- Pauses a download by name."""
        if self.selection_type is None:
            print "Error: No feed/playlist selected"
            return
        item = self._find_item(line)
        if item is None:
            print "No item named %r" % line
            return
        if item.get_state() == 'downloading':
            item.pause()
        else:
            print '%s is not being downloaded' % item.get_title()

    @run_in_event_loop
    def complete_pause(self, text, line, begidx, endidx):
        return self.handle_item_complete(text, self._get_item_view(),
                lambda i: i.get_state() == 'downloading')

    @run_in_event_loop
    def do_resume(self, line):
        """resume <name> -- Resumes a download by name."""
        if self.selection_type is None:
            print "Error: No feed/playlist selected"
            return
        item = self._find_item(line)
        if item is None:
            print "No item named %r" % line
            return
        if item.get_state() == 'paused':
            item.resume()
        else:
            print '%s is not a paused download' % item.get_title()

    @run_in_event_loop
    def complete_resume(self, text, line, begidx, endidx):
        return self.handle_item_complete(text, self._get_item_view(),
                lambda i: i.get_state() == 'paused')

    @run_in_event_loop
    def do_rm(self, line):
        """rm <name> -- Removes an item by name in the feed/playlist selected."""
        if self.selection_type is None:
            print "Error: No feed/playlist selected"
            return
        item = self._find_item(line)
        if item is None:
            print "No item named %r" % line
            return
        if item.is_downloaded():
            item.expire()
        else:
            print '%s is not downloaded' % item.get_title()

    @run_in_event_loop
    def complete_rm(self, text, line, begidx, endidx):
        return self.handle_item_complete(text, self._get_item_view(),
                lambda i: i.is_downloaded())

    @run_in_event_loop
    def do_testdialog(self, line):
        """testdialog -- Tests the cli dialog system."""
        d = dialogs.ChoiceDialog("Hello", "I am a test dialog",
                dialogs.BUTTON_OK, dialogs.BUTTON_CANCEL)
        def callback(dialog):
            print "TEST CHOICE: %s" % dialog.choice
        d.run(callback)

    @run_in_event_loop
    def do_dumpdatabase(self, line):
        """dumpdatabase -- Dumps the database."""
        from miro import database
        print "Dumping database...."
        database.defaultDatabase.liveStorage.dumpDatabase(database.defaultDatabase)
        print "Done."

