# -*- coding: utf-8 -*-
"""Provides tools for UPNP searches"""

from exceptions import MythError

from time import time
import socket

class MSearch( object ):
    """
    Opens a socket for performing UPNP searches.
    """
    def __init__(self):
        port = 1900
        addr = '239.255.255.250'
        self.dest = (addr, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                             socket.IPPROTO_UDP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            self.sock.bind(('', port))
        except socket.error, e:
             raise MythError(MythError.SOCKET, e)
        self.sock.setblocking(0.1)

    def __del__(self):
        self.sock.close()

    def search(self, timeout=5.0, filter=None):
        """
        obj.search(timeout=5.0, filter=None) -> response dicts

            timeout -- in seconds
            filter -- optional list of ST strings to search for
            response -- a generator returning dicts with the following fields
                content-length,     usn,    request,    ext,    st
                server,      location,      cache-control,      date
        """
        sock = self.sock
        sreq = '\r\n'.join(['M-SEARCH * HTTP/1.1',
                            'HOST: %s:%s' % self.dest,
                            'MAN: "ssdp:discover"',
                            'MX: %d' % timeout,
                            'ST: ssdp:all',''])
        self._runsearch = True
        #reLOC = re.compile('http://(?P<ip>[0-9\.]+):(?P<port>[0-9]+)/.*')

        # spam the request a couple times
        [sock.sendto(sreq, self.dest) for i in range(3)]

        atime = time()+timeout
        while (time()<atime) and self._runsearch:
            try:
                sdata, saddr = sock.recvfrom(2048)
            except socket.error:
                continue #no data, continue

            lines = sdata.split('\n')
            sdict = {'request':lines[0].strip()}
            for line in lines[1:]:
                fields = line.split(':',1)
                if len(fields) == 2:
                    sdict[fields[0].strip().lower()] = fields[1].strip()

            if 'st' not in sdict:
                continue

            if filter:
                if sdict['st'] not in filter:
                    continue

            yield sdict

    def searchMythBE(self, timeout=5.0):
        """Custom search that filters for mythbackend."""
        location = []
        for res in self.search(timeout, (\
                'urn:schemas-mythtv-org:device:MasterMediaServer:1',
                'urn:schemas-mythtv-org:device:SlaveMediaServer:1')):
            if res['location'] not in location:
                location.append(res['location'])
                yield res

    def searchMythFE(self, timeout=5.0):
        """Custom search that filters for mythfrontend."""
        location = []
        for res in self.search(timeout, \
                'urn:schemas-upnp-org:device:MediaRenderer:1'):
            if 'MythTv' not in res['server']:
                continue
            if res['location'] not in location:
                location.append(res['location'])
                yield res

    def terminateSearch(self):
        """Prematurely terminate an in-progress search."""
        self._runsearch = False

