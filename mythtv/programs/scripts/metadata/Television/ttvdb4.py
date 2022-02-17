#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

# ----------------------------------------------------
# Purpose:   MythTV Python Bindings for TheTVDB v4 API
# Copyright: (c) 2021 Roland Ernst
# License:   GPL v2 or later, see LICENSE for details
# ----------------------------------------------------


import argparse
import sys
import os
import shutil
import shlex
from pprint import pprint
from configparser import ConfigParser


__title__ = "TheTVDatabaseV4"
__author__ = "Roland Ernst"
__version__ = "0.5.1"


def print_etree(etostr):
    """lxml.etree.tostring is a bytes object in python3, and a str in python2.
    """
    sys.stdout.write(etostr.decode("utf-8"))


def _parse_config(config):
    """ Parse the config read by ConfigParser."""
    d = {}
    for section in config.sections():
        d[section] = {}
        for k, v in config[section].items():
            d[section][k] = v
    return d


def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "thumbnail").text = 'ttvdb.png'
    etree.SubElement(version, "command").text = 'ttvdb4.py'
    etree.SubElement(version, "type").text = 'television'
    etree.SubElement(version, "description").text = \
        'Search and downloads metadata from TheTVDB.com (API v4)'
    etree.SubElement(version, "version").text = __version__
    print_etree(etree.tostring(version, encoding='UTF-8', pretty_print=True,
                               xml_declaration=True))
    sys.exit(0)


def performSelfTest(args):
    err = 0
    try:
        import lxml
    except:
        err = 1
        print("Failed to import python lxml library.")
    try:
        import requests
        import requests_cache
    except:
        err = 1
        print("Failed to import python-requests or python-request-cache library.")
    try:
        import MythTV
    except:
        err = 1
        print("Failed to import MythTV bindings. Check your `configure` output "
              "to make sure installation was not disabled due to external dependencies.")
    try:
        from MythTV.ttvdbv4.myth4ttvdbv4 import Myth4TTVDBv4
        from MythTV.ttvdbv4 import ttvdbv4_api as ttvdb
        if args.debug:
            print("TheTVDBv4 Script Version:    ", __version__)
            print("TheTVDBv4-API version:       ", ttvdb.MYTHTV_TTVDBV4_API_VERSION)
            print("TheTVDBv4-API file location: ", ttvdb.__file__)
    except:
        err = 1
        print("Failed to import Py TTVDB4 library. This should have been included "
              "with the python MythTV bindings.")
    try:
        inipath = os.path.abspath(os.path.dirname(sys.argv[0]))
        inifile = os.path.join(inipath, "ttvdb4.ini")
        config = ConfigParser()
        # preserve capital letters:
        config.optionxform = str
        config.read(inifile, 'UTF-8')
        config_dict = _parse_config(config)
        config_version = config_dict['ConfigVersion']['TTVDBv4ConfigVersion']
        if args.debug:
            print("Config version of 'ttvdb4.ini': ", config_version)
    except:
        err = 1
        print("Failed to read the ini file 'ttvdb4.ini'. Check your installation "
              "if such a file exists alongside this grabber script.")
    if not err:
        print("Everything appears in order.")
    sys.exit(err)


