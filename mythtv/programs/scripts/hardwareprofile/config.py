# -*- coding: utf-8 -*-

import os,re
import commands
import os_detect

SMOON_URL = "http://smolt.mythvantage.com/"
SECURE = 0
from user import home
HW_UUID = home + "/.mythtv/HardwareProfile/hw-uuid"
PUB_UUID= home +  "/.mythtv/HardwareProfile/pub-uuid"
UUID_DB = home + '/.mythtv/HardwareProfile/uuiddb.cfg'
ADMIN_TOKEN = home +  "/.mythtv/HardwareProfile/smolt_token"


#These are the defaults taken from the source code.
#fs_types = get_config_attr("FS_TYPES", ["ext2", "ext3", "xfs", "reiserfs"])
#fs_mounts = get_config_attr("FS_MOUNTS", ["/", "/home", "/etc", "/var", "/boot"])
#fs_m_filter = get_config_attr("FS_M_FILTER", False)
#fs_t_filter = get_config_attr("FS_T_FILTER", False)

FS_T_FILTER=False
FS_M_FILTER=True
FS_MOUNTS=commands.getoutput('rpm -ql filesystem').split('\n')


#This will attempt to find the distro.
#Uncomment any of the OS lines below to hardcode.
OS = os_detect.get_os_info()


#For non RH Distros
#HW_UUID = "/etc/smolt/hw-uuid"

