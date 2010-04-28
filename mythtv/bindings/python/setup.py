#!/usr/bin/env python

from distutils.core import setup

setup(
        name='MythTV',
        version='0.23.0',
        description='MythTV Python bindings.',
        packages=['MythTV', 'MythTV/tmdb', 'MythTV/ttvdb', 'MythTV/wikiscripts'],
        url=['http://www.mythtv.org/'],
        scripts=['scripts/mythpython', 'scripts/mythwikiscripts'],
        )
