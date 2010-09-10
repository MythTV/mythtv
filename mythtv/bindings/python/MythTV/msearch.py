# -*- coding: utf-8 -*-
"""Provides tools for UPNP searches"""

from exceptions import MythError
from logging import MythLog

from time import time
import socket

class MSearch( object ):
    """
    Opens a socket for performing UPNP searches.
    """
    def __init__(self):
        self.log = MythLog('Python M-Search')
        port = 1900
        addr = '239.255.255.250'
        self.dest = (addr, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                             socket.IPPROTO_UDP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listening = False
        while listening == False:
            try:
                self.sock.bind(('', port))
                self.addr = (addr, port)
                listening = True
            except socket.error, e:
                if port < 1910:
                    port += 1
                else:
                    raise MythError(MythError.SOCKET, e)
        self.log(MythLog.UPNP|MythLog.SOCKET,
                    'Port %d opened for UPnP search.' % port)
        self.sock.setblocking(0.1)

    def __del__(self):
        self.sock.close()

    def search(self, timeout=5.0, filter=None):
        """
        obj.search(timeout=5.0, filter=None) -> response dicts

          IN:
            timeout  -- in seconds
            filter   -- optional list of ST strings to search for
          OUT:
            response -- a generator returning dicts containing the fields
                content-length,   request,   date,   usn,    location,
                cache-control,    server,    ext,    st
        """
        self.log(MythLog.UPNP, 'running UPnP search')
        sock = self.sock
        sreq = '\r\n'.join(['M-SEARCH * HTTP/1.1',
                            'HOST: %s:%s' % self.addr,
                            'MAN: "ssdp:discover"',
                            'MX: %d' % timeout,
                            'ST: ssdp:all',''])
        self._runsearch = True
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

            if ('st' not in sdict) or ('location' not in sdict):
                continue

            if filter:
                if sdict['st'] not in filter:
                    continue

            self.log(MythLog.UPNP, sdict['st'], sdict['location'])
            yield sdict

    def searchMythBE(self, timeout=5.0):
        """
        obj.searchMythBE(timeout=5.0) -> response dicts

            Filters responses for those from `mythbackend`.

          IN:
            timeout  -- in seconds
          OUT:
            response -- a generator returning dicts containing the fields
                content-length,   request,   date,   usn,    location,
                cache-control,    server,    ext,    st
        """
        location = []
        for res in self.search(timeout, (\
                'urn:schemas-mythtv-org:device:MasterMediaServer:1',
                'urn:schemas-mythtv-org:device:SlaveMediaServer:1')):
            if res['location'] not in location:
                location.append(res['location'])
                yield res

    def searchMythFE(self, timeout=5.0):
        """
        obj.searchMythFE(timeout=5.0) -> response dicts

            Filters responses for those from `mythfrontend`.

          IN:
            timeout  -- in seconds
          OUT:
            response -- a generator returning dicts containing the fields
                content-length,   request,   date,   usn,    location,
                cache-control,    server,    ext,    st
        """
        location = []
        for res in self.search(timeout, \
                'urn:schemas-upnp-org:device:MediaRenderer:1'):
            if 'MythTv' not in res['server']:
                continue
            if res['location'] not in location:
                location.append(res['location'])
                yield res

    def terminateSearch(self):
        """
        Prematurely terminate an running search prior
            to the specified timeout.
        """
        self._runsearch = False

