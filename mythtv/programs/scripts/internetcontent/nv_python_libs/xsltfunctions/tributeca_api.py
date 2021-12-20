# -*- coding: UTF-8 -*-

# ----------------------
# Name: tributeca_api - XPath and XSLT functions for the Tribute.ca grabber
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions
#           for the conversion of data to the MNV standard RSS output format.
#           See this link for the specifications:
#           http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tributeca_api - XPath and XSLT functions for the Tribute.ca grabber"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MNV standard RSS output format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
'''

__version__="v0.1.1"
# 0.1.0 Initial development
# 0.1.1 Changes to due to Web site modifications


# Specify the class names that have XPath extention functions
__xpathClassList__ = ['xpathFunctions', ]

# Specify the XSLT extention class names. Each class is a stand lone extention function
#__xsltExtentionList__ = ['xsltExtExample', ]
__xsltExtentionList__ = []

import os, sys, re, time, datetime, shutil, urllib.request, urllib.parse, urllib.error, string
from copy import deepcopy


class OutStreamEncoder(object):
    """Wraps a stream with an encoder"""
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, str):
            obj = obj.encode(self.encoding)
        try:
            self.out.buffer.write(obj)
        except OSError:
            pass

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)

if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
    sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')

try:
    from io import StringIO
    from lxml import etree
except Exception as e:
    sys.stderr.write('\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
    sys.exit(1)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
version = ''
for digit in etree.LIBXML_VERSION:
    version+=str(digit)+'.'
version = version[:-1]
if version < '2.7.2':
    sys.stderr.write('''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


