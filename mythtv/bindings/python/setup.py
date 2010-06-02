#!/usr/bin/env python

from distutils.core import setup
from distutils.command.install import INSTALL_SCHEMES
import glob

for scheme in INSTALL_SCHEMES.values():
    scheme['data'] = scheme['purelib']

setup(
        name='MythTV',
        version='0.23.0',
        description='MythTV Python bindings.',
        packages=['MythTV', 'MythTV/tmdb', 'MythTV/ttvdb', 'MythTV/wikiscripts'],
        data_files=[('MythTV/tmdb/XSLT', glob.glob('MythTV/tmdb/XSLT/*')), ('MythTV/ttvdb/XSLT', glob.glob('MythTV/ttvdb/XSLT/*'))],
        url=['http://www.mythtv.org/'],
        scripts=['scripts/mythpython', 'scripts/mythwikiscripts'],
        )
