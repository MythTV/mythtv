# smolt - Fedora hardware profiler
#
# Copyright (C) 2010 Mike McGrath <mmcgrath@redhat.com>
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

import os
from hwdata import DeviceMap


# Info pulled from - http://www.acm.uiuc.edu/sigops/roll_your_own/7.c.1.html
DEVICE_CLASS_LIST ={
                '00' : 'NONE',
                '0001' : 'VIDEO',
                '01' : 'STORAGE',
                '0100' : 'SCSI',
                '0102' : 'FLOPPY',
                '0103' : 'IPI',
                '0104' : 'RAID',
                '02' : 'NETWORK',
                '0200' : 'ETHERNET',
                '0201' : 'TOKEN_RING',
                '0202' : 'FDDI',
                '0203' : 'ATM',
                '03' : 'VIDEO',
                '04' : 'MULTIMEDIA',
                '0400' : 'MULTIMEDIA_VIDEO',
                '0401' : 'MULTIMEDIA_AUDIO',
                '05' : 'MEMORY',
                '0500' : 'RAM',
                '0501' : 'FLASH',
                '06' : 'BRIDGE',
                '0600' : 'HOST/PCI',
                '0601' : 'PCI/ISA',
                '0602' : 'PCI/EISA',
                '0603' : 'PCI/MICRO',
                '0604' : 'PCI/PCI',
                '0605' : 'PCI/PCMCIA',
                '0606' : 'PCI/NUBUS',
                '0607' : 'PCI/CARDBUS',
                '07' : 'SIMPLE',
                '070000' : 'XT_SERIAL',
                '070001' : '16450_SERIAL',
                '070002' : '16550_SERIAL',
                '070100' : 'PARALLEL',
                '070101' : 'BI_PARALLEL',
                '070102' : 'ECP_PARALLEL',
                '08' : 'BASE',
                '080000' : '8259_INTERRUPT',
                '080001' : 'ISA',
                '080002' : 'EISA',
                '080100' : '8237_DMA',
                '080101' : 'ISA_DMA',
                '080102' : 'ESA_DMA',
                '080200' : '8254_TIMER',
                '080201' : 'ISA_TIMER',
                '080202' : 'EISA_TIMER',
                '080300' : 'RTC',
                '080301' : 'ISA_RTC',
                '09' : 'INPUT',
                '0900' : 'KEYBOARD',
                '0901' : 'PEN',
                '0902' : 'MOUSE',
                '0A' : 'DOCKING',
                '0B' : 'PROCESSOR',
                '0C' : 'SERIAL',
                '0C00' : 'FIREWIRE',
                '0C01' : 'ACCESS',
                '0C02' : 'SSA',
                '0C03' : 'USB',
                'FF' : 'MISC' }

BUS_LIST = [ 'pci', 'usb' ]
PCI_PATH='/sys/bus/pci/devices/'
DATA_LIST = [   'vendor',
                'device',
                'subsystem_vendor',
                'subsystem_device']

def cat(file_name):
    fd = open(file_name)
    results = fd.readlines()
    fd.close()
    return results

def get_class(class_id):
    for i in [ 8, 6, 4 ]:
        try:
            return DEVICE_CLASS_LIST[class_id[2:i].upper()]
        except KeyError:
            pass
    return 'NONE'

pci = DeviceMap('pci')
usb = DeviceMap('usb')



def device_factory(cls, id):
    cls = eval(cls.upper() + 'Device')
    device = cls(id)
    try:
        device.process()
    except OSError:
        pass
    except OSError:
        pass
    return device

class Device:
    bus = 'Unknown'
    def __init__(self, id):
        self.id             = id
        self.vendorid       = None
        self.deviceid       = None
        self.type           = None
        self.description    = 'Unknown'
        self.subsysvendorid = None
        self.subsysdeviceid = None
        self.driver         = 'None'
    def process(self):
        pass

class PCIDevice( Device ):
    bus = 'PCI'
    def process(self):
        PATH = '/sys/bus/pci/devices/' + self.id + '/'
        self.vendorid       = int(cat(PATH +           'vendor')[0].strip(), 16)
        self.deviceid       = int(cat(PATH +           'device')[0].strip(), 16)
        self.subsysvendorid = int(cat(PATH + 'subsystem_vendor')[0].strip(), 16)
        self.subsysdeviceid = int(cat(PATH + 'subsystem_device')[0].strip(), 16)
        self.type           = get_class(cat(PATH +      'class')[0].strip())
        self.description    = pci.subdevice(self.vendorid,
                                            self.deviceid,
                                            self.subsysvendorid,
                                            self.subsysdeviceid)

class USBDevice( Device ):
    bus = 'USB'
    def process(self):
        PATH = '/sys/bus/usb/devices/' + self.id + '/'
        self.vendorid       = int(cat(PATH +         'idVendor')[0].strip(), 16)
        self.deviceid       = int(cat(PATH +        'idProduct')[0].strip(), 16)
        try:
            desc                =     cat(PATH +          'product')[0].strip().decode('ASCII',  errors='ignore')
        except:
            #The fedora python pkg is broken and doesn't like decode with options, so this is a fallback
            desc                =     cat(PATH +          'product')[0].strip()
        self.description    = usb.device(self.vendorid,
                                         self.deviceid,
                                         alt=desc)

def get_device_list():
    devices = {}
    for bus in BUS_LIST:
        PATH = '/sys/bus/' + bus + '/devices/'
        if os.path.exists(PATH):
            for device in os.listdir(PATH):
                devices[device] = device_factory(bus, device)
    return devices

