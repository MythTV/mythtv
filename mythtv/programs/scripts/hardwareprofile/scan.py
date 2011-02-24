# smolt - Fedora hardware profiler
#
# Copyright (C) 2007 Mike McGrath
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

import smolt
import simplejson, urllib
from i18n import _
import config

h = None

def hardware():
    # Singleton pattern
    global h
    if h == None:
        h = smolt.Hardware()
    return h

def get_config_attr(attr, default=""):
    if hasattr(config, attr):
        return getattr(config, attr)
    else:
        return default

def rating(profile, smoonURL):
    print ""
    print _("Current rating for vendor/model.")
    print ""
    scanURL='%s/client/host_rating?vendor=%s&system=%s' % (smoonURL, urllib.quote(hardware().host.systemVendor), urllib.quote(hardware().host.systemModel))
    r = simplejson.load(urllib.urlopen(scanURL))['ratings']
    rating_system = { '0' : _('Unrated/Unknown'),
                      '1' : _('Non-working'),
                      '2' : _('Partially-working'),
                      '3' : _('Requires 3rd party drivers'),
                      '4' : _('Works, needs additional configuration'),
                      '5' : _('Works out of the box')
                    }
    print "\tCount\tRating"
    print "\t-----------------"
    for rate in r:
        print "\t%s\t%s" % (r[rate], rating_system[rate])

def scan(profile, smoonURL):
    print _("Scanning %s for known errata.\n" % smoonURL)
    devices = []
    for VendorID, DeviceID, SubsysVendorID, SubsysDeviceID, Bus, Driver, Type, Description in hardware().deviceIter():
        if VendorID:
            devices.append('%s/%04x/%04x/%04x/%04x' % (Bus,
                                             int(VendorID or 0),
                                             int(DeviceID or 0),
                                             int(SubsysVendorID or 0),
                                             int(SubsysDeviceID or 0)) )
    searchDevices = 'NULLPAGE'
    devices.append('System/%s/%s' % ( urllib.quote(hardware().host.systemVendor), urllib.quote(hardware().host.systemModel) ))
    for dev in devices:
        searchDevices = "%s|%s" % (searchDevices, dev)
    scanURL='%s/smolt-w/api.php' % smoonURL
    scanData = 'action=query&titles=%s&format=json' % searchDevices
    try:
         r = simplejson.load(urllib.urlopen(scanURL, scanData))
    except ValueError:
        print "Could not wiki for errata!"
        return
    found = []

    for page in r['query']['pages']:
        try:
            if int(page) > 0:
                found.append('\t%swiki/%s' % (smoonURL, r['query']['pages'][page]['title']))
        except KeyError:
            pass

    if found:
        print _("\tErrata Found!")
        for f in found: print "\t%s" % f
    else:
        print _("\tNo errata found, if this machine is having issues please go to")
        print _("\tyour profile and create a wiki page for the device so others can")
        print _("\tbenefit")
      
if __name__ == "__main__":  
    # read the profile
    smoonURL = get_config_attr("SMOON_URL", "http://smolts.org/")
    try:
        profile = smolt.Hardware()
    except smolt.SystemBusError, e:
        error(_('Error:') + ' ' + e.msg)
        if e.hint is not None:
            error('\t' + _('Hint:') + ' ' + e.hint)
        sys.exit(8)
    scan(profile, smoonURL)
    rating(profile, smoonURL)