def main():
    """
    Main executor for MythTV's ttvdb v4 grabber.
    """
    description = '''A python script to retrieve metadata for TV-Shows.'''

    parser = argparse.ArgumentParser(description=description)

    parser.add_argument('-v', '--version', action="store_true",
                         dest="version", help="Display version and author")

    parser.add_argument('-t', '--test', action="store_true", default=False,
                         dest="test", help="Perform self-test for dependencies.")

    parser.add_argument('-l', "--language", metavar="LANGUAGE", default=u'en',
                        dest="language", help="Specify language for filtering.")

    parser.add_argument('-a', "--area", metavar="COUNTRY", default=None,
                        dest="country", help="Specify country for custom data.")

    group = parser.add_mutually_exclusive_group()

    group.add_argument('-M', "--list", nargs=1,
                       dest="tvtitle", help="Get TV Shows matching 'tvtitle'.")

    group.add_argument('-D', "--data", metavar=("INETREF","SEASON","EPISODE"),
                       nargs=3, type=int, dest="tvdata",
                       help="Get TV-Show data for 'inetref', 'season' and 'episode.")

    group.add_argument('-C', "--collection", nargs=1, type=int, dest="collectionref",
                       help="Get Collection data for 'collectionref'.")

    group.add_argument('-N', "--numbers", metavar=("ARG0","ARG1"), nargs=2, type=str,
                       dest="tvnumbers",
                       help="Get Season and Episode numbers: "
                            "'ARG0' can be ['series-title', 'inetref'] and "
                            "'ARG1': ['episode-title', 'iso-date-time'].")

    parser.add_argument('--configure', nargs='?', type=str, default='ttvdb4.ini',
                        dest="inifile", help="Use local configuration file, defaults to "
                                             "'~/.mythtv/'ttvdb4.ini'.")

    parser.add_argument('--debug', action="store_true", default=False, dest="debug",
                        help="Disable caching and enable raw data output.")

    parser.add_argument('--jsondebug', action="store_true", default=False, dest="jsondebug",
                        help="Enable raw json data output.")

    parser.add_argument('--doctest', action="store_true", default=False, dest="doctest",
                        help="Run doctests. You need to change to the folder where ttvdb4.py "
                             "is installed.")

    args = parser.parse_args()

    if args.version:
        buildVersion()

    if args.test:
        performSelfTest(args)

    # assemble arguments
    cmd_args = vars(args)
    if args.debug:
        print("0000: Init: cmd_args: ", cmd_args)

    # read the ini files
    import requests
    confdir = os.environ.get('MYTHCONFDIR', '')
    if (not confdir) or (confdir == '/'):
        confdir = os.environ.get('HOME', '')
        if (not confdir) or (confdir == '/'):
            print("Unable to find MythTV directory for grabber initialization.")
            sys.exit(1)
        confdir = os.path.join(confdir, '.mythtv')

    cachedir = os.path.join(confdir, 'cache')
    if not os.path.exists(cachedir):
        os.makedirs(cachedir)

    if not args.debug and not args.doctest:
        cache_name = os.path.join(cachedir, 'py3ttvdb4')
        import requests_cache
        requests_cache.install_cache(cache_name, backend='sqlite', expire_after=3600)

    # Add config from config file to cmd_args:
    config_dict = {}
    # read global config
    inipath = os.path.abspath(os.path.dirname(sys.argv[0]))
    inifile = os.path.join(inipath, "ttvdb4.ini")
    try:
        global_config = ConfigParser()
        # preserve capital letters:
        global_config.optionxform = str
        global_config.read(inifile, 'UTF-8')
        config_dict = _parse_config(global_config)
        if args.debug:
            print("0000: Init: Global Config File parsed successfully.")
    except KeyError:
        if args.debug:
            print("0000: Init: Parsing Global Config File failed.")
    # read local config, which overrides the global one
    if args.inifile:
        local_config_file = os.path.join(confdir, args.inifile)
        if os.path.isfile(local_config_file):
            try:
                local_config = ConfigParser()
                # preserve capital letters:
                local_config.optionxform = str
                local_config.read(local_config_file, 'UTF-8')
                for section in local_config.sections():
                    for k,v in local_config[section].items():
                        config_dict[section][k] = v
                if args.debug:
                    print("0000: Init: Local Config File '%s' parsed successfully."
                          % local_config_file)
            except KeyError:
                if args.debug:
                    print("0000: Init: Parsing Local Config File failed.")
        else:
            # create local config with values from global config
            shutil.copy(inifile, local_config_file)
            if args.debug:
                print("0000: Init: Local config file '%s' created." % local_config_file)

    # storage for authentication bearer token
    config_dict['auth_file'] = os.path.join(cachedir, "py3ttvdb4_bearer.pickle")

    cmd_args["config"] = config_dict
    if args.debug:
        print("0000: Init: Using this configuration:")
        pprint(cmd_args["config"])

    if args.doctest:
        import doctest
        try:
            with open("ttvdb4_doctests") as f:
                dtests = "".join(f.readlines())
            main.__doc__ += dtests
        except IOError:
            pass
        # perhaps try optionflags=doctest.ELLIPSIS | doctest.NORMALIZE_WHITESPACE
        return doctest.testmod(verbose=args.debug, optionflags=doctest.ELLIPSIS)

    # finally, grab the damn metadata
    try:
        from MythTV.ttvdbv4.myth4ttvdbv4 import Myth4TTVDBv4
        from lxml import etree
        mttvdb = Myth4TTVDBv4(**cmd_args)
        if args.tvdata:
            # option -D inetref season episode
            mttvdb.buildSingle()
        elif args.collectionref:
            # option -C inetref
            mttvdb.buildCollection()
        elif args.tvtitle:
            # option -M title
            mttvdb.buildList()
        elif args.tvnumbers:
            # option -N title subtitle
            # option -N inetref subtitle
            mttvdb.buildNumbers()
        else:
            sys.stdout.write('ERROR: This script must be called with one of '
                             '[-t, -v, -C, -D, -M, -N] switches.')
            sys.exit(1)

        print_etree(etree.tostring(mttvdb.tree, encoding='UTF-8', pretty_print=True,
                                   xml_declaration=True))
    except:
        if args.debug:
            raise
        sys.stdout.write('ERROR: ' + str(sys.exc_info()[0]) + ' : '
                                   + str(sys.exc_info()[1]) + '\n')
        sys.exit(1)


if __name__ == "__main__":
    main()
