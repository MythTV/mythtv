#!/usr/bin/python
# -*- coding: utf8 -*-

# This script performs movie data lookup using the German www.ofdb.de
# website.
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# This script is based on a PHP script contributed by Christian Güdel and
# the original ofdb.pl written by Xavier Hervy (maxpower44 AT tiscali DOT fr).
#
# Requirements:
# PyXML

import os
import sys
import re
import urlparse
import urllib
import urllib2
import cgi
import traceback
import codecs
from optparse import OptionParser

# This is done for Ubuntu, they are removing PyXML.
alt_paths = (
		'/usr/lib/python%s/site-packages/oldxml' % sys.version[:3],
		'/usr/lib/python%s/dist-packages/oldxml' % sys.version[:3]
		)

for path in alt_paths:
	if os.path.exists(path):
		sys.path.append(path)

from xml.dom.ext.reader import HtmlLib
from xml.dom import EMPTY_NAMESPACE
from xml import xpath

# The Python SGML based HTML parser is unusable (it finds
# tag start/ends in the worst way possible).

VERBOSE=False
URL_BASE="http://www.ofdb.de"
DUMP_RESPONSE=False

ofdb_version = "0.3"
mythtv_version = "0.21"

def comment_out(str):
	s = str
	try:
		s = unicode(str, "utf8")
	except:
		pass

	print("# %s" % (s,))

def debug_out(str):
	if VERBOSE:
		comment_out(str)

def response_out(str):
	if DUMP_RESPONSE:
		s = str
		try:
			s = unicode(str, "utf8")
		except:
			pass

		print(s)

def print_exception(str):
	for line in str.splitlines():
		comment_out(line)

def _xmlprep(content):
	"""Removes any HTML tags that just confuse the parser."""

	pat = re.compile(r'<\s*meta.*?>', re.M)
	ret = pat.sub('', content)
	pat = re.compile(r'<\s*script.*?<\s*/script\s*>', re.M | re.S)
	return pat.sub('', ret)


def _myth_url_get(url, data = None, as_post = False):
	extras = ['ofdb', ofdb_version]

	debug_out("_myth_url_get(%s, %s, %s)" % (url, data, as_post))
	send_data = {}
	if data:
		send_data.update(data)
	dest_url = url

	if not as_post:
		# TODO: cgi.parse_qs does not handle valueless entries
		# so things like ?foo will fail, ?foo=val works.
		(scheme, netloc, path, query, frag) = urlparse.urlsplit(dest_url)
		send_data = {}

		old_qa = cgi.parse_qs(query)
		if old_qa:
			send_data.update(old_qa)

		if data:
			send_data.update(data)

		query = urllib.urlencode(send_data)
		send_data = None
		dest_url = urlparse.urlunsplit((scheme, netloc, path, query, frag))

	req = urllib2.Request(url = dest_url, headers =
			{ 'User-Agent' : "MythTV/%s (%s)" %
				(mythtv_version, "; ".join(extras))})

	if send_data:
		req.add_data(urllib.urlencode(send_data))

	try:
		debug_out("Get URL '%s:%s'" % (req.get_full_url(), req.get_data()))
		res = urllib2.urlopen(req)
		content = res.read()
		res.close()
		return (res, content)
	except:
		print_exception(traceback.format_exc())
		return (None, None)

def ofdb_url_get(url, data = None, as_post = False):
	(rc, content) = _myth_url_get(url, data, as_post)

	m = re.search(r'<\s*meta[^>]*charset\s*=\s*([^" ]+)', content, re.I)
	if m:
		charset = m.group(1)
		debug_out("Page charset reported as %s" % (charset))
		# The page lies about encoding (often using two character
		# encodings on the same page).
		content = _xmlprep(unicode(content, charset, 'replace')).encode("utf8")
	else:
		# hope it is ascii
		content = _xmlprep(unicode(content, errors='replace')).encode("utf8")

	response_out(content)
	return (rc, content)

def search_title(title):
	def clean_title(t):
		t = urllib.unquote(t)
		(t, ext) = os.path.splitext(t)
		m = re.match("(.*)(?:[(|\[]|, The$)",t, re.I)
		ret = t
		if m:
			ret = m.group(1)
		return ret.strip().encode("utf8")

	try:
		data = {
				"page" : "suchergebnis",
				"Kat" : "DTitel",
				"SText" : clean_title(title)
				}

		debug_out("Starting search for title '%s'" % (title,))

		(rc, content) = ofdb_url_get(urlparse.urljoin(URL_BASE, "view.php"),
			data, True)

		reader = HtmlLib.Reader()
		doc = reader.fromString(content, charset='utf8')

		nodes = xpath.Evaluate("//A[starts-with(@href, 'film/')]",
				doc.documentElement)

		title_matches = []
		uid_match = re.compile('/(\d+,.*)', re.I)
		for title in nodes:
			rm = uid_match.search(title.getAttributeNS(EMPTY_NAMESPACE, 'href'))
			if rm:
				title_matches.append((rm.group(1), title.firstChild.nodeValue))

		for id, title in title_matches:
			print("%s:%s" % (id, title.strip()))
	except:
		print_exception(traceback.format_exc())

def get_ofdb_doc(uid, context):
	"""Returns the OFDb film page as an XML document."""
	debug_out("Starting search for %s '%s'" % (context, uid))

	(rc, content) = ofdb_url_get(urlparse.urljoin(URL_BASE,
		"film/%s" % (uid.encode("utf8"),)))

	reader = HtmlLib.Reader()
	return reader.fromString(content, charset='utf8')

class NoIMDBURL(Exception):
	pass

def search_data(uid, rating_country):
	def possible_error(path):
		comment_out("Warning: expected to find content at '%s', site format " \
				"may have changed, look for a new version of this script." %
				(path,))

	def single_value(doc, path):
		nodes = xpath.Evaluate(path, doc)
		if len(nodes):
			return nodes[0].firstChild.nodeValue.strip()
		possible_error(path)
		return ""

	def attr_value(doc, path, attrname):
		nodes = xpath.Evaluate(path, doc)
		if len(nodes):
			return nodes[0].getAttributeNS(EMPTY_NAMESPACE, attrname).strip()
		possible_error(path)
		return ""

	def multi_value(doc, path):
		ret = []
		nodes = xpath.Evaluate(path, doc)
		if len(nodes):
			for i in nodes:
				ret.append(i.firstChild.nodeValue.strip())
			return ret
		possible_error(path)
		return ""

	def direct_value(doc, path):
		nodes = xpath.Evaluate(path, doc)
		if len(nodes):
			return nodes[0].nodeValue.strip()
		possible_error(path)
		return ""

	def all_text_children(doc, path):
		nodes = xpath.Evaluate(path, doc)
		if len(nodes):
			ret = []
			for n in nodes:
				for c in n.childNodes:
					if c.nodeType == c.TEXT_NODE:
						ret.append(c.nodeValue.strip())
			return " ".join(ret)
		possible_error(path)
		return ""

	try:
		doc = get_ofdb_doc(uid, "data")

