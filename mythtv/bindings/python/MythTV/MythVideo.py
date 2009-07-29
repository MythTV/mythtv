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

	def getTitleId(self, title, subtitle=None, season=None, episode=None, array=False):
		"""
		Finds the MythVideo metadata id for the given Title from the MythDB, if any.
		Searches can be more specific if additional fields are supplied.
		If array is True return an array of intid's for the record(s) matching the search criteria.
		Without the array option a search with multiple matches will return a random result if more
		than one match was found. Non-array	return results are only supported for backward compatibilty
		with older scripts.
		
		Returns None if no metadata was found.
		"""
		tablenames = self.getTableFieldNames('videometadata')

		mysqlcommand = u"SELECT intid FROM videometadata WHERE title = %s"

		# Adjust the specificity of the title search according to the supplied criteria
		# Make sure all the additional query fields are actually in the videometadata schema
		if subtitle and 'subtitle' in tablenames: 
			mysqlcommand+=u" AND subtitle=%s" % subtitle
		if not season == None and 'season' in tablenames: 
			mysqlcommand+=u" AND season=%d" % int(season)
		if not episode == None and 'episode' in tablenames: 
			mysqlcommand+=u" AND episode=%d" % int(episode)

		c = self.db.cursor()
		c.execute(mysqlcommand, (title,))
		intids=[]
		while True:
			row = c.fetchone()
			if not row:
				break
			intids.append(row[0])
			if not array:
				break
		c.close()

		if len(intids):
			if not array: # To be backward compatible just return the first matching video intid found
				return intids[0] # but it could easily be one of many matching video intids 
			else:
				return intids
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

	def getTableFieldNames(self, tablename):
		"""Finds the table field names
		return array of field names in the order they are in the table
		return None if the table does not exist
		"""
		table_names=[]
		c = self.db.cursor()
		try:
			c.execute("DESC %s" % (tablename,))
		except:
			return None

		while True: # Add field names until a break (no more fields)
			name = c.fetchone()
			if name != None:
				table_names.append(name[0])
				continue
			break
		c.close()
		return table_names


	def getMetadataDictionary(self, id):
		"""
		Finds the MythVideo metadata for the given id from the MythDB, if any and returns the meta data
		as a dictionary of field names and meta data

		Returns None if no metadata was found.
		"""
		field_names = self.getTableFieldNames('videometadata')
		metadata = self.getMetadata(id)
		if not metadata or not field_names:
			return None

		# Build a dictionary of the existing meta data
		meta_dict={}
		for i in range(len(field_names)):
			meta_dict[field_names[i]] = metadata[i]
		
		return meta_dict

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