class xpathFunctions(object):
    """Functions specific extending XPath
    """
    def __init__(self):
        self.functList = ['tributecaLinkGeneration', 'tributecaThumbnailLink', 'tributecaTopTenTitle', 'tributecaIsCustomHTML', 'tributecaCheckIfDBItem', 'tributecaDebug', 'tributecaGetAnchors', ]
        self.TextTail = etree.XPath("string()")
        self.anchorList = etree.XPath(".//a", namespaces=common.namespaces)
        self.persistence = {}
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def tributecaLinkGeneration(self, context, *args):
        '''Generate a link for the Tribute.ca site. Sigificant massaging of the title is required.
        Call example: 'mnvXpath:tributecaLinkGeneration(position(), ..//a)'
        return the url link
        '''
        downloadURL = 'http://www.tribute.ca/streamingflash/%s.flv'
        position = int(args[0])-1
        webURL = 'http://www.tribute.ca%s' % args[1][position].attrib['href'].strip()

        # If this is for the download then just return what was found for the "link" element
        if 'tributecaLinkGeneration' in self.persistence:
            if self.persistence['tributecaLinkGeneration'] is not None:
                returnValue = self.persistence['tributecaLinkGeneration']
                self.persistence['tributecaLinkGeneration'] = None
                if returnValue != webURL:
                    return downloadURL % returnValue
                else:
                    return webURL

        currentTitle = self.TextTail(args[1][position]).strip()
        if position == 0:
            previousTitle = ''
        else:
           previousTitle = self.TextTail(args[1][position-1]).strip()

        # Rule: "IMAX: Hubble 3D": http://www.tribute.ca/streamingflash/hubble3d.flv
        titleArray = [currentTitle, previousTitle]
        if titleArray[0].startswith('IMAX:'):
            titleArray[0] = titleArray[0].replace('IMAX:', '').strip()
        else:
            # Rule: "How to Train Your Dragon: An IMAX 3D Experience" did not even have a trailer
            #       on the Web page but stip off anything after the ":"
            for counter in range(len(titleArray)):
                index = titleArray[counter].find(": ")
                if index != -1:
                    titleArray[counter] = titleArray[counter][:index].strip()
                index = titleArray[counter].find(" (")
                if index != -1:
                    titleArray[counter] = titleArray[counter][:index].strip()
                if titleArray[0].startswith(titleArray[1]) and titleArray[1]:
                    index = titleArray[counter].find("3D")
                    if index != -1:
                        titleArray[counter] = titleArray[counter][:index].strip()

        # If the previous title starts with the same title as the current then this is trailer #2
        trailer2 = ''
        if titleArray[0].startswith(titleArray[1]) and titleArray[1]:
            trailer2 = 'tr2'
        if currentTitle.find(': An IMAX') != -1:
            trailer2 = 'tr2'
        titleArray[0] = titleArray[0].replace('&', 'and')
        self.persistence['tributecaThumbnailLink'] = urllib.parse.quote_plus(titleArray[0].lower().replace(' ', '_').replace("'", '').replace('-', '_').replace('?', '').replace('.', '').encode("utf-8"))
        titleArray[0] = urllib.parse.quote_plus(re.sub('[%s]' % re.escape(string.punctuation), '', titleArray[0].lower().replace(' ', '').encode("utf-8")))

        # Verify that the FLV file url really exits. If it does not then use the Web page link.
        videocode = '%s%s' % (titleArray[0], trailer2)
        flvURL = downloadURL % videocode
        resultCheckUrl = common.checkURL(flvURL)
        if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != 'video/x-flv':
            if trailer2 != '':
                videocode = titleArray[0]
                flvURL = downloadURL % titleArray[0]
                resultCheckUrl = common.checkURL(flvURL) # Drop the 'tr2' this time
                if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != 'video/x-flv':
                    flvURL = webURL
            else:
                videocode = titleArray[0]+'tr2'
                flvURL = downloadURL % videocode
                resultCheckUrl = common.checkURL(flvURL) # Add the 'tr2' this time
                if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != 'video/x-flv':
                    if currentTitle.find(': An IMAX') == -1 and currentTitle.find(': ') != -1:
                        titleArray[0] = currentTitle.replace('&', 'and')
                        titleArray[0] = urllib.parse.quote_plus(re.sub('[%s]' % re.escape(string.punctuation), '', titleArray[0].lower().replace(' ', '').encode("utf-8")))
                        videocode = titleArray[0]
                        flvURL = downloadURL % videocode
                        resultCheckUrl = common.checkURL(flvURL) # Add the 'tr2' this time
                        if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != 'video/x-flv':
                            flvURL = webURL
                    else:
                        flvURL = webURL
        if flvURL != webURL:
            self.persistence['tributecaLinkGeneration'] = videocode
            return common.linkWebPage('dummycontext', 'tributeca')+videocode
        else:
            self.persistence['tributecaLinkGeneration'] = flvURL
            return flvURL
    # end linkGeneration()

    def tributecaThumbnailLink(self, context, *args):
        '''Verify that the thumbnail actually exists. If it does not then use the site image.
        Call example: 'mnvXpath:tributecaThumbnailLink(string(.//img/@src))'
        return the thumbnail url
        '''
        siteImage = 'http://www.tribute.ca/images/tribute_title.gif'
        if not len(args[0]) or not self.persistence['tributecaThumbnailLink']:
            return siteImage

        if args[0].startswith('http:'):
            url = args[0].strip()
        else:
            url = 'http://www.tribute.ca/tribute_objects/images/movies/%s%s' % (self.persistence['tributecaThumbnailLink'], '/poster.jpg')
        resultCheckUrl = common.checkURL(url)
        if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != 'image/jpeg':
            return siteImage

        return url
    # end tributecaThumbnailLink()

    def tributecaTopTenTitle(self, context, *args):
        '''Take a top ten title and add a leading '0' if less than 10 as it forces correct sort order
        Call example: 'mnvXpath:tributecaTopTenTitle(string(..))'
        return a replacement title
        '''
        if not len(args[0]):
            return args[0]

        index = args[0].find('.')
        if index == 1:
            return '0'+args[0]
        else:
            return args[0]
    # end tributecaTopTenTitle()

    def tributecaIsCustomHTML(self, context, *args):
        '''Check if the link is for a custom HTML
        Example call: mnvXpath:isCustomHTML(('dummy'))
        return True if the link does not starts with "http://"
        return False if the link starts with "http://"
        '''
        if self.persistence['tributecaLinkGeneration'] is None:
            return False

        if self.persistence['tributecaLinkGeneration'].startswith('http://'):
            return False
        else:
            return True
    # end isCustomHTML()

    def tributecaCheckIfDBItem(self, context, *arg):
        '''Use a unique key value pairing to find out if the 'internetcontentarticles' table already
        has a matching item. This is done to save accessing the Internet when not required.
        Call example: 'mnvXpath:tributecaCheckIfDBItem(.)'
        return True if a match was found
        return False if a match was not found
        '''
        return common.checkIfDBItem('dummy', {'feedtitle': 'Movie Trailers', 'title': arg[0].replace('Trailer', '').strip(), 'author': arg[1], 'description': arg[2]})
    # end tributecaCheckIfDBItem()

    def tributecaGetAnchors(self, context, *arg):
        ''' Routine used to get specific anchor elements.
        Unfortunitely position dependant.
        Call: mnvXpath:tributecaGetAnchors(//ul[@class='clump'], 3)
        '''
        return self.anchorList(arg[0][int(arg[1])])
    # end tributecaGetAnchors()

    def tributecaDebug(self, context, *arg):
        ''' Routine only used for debugging. Prints out the node
        passed as an argument. Not to be used in production.
        Call example: mnvXpath:tributecaDebug(//a)
        '''
        testpath = etree.XPath(".//a", namespaces=common.namespaces)
        print(arg)
        count = 0
        for x in arg:
            sys.stdout.write('\nElement Count (%s):\n' % count)
#            for y in testpath(x):
#                sys.stdout.write(etree.tostring(y, encoding='UTF-8', pretty_print=True))
            print("testpath(%s)" % testpath(x))
            count+=1
        print()
#        sys.stdout.write(etree.tostring(arg[0], encoding='UTF-8', pretty_print=True))
        return "========tributecaDebug Called========="
    # end tributecaDebug()

######################################################################################################
#
# End of XPath extension functions
#
######################################################################################################

######################################################################################################
#
# Start of XSLT extension functions
#
######################################################################################################

class xsltExtExample(etree.XSLTExtension):
    '''Example of an XSLT extension. This code must be changed to do anything useful!!!
    return nothing
    '''
    def execute(self, context, self_node, input_node, output_parent):
        copyItem = deepcopy(input_node)
        min_sec = copyItem.xpath('duration')[0].text.split(':')
        seconds = 0
        for count in range(len(min_sec)):
            seconds+=int(min_sec[count])*(60*(len(min_sec)-count-1))
        output_parent.text = '%s' % seconds

######################################################################################################
#
# End of XSLT extension functions
#
######################################################################################################
