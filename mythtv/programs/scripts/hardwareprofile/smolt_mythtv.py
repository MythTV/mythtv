# -*- coding: utf-8 -*-
from user import home
import glob
import time
import os
import urlgrabber
import string
from i18n import _

from smolt import smoltProtocol

# functions used to overwrite their respective methods in the _hardware class
# they should be updated whenever _hardware is updated.

def hardware_get_sendable_host(self, protocol_version=smoltProtocol):
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
    'selinux_enforce':  self.host.selinux_enforce,
    'myth_remote':      self.host.mythRemote,
    'myth_role':        self.host.mythRole,
    'myth_theme':       self.host.mythTheme,
    'myth_plugins':      self.host.mythPlugins,
    'myth_tuner':       self.host.mythTuner
    }

def hardware_hostIter(self):
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
    yield _('MythTV Remote'), self.host.mythRemote
    yield _('MythTV Role'), self.host.mythRole
    yield _('MythTV Theme'), self.host.mythTheme
    yield _('MythTV Plugin'), self.host.mythPlugins
    yield _('MythTV Tuner'), self.host.mythTuner

#  start of myth specific functions

def runMythRemote():
    smoltfile=home+"/.mythtv/smolt.info"
    config = {}
    try:
        config_file= open(smoltfile)
        for line in config_file:
            line = line.strip()
            if line and line[0] is not "#" and line[-1] is not "=":
                var,val = line.rsplit("=",1)
                config[var.strip()] = val.strip("\"")
    except:
        pass
    try:
        mythremote  = config["remote" ]
    except:
        mythremote = 'Unknown'
    return mythremote


def runMythTheme():
    root=home+"/.mythtv/themecache"
    date_file_list = []
    for folder in glob.glob(root):
    #    print "folder =", folder
        for file in glob.glob(folder + '/*'):
            if  os.path.isdir(file):
                stats = os.stat(file)
                lastmod_date = time.localtime(stats[8])
                date_file_tuple = lastmod_date, file
                date_file_list.append(date_file_tuple)

    date_file_list.sort()
    date_file_list.reverse()  # newest mod date now first
    try:
        file = date_file_list[0]
        folder, theme_name = os.path.split(file[1])
    except:
        theme_name="Unknown"
    myththeme=theme_name
    return myththeme

def runMythPlugins():
    pluginPath="lib/mythtv/plugins"
    pluginDir=''
    pluginList=[]
    if os.path.exists("/usr/local/%s" %pluginPath):
        pluginDir="/usr/local/%s" %pluginPath
    elif os.path.exists("/usr/%s" %pluginPath):
        pluginDir="/usr/%s" %pluginPath

    if pluginDir:
        plugglob="%s/libmyth*.so" %pluginDir
        plugins= glob.glob(plugglob)
        for plugin_file in plugins :
            ppath,pfi = os.path.split(plugin_file)
            pfname=pfi.partition(".")[0].replace("lib","",1)
            pluginList.append(pfname)

    pluginString=",".join(pluginList)
    return pluginString

def runMythRole():
    smoltfile=home+"/.mythtv/smolt.info"
    config = {}
    try:
        config_file= open(smoltfile)
        for line in config_file:
            line = line.strip()
            if line and line[0] is not "#" and line[-1] is not "=":
                var,val = line.rsplit("=",1)
                config[var.strip()] = val.strip("\"")
    except:
        pass
    try:
        myth_systemrole = config["systemtype" ]
        if myth_systemrole == "1":
            myth_systemrole="Master backend"
        elif myth_systemrole == "2":
            myth_systemrole="Master backend with Frontend"
        elif myth_systemrole == "3":
            myth_systemrole="Frontend "
        elif myth_systemrole == "4":
            myth_systemrole="Slave backend"
        elif myth_systemrole == "5":
            myth_systemrole="Slave backend with Frontend"
        elif myth_systemrole == "6":
            myth_systemrole="StandAlone"
    except:
        myth_systemrole = 'Unknown'
    return myth_systemrole

def runMythTuner():
    #Yes this is bad, but the bindings can't be trusted.
    role=runMythRole()
    num_encoders = -1
    if role == "StandAlone" or role == "Master backend" or role == "Master backend with Frontend":
        fencoder = False
        url="http://localhost:6544"
        try:
            web_page = urlgrabber.urlopen(url).readlines()
        except:
            return 0
        num_encoders = 0
        for line in web_page:
            line = line.strip()
            if line == '<h2>Encoder status</h2>':
                fencoder= True
                continue
            if fencoder:
                #print line
                encoders = line.split('.<br />')
                for encoder in encoders:
                    if encoder.find("currently not connected") == -1 and encoder.startswith("Encoder"):
                        num_encoders = num_encoders + 1
                if line == '<div class="content">':
                    break
    return num_encoders



if __name__ == '__main__':
    print "remote:"
    print runMythRemote()
    print "plugins:"
    print runMythPlugins()
    print "Role:"
    print runMythRole()
    print "Theme:"
    print runMythTheme()
    print "Tuner:"
    print runMythTuner()