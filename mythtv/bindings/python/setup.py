#!/usr/bin/env python

from distutils.core import setup
from distutils.cmd import Command
from distutils.sysconfig import get_python_lib
from distutils.command.install import INSTALL_SCHEMES
import os
import glob

for scheme in INSTALL_SCHEMES.values():
    scheme['data'] = scheme['purelib']

class uninstall(Command):
    user_options=[('prefix=', None, 'installation prefix')]
    def initialize_options(self):
        self.prefix = None
    def finalize_options(self):
        pass
    def run(self):
        install_path = get_python_lib(prefix=self.prefix)
        mythtv_path = os.path.join(install_path,'MythTV')
        if os.access(mythtv_path, os.F_OK):
            for path,dirs,files in os.walk(mythtv_path, topdown=False):
                for fname in files:
                    print 'unlinking '+os.path.join(path,fname)
                    os.unlink(os.path.join(path,fname))
                print 'removing folder '+path
                os.rmdir(path)
            for fname in os.listdir(install_path):
                if fname.endswith('.egg-info') and fname.startswith('MythTV'):
                    print 'unlinking '+os.path.join(install_path, fname)
                    os.unlink(os.path.join(install_path, fname))

setup(
        name='MythTV',
        version='0.23.0',
        description='MythTV Python bindings.',
        long_description='Provides canned database and protocol access to the MythTV database, mythproto, mythxml, and frontend remote control.',
        packages=['MythTV', 'MythTV/tmdb', 'MythTV/ttvdb', 'MythTV/wikiscripts'],
        data_files=[('MythTV/tmdb/XSLT',glob.glob('MythTV/tmdb/XSLT/*')), ('MythTV/ttvdb/XSLT', glob.glob('MythTV/ttvdb/XSLT/*'))],
        url=['http://www.mythtv.org/'],
        scripts=['scripts/mythpython', 'scripts/mythwikiscripts'],
        requires=['MySQLdb','lxml'],
        cmdclass={'uninstall':uninstall},
        )
