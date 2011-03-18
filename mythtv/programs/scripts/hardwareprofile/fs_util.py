#
# fs_util.py - misc. routines not categorized in other modules
#
# Copyright 2008 Mike McGrath
# This file incorporates work covered by the following copyright 
# Copyright 2006-2007 Red Hat, Inc.
#
# This copyrighted material is made available to anyone wishing to use, modify,
# copy, or redistribute it subject to the terms and conditions of the GNU
# General Public License v.2.  This program is distributed in the hope that it
# will be useful, but WITHOUT ANY WARRANTY expressed or implied, including the
# implied warranties of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  Any Red Hat
# trademarks that are incorporated in the source code or documentation are not
# subject to the GNU General Public License and may only be used or replicated
# with the express permission of Red Hat, Inc. 
#

import os

# We cache the contents of /etc/mtab ... the following variables are used 
# to keep our cache in sync
mtab_mtime = None
mtab_map = []

class MntEntObj(object):
    mnt_fsname = None #* name of mounted file system */
    mnt_dir = None    #* file system path prefix */
    mnt_type = None   #* mount type (see mntent.h) */
    mnt_opts = None   #* mount options (see mntent.h) */
    mnt_freq = 0      #* dump frequency in days */
    mnt_passno = 0    #* pass number on parallel fsck */

    def __init__(self,input=None):
        if input and isinstance(input, str):
            (self.mnt_fsname, self.mnt_dir, self.mnt_type, self.mnt_opts, \
             self.mnt_freq, self.mnt_passno) = input.split()
    def __dict__(self):
        return {"mnt_fsname": self.mnt_fsname, "mnt_dir": self.mnt_dir, \
                "mnt_type": self.mnt_type, "mnt_opts": self.mnt_opts, \
                "mnt_freq": self.mnt_freq, "mnt_passno": self.mnt_passno}
    def __str__(self):
        return "%s %s %s %s %s %s" % (self.mnt_fsname, self.mnt_dir, self.mnt_type, \
                                      self.mnt_opts, self.mnt_freq, self.mnt_passno)
        
class FileSystem(object):
    def __init__(self, mntent):
        self.mnt_dev = mntent.mnt_fsname
        self.mnt_pnt = mntent.mnt_dir
        self.fs_type = mntent.mnt_type
        try:
            stat_res = os.statvfs('%s' % self.mnt_pnt)
        except OSError:
            stat_res = self.f_bsize = self.f_frsize = self.f_blocks = self.f_blocks = self.f_bfree = self.f_bavail = self.f_files = self.f_ffree = self.f_favail = "UNKNOWN"
        else:
            self.f_bsize = stat_res.f_bsize
            self.f_frsize = stat_res.f_frsize
            self.f_blocks = stat_res.f_blocks
            self.f_bfree = stat_res.f_bfree
            self.f_bavail = stat_res.f_bavail
            self.f_files = stat_res.f_files
            self.f_ffree = stat_res.f_ffree
            self.f_favail = stat_res.f_favail
        
    def to_dict(self):
        return dict(mnt_pnt=self.mnt_pnt, fs_type=self.fs_type, f_favail=self.f_favail,
                    f_bsize=self.f_bsize, f_frsize=self.f_frsize, f_blocks=self.f_blocks,
                    f_bfree=self.f_bfree, f_bavail=self.f_bavail, f_files=self.f_files,
                    f_ffree=self.f_ffree)

    def __str__(self):
        return "%s %s %s %s %s %s %s %s %s %s %s" % \
            (self.mnt_dev, self.mnt_pnt, self.fs_type, 
             self.f_bsize, self.f_frsize, self.f_blocks,
             self.f_bfree, self.f_bavail, self.f_files,
             self.f_ffree, self.f_favail)

        

def get_mtab(mtab="/proc/mounts", vfstype=None):
    global mtab_mtime, mtab_map

    mtab_stat = os.stat(mtab)
    if mtab_stat.st_mtime != mtab_mtime:
        #print '''cache is stale ... refresh'''
        mtab_mtime = mtab_stat.st_mtime
        mtab_map = __cache_mtab__(mtab)

    # was a specific fstype requested?
    if vfstype:
        return [ent for ent in mtab_map if ent.mnt_type == vfstype]

    return mtab_map

def get_fslist():
    return [FileSystem(mnt) for mnt in get_mtab()]

def __cache_mtab__(mtab="/proc/mounts"):
    global mtab_mtime

    f = open(mtab)
    #don't want empty lines
    mtab = [MntEntObj(line) for line in f.read().split('\n') if len(line) > 0]
    f.close()

    return mtab

def get_file_device_path(fname):
    '''What this function attempts to do is take a file and return:
         - the device the file is on
         - the path of the file relative to the device.
       For example:
         /boot/vmlinuz -> (/dev/sda3, /vmlinuz)
         /boot/efi/efi/redhat/elilo.conf -> (/dev/cciss0, /elilo.conf)
         /etc/fstab -> (/dev/sda4, /etc/fstab)
    '''

    # resolve any symlinks
    fname = os.path.realpath(fname)

    # convert mtab to a dict
    mtab_dict = {}
    for ent in get_mtab():
        mtab_dict[ent.mnt_dir] = ent.mnt_fsname

    # find a best match
    fdir = os.path.dirname(fname)
    match = mtab_dict.has_key(fdir)
    while not match:
        fdir = os.path.realpath(os.path.join(fdir, os.path.pardir))
        match = mtab_dict.has_key(fdir)

    # construct file path relative to device
    if fdir != os.path.sep:
        fname = fname[len(fdir):]

    return (mtab_dict[fdir], fname)

