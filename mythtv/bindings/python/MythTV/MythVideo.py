"""
Provides the MythVideo class with convenient methods to access the MythTV
MythVideo database.
"""

from MythDB import *
from MythLog import *
from socket import gethostname

log = MythLog(CRITICAL, '#%(levelname)s - %(message)s', 'MythVideo')

MVSCHEMA_VERSION = 1028

class MythVideo(object):
	"""
	Provides convinience methods to access the MythTV MythVideo database.
	"""
	def __init__(self):
		"""
		Initialise the MythDB connection.
		"""
		self.db = MythDB()

		# check schema version
		sver = int(self.db.getSetting('mythvideo.DBSchemaVer'))
		if MVSCHEMA_VERSION != sver:
			log.Msg(CRITICAL, 'DB speaks schema version %d, but we speak version %d', 
					sver, MVSCHEMA_VERSION)
			raise MythError('Mismatched schema version')

	def rtnVideoStorageGroup(self, host=None):
		'''Get the storage group 'Videos' directory for the suppied host name or default to the localhost.
		return None if no Videos Storage Group found
		return dirname field
		'''
		if not host: # If a hostname was not supplied then use the local host name
			host = gethostname()

		ret = []
		for i in self.db.getStorageGroup('Videos',host):
			ret.append(i['dirname'])

		if ret:
			return ret
		else:
			return None

	def pruneMetadata(self):
		"""
		Removes metadata from the database for files that no longer exist. 
		Respects 'Videos' storage groups and relative paths.
		"""
		host = gethostname() # Pruning can only be done for local files therefore the local host forced
		host = host.lower() # Required as videometadata stores lowercase hostname unlike other db tables
		vid_sg = self.rtnVideoStorageGroup(host) # Get the 'Videos' storage groups directory for a host

		c = self.db.cursor()
		c.execute(u"""
				SELECT intid, filename, host
				FROM videometadata""")

		while True:
			row = c.fetchone()
			if not row:
				break
			intid = row[0]
			filename = row[1]
			hostname = row[2]
			filename_array = []
			if vid_sg == None: # If no 'Videos' storagegroups process only filenames with an absolute path
				if filename[0] != u'/':  # Skip any video filenames with relative path
					continue
				filename_array.append(filename)
			else:   # If this localhost has a 'Videos' storagegroup process only filenames
					# with a relative path and belongs to this localhost
				if filename[0] == u'/': 
					continue
				if hostname != host: 
					continue
				for sg in vid_sg: # Handle multiple Videos SGs for one backend
					filename_array.append(sg+filename) # Make an absolute path from a SG and relative path
			for filename in filename_array:
				if os.path.exists(filename): # Handle multiple Videos SGs for one backend
					break
			else:
				log.Msg(INFO, u'%s not exist, removing metadata...', filename)
				self.rmMetadata(intid)
		c.close()


	def getGenreId(self, genre_name):
		"""
		Find the id of the given genre from MythDB.

		If the category does not exist, insert it and return its id.
		This function should have been called getCategoryId
		"""
		c = self.db.cursor()
		c.execute("""SELECT intid FROM videocategory
				WHERE lower(category) = %s""", (genre_name,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row[0]

		# Insert a new genre.
		c = self.db.cursor()
		c.execute("""INSERT INTO videocategory(category)
				VALUES (%s)""", (genre_name.capitalize(),))
		newid = c.lastrowid
		c.close()

		return newid

	def getCastId(self, cast_name):
		"""
		Find the id of the given cast from MythDB.

		If the cast does not exist, insert it and return its id.
		"""
		return self.getFieldId('cast', cast_name)

	def setCast(self, cast_name, idvideo):
		"""
		Insert the cast_name into videometadatacast if it does already exist.

		If the cast does not exist, insert it and return its id.
		"""

		return self.setField('cast', cast_name, idvideo)

	def getMetadataId(self, videopath, host=None):
		"""
		Finds the MythVideo metadata id for the given video path from the MythDB, if any. 
		Returns None if no metadata was found.
		"""
		mysqlcommand = u'SELECT intid FROM videometadata WHERE filename=%s'
		if host != None and videopath[0] != '/': 
			mysqlcommand+=u' AND host=%s'
		else:
			host = None
		c = self.db.cursor()
		if host:
			c.execute(mysqlcommand, (videopath, host))
		else:
			c.execute(mysqlcommand, (videopath,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row[0]
		else:
			return None

	def getTitleId(self, title, subtitle=None, season=None, episode=None, array=False, host=None):
		"""
		Finds the MythVideo metadata id for the given Title from the MythDB, if any.
		Searches consider storage groups if a host is supplied
		Searches can be more specific if additional fields supplied.
		If array is True return an array of intid's for the record(s) matching the search criteria.
		Without the array option a search with multiple matches will return a random result if more
		than one match was found. Non-array	return results are only supported for backward compatibilty
		with older scripts.
		
		Returns None if no metadata was found.
		"""
		if host: # Get the 'Videos' storage groups directory for a host
			if self.rtnVideoStorageGroup(host):
				# Required as videometadata stores lowercase hostname unlike other db tables
				host2 = host.lower() 
			else:
				host2 = None # If there is no storage group for a specific host then host is irrelevant
		else:
			host2 = None

		tablenames = self.getTableFieldNames('videometadata')

		mysqlcommand = u"SELECT intid FROM videometadata WHERE title = %s"

		# Adjust the specificity of the title search according to the supplied criteria
		# Make sure all the additional query fields are actually in the videometadata schema
		if subtitle and 'subtitle' in tablenames: 
			mysqlcommand+=u" AND subtitle=%s" % subtitle
		if season != None and 'season' in tablenames: 
			mysqlcommand+=u" AND season=%d" % int(season)
		if episode != None and 'episode' in tablenames: 
			mysqlcommand+=u" AND episode=%d" % int(episode)
		if host2 != None and 'host' in tablenames:
			mysqlcommand+=u" AND host=%s"

		c = self.db.cursor()
		if host2 != None:
			c.execute(mysqlcommand, (title, host2))
		else:
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

	def hasMetadata(self, videopath, host=None):
		"""
		Determines if the given videopath has any metadata in the DB

		Returns False if no metadata was found.
		"""
		intid = self.getMetadataId(videopath, host)
		if not intid:
			return False
		metadata = self.getMetadataDictionary(intid)
		if not metadata:
			return False

		if (metadata['category'] == 0) and (metadata['year'] == 1895):
			return False
		else:
			return True


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

	def rmMetadata(self, video, host=None):
		"""
		Removes the metadata for a given id from the database.

		Accepts either a videopath as a str, or an id as an int.

		Has no effect if it does not exist.
		"""
		c = self.db.cursor()
		if isinstance(video, str):
			if video[0] !='/' and host == None: # Protect against accidental multiple deletions on SG's
				log.Msg(WARNING, u"Attempting to delete a video for a storage group when the hostname is not supplied. That could potentially remove the videometadata on multiple backends " +
						u"for videometadata: %s" % video)
				return
			intid = self.getMetadataId(video, host)
			if intid == None:
				log.Msg(WARNING, u"Attempt to delete a video that does not exist " +
						u"for videometadata: %s" % video)
				return
			c.execute("DELETE FROM videometadata WHERE intid = %s", (intid,))
			video = intid
		elif isinstance(video, int) or isinstance(video, long):
			c.execute("DELETE FROM videometadata WHERE intid = %s", (video,))
		else:
			log.Msg(WARNING, "Attempt to delete non-str, non-int item" +
					"from videometadata: %s" % str(video))
		# Clean up this video's relationships to Genres, Country and Cast
		self.cleanGenres(video)
		self.cleanCountry(video)
		self.cleanCast(video)
		c.close()

	def getCategoryId(self, category_name):
		"""
		Find the id of the given category from MythDB.

		If the category does not exist, insert it and return its id.
		"""
		c = self.db.cursor()
		c.execute("SELECT intid FROM videocategory WHERE lower(category) = %s", (category_name,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row[0]

		# Insert a new genre.
		c = self.db.cursor()
		c.execute("INSERT INTO videocategory(category) VALUES (%s)", (category_name.capitalize(),))
		newid = c.lastrowid
		c.close()

		return newid

	def getFieldId(self, field, name):
		"""
		Find the id of the given name from MythDB.

		If the name does not exist, insert it and return its id.
		"""
		c = self.db.cursor()
		# print ""SELECT intid FROM video%s WHERE lower(%s) = %%s" % (field, field), (name,)
		c.execute("SELECT intid FROM video%s WHERE lower(%s) = %%s" % (field, field), (name,))
		row = c.fetchone()
		c.close()

		if row is not None:
			return row[0]

		# Insert a new cast.
		c = self.db.cursor()
		c.execute("INSERT INTO video%s(%s) VALUES (%%s)" % (field, field), (name,))
		#print "INSERT INTO video%s(%s) VALUES (%%s)" % (field, field), (name,)
		c.close()

		c = self.db.cursor()
		c.execute("SELECT intid FROM video%s WHERE lower(%s) = %%s" % (field, field), (name,))
		row = c.fetchone()
		c.close()

		return row[0]
	
	def setField(self, field, name, idvideo):
		"""
		Insert the name into videometadata"field" if it does already exist.

		If the name does not exist, insert it.
		"""

		id = self.getFieldId(field, name);

		c = self.db.cursor()
		c.execute("SELECT * FROM videometadata%s WHERE idvideo = %%s AND id%s = %%s" % (field,field), (idvideo, id))
		row = c.fetchone()
		c.close()

		if row is None:
			# Insert a new cast.
			c = self.db.cursor()
			c.execute("INSERT INTO videometadata%s VALUES (%%s,%%s)" % field, (idvideo,id))
			#print "INSERT INTO videometadata%s VALUES (%s,%s)" % field, (idvideo, id)
			c.close()
		
		return id
	

	def getGenresId(self, genre_name):
		"""
		Find the id of the given cast from MythDB.

		If the genre does not exist, insert it and return its id.
		"""
		return self.getFieldId('genre', genre_name.capitalize())

	def setGenres(self, genre_name, idvideo):
		"""
		Insert the idvideo file in given genre list if it does already exist.
		Genre will be created if it doesn't exist return its id.
		"""

		return self.setField('genre', genre_name.capitalize(), idvideo)

	def getCountryId(self, country_name):
		"""
		Find the id of the given country from MythDB.

		If the country does not exist, insert it and return its id.
		"""
		return self.getFieldId('country', country_name)

	def setCountry(self, country_name, idvideo):
		"""
		Insert the idvideo file in given country list if it does already exist.
		Country will be created if it doesn't exist return its id.
		"""

		return self.setField('country', country_name, idvideo)
		
	def cleanGenres(self, idvideo):
		"""
		Remove all cross-references of video from genres list.
		"""
		c = self.db.cursor()
		c.execute("""DELETE FROM videometadatagenre WHERE idvideo = %s""", (idvideo,))
		c.close()

	def cleanCountry(self, idvideo):
		"""
		Remove all cross-references of video from country list.
		"""
		c = self.db.cursor()
		c.execute("""DELETE FROM videometadatacountry WHERE idvideo = %s""", (idvideo,))
		c.close()

	def cleanCast(self, idvideo):
		"""
		Remove all cross-references of video from cast list.
		"""
		c = self.db.cursor()
		c.execute("""DELETE FROM videometadatacast WHERE idvideo = %s""", (idvideo,))
		c.close()
		
# vim: ts=4 sw=4:
