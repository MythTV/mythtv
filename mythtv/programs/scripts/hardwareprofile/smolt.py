# -*- coding: utf-8 -*-

# smolt - Fedora hardware profiler
#
# Copyright (C) 2007 Mike McGrath
# Copyright (C) 2009 Sebastian Pipping <sebastian@pipping.org>
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

######################################################
# This class is a basic wrapper for the dbus bindings
#
# I have completely destroyed this file, it needs some cleanup
# - mmcgrath
######################################################
# TODO
#
# Abstract "type" in device class
# Find out what we're not getting
#

from __future__ import print_function
#import dbus
from i18n import _
import platform
import software
import subprocess
import requests
import sys
import os
try:
    from urllib2 import build_opener
except ImportError:
    from urllib.request import build_opener
try:
    from urllib.parse import urlparse
except ImportError:
    from urlparse import urlparse
import json
from json import JSONEncoder
import datetime
import logging

import config
from smolt_config import get_config_attr
from fs_util import get_fslist
from devicelist import cat

from devicelist import get_device_list
import logging
from logging.handlers import RotatingFileHandler
import codecs
import MultipartPostHandler

try:
    import subprocess
except ImportError as e:
    pass

try:
    long()
except NameError:
    long = int

WITHHELD_MAGIC_STRING = 'WITHHELD'
SELINUX_ENABLED = 1
SELINUX_DISABLED = 0
SELINUX_WITHHELD = -1

EXCEPTIONS = (requests.exceptions.HTTPError,
              requests.exceptions.URLRequired,
              requests.exceptions.Timeout,
              requests.exceptions.ConnectionError,
              requests.exceptions.InvalidURL)

fs_types = get_config_attr("FS_TYPES", ["ext2", "ext3", "xfs", "reiserfs"])
fs_mounts = dict.fromkeys(get_config_attr("FS_MOUNTS", ["/", "/home", "/etc", "/var", "/boot"]), True)
fs_m_filter = get_config_attr("FS_M_FILTER", False)
fs_t_filter = get_config_attr("FS_T_FILTER", False)

smoonURL = get_config_attr("SMOON_URL", "http://smolts.org/")
secure = get_config_attr("SECURE", 0)
hw_uuid_file = get_config_attr("HW_UUID", "/etc/smolt/hw-uuid")
admin_token_file = get_config_attr("ADMIN_TOKEN", '' )

clientVersion = '1.3.2'
smoltProtocol = '0.97'
supported_protocols = ['0.97',]
user_agent = 'smolt/%s' % smoltProtocol
timeout = 120.0
proxies = dict()
DEBUG = False


PCI_BASE_CLASS_STORAGE =        1
PCI_CLASS_STORAGE_SCSI =        0
PCI_CLASS_STORAGE_IDE =         1
PCI_CLASS_STORAGE_FLOPPY =      2
PCI_CLASS_STORAGE_IPI =         3
PCI_CLASS_STORAGE_RAID =        4
PCI_CLASS_STORAGE_OTHER =       80

PCI_BASE_CLASS_NETWORK =        2
PCI_CLASS_NETWORK_ETHERNET =    0
PCI_CLASS_NETWORK_TOKEN_RING =  1
PCI_CLASS_NETWORK_FDDI =        2
PCI_CLASS_NETWORK_ATM =         3
PCI_CLASS_NETWORK_OTHER =       80
PCI_CLASS_NETWORK_WIRELESS =    128

PCI_BASE_CLASS_DISPLAY =        3
PCI_CLASS_DISPLAY_VGA =         0
PCI_CLASS_DISPLAY_XGA =         1
PCI_CLASS_DISPLAY_3D =          2
PCI_CLASS_DISPLAY_OTHER =       80

PCI_BASE_CLASS_MULTIMEDIA =     4
PCI_CLASS_MULTIMEDIA_VIDEO =    0
PCI_CLASS_MULTIMEDIA_AUDIO =    1
PCI_CLASS_MULTIMEDIA_PHONE =    2
PCI_CLASS_MULTIMEDIA_HD_AUDIO = 3
PCI_CLASS_MULTIMEDIA_OTHER =    80

PCI_BASE_CLASS_BRIDGE =         6
PCI_CLASS_BRIDGE_HOST =         0
PCI_CLASS_BRIDGE_ISA =          1
PCI_CLASS_BRIDGE_EISA =         2
PCI_CLASS_BRIDGE_MC =           3
PCI_CLASS_BRIDGE_PCI =          4
PCI_CLASS_BRIDGE_PCMCIA =       5
PCI_CLASS_BRIDGE_NUBUS =        6
PCI_CLASS_BRIDGE_CARDBUS =      7
PCI_CLASS_BRIDGE_RACEWAY =      8
PCI_CLASS_BRIDGE_OTHER =        80

PCI_BASE_CLASS_COMMUNICATION =  7
PCI_CLASS_COMMUNICATION_SERIAL = 0
PCI_CLASS_COMMUNICATION_PARALLEL = 1
PCI_CLASS_COMMUNICATION_MULTISERIAL = 2
PCI_CLASS_COMMUNICATION_MODEM = 3
PCI_CLASS_COMMUNICATION_OTHER = 80

PCI_BASE_CLASS_INPUT =          9
PCI_CLASS_INPUT_KEYBOARD =      0
PCI_CLASS_INPUT_PEN =           1
PCI_CLASS_INPUT_MOUSE =         2
PCI_CLASS_INPUT_SCANNER =       3
PCI_CLASS_INPUT_GAMEPORT =      4
PCI_CLASS_INPUT_OTHER =         80

PCI_BASE_CLASS_SERIAL =         12
PCI_CLASS_SERIAL_FIREWIRE =     0
PCI_CLASS_SERIAL_ACCESS =       1

PCI_CLASS_SERIAL_SSA =          2
PCI_CLASS_SERIAL_USB =          3
PCI_CLASS_SERIAL_FIBER =        4
PCI_CLASS_SERIAL_SMBUS =        5


# Taken from the DMI spec
FORMFACTOR_LIST = [ "Unknown",
                "Other",
                "Unknown",
                "Desktop",
                "Low Profile Desktop",
                "Pizza Box",
                "Mini Tower",
                "Tower",
                "Portable",
                "Laptop",
                "Notebook",
                "Hand Held",
                "Docking Station",
                "All In One",
                "Sub Notebook",
                "Space-saving",
                "Lunch Box",
                "Main Server Chassis",
                "Expansion Chassis",
                "Sub Chassis",
                "Bus Expansion Chassis",
                "Peripheral Chassis",
                "RAID Chassis",
                "Rack Mount Chassis",
                "Sealed-case PC",
                "Multi-system",
                "CompactPCI",
                "AdvancedTCA"
    ]

def to_ascii(o, current_encoding='utf-8'):
    ''' This shouldn't even be required in python3 '''
    return o
    if not isinstance(o, basestring):
        return o

    if isinstance(o, unicode):
        s = o
    else:
        s = unicode(o, current_encoding)
    return s


class Host:
    def __init__(self, gate, uuid):
        cpuInfo = read_cpuinfo()
        memory = read_memory()
        self.UUID = uuid
        self.os = gate.process('distro', software.read_os(), WITHHELD_MAGIC_STRING)
        self.defaultRunlevel = gate.process('run_level', software.read_runlevel(), -1)

        self.bogomips = gate.process('cpu', cpuInfo.get('bogomips', 0), 0)
        self.cpuVendor = gate.process('cpu', cpuInfo.get('type', ''), WITHHELD_MAGIC_STRING)
        self.cpuModel = gate.process('cpu', cpuInfo.get('model', ''), WITHHELD_MAGIC_STRING)
        self.cpu_stepping = gate.process('cpu', cpuInfo.get('cpu_stepping', 0), 0)
        self.cpu_family = gate.process('cpu', cpuInfo.get('cpu_family', ''), '')
        self.cpu_model_num = gate.process('cpu', cpuInfo.get('cpu_model_num', 0), 0)
        self.numCpus = gate.process('cpu', cpuInfo.get('count', 0), 0)
        self.cpuSpeed = gate.process('cpu', cpuInfo.get('speed', 0), 0)

        try: #  These fail on the one *buntu 19.04 host tested, see get_sendable_host()
            self.systemMemory = gate.process('ram_size', memory['ram'], 0)
            self.systemSwap = gate.process('swap_size', memory['swap'], 0)
        except TypeError:
            self.systemMemory = 0
            self.systemSwap = 0

        self.kernelVersion = gate.process('kernel', os.uname()[2], WITHHELD_MAGIC_STRING)
        if gate.grants('language'):
            try:
                self.language = os.environ['LANG']
            except KeyError:
                try:
                    lang = subprocess.run(['grep', 'LANG', '/etc/sysconfig/i18n'],
                                          stdout=subprocess.PIPE)
                    if lang.returncode == 0:
                        self.language = lang.stdout.strip().split(b'"')[1]
                    else:
                        self.language = 'Unknown'
                except subprocess.CalledProcessError:
                    self.language = 'Unknown'
        else:
            self.language = WITHHELD_MAGIC_STRING

        tempform = platform.machine()
        self.platform = gate.process('arch', tempform, WITHHELD_MAGIC_STRING)

        if gate.grants('vendor'):
            #self.systemVendor = hostInfo.get('system.vendor'
            try:
                self.systemVendor = cat('/sys/devices/virtual/dmi/id/sys_vendor')[0].strip()
            except:
                self.systemVendor = 'Unknown'
        else:
            self.systemVendor = WITHHELD_MAGIC_STRING

        if gate.grants('model'):
            try:
                self.systemModel = cat('/sys/devices/virtual/dmi/id/product_name')[0].strip() + ' ' + cat('/sys/devices/virtual/dmi/id/product_version')[0].strip()
            except:
                self.systemModel = 'Unknown'
            #hostInfo was removed with the hal restructure
            #if not self.systemModel:
                #self.systemModel = hostInfo.get('system.hardware.product')
                #if hostInfo.get('system.hardware.version'):
                    #self.systemModel += ' ' + hostInfo.get('system.hardware.version')
            #if not self.systemModel:
                #self.systemModel = 'Unknown'
        else:
            self.systemModel = WITHHELD_MAGIC_STRING

        if gate.grants('form_factor'):
            try:
                formfactor_id = int(cat('/sys/devices/virtual/dmi/id/chassis_type')[0].strip())
                self.formfactor = FORMFACTOR_LIST[formfactor_id]
            except:
                self.formfactor = 'Unknown'
        else:
            self.formfactor = WITHHELD_MAGIC_STRING

        if tempform == 'ppc64':
            pass
            # if hostInfo.get('openfirmware.model'):
            #     if hostInfo['openfirmware.model'][:3] == 'IBM':
            #         self.systemVendor = 'IBM'
            #     model = hostInfo['openfirmware.model'][4:8]

            #     model_map = {
            #         '8842':'JS20',
            #         '6779':'JS21',
            #         '6778':'JS21',
            #         '7988':'JS21',
            #         '8844':'JS21',
            #         '0200':'QS20',
            #         '0792':'QS21',
            #     }
            #     try:
            #         model_name = model_map[model]
            #         self.systemModel = gate.process('model', model_name)
            #         self.formfactor = gate.process('form_factor', 'Blade')
            #     except KeyError:
            #         pass

        if gate.grants('selinux'):
            try:
                import selinux
                try:
                    if selinux.is_selinux_enabled() == 1:
                        self.selinux_enabled = SELINUX_ENABLED
                    else:
                        self.selinux_enabled = SELINUX_DISABLED
                except:
                    self.selinux_enabled = SELINUX_DISABLED
                try:
                    self.selinux_policy = selinux.selinux_getpolicytype()[1]
                except:
                    self.selinux_policy = "Unknown"
                try:
                    enforce = selinux.security_getenforce()
                    if enforce == 0:
                        self.selinux_enforce = "Permissive"
                    elif enforce == 1:
                        self.selinux_enforce = "Enforcing"
                    elif enforce == -1:
                        self.selinux_enforce = "Disabled"
                    else:
                        self.selinux_enforce = "FUBARD"
                except:
                    self.selinux_enforce = "Unknown"
            except ImportError:
                self.selinux_enabled = SELINUX_DISABLED
                self.selinux_policy = "Not Installed"
                self.selinux_enforce = "Not Installed"
        else:
            self.selinux_enabled = SELINUX_WITHHELD
            self.selinux_policy = WITHHELD_MAGIC_STRING
            self.selinux_enforce = WITHHELD_MAGIC_STRING


def get_file_systems(gate):
    if not gate.grants('file_systems'):
        return []

    if fs_t_filter:
        file_systems = [fs for fs in get_fslist() if fs.fs_type in fs_types]
    else:
        file_systems = get_fslist()

    file_systems = [fs for fs in file_systems if fs.mnt_dev.startswith('/dev/')]

    if fs_m_filter:
        for fs in file_systems:
            if not fs.mnt_pnt in fs_mounts:
                fs.mnt_pnt = WITHHELD_MAGIC_STRING
    else:
        for fs in file_systems:
            fs.mnt_pnt = WITHHELD_MAGIC_STRING

    return file_systems

def ignoreDevice(device):
    ignore = 1
    if device.bus == 'Unknown' or device.bus == 'unknown':
        return 1
    if device.vendorid in (0, None) and device.type is None:
        return 1
    if device.bus == 'usb' and device.driver == 'hub':
        return 1
    if device.bus == 'usb' and 'Hub' in device.description:
        return 1
    if device.bus == 'sound' and device.driver == 'Unknown':
        return 1
    if device.bus == 'pnp' and device.driver in ('Unknown', 'system'):
        return 1
    if device.bus == 'block' and device.type == 'DISK':
        return 1
    if device.bus == 'usb_device' and device.type is None:
        return 1
    return 0

class ServerError(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

def serverMessage(page):
    for line in page.split(b"\n"):
        if b'UUID:' in line:
            return line.strip()[6:]
        if b'ServerMessage:' in line:
            if b'Critical' in line:
                raise ServerError(line.split('ServerMessage: ')[1])
            else:
                print(_('Server Message: "%s"') % line.split(b'ServerMessage: ')[1])

def error(message):
    print(message)
    #print(message, file=sys.stderr)

def debug(message):
    if DEBUG:
        print(message)

def reset_resolver():
    '''Attempt to reset the system hostname resolver.
    returns 0 on success, or -1 if an error occurs.'''
    try:
        import ctypes
        try:
            resolv = ctypes.CDLL("libresolv.so.2")
            r = resolv.__res_init()
        except (OSError, AttributeError):
            print("Warning: could not find __res_init in libresolv.so.2")
            r = -1
        return r
    except ImportError:
        # If ctypes isn't supported (older versions of python for example)
        # Then just don't do anything
        pass

class SystemBusError(Exception):
    def __init__(self, message, hint = None):
        self.msg = message
        self.hint = hint

    def __str__(self):
        return str(self.msg)

class UUIDError(Exception):
    def __init__(self, message):
        self.msg = message

    def __str__(self):
        return str(self.msg)

class PubUUIDError(Exception):
    def __init__(self, message):
        self.msg = message

    def __str__(self):
        return str(self.msg)

class _HardwareProfile:
    devices = {}
    def __init__(self, gate, uuid):
#        try:
#            systemBus = dbus.SystemBus()
#        except:
#            raise SystemBusError, _('Could not bind to dbus.  Is dbus running?')
#
#        try:
#            mgr = self.dbus_get_interface(systemBus, 'org.freedesktop.Hal', '/org/freedesktop/Hal/Manager', 'org.freedesktop.Hal.Manager')
#            all_dev_lst = mgr.GetAllDevices()
#        except:
#            raise SystemBusError, _('Could not connect to hal, is it running?\nRun "service haldaemon start" as root')
#
#        self.systemBus = systemBus

        if gate.grants('devices'):
                self.host = Host(gate, uuid)
                self.devices = get_device_list()
#        for udi in all_dev_lst:
#            props = self.get_properties_for_udi (udi)
#            if udi == '/org/freedesktop/Hal/devices/computer':
#                try:
#                    vendor = props['system.vendor']
#                    if len(vendor.strip()) == 0:
#                        vendor = None
#                except KeyError:
#                    try:
#                        vendor = props['vendor']
#                        if len(vendor.strip()) == 0:
#                            vendor = None
#                    except KeyError:
#                        vendor = None
#                try:
#                    product = props['system.product']
#                    if len(product.strip()) == 0:
#                        product = None
#                except KeyError:
#                    try:
#                        product = props['product']
#                        if len(product.strip()) == 0:
#                            product = None
#                    except KeyError:
#                        product = None
#
#                # This could be done with python-dmidecode but it would pull
#                # In an extra dep on smolt.  It may not be worth it
#                if vendor is None or product is None:
#                    try:
#                        dmiOutput = subprocess.Popen('/usr/sbin/dmidecode r 2> /dev/null', shell=True, stdout=subprocess.PIPE).stdout
#                    except NameError:
#                        i, dmiOutput, e = os.popen('/usr/sbin/dmidecode', 'r')
#                    section = None
#                    sysvendor = None
#                    sysproduct = None
#                    boardvendor = None
#                    boardproduct = None
#                    for line in dmiOutput:
#                        line = line.strip()
#                        if "Information" in line:
#                            section = line
#                        elif section is None:
#                            continue
#                        elif line.startswith("Manufacturer: ") and section.startswith("System"):
#                            sysvendor = line.split("Manufacturer: ", 1)[1]
#                        elif line.startswith("Product Name: ") and section.startswith("System"):
#                            sysproduct = line.split("Product Name: ", 1)[1]
#                        elif line.startswith("Manufacturer: ") and section.startswith("Base Board"):
#                            boardvendor = line.split("Manufacturer: ", 1)[1]
#                        elif line.startswith("Product Name: ") and section.startswith("Base Board"):
#                            boardproduct = line.split("Product Name: ", 1)[1]
#                    status = dmiOutput.close()
#                    if status is None:
#                        if sysvendor not in (None, 'System Manufacturer') and sysproduct not in (None, 'System Name'):
#                            props['system.vendor'] = sysvendor
#                            props['system.product'] = sysproduct
#                        elif boardproduct is not None and boardproduct is not None:
#                            props['system.vendor'] = boardvendor
#                            props['system.product'] = boardproduct

        self.fss = get_file_systems(gate)

        self.distro_specific = self.get_distro_specific_data(gate)
        self.session = requests.Session()
        self.session.headers.update({'USER-AGENT': user_agent})

    def get_distro_specific_data(self, gate):
        dist_dict = {}
        try:
            import distros.all
        except ImportError:
            return dist_dict

        for d in distros.all.get():
            key = d.key()
            if d.detected():
                logging.info('Distro "%s" detected' % (key))
                d.gather(gate, debug=True)
                dist_dict[key] = {
                    'data':d.data(),
                    'html':d.html(),
                    'rst':d.rst(),
                    'rst_excerpt':d.rst_excerpt(),
                }
        return dist_dict

#    def get_properties_for_udi (self, udi):
#        dev = self.dbus_get_interface(self.systemBus, 'org.freedesktop.Hal',
#                                      udi, 'org.freedesktop.Hal.Device')
#        return dev.GetAllProperties()

#    def dbus_get_interface(self, bus, service, object, interface):
#        iface = None
#        # dbus-python bindings as of version 0.40.0 use new api
#        if getattr(dbus, 'version', (0,0,0)) >= (0,40,0):
#            # newer api: get_object(), dbus.Interface()
#            proxy = bus.get_object(service, object)
#            iface = dbus.Interface(proxy, interface)
#        else:
#            # deprecated api: get_service(), get_object()
#            svc = bus.get_service(service)
#            iface = svc.get_object(object, interface)
#        return iface

    def get_sendable_devices(self, protocol_version=smoltProtocol):
        my_devices = []
        for device in self.devices:
            try:
                Bus = self.devices[device].bus
                VendorID = self.devices[device].vendorid
                DeviceID = self.devices[device].deviceid
                SubsysVendorID = self.devices[device].subsysvendorid
                SubsysDeviceID = self.devices[device].subsysdeviceid
                Driver = self.devices[device].driver
                Type = self.devices[device].type
                Description = self.devices[device].description
            except:
                continue
            else:
                if not ignoreDevice(self.devices[device]):
                    my_devices.append({"vendor_id": VendorID,
                                       "device_id": DeviceID,
                                       "subsys_vendor_id": SubsysVendorID,
                                       "subsys_device_id": SubsysDeviceID,
                                       "bus": Bus,
                                       "driver": Driver,
                                       "type": Type,
                                       "description": Description})

        return my_devices

    def get_sendable_host(self, protocol_version=smoltProtocol):
        return {'uuid' :            self.host.UUID,
                'os' :              self.host.os,
                'default_runlevel': self.host.defaultRunlevel,
                'language' :        self.host.language,
                'platform' :        self.host.platform,
                'bogomips' :        self.host.bogomips,
                'cpu_vendor' :      self.host.cpuVendor,
                'cpu_model' :       self.host.cpuModel,
                'cpu_stepping' :    self.host.cpu_stepping,
                'cpu_family' :      self.host.cpu_family,
                'cpu_model_num' :   self.host.cpu_model_num,
                'num_cpus':         self.host.numCpus,
                'cpu_speed' :       self.host.cpuSpeed,
                'system_memory' :   self.host.systemMemory,
                'system_swap' :     self.host.systemSwap,
                'vendor' :          self.host.systemVendor,
                'system' :          self.host.systemModel,
                'kernel_version' :  self.host.kernelVersion,
                'formfactor' :      self.host.formfactor,
                'selinux_enabled':  self.host.selinux_enabled,
                'selinux_policy':   self.host.selinux_policy,
                'selinux_enforce':  self.host.selinux_enforce
                }

    def get_sendable_fss(self, protocol_version=smoltProtocol):
        return [fs.to_dict() for fs in self.fss]

    def write_pub_uuid(self, uuiddb, smoonURL, pub_uuid, uuid):
        smoonURLparsed=urlparse(smoonURL)
        if pub_uuid is None:
            return

        try:
            uuiddb.set_pub_uuid(uuid, smoonURLparsed.netloc, pub_uuid)
        except Exception as e:
            sys.stderr.write(_('\tYour pub_uuid could not be written: {}.\n\n'.format(e)))
        return

    def write_admin_token(self,smoonURL,admin,admin_token_file):
        smoonURLparsed=urlparse(smoonURL)
        admin_token_file += ("-"+smoonURLparsed.netloc)
        try:
            with open(admin_token_file, 'w') as at_file:
                at_file.write(admin)
        except Exception as e:
            sys.stderr.write(_('\tYour admin token could not be cached: %s\n' % e))
        return

    def get_submission_data(self, prefered_protocol=None):
        send_host_obj = self.get_sendable_host(prefered_protocol)
        send_host_obj['devices'] = self.get_sendable_devices(prefered_protocol)
        send_host_obj['fss'] = self.get_sendable_fss(prefered_protocol)
        send_host_obj['smolt_protocol'] = prefered_protocol

        dist_data_dict = {}
        for k, v in self.distro_specific.items():
            dist_data_dict[k] = v['data']
        send_host_obj['distro_specific'] = dist_data_dict

        return send_host_obj

    def get_distro_specific_html(self):
        lines = []
        if not self.distro_specific:
            lines.append(_('No distribution-specific data yet'))
        else:
            for k, v in self.distro_specific.items():
                lines.append(v['html'])
        return '\n'.join(lines)

    def send(self, uuiddb, uuid, user_agent=user_agent, smoonURL=smoonURL, timeout=timeout, proxies=proxies, batch=False):
        def serialize(object, human=False):
            if human:
                indent = 2
                sort_keys = True
            else:
                indent = None
                sort_keys = False
            return JSONEncoder(indent=indent, sort_keys=sort_keys).encode(object)

        reset_resolver()

        #first find out the server desired protocol
        try:
            current_url = smoonURL + 'tokens/token_json?uuid=%s' % self.host.UUID
            token = self.session.get(current_url, proxies=proxies, timeout=timeout)
        except EXCEPTIONS as e:
            error(_('Error contacting Server (tokens): {}'.format(e)))
            self.session.close()
            return (1, None, None)
        tok_obj = token.json()
        try:
            if tok_obj['prefered_protocol'] in supported_protocols:
                prefered_protocol = tok_obj['prefered_protocol']
            else:
                self.session.close()
                error(_('Wrong version, server incapable of handling your client'))
                return (1, None, None)
            tok = tok_obj['token']

        except ValueError as e:
            self.session.close()
            error(_('Something went wrong fetching a token'))

        send_host_obj = self.get_submission_data(prefered_protocol)


        debug('smoon server URL: %s' % smoonURL)

        serialized_host_obj_machine = serialize(send_host_obj, human=False)

        # Log-dump submission data
        log_matrix = {
            '.json':serialize(send_host_obj, human=True),
            '-distro.html':self.get_distro_specific_html(),
            '.rst':'\n'.join(map(to_ascii, self.getProfile())),
        }
        logdir = os.path.expanduser('~/.smolt/')
        try:
            if not os.path.exists(logdir):
                os.mkdir(logdir, 0o0700)

            for k, v in log_matrix.items():
                filename = os.path.expanduser(os.path.join(
                        logdir, 'submission%s' % k))
                r = RotatingFileHandler(filename, \
                        maxBytes=1000000, backupCount=9)
                r.stream.write(v)
                r.doRollover()
                r.close()
                os.remove(filename)
        except:
            pass
        del logdir
        del log_matrix


        debug('sendHostStr: %s' % serialized_host_obj_machine)
        debug('Sending Host')

        if batch:
            entry_point = "client/batch_add_json"
            logging.debug('Submitting in asynchronous mode')
        else:
            entry_point = "client/add_json"
            logging.debug('Submitting in synchronous mode')
        request_url = smoonURL + entry_point
        logging.debug('Sending request to %s' % request_url)
        try:
            opener = build_opener(MultipartPostHandler.MultipartPostHandler)
            params = {  'uuid':self.host.UUID,
                        'host':serialized_host_obj_machine,
                        'token':tok,
                        'smolt_protocol':smoltProtocol}
            o = opener.open(request_url, params)

        except Exception as e:
            error(_('Error contacting Server ([batch_]add_json): {}'.format(e)))
            return (1, None, None)
        else:
            try:
                server_response = serverMessage(o.read())
            except ServerError as e:
                self.session.close()
                error(_('Error contacting server: %s') % e)
                return (1, None, None)

            o.close()
            if batch:
                pub_uuid = None
            else:
                pub_uuid = server_response.decode('latin1')
            self.write_pub_uuid(uuiddb, smoonURL, pub_uuid, uuid)

            try:
                admin_token = self.session.get(smoonURL + 'tokens/admin_token_json?uuid=%s' % self.host.UUID,
                                               proxies=proxies, timeout=timeout)
            except EXCEPTIONS as e:
                self.session.close()
                error(_('An error has occured while contacting the server: %s' % e))
                sys.exit(1)

            try:
                admin_obj = json.loads(admin_token.content)
            except json.JSONDecodeError:
                self.session.close()
                error(_('Incorrect server response. Expected a JSON string'))
                return (1, None, None)

            if admin_obj['prefered_protocol'] in supported_protocols:
                prefered_protocol = admin_obj['prefered_protocol']
            else:
                self.session.close()
                error(_('Wrong version, server incapable of handling your client'))
                return (1, None, None)
            admin = admin_obj['token']

            if  not admin_token_file == '' :
                self.write_admin_token(smoonURL,admin,admin_token_file)

        return (0, pub_uuid, admin)

# end of _HardwareProfile.send()


    def regenerate_pub_uuid(self, uuiddb, uuid, user_agent=user_agent, smoonURL=smoonURL, timeout=timeout):
        try:
            new_uuid = self.session.get(smoonURL + 'client/regenerate_pub_uuid?uuid=%s' % self.host.UUID,
                                        proxies=proxies, timeout=timeout)
        except EXCEPTIONS as e:
            raise ServerError(str(e))

        try:
            response_dict = new_uuid.json()  # Either JSON or an error page in (X)HTML
        except Exception as e:
            self.session.close()
            serverMessage(new_uuid.text)
            raise ServerError(_('Reply from server could not be interpreted'))
        else:
            try:
                pub_uuid = response_dict['pub_uuid']
            except KeyError:
                self.session.close()
                raise ServerError(_('Reply from server could not be interpreted'))
            self.write_pub_uuid(uuiddb, smoonURL, pub_uuid, uuid)
            return pub_uuid


    def get_general_info_excerpt(self):
        d = {
            _('OS'):self.host.os,
            _('Default run level'):self.host.defaultRunlevel,
            _('Language'):self.host.language,
        }
        lines = []
        for k, v in d.items():
            lines.append('%s: %s' % (k, v))
        lines.append('...')
        return '\n'.join(lines)

    def get_devices_info_excerpt(self):
        lines = []
        for i, (VendorID, DeviceID, SubsysVendorID, SubsysDeviceID, Bus, Driver, Type, Description) \
                in enumerate(self.deviceIter()):
            if i == 3:
                break
            lines.append('(%s:%s:%s:%s) %s, %s, %s, %s' % (VendorID, DeviceID, SubsysVendorID, \
                    SubsysDeviceID, Bus, Driver, Type, Description))
        lines.append('...')
        return '\n'.join(lines)

    def get_file_system_info_excerpt(self):
        lines = []
        lines.append('device mtpt type bsize frsize blocks bfree bavail file ffree favail')
        for i, v in enumerate(self.fss):
            if i == 2:
                break
            lines.append(str(v))
        lines.append('...')
        return '\n'.join(lines)

    def get_distro_info_excerpt(self):
        for k, v in self.distro_specific.items():
            return v['rst_excerpt']
        return "No data, yet"

    def getProfile(self):
        printBuffer = []

        printBuffer.append('# ' + _('This is a Smolt report shown within your default pager.'))
        printBuffer.append('# ' + _('Below you can see what data you will submit to the server.'))
        printBuffer.append('# ' + _('To get back to Smolt exit the pager (try hitting "q").'))
        printBuffer.append('#')
        printBuffer.append('# ' + _('NOTE:  Editing this file does not change the data submitted.'))
        printBuffer.append('')
        printBuffer.append('')

        printBuffer.append(_('General'))
        printBuffer.append('=================================')
        for label, data in self.hostIter():
            try:
                printBuffer.append('%s: %s' % (label, data))
            except UnicodeDecodeError:
                try:
                    printBuffer.append('%s: %s' % (unicode(label, 'utf-8'), data))
                except UnicodeDecodeError:
                    printBuffer.append('%r: %r' % (label, data))

        if self.devices:
            printBuffer.append('')
            printBuffer.append('')
            printBuffer.append(_('Devices'))
            printBuffer.append('=================================')

            for VendorID, DeviceID, SubsysVendorID, SubsysDeviceID, Bus, Driver, Type, Description in self.deviceIter():
                printBuffer.append('(%s:%s:%s:%s) %s, %s, %s, %s' % (VendorID, DeviceID, SubsysVendorID, SubsysDeviceID, Bus, Driver, Type, Description))

            printBuffer.append('')
            printBuffer.append('')
            printBuffer.append(_('Filesystem Information'))
            printBuffer.append('=================================')
            printBuffer.append('device mtpt type bsize frsize blocks bfree bavail file ffree favail')
            printBuffer.append('-------------------------------------------------------------------')
            for fs in self.fss:
                printBuffer.append(str(fs))

            for k, v in self.distro_specific.items():
                printBuffer.append('')
                printBuffer.append('')
                printBuffer.append(v['rst'])

            printBuffer.append('')
        return printBuffer


    def hostIter(self):
        '''Iterate over host information.'''
        yield _('UUID'), self.host.UUID
        yield _('OS'), self.host.os
        yield _('Default run level'), self.host.defaultRunlevel
        yield _('Language'), self.host.language
        yield _('Platform'), self.host.platform
        yield _('BogoMIPS'), self.host.bogomips
        yield _('CPU Vendor'), self.host.cpuVendor
        yield _('CPU Model'), self.host.cpuModel
        yield _('CPU Stepping'), self.host.cpu_stepping
        yield _('CPU Family'), self.host.cpu_family
        yield _('CPU Model Num'), self.host.cpu_model_num
        yield _('Number of CPUs'), self.host.numCpus
        yield _('CPU Speed'), self.host.cpuSpeed
        yield _('System Memory'), self.host.systemMemory
        yield _('System Swap'), self.host.systemSwap
        yield _('Vendor'), self.host.systemVendor
        yield _('System'), self.host.systemModel
        yield _('Form factor'), self.host.formfactor
        yield _('Kernel'), self.host.kernelVersion
        yield _('SELinux Enabled'), self.host.selinux_enabled
        yield _('SELinux Policy'), self.host.selinux_policy
        yield _('SELinux Enforce'), self.host.selinux_enforce

    def deviceIter(self):
        '''Iterate over our devices.'''
        for device in self.devices:
            Bus = self.devices[device].bus
            VendorID = self.devices[device].vendorid
            DeviceID = self.devices[device].deviceid
            SubsysVendorID = self.devices[device].subsysvendorid
            SubsysDeviceID = self.devices[device].subsysdeviceid
            Driver = self.devices[device].driver
            Type = self.devices[device].type
            Description = self.devices[device].description
            #Description = Description.decode('latin1')
            if not ignoreDevice(self.devices[device]):
                yield VendorID, DeviceID, SubsysVendorID, SubsysDeviceID, Bus, Driver, Type, Description


# This has got to be one of the ugliest fucntions alive
def read_cpuinfo():
    def get_entry(a, entry):
        e = entry.lower()
        if e not in a:
            return ""
        return a[e]

    if not os.access("/proc/cpuinfo", os.R_OK):
        return {}

    cpulist = open("/proc/cpuinfo", "r").read()
    uname = os.uname()[4].lower()

    # This thing should return a hwdict that has the following
    # members:
    #
    # class, desc (required to identify the hardware device)
    # count, type, model, model_number, model_ver, model_rev
    # bogomips, platform, speed, cache
    hwdict = { 'class': "CPU",
               'desc' : "Processor",
               }
    if uname[0] == "i" and uname[-2:] == "86" or (uname == "x86_64"):
        # IA32 compatible enough
        count = 0
        tmpdict = {}
        for cpu in cpulist.split("\n\n"):
            if not len(cpu):
                continue
            count = count + 1
            if count > 1:
                continue # just count the rest
            for cpu_attr in cpu.split("\n"):
                if not len(cpu_attr):
                    continue
                vals = cpu_attr.split(':')
                if len(vals) != 2:
                    # XXX: make at least some effort to recover this data...
                    continue
                name, value = vals[0].strip(), vals[1].strip()
                tmpdict[name.lower()] = value

        if uname == "x86_64":
            hwdict['platform'] = 'x86_64'
        else:
            hwdict['platform']      = "i386"

        hwdict['count']         = count
        hwdict['type']          = get_entry(tmpdict, 'vendor_id')
        hwdict['model']         = get_entry(tmpdict, 'model name')
        hwdict['model_number']  = get_entry(tmpdict, 'cpu family')
        hwdict['model_ver']     = get_entry(tmpdict, 'model')
        hwdict['cpu_stepping']  = get_entry(tmpdict, 'stepping')
        hwdict['cpu_family']    = get_entry(tmpdict, 'cpu family')
        hwdict['cpu_model_num'] = get_entry(tmpdict, 'model')
        hwdict['cache']         = get_entry(tmpdict, 'cache size')
        hwdict['bogomips']      = get_entry(tmpdict, 'bogomips')
        hwdict['other']         = get_entry(tmpdict, 'flags')
        mhz_speed               = get_entry(tmpdict, 'cpu mhz')
        if mhz_speed == "":
            # damn, some machines don't report this
            mhz_speed = "-1"
        try:
            hwdict['speed']         = int(round(float(mhz_speed)) - 1)
        except ValueError:
            hwdict['speed'] = -1


    elif uname in["alpha", "alphaev6"]:
        # Treat it as an an Alpha
        tmpdict = {}
        for cpu_attr in cpulist.split("\n"):
            if not len(cpu_attr):
                continue
            vals = cpu_attr.split(':')
            if len(vals) != 2:
                # XXX: make at least some effort to recover this data...
                continue
            name, value = vals[0].strip(), vals[1].strip()
            tmpdict[name.lower()] = value.lower()

        hwdict['platform']      = "alpha"
        hwdict['count']         = get_entry(tmpdict, 'cpus detected')
        hwdict['type']          = get_entry(tmpdict, 'cpu')
        hwdict['model']         = get_entry(tmpdict, 'cpu model')
        hwdict['model_number']  = get_entry(tmpdict, 'cpu variation')
        hwdict['model_version'] = "%s/%s" % (get_entry(tmpdict, 'system type'),
                                             get_entry(tmpdict,'system variation'))
        hwdict['model_rev']     = get_entry(tmpdict, 'cpu revision')
        hwdict['cache']         = "" # pitty the kernel doesn't tell us this.
        hwdict['bogomips']      = get_entry(tmpdict, 'bogomips')
        hwdict['other']         = get_entry(tmpdict, 'platform string')
        hz_speed                = get_entry(tmpdict, 'cycle frequency [Hz]')
        # some funky alphas actually report in the form "462375000 est."
        hz_speed = hz_speed.split()
        try:
            hwdict['speed']         = int(round(float(hz_speed[0]))) / 1000000
        except ValueError:
            hwdict['speed'] = -1

    elif uname in ["ia64"]:
        tmpdict = {}
        count = 0
        for cpu in cpulist.split("\n\n"):
            if not len(cpu):
                continue
            count = count + 1
            # count the rest
            if count > 1:
                continue
            for cpu_attr in cpu.split("\n"):
                if not len(cpu_attr):
                    continue
                vals = cpu_attr.split(":")
                if len(vals) != 2:
                    # XXX: make at least some effort to recover this data...
                    continue
                name, value = vals[0].strip(), vals[1].strip()
                tmpdict[name.lower()] = value.lower()

        hwdict['platform']      = uname
        hwdict['count']         = count
        hwdict['type']          = get_entry(tmpdict, 'vendor')
        hwdict['model']         = get_entry(tmpdict, 'family')
        hwdict['model_ver']     = get_entry(tmpdict, 'archrev')
        hwdict['model_rev']     = get_entry(tmpdict, 'revision')
        hwdict['bogomips']      = get_entry(tmpdict, 'bogomips')
        mhz_speed = tmpdict['cpu mhz']
        try:
            hwdict['speed'] = int(round(float(mhz_speed)) - 1)
        except ValueError:
            hwdict['speed'] = -1
        hwdict['other']         = get_entry(tmpdict, 'features')

    elif uname in ['ppc64','ppc']:
        tmpdict = {}
        count = 0
        for cpu in cpulist.split("processor"):
            if not len(cpu):
                continue
            count = count + 1
            # count the rest
            if count > 1:
                continue
            for cpu_attr in cpu.split("\n"):
                if not len(cpu_attr):
                    continue
                vals = cpu_attr.split(":")
                if len(vals) != 2:
                    # XXX: make at least some effort to recover this data...
                    continue
                name, value = vals[0].strip(), vals[1].strip()
                tmpdict[name.lower()] = value.lower()

        hwdict['platform'] = uname
        hwdict['count'] = count
        hwdict['model'] = get_entry(tmpdict, "cpu")
        hwdict['model_ver'] = get_entry(tmpdict, 'revision')
        hwdict['bogomips'] = get_entry(tmpdict, 'bogomips')
        hwdict['vendor'] = get_entry(tmpdict, 'machine')
        if get_entry(tmpdict, 'cpu').startswith('ppc970'):
            hwdict['type'] = 'IBM'
        else:
            hwdict['type'] = get_entry(tmpdict, 'platform')
        hwdict['system'] = get_entry(tmpdict, 'detected as')
        # strings are postpended with "mhz"
        mhz_speed = get_entry(tmpdict, 'clock')[:-3]
        try:
            hwdict['speed'] = int(round(float(mhz_speed)) - 1)
        except ValueError:
            hwdict['speed'] = -1

    elif uname in ["sparc64","sparc"]:
        tmpdict = {}
        bogomips = 0
        for cpu in cpulist.split("\n\n"):
            if not len(cpu):
                continue

            for cpu_attr in cpu.split("\n"):
                if not len(cpu_attr):
                    continue
                vals = cpu_attr.split(":")
                if len(vals) != 2:
                    # XXX: make at least some effort to recover this data...
                    continue
                name, value = vals[0].strip(), vals[1].strip()
                if name.endswith('Bogo'):
                    if bogomips == 0:
                         bogomips = int(round(float(value)) )
                         continue
                    continue
                tmpdict[name.lower()] = value.lower()
        system = ''
        if not os.access("/proc/openprom/banner-name", os.R_OK):
            system = 'Unknown'
        if os.access("/proc/openprom/banner-name", os.R_OK):
            with open("/proc/openprom/banner-name", "r") as banner_name:
                banner_name.read()
        hwdict['platform'] = uname
        hwdict['count'] = get_entry(tmpdict, 'ncpus probed')
        hwdict['model'] = get_entry(tmpdict, 'cpu')
        hwdict['type'] = get_entry(tmpdict, 'type')
        hwdict['model_ver'] = get_entry(tmpdict, 'type')
        hwdict['bogomips'] = bogomips
        hwdict['vendor'] = 'sun'
        hwdict['cache'] = "" # pitty the kernel doesn't tell us this.
        speed = int(round(float(bogomips))) / 2
        hwdict['speed'] = speed
        hwdict['system'] = system

    else:
        # XXX: expand me. Be nice to others
        hwdict['platform']      = uname
        hwdict['count']         = 1 # Good as any
        hwdict['type']          = uname
        hwdict['model']         = uname
        hwdict['model_number']  = ""
        hwdict['model_ver']     = ""
        hwdict['model_rev']     = ""
        hwdict['cache']         = ""
        hwdict['bogomips']      = ""
        hwdict['other']         = ""
        hwdict['speed']         = 0

    # make sure we get the right number here
    if not hwdict["count"]:
        hwdict["count"] = 1
    else:
        try:
            hwdict["count"] = int(hwdict["count"])
        except:
            hwdict["count"] = 1
        else:
            if hwdict["count"] == 0: # we have at least one
                hwdict["count"] = 1

    # If the CPU can do frequency scaling the CPU speed returned
    # by /proc/cpuinfo might be less than the maximum possible for
    # the processor. Check sysfs for the proper file, and if it
    # exists, use that value.  Only use the value from CPU #0 and
    # assume that the rest of the CPUs are the same.

    if os.path.exists('/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq'):
        with open('/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq') as cpu_m_freq:
            hwdict['speed'] = int(cpu_m_freq.read().strip()) / 1000

    # This whole things hurts a lot.
    return hwdict



def read_memory():
    un = os.uname()
    kernel = un[2]
    if kernel[:2] == "5.":
        return read_memory_2_6()
    if kernel[:2] == "4.":
        return read_memory_2_6()
    if kernel[:2] == "3.":
        return read_memory_2_6()
    if kernel[:3] == "2.6":
        return read_memory_2_6()
    if kernel[:3] == "2.4":
        return read_memory_2_4()

def read_memory_2_4():
    if not os.access("/proc/meminfo", os.R_OK):
        return {}

    with open("/proc/meminfo", "r") as m_info:
        meminfo = m_info.read()
    lines = meminfo.split("\n")
    curline = lines[1]
    memlist = curline.split()
    memdict = {}
    memdict['class'] = "MEMORY"
    megs = long(memlist[1])/(1024*1024)
    if megs < 32:
        megs = megs + (4 - (megs % 4))
    else:
        megs = megs + (16 - (megs % 16))
    memdict['ram'] = str(megs)
    curline = lines[2]
    memlist = curline.split()
    # otherwise, it breaks on > ~4gigs of swap
    megs = long(memlist[1])/(1024*1024)
    memdict['swap'] = str(megs)
    return memdict

def read_memory_2_6():
    if not os.access("/proc/meminfo", os.R_OK):
        return {}
    with open("/proc/meminfo", "r") as m_info:
        meminfo = m_info.read()
    lines = meminfo.split("\n")
    dict = {}
    for line in lines:
        blobs = line.split(":", 1)
        key = blobs[0]
        if len(blobs) == 1:
            continue
        #print blobs
        value = blobs[1].strip()
        dict[key] = value

    memdict = {}
    memdict["class"] = "MEMORY"

    total_str = dict['MemTotal']
    blips = total_str.split(" ")
    total_k = long(blips[0])
    megs = long(total_k/(1024))

    swap_str = dict['SwapTotal']
    blips = swap_str.split(' ')
    swap_k = long(blips[0])
    swap_megs = long(swap_k/(1024))

    memdict['ram'] = str(megs)
    memdict['swap'] = str(swap_megs)
    return memdict


def create_profile_nocatch(gate, uuid):
    return _HardwareProfile(gate, uuid)


## For refactoring, I'll probably want to make a library
## Of command line tool functions
## This is one of them
def create_profile(gate, uuid):
    try:
        return create_profile_nocatch(gate, uuid)
    except SystemBusError as e:
        error(_('Error:') + ' ' + e.msg)
        if e.hint is not None:
            error('\t' + _('Hint:') + ' ' + e.hint)
        sys.exit(8)

##This is another
def get_profile_link(smoonURL, pub_uuid):
    return smoonURL + 'client/show/%s' % pub_uuid

def read_uuid():
    try:
        with open(hw_uuid_file) as hw_uuid:
            UUID = hw_uuid.read().strip()
    except (FileNotFoundError, IOError):
        try:
            with open('/proc/sys/kernel/random/uuid') as rand_uuid:
                UUID = rand_uuid.read().strip()
            with open(hw_uuid_file, 'w') as write_uuid:
                write_uuid.write(UUID)
        except (FileNotFoundError, IOError):
            sys.stderr.write(_('Unable to determine UUID of system!\n'))
            raise UUIDError('Unable to get/save UUID. file = %s.  Please run once as root.' % hw_uuid_file)
    return UUID

def read_pub_uuid(uuiddb, uuid, user_agent=user_agent, smoonURL=smoonURL, timeout=timeout, silent=False):
    smoonURLparsed=urlparse(smoonURL)
    res = uuiddb.get_pub_uuid(uuid, smoonURLparsed.netloc)
    if res:
        return res

    try:
        o = requests.get(smoonURL + 'client/pub_uuid?uuid=%s' % uuid,
                             proxies=proxies, timeout=timeout)
        pudict = o.json()
        uuiddb.set_pub_uuid(uuid, smoonURLparsed.netloc, pudict["pub_uuid"])
        return pudict["pub_uuid"]
    except Exception as e:
        if not silent:
            error(_('Error determining public UUID: %s') % e)
            sys.stderr.write(_("Unable to determine Public UUID!  This could be a network error or you've\n"))
            sys.stderr.write(_("not submitted your profile yet.\n"))
            raise PubUUIDError('Could not determine Public UUID!\n')