#TODO: Add details from tmdb
		data = {'title' : '',
				'countries' : '',
				'year' : '',
				'directors' : '',
				'cast' : '',
				'genre' : '',
				'user_rating' : '',
#				'movie_rating' : '',
				'plot' : '',
#				'release_date' : '',
#				'runtime' : '',
#				'writers' : '',
				}

		data['title'] = single_value(doc.documentElement,
				"//TD[@width='99%']/H2/FONT[@size='3']/B")
		data['countries'] = ",".join(multi_value(doc.documentElement,
			"//A[starts-with(@href, 'view.php?page=blaettern&Kat=Land&')]"))
		data['year'] = single_value(doc.documentElement,
				"//A[starts-with(@href, 'view.php?page=blaettern&Kat=Jahr&')]")
		data['directors'] = ",".join(multi_value(doc.documentElement,
				"//TD[@width='99%']/TABLE/TR[4]/TD[3]//A[starts-with(@href, " \
						"'view.php?page=liste')]"))
		data['cast'] = ",".join(multi_value(doc.documentElement,
				"//TD[@width='99%']/TABLE/TR[5]/TD[3]//A[starts-with(@href, " \
						"'view.php?page=liste')]"))
		data['genre'] = ",".join(multi_value(doc.documentElement,
				"//A[starts-with(@href, 'view.php?page=genre&Genre=')]"))
		data['user_rating'] = attr_value(doc.documentElement,
				"//IMG[@src='images/design3/notenspalte.png']", "alt")

		tmp_sid = attr_value(doc.documentElement,
				"//A[starts-with(@href, 'plot/')]", "href")

		sid_match = re.search("/(\d+,\d+,.*)", tmp_sid, re.I)
		sid = None
		if sid_match:
			sid = sid_match.group(1)

			debug_out("Looking for plot...")
			(rc, content) = ofdb_url_get(urlparse.urljoin(URL_BASE,
				"plot/%s" % sid.encode("utf8")))

			reader = HtmlLib.Reader()
			doc = reader.fromString(content, charset='utf8')

			data['plot'] = unicode(all_text_children(doc.documentElement,
					"//FONT[@class='Blocksatz']"))

#ReleaseDate:%(release_date)s
#MovieRating:%(movie_rating)s
#Runtime:%(runtime)s
#Writers:%(writers)s
		print("""\
Title:%(title)s
Year:%(year)s
Director:%(directors)s
Plot:%(plot)s
UserRating:%(user_rating)s
Cast:%(cast)s
Genres:%(genre)s
Countries:%(countries)s
""" % data)

	except:
		print_exception(traceback.format_exc())

def search_poster(uid):
	try:
		debug_out("Looking for posters...")
		poster_urls = []
		ofdoc = get_ofdb_doc(uid, "poster")

		nodes = xpath.Evaluate("//IMG[starts-with(@src, 'http://img.ofdb.de/film/')]",
				ofdoc.documentElement)
		for node in nodes:
			poster_urls.append(node.getAttributeNS(EMPTY_NAMESPACE, 'src'))

		for p in poster_urls:
			print(p)
	except:
		print_exception(traceback.format_exc())

def main():
	parser = OptionParser(usage="""\
Usage: %prog [-M TITLE | -D UID [-R COUNTRY[,COUNTRY]] | -P UID]
""", version="%%prog %s" % (ofdb_version))
	parser.add_option("-M", "--title", type="string", dest="title_search",
			metavar="TITLE", help="Search for TITLE")
	parser.add_option("-D", "--data", type="string", dest="data_search",
			metavar="UID", help="Search for video data for UID")
	parser.add_option("-R", "--rating-country", type="string",
			dest="ratings_from", metavar="COUNTRY",
			help="When retrieving data, use ratings from COUNTRY")
	parser.add_option("-P", "--poster", type="string", dest="poster_search",
			metavar="UID", help="Search for images associated with UID")
	parser.add_option("-d", "--debug", action="store_true", dest="verbose",
			default=False, help="Display debug information")
	parser.add_option("-r", "--dump-response", action="store_true",
			dest="dump_response", default=False,
			help="Output the raw response")

	(options, args) = parser.parse_args()

	global VERBOSE, DUMP_RESPONSE
	VERBOSE = options.verbose
	DUMP_RESPONSE = options.dump_response

	if options.title_search:
		search_title(unicode(options.title_search, "utf8"))
	elif options.data_search:
		rf = options.ratings_from
		if rf:
			rf = unicode(rf, "utf8")
		search_data(unicode(options.data_search, "utf8"), rf)
	elif options.poster_search:
		search_poster(unicode(options.poster_search, "utf8"))
	else:
		parser.print_usage()
		sys.exit(1)

if __name__ == '__main__':
	try:
		codecinfo = codecs.lookup('utf8')

		u2utf8 = codecinfo.streamwriter(sys.stdout)
		sys.stdout = u2utf8

		main()
	except SystemExit:
		pass
	except:
		print_exception(traceback.format_exc())

# vim: ts=4 sw=4:
