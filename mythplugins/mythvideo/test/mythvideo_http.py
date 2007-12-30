#!/usr/bin/python

import sys
import os
import time
import shutil
import BaseHTTPServer
from optparse import OptionParser
from mythvideo_test import HTTP_PORT

GET_DELAY=0

class MyRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
	def do_HEAD(self):
		self.__send_head(False)

	def do_GET(self):
		f = self.__send_head(True)
		self.log_message("Sleeping for %d" % (GET_DELAY,))
		time.sleep(GET_DELAY)
		if f:
			shutil.copyfileobj(f, self.wfile)
			f.close()

	def __send_head(self, open_file):
		filename = os.path.normpath(os.getcwd() + self.path)
		if self.__file_readable(filename):
			f = None
			if open_file:
				try:
					f = open(filename, 'rb')
				except IOError:
					self.send_error(404, "File not found")
					return None

			stats = os.stat(filename)
			self.send_response(200)
			self.send_header("Content-type", 'image/png')
			self.send_header("Content-Length", str(stats[6]))
			self.send_header("Last-Modified", self.date_time_string(stats[8]))
			self.end_headers()
			return f
		else:
			self.send_error(404, "File not found")
			return None

	def __file_readable(self, filepath):
		return os.path.isfile(filepath)

def main():
	parser = OptionParser()
	parser.add_option("-d", "--delay", type="int", dest="delay",
			help="Causes image GETs to wait DELAY seconds before returning "
			     "the image.", metavar="DELAY")
	parser.add_option("-p", "--port", type="int", dest="port",
			          help="Port to bind to.")
	(options, args) = parser.parse_args()

	bind_port = HTTP_PORT

	if options.port:
		bind_port = options.port

	if options.delay:
		global GET_DELAY
		GET_DELAY = int(options.delay)

	try:
		httpd = BaseHTTPServer.HTTPServer(('', bind_port), MyRequestHandler)
		print('Waiting for requests')
		httpd.serve_forever()
	except KeyboardInterrupt:
		print("Done.")

if __name__ == '__main__':
	main()
