"""
Provides the MythVideo class with convinience methods to access the MythTV
MythVideo database.
"""

from MythDB import *
from MythLog import *

log = MythLog(CRITICAL, '#%(levelname)s - %(message)s', 'MythVideo')

class MythVideo:
	"""
	Provides convinience methods to access the MythTV MythVideo database.
	"""
	def __init__(self):
		"""
		Initialise the MythDB connection.
		"""
		self.db = MythDB()

	def pruneMetadata(self):
		"""
		Removes metadata from the database for files that no longer exist.
		"""
		c = self.db.cursor()
		c.execute("""
				SELECT intid, filename
				FROM videometadata""")

		row = c.fetchone()
		while row is not None:
			intid = row[0]
			filename = row[1]
			if not os.path.exists(filename):
				log.Msg(INFO, '%s not exist, removing metadata...', filename)
				c2 = self.db.cursor()
				c2.execute("""DELETE FROM videometadata WHERE intid = %s""", (intid,))
				c2.close()
			row = c.fetchone()
		c.close()

	def getGenreId(self, genre_name):
		"""
		Find the id of the given genre from MythDB.

		If the genre does not exist, insert it and return its id.
		"""
		c = self.db.cursor()
		c.execute("SELECT intid FROM videocategory WHERE lower(category) = %s", (genre_name,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row[0]

		# Insert a new genre.
		c = self.db.cursor()
		c.execute("INSERT INTO videocategory(category) VALUES (%s)", (genre_name.capitalize(),))
		newid = c.lastrowid
		c.close()

		return newid

	def getCastId(self, cast_name):
		"""
		Find the id of the given cast from MythDB.

		If the cast does not exist, insert it and return its id.
		"""
		c = self.db.cursor()
		# print "SELECT intid FROM videocast WHERE lower(cast) = '%s'" % (cast_name,)
		c.execute("SELECT intid FROM videocast WHERE lower(cast) = %s", (cast_name,))
		row = c.fetchone()
		c.close()

		if row is not None:
			# print "getCastId %s %s" % (cast_name, row[0])
			return row[0]

		# Insert a new cast.
		c = self.db.cursor()
		c.execute("INSERT INTO videocast(cast) VALUES (%s)", (cast_name,))
		#print "INSERT INTO videocast(cast) VALUES ('%s')" % (cast_name,)
		c.close()

		c = self.db.cursor()
		c.execute("SELECT intid FROM videocast WHERE lower(cast) = %s", (cast_name,))
		row = c.fetchone()
		c.close()

		return row[0]

	def setCast(self, cast_name, idvideo):
		"""
		Insert the cast_name into videometadatacast if it does already exist.

		If the cast does not exist, insert it and return its id.
		"""

		idcast = self.getCastId(cast_name);

		c = self.db.cursor()
		c.execute("SELECT * FROM videometadatacast WHERE idvideo = %s AND idcast = %s", (idvideo,idcast))
		row = c.fetchone()
		c.close()

		if row is None:
			# Insert a new cast.
			c = self.db.cursor()
			c.execute("INSERT INTO videometadatacast VALUES (%s,%s)", (idvideo,idcast))
			#print "INSERT INTO videometadatacast VALUES (%s,%s)" % (idvideo,idcast)
			c.close()

	def getMetadataId(self, videopath):
		"""
		Finds the MythVideo metadata id for the given video path from the MythDB, if any.

		Returns None if no metadata was found.
		"""
		c = self.db.cursor()
		c.execute("""
				SELECT intid
				FROM videometadata
				WHERE filename = %s""", (videopath,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row[0]
		else:
			return None

	def hasMetadata(self, videopath):
		"""
		Determines if the given videopath has any metadata in the DB

		Returns False if no metadata was found.
		"""
		c = self.db.cursor()
		c.execute("""
				SELECT category, year
				FROM videometadata
				WHERE filename = %s""", (videopath,))
		row = c.fetchone()
		c.close()

		if row is not None:
			# If category is 0 and year is 1895, we can safely assume no metadata
			if (row[0] == 0) and (row[1] == 1895):
				return False
			else:
				return True
		else:
			return False

	def getMetadata(self, id):
		"""
		Finds the MythVideo metadata for the given id from the MythDB, if any.

		Returns None if no metadata was found.
		"""
		c = self.db.cursor()
		c.execute("""
				SELECT *
				FROM videometadata
				WHERE intid = %s""", (id,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row
		else:
			return None

	def setMetadata(self, data, id=None):
		"""
		Adds or updates the metadata in the database for a video item.
		"""
		c = self.db.cursor()
		if id is None:
			fields = ', '.join(data.keys())
			format_string = ', '.join(['%s' for d in data.values()])
			sql = "INSERT INTO videometadata(%s) VALUES(%s)" % (fields, format_string)
			c.execute(sql, data.values())
			intid = c.lastrowid
			c.close()
			return intid
		else:
			log.Msg(DEBUG, 'Updating metadata for %s', id)
			format_string = ', '.join(['%s = %%s' % d for d in data])
			sql = "UPDATE videometadata SET %s WHERE intid = %%s" % format_string
			sql_values = data.values()
			sql_values.append(id)
			c.execute(sql, sql_values)
			c.close()

# vim: ts=4 sw=4:
