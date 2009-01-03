#!/usr/bin/python

import sys
import os
from optparse import OptionParser
from subprocess import check_call, CalledProcessError

def GIMPCreateDialog(name, pngName, width, height):
	try:
		check_call(["gimp", "--no-interface", "--batch",
			"(python-fu-mythvideo-popup RUN-NONINTERACTIVE %d %d "
			"\"%s\" \"%s\" TRUE FALSE) (gimp-quit 1)" %
			(width, height, name, pngName)])
	except CalledProcessError, cpe:
			print("Error code %d returned when attemptin to create %s" %
					(cpe.returncode, name))

class DirError(Exception):
	def __init__(self, name):
		self.name = name

	def __str__(self):
		return self.name

def main():
	parser = OptionParser()
	defaultThemeDir = os.path.normpath(os.path.join(os.getcwd(), "..", "theme"))
	parser.add_option("--theme-directory", dest="themedir",
			default=defaultThemeDir)

	(options, args) = parser.parse_args()

	smallSave = os.path.join(options.themedir, "default", "images")
	wideSave = os.path.join(options.themedir, "default-wide", "images")
	try:
		checkdirs = [ options.themedir, smallSave, wideSave ]

		for dir in checkdirs:
			if not (os.path.exists(dir) and os.path.isdir(dir)):
				raise DirError(dir)
	except DirError, e:
		print(e.name)
		print("Error: missing theme directory \"%s\"" % (str(e),))
		sys.exit(1)

	images = [
				{
					"name" : ("MythVideo-SearchSelect", "mv_results_popup.png"),
					"Small" : (387, 400),
					"Wide" : (592, 400),
				},
				{
					"name" : ("MythVideo-ItemDetailPopup",
						"mv_itemdetail_popup.png"),
					"Small" : (720, 540), # 800x600 - 10%
					"Wide" : (1152, 648), # 1280x720 - 10%
				},
			 ]

	savepath =  { "Small" : smallSave, "Wide" : wideSave, }

	for image in images:
		for size in [ "Small", "Wide" ]:
			name = "%s-%s" % (image['name'][0], size)
			xcfname = os.path.join(os.getcwd(), name)
			pngname = os.path.join(savepath[size], image['name'][1])
			GIMPCreateDialog(xcfname, pngname,
					image[size][0], image[size][1])

if __name__ == '__main__':
	main()
