# smolt - Fedora hardware profiler
#
# Copyright (C) 2007 Mike McGrath
# Copyright (C) 2011 Sebastian Pipping <sebastian@pipping.org>
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
import smolt
import json

try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen

try:
    import urlparse as parse
except ImportError:
    from urllib import parse

from i18n import _
from smolt_config import get_config_attr

def rating(profile, smoonURL, gate):
    print("")
    print(_("Current rating for vendor/model."))
    print("")
    scanURL='%s/client/host_rating?vendor=%s&system=%s' % (smoonURL, parse.quote(profile.host.systemVendor), parse.quote(profile.host.systemModel))
    r = json.load(urlopen(scanURL))['ratings']
    rating_system = { '0' : _('Unrated/Unknown'),
                      '1' : _('Non-working'),
                      '2' : _('Partially-working'),
                      '3' : _('Requires 3rd party drivers'),
                      '4' : _('Works, needs additional configuration'),
                      '5' : _('Works out of the box')
                    }
    print("\tCount\tRating")
    print("\t-----------------")
    for rate in r:
        print("\t%s\t%s" % (r[rate], rating_system[rate]))

def scan(profile, smoonURL, gate):
    print(_("Scanning %s for known errata.\n" % smoonURL))
    devices = []
    for VendorID, DeviceID, SubsysVendorID, SubsysDeviceID, Bus, Driver, Type, Description in profile.deviceIter():
        if VendorID:
            devices.append('%s/%04x/%04x/%04x/%04x' % (Bus,
                                             int(VendorID or 0),
                                             int(DeviceID or 0),
                                             int(SubsysVendorID or 0),
                                             int(SubsysDeviceID or 0)) )
    searchDevices = 'NULLPAGE'
    devices.append('System/%s/%s' % ( parse.quote(profile.host.systemVendor), parse.quote(profile.host.systemModel) ))
    for dev in devices:
        searchDevices = "%s|%s" % (searchDevices, dev)
    scanURL='%ssmolt-w/api.php' % smoonURL
    scanData = 'action=query&titles=%s&format=json' % searchDevices
    try:
         r = json.load(urlopen(scanURL, bytes(scanData, 'latin1')))
    except ValueError:
        print("Could not wiki for errata!")
        return
    found = []

    for page in r['query']['pages']:
        try:
            if int(page) > 0:
                found.append('\t%swiki/%s' % (smoonURL, r['query']['pages'][page]['title']))
        except KeyError:
            pass

    if found:
        print(_("\tErrata Found!"))
        for f in found: print("\t%s" % f)
    else:
        print(_("\tNo errata found, if this machine is having issues please go to"))
        print(_("\tyour profile and create a wiki page for the device so others can"))
        print(_("\tbenefit"))

if __name__ == "__main__":
    from gate import create_passing_gate
    gate = create_passing_gate()
    smoonURL = get_config_attr("SMOON_URL", "http://smolts.org/")
    profile = smolt.create_profile(gate, smolt.read_uuid())
    scan(profile, smoonURL, gate)
    rating(profile, smoonURL, gate)
