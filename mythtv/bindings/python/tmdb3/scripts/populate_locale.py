#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: populate_locale.py    Helper for grabbing ISO639 and ISO3316 data
# Python Library
# Author: Raymond Wagner
#-----------------------

import lxml.html
import sys
import os


def sanitize(name):
    name = ' '.join(name.split())
    return name


fpath = os.path.join(os.getcwd(), __file__) if not __file__.startswith('/') else __file__
fpath = os.path.join(fpath.rsplit('/', 2)[0], 'tmdb3/locales.py')

fd = open(fpath, 'r')
while True:
    line = fd.readline()
    if len(line) == 0:
        print "code endpoint not found, aborting!"
        sys.exit(1)
    if line.startswith('########'):
        endpt = fd.tell()
        break

fd = open(fpath, 'a')
fd.seek(endpt)
fd.truncate()
fd.write('\n')

root = lxml.html.parse('http://www.loc.gov/standards/iso639-2/php/English_list.php')
for row in root.getroot().getchildren()[3].getchildren()[2].getchildren()[0]\
                         .getchildren()[0].getchildren()[9]:
    if row.getchildren()[0].tag == "th":
        # skip header
        continue
    if row.getchildren()[-1].text == u"\xa0":
        # skip empty 639-1 code
        continue
    name, _, _, iso639_2, iso639_1 = [t.text for t in row]
    
    fd.write('Language("{0}", "{1}", u"{2}")\n'.format(iso639_1, iso639_2, sanitize(name).encode('utf8')))

root = lxml.html.parse('http://www.iso.org/iso/country_codes/iso_3166_code_lists/country_names_and_code_elements.htm').getroot()
for row in root.get_element_by_id('tc_list'):
    if row.tag == 'thead':
        # skip header
        continue
    name, _, alpha2 = [t.text if t.text else t.getchildren()[0].tail for t in row]
    fd.write('Country("{0}", u"{1}")\n'.format(alpha2, sanitize(name).encode('utf8')))
