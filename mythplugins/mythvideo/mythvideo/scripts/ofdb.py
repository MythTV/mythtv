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
import traceback
import codecs
from optparse import OptionParser
from xml.dom.ext.reader import HtmlLib
from xml.dom import EMPTY_NAMESPACE
from xml import xpath

# The Python SGML based HTML parser is unusable (it finds
# tag start/ends in the worst way possible).

VERBOSE=False
URL_BASE="http://www.ofdb.de"

def comment_out(str):
	print("# %s" % (str,))

def debug_out(str):
	if VERBOSE:
		comment_out(str)

def print_exception(str):
	for line in str.splitlines():
		comment_out(line)

def search_title(title):
	def clean_title(t):
		t = urllib.unquote(t)
		(t, ext) = os.path.splitext(t)
		m = re.match("(.*)(?:[(|\[]|, The$)",t, re.I)
		ret = t
		if m:
			ret = m.group(1)
		return urllib.quote(ret.strip())

	try:
		req = urllib2.Request(urlparse.urljoin(URL_BASE, "view.php"))
		data = {
				"page" : "suchergebnis",
				"Kat" : "DTitel",
				"SText" : clean_title(title)
				}
		req.add_data(urllib.urlencode(data))

		debug_out("Starting search for title '%s'" % (title,))
		debug_out("Search query '%s:%s'" % (req.get_full_url(), req.get_data()))

		res = urllib.urlopen(req)
		content = res.read()
		res.close()

		reader = HtmlLib.Reader()
		doc = reader.fromString(content)

		nodes = xpath.Evaluate("//A[starts-with(@href, 'view.php?page=film&fid=')]", doc.documentElement)

		title_matches = []
		uid_match = re.compile('fid=(\d+)', re.I)
		for title in nodes:
			rm = uid_match.search(title.getAttributeNS(EMPTY_NAMESPACE, 'href'))
			if rm:
				title_matches.append((rm.group(1), title.firstChild.nodeValue))

		for id, title in title_matches:
			print("%s:%s" % (id, title.strip()))
	except:
		print_exception(traceback.format_exc())

def get_ofdb_doc(uid, context):
	req = urllib2.Request(urlparse.urljoin(URL_BASE, "view.php"))
	data = { "page" : "film", "fid" : uid }
	req.add_data(urllib.urlencode(data))

	debug_out("Starting search for %s '%s'" % (context, uid))
	debug_out("Search query '%s:%s'" % (req.get_full_url(), req.get_data()))

	res = urllib2.urlopen(req)
	content = res.read()
	res.close()

	reader = HtmlLib.Reader()
	return reader.fromString(content)

class NoIMDBURL(Exception):
	pass

def get_imdb_url(doc):
	nodes = xpath.Evaluate("//A[contains(@href, 'imdb.com/Title')]",
			doc.documentElement)
	if len(nodes):
		uid_find = re.compile('imdb.com/Title\?(\d+)', re.I)
		url = nodes[0].getAttributeNS(EMPTY_NAMESPACE, 'href')
		m = uid_find.search(url)
		if m:
			return (url, m.group(1))
	raise NoIMDBURL()

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
		alturl = None
		try:
			(url, id) = get_imdb_url(doc)
			alturl = url
		except:
			print_exception(traceback.format_exc())

		data = {'title' : '',
				'countries' : '',
				'year' : '',
				'directors' : '',
				'cast' : '',
				'genre' : '',
				'user_rating' : '',
				'plot' : '',
				'release_date' : '',
				'runtime' : '',
				}

		data['title'] = single_value(doc.documentElement,
				"//TD[@width='99%']/FONT[@size='3']/B")
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
				"//IMG[@src='images/notenspalte.gif']", "alt")

		tmp_sid = attr_value(doc.documentElement,
				"//A[starts-with(@href, 'view.php?page=inhalt&fid=')]", "href")

		sid_match = re.search("sid=(\d+)", tmp_sid, re.I)
		sid = None
		if sid_match:
			sid = sid_match.group(1)

			req_data = {'page' : 'inhalt', 'sid' : sid, 'fid' : uid}
			req = urllib2.Request(urlparse.urljoin(URL_BASE,
				"view.php?%s" % (urllib.urlencode(req_data),)))
			debug_out("Looking for plot here '%s'" %
					(req.get_full_url(),))
			res = urllib2.urlopen(req)
			content = res.read()
			res.close()

			reader = HtmlLib.Reader()
			doc = reader.fromString(content)

			data['plot'] = unicode(all_text_children(doc.documentElement,
					"//FONT[@class='Blocksatz']"))

		if alturl:
			req = urllib2.Request(alturl)
			debug_out("Looking for other info here '%s'" %
					(req.get_full_url(),))
			res = urllib2.urlopen(req)
			content = res.read()
			res.close()

			reader = HtmlLib.Reader()
			doc = reader.fromString(content)

			data['release_date'] = direct_value(doc.documentElement, "//DIV[@class='info']/H5[starts-with(., 'Premierendatum')]/../child::text()[2]")
			data['runtime'] = direct_value(doc.documentElement, u"//DIV[@class='info']/H5[starts-with(., 'L\u00E4nge')]/../child::text()[2]").split()[0]

			movie_ratings = multi_value(doc.documentElement, u"//DIV[@class='info']/H5[starts-with(., 'Altersfreigabe')]/../A")

			if len(movie_ratings):
				found = False
				if rating_country:
					for rc in rating_country.split(','):
						for m in movie_ratings:
							(country, rating) = m.split(':')
							if country.lower().find(rc.lower()) != -1:
								data['movie_rating'] = rating
								found = True
								break

						if found:
							break

				if not found:
					data['movie_rating'] = ",".join(movie_ratings)

			writers = multi_value(doc.documentElement, u"//DIV[@class='info']/H5[starts-with(., 'Drehbuchautoren')]/../A")
			if len(writers):
				data['writers'] = writers[0]

		print("""\
Title:%(title)s
Year:%(year)s
ReleaseDate:%(release_date)s
Director:%(directors)s
Plot:%(plot)s
UserRating:%(user_rating)s
MovieRating:%(movie_rating)s
Runtime:%(runtime)s
Writers:%(writers)s
Cast:%(cast)s
Genres:%(genre)s
Countries:%(countries)s
""" % data)

	except:
		print_exception(traceback.format_exc())

def search_poster(uid):
	try:
		doc = get_ofdb_doc(uid, "poster")

		(url, id) = get_imdb_url(doc)

		req = urllib2.Request(urlparse.urljoin("http://www.imdb.com",
			"title/tt%s/posters" % (id)))

		debug_out("Looking at %s for posters" % (req.get_full_url()))
		res = urllib2.urlopen(req)
		content = res.read()
		res.close()

		reader = HtmlLib.Reader()
		doc = reader.fromString(content)

		poster_urls = []

		nodes = xpath.Evaluate("//TABLE[starts-with(@background, 'http://posters.imdb.com/posters/')]", doc.documentElement)
		for i in nodes:
			poster_urls.append(i.getAttributeNS(EMPTY_NAMESPACE,
				'background'))

		nodes = xpath.Evaluate("//A[contains(@href, 'impawards.com')]",
				doc.documentElement)
		if len(nodes):
			try:
				base = nodes[0].getAttributeNS(EMPTY_NAMESPACE, 'href')
				req = urllib2.Request(base)
				res = urllib2.urlopen(req)
				content = res.read()
				res.close()

				reader = HtmlLib.Reader()
				doc = reader.fromString(content)
				nodes = xpath.Evaluate(
						"//IMG[starts-with(@SRC, 'posters/')]",
						doc.documentElement)
				for i in nodes:
					(scheme, netloc, path, query, frag) = \
							urlparse.urlsplit(base)
					np = path.split('/')[:-1]
					np = "/".join(np)
					poster_urls.insert(0, urlparse.urljoin(base,
						"%s/%s" % (np, i.getAttributeNS(EMPTY_NAMESPACE,
							'SRC'))))
			except:
				print_exception(traceback.format_exc())

		for p in poster_urls:
			print(p)
	except:
		print_exception(traceback.format_exc())

def main():
	parser = OptionParser(usage="""\
Usage: %prog [-M TITLE | -D UID [-r COUNTRY[,COUNTRY]] | -P UID]
""", version="%prog 0.1")
	parser.add_option("-M", "--title", type="string", dest="title_search",
			metavar="TITLE", help="Search for TITLE")
	parser.add_option("-D", "--data", type="string", dest="data_search",
			metavar="UID", help="Search for video data for UID")
	parser.add_option("-r", "--rating-country", type="string",
			dest="ratings_from", metavar="COUNTRY",
			help="When retrieving data, use ratings from COUNTRY")
	parser.add_option("-P", "--poster", type="string", dest="poster_search",
			metavar="UID", help="Search for images associated with UID")
	parser.add_option("-d", "--debug", action="store_true", dest="verbose",
			default=False, help="Display debug information")

	(options, args) = parser.parse_args()

	global VERBOSE
	VERBOSE = options.verbose

	if options.title_search:
		search_title(options.title_search)
	elif options.data_search:
		search_data(options.data_search, options.ratings_from)
	elif options.poster_search:
		search_poster(options.poster_search)
	else:
		parser.print_usage()
		sys.exit(1)

if __name__ == '__main__':
	try:
		codecinfo = codecs.lookup('utf8')

		u2utf8 = codecinfo.streamwriter(sys.stdout)
		sys.stdout = u2utf8

		main()
	except:
		print_exception(traceback.format_exc())
