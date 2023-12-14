# -*- coding: utf-8 -*-

# smolt - Fedora hardware profiler
#
# Copyright (C) 2010 Mike McGrath <mmcgrath@redhat.com>
# Copyright (C) 2011 Alexandre Rostovtsev <tetromino@gmail.com>
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

from __future__ import print_function
from __future__ import unicode_literals
from __future__ import division
from __future__ import absolute_import
from builtins import int
from builtins import open
from builtins import object
from smolt_config import get_config_attr

class myVendor(object):
    def __init__(self):
        self.name = ""
        self.num = ""
        self.devices = {}

class myDevice(object):
    def __init__(self):
        self.name = ""
        self.num = ""
        self.subvendors = {}
        self.vendor = ""

class DeviceMap(object):
    vendors = {}

    def __init__(self, bus='pci'):
        self.vendors['pci'] = self.device_map('pci')
        self.vendors['usb'] = self.device_map('usb')

    def device_map(self, bus='pci'):
        import os
        HWDATA_DIRS = [ get_config_attr("HWDATA_DIR"), '/usr/share/hwdata','/usr/share/misc','/usr/share/' ]
        for hwd_file in HWDATA_DIRS:
            fn = "%s/%s.ids" % (hwd_file, bus)
            if os.path.isfile(fn + ".gz"):
                import gzip
                try:
                    # File isn't in the default python3 UTF-8, using latin1
                    fo = gzip.open(fn + ".gz", 'rt', encoding='latin1')
                    break
                except IOError:
                    pass
            else:
                try:
                    fo = open(fn, 'rt', encoding='latin1')
                    break
                except IOError:
                    pass
        else:
            raise Exception('Hardware data file not found.  Please set the location HWDATA_DIR in config.py')



        vendors = {}
        curvendor = None
        curdevice = None
        for line in fo.readlines():
            if line.startswith('#') or not line.strip():
                continue
            elif not line.startswith('\t'):
                curvendor = myVendor()
                try:
                    curvendor.num = int(line[0:4], 16)
                except:
                    continue
                curvendor.name = line[6:-1]
                vendors[curvendor.num] = curvendor
                continue

            elif line.startswith('\t\t'):
                line = line.replace('\t', '')
                thisdev = myDevice()
                try:
                    thisdev.vendor = int(line[0:4], 16)
                except:
                    continue
                try:
                    thisdev.num = int(line[5:9], 16)
                except:
                    continue
                thisdev.name = line[11:-1]
                subvend = myVendor()
                try:
                    subvend.num = thisdev.vendor
                except:
                    continue
                subvend.name = ""

                if subvend.num not in curdevice.subvendors:
                    curdevice.subvendors[subvend.num] = subvend
                    subvend.devices[thisdev.num] = thisdev
                else:
                    subvend = curdevice.subvendors[subvend.num]
                    subvend.devices[thisdev.num] = thisdev

                continue

            elif line.startswith('\t'):
                line = line.replace('\t', '')
                curdevice = myDevice()
                try:
                    curdevice.num = int(line[0:4], 16)
                except:
                    continue
                curdevice.name = line[6:-1]
                try:
                    curdevice.vendor = int(curvendor.num)
                except:
                    continue
                curvendor.devices[curdevice.num] = curdevice
                continue
            else:
                print(line)
                continue
        fo.close()
        # This introduces a bug, will fix later.
#        vendors[0] = myVendor()
#        vendors[0].name = 'N/A'
        return vendors

    def vendor(self, vend, subvend=None, alt='N/A', bus='pci'):
        try:
            vend = int(vend)
        except:
            return alt
        if vend == 0:
            return alt
        try:
            return self.vendors[bus][vend].devices[dev].subvendors[subvend].name + "a"
        except:
            try:
                return self.vendors[bus][vend].name
            except:
                return alt

    def device(self, vend, dev, subvend=None, subdevice=None, alt='N/A', bus='pci'):
        try:
            vend = int(vend)
        except:
            pass
        try:
            dev = int(dev)
        except:
            pass
        try:
            subvend = int(subvend)
        except:
            pass
        try:
            subdevice = int(subdevice)
        except:
            pass
        try:
            return self.vendors[bus][vend].devices[dev].name
        except:
            try:
                    self.vendors[bus][vend].devices[dev].name
            except:
                return alt

    def subdevice(self, vend, dev, subvend, subdevice, alt='N/A', bus='pci'):
#        return self.vendors[vend].devices[dev].name
        try:
            vend = int(vend)
        except:
            pass
        if vend == 0:
            return alt
        try:
            dev = int(dev)
        except:
            pass
        try:
            subvend = int(subvend)
        except:
            pass
        try:
            subdevice = int(subdevice)
        except:
            pass
        try:
            var = self.vendors[bus][vend].devices[dev].subvendors[subvend].devices[subdevice].name
            return var
        except:
            try:
                return self.vendors[bus][vend].devices[dev].name
            except:
                return alt
