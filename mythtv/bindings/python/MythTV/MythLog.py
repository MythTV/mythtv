"""
Provides simple logging and exception classes.
"""

import logging

# Vars to imporove readability
CRITICAL = logging.CRITICAL
FATAL = logging.FATAL
ERROR = logging.ERROR
WARNING = logging.WARNING
INFO = logging.INFO
DEBUG = logging.DEBUG

class MythLog:
	"""
	A simple logging class
	"""
	def __init__(self, level, format, instance):
		self.log = logging.getLogger(instance)
		self.log.setLevel(level)
		self.ch = logging.StreamHandler()
		self.ch.setFormatter(logging.Formatter(format))
		self.log.addHandler(self.ch)

	def Msg(self, level, msg, *args, **kwargs):
		self.log.log(level, msg, *args, **kwargs)

class MythError:
	"""
	A simple exception class
	"""
	def __init__(self, message):
		self.message = message

	def __repr__(self):
		print ': ' + self.message

# vim: ts=4 sw=4:
