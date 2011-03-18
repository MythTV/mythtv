/* Integrates the MythVideo DB schema into the core MythTV DB schema.  Used
 * only by DB update 1267.  Any further changes to the core MythTV schema,
 * including changes to those parts that were previously MythVideo, should
 * be done in mythtv/libs/libmythtv/dbcheck.cpp. */

#include <QString>
#include <QStringList>

#include "videodbcheck.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "mythverbose.h"
#include "remotefile.h"
#include "util.h"
#include "libmythmetadata/videoutils.h"

const QString minimumVideoDatabaseVersion = "1016";
const QString finalVideoDatabaseVersion = "1038";
const QString MythVideoVersionName = "mythvideo.DBSchemaVer";

static bool UpdateDBVersionNumber(const QString &field_name,
                                  const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec(QString("DELETE FROM settings WHERE value='%1';")
                    .arg(field_name)))
    {
        MythDB::DBError("UpdateDBVersionNumber - delete", query);
        return false;
    }

    if (!query.exec(QString("INSERT INTO settings (value, data, hostname) "
                            "VALUES ('%1', %2, NULL);")
                    .arg(field_name).arg(newnumber)))
    {
        MythDB::DBError("UpdateDBVersionNumber - insert", query);
        return false;
    }

    VERBOSE(VB_IMPORTANT,
            QString("Upgraded to MythVideo schema version %1")
            .arg(newnumber));
    return true;
}

static bool performActualUpdate(const QStringList &updates,
                                const QString &version,
                                QString &dbver, const QString &field_name)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT,
            QString("Upgrading to MythVideo schema version %1")
            .arg(version));

    for (QStringList::const_iterator p = updates.begin();
         p != updates.end(); ++p)
    {
        if (!query.exec(*p))
        {
            MythDB::DBError("performActualUpdate", query);
            return false;
        }
    }

    if (!UpdateDBVersionNumber(field_name, version))
        return false;
    dbver = version;
    return true;
}

static bool performActualUpdate(const QString updates[], const QString &version,
                                QString &dbver, const QString &field_name)
{
    QStringList upQuery;
    for (int i = 0; ; ++i)
    {
        QString q = updates[i];
        if (q == "") break;
        upQuery.append(q);
    }
    return performActualUpdate(upQuery, version, dbver, field_name);
}

static void AddFileType(const QString &extension,
                        const QString &playCommand = QString("Internal"),
                        bool ignored = false, bool useDefault = false)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM videotypes WHERE "
                  "LOWER(extension) = LOWER(:EXTENSION) LIMIT 1");
    query.bindValue(":EXTENSION", extension);
    if (query.exec() && !query.size())
    {
        query.prepare("INSERT INTO videotypes (extension, playcommand, "
                "f_ignore, use_default) VALUES (:EXTENSION, :PLAYCOMMAND, "
                ":IGNORE, :USEDEFAULT)");
        query.bindValue(":EXTENSION", extension);
        query.bindValue(":PLAYCOMMAND", playCommand);
        query.bindValue(":IGNORE", ignored);
        query.bindValue(":USEDEFAULT", useDefault);
        if (!query.exec())
            MythDB::DBError(QObject::tr("Error: failed to add new file "
                                        "type '%1'").arg(extension), query);
    }
}

static void UpdateHashes(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT `filename`, `host` FROM videometadata WHERE "
                  "`hash` = \"\"");
    if (query.exec() && query.size())
    {
        while (query.next())
        {
            QString filename = query.value(0).toString();
            QString host = query.value(1).toString();
            QString hash;

            if (!host.isEmpty())
            {
                QString url = generate_file_url("Videos", host, filename);
                hash =  RemoteFile::GetFileHash(url);
            }
            else
                hash = FileHash(filename);

            if (hash == "NULL")
                hash = QString();

            MSqlQuery updatequery(MSqlQuery::InitCon());

            updatequery.prepare("UPDATE videometadata set `hash` = :HASH "
                          "WHERE `filename` = :FILENAME AND "
                          "`host` = :HOST");
            updatequery.bindValue(":HASH", hash);
            updatequery.bindValue(":FILENAME", filename);
            updatequery.bindValue(":HOST", host);
            if (!updatequery.exec())
                MythDB::DBError(QObject::tr("Error: failed to hash file "
                                            "'%1'").arg(filename), updatequery);
            else
                VERBOSE(VB_GENERAL,
                    QString("Hash (%1) generated for file (%2)")
                    .arg(hash).arg(filename));
        }
    }
}

static bool InitializeVideoSchema(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    VERBOSE(VB_IMPORTANT, "Inserting initial video database information.");

    const QString updates[] = {
"CREATE TABLE dvdinput ("
"  intid int(10) unsigned NOT NULL,"
"  hsize int(10) unsigned DEFAULT NULL,"
"  vsize int(10) unsigned DEFAULT NULL,"
"  ar_num int(10) unsigned DEFAULT NULL,"
"  ar_denom int(10) unsigned DEFAULT NULL,"
"  fr_code int(10) unsigned DEFAULT NULL,"
"  letterbox tinyint(1) DEFAULT NULL,"
"  v_format varchar(16) DEFAULT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE dvdtranscode ("
"  intid int(11) NOT NULL AUTO_INCREMENT,"
"  input int(10) unsigned DEFAULT NULL,"
"  `name` varchar(128) NOT NULL,"
"  sync_mode int(10) unsigned DEFAULT NULL,"
"  use_yv12 tinyint(1) DEFAULT NULL,"
"  cliptop int(11) DEFAULT NULL,"
"  clipbottom int(11) DEFAULT NULL,"
"  clipleft int(11) DEFAULT NULL,"
"  clipright int(11) DEFAULT NULL,"
"  f_resize_h int(11) DEFAULT NULL,"
"  f_resize_w int(11) DEFAULT NULL,"
"  hq_resize_h int(11) DEFAULT NULL,"
"  hq_resize_w int(11) DEFAULT NULL,"
"  grow_h int(11) DEFAULT NULL,"
"  grow_w int(11) DEFAULT NULL,"
"  clip2top int(11) DEFAULT NULL,"
"  clip2bottom int(11) DEFAULT NULL,"
"  clip2left int(11) DEFAULT NULL,"
"  clip2right int(11) DEFAULT NULL,"
"  codec varchar(128) NOT NULL,"
"  codec_param varchar(128) DEFAULT NULL,"
"  bitrate int(11) DEFAULT NULL,"
"  a_sample_r int(11) DEFAULT NULL,"
"  a_bitrate int(11) DEFAULT NULL,"
"  two_pass tinyint(1) DEFAULT NULL,"
"  tc_param varchar(128) DEFAULT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE filemarkup ("
"  filename text NOT NULL,"
"  mark mediumint(8) unsigned NOT NULL DEFAULT '0',"
"  `offset` bigint(20) unsigned DEFAULT NULL,"
"  `type` tinyint(4) NOT NULL DEFAULT '0',"
"  KEY filename (filename(255))"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videocast ("
"  intid int(10) unsigned NOT NULL AUTO_INCREMENT,"
"  cast varchar(128) NOT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videocategory ("
"  intid int(10) unsigned NOT NULL AUTO_INCREMENT,"
"  category varchar(128) NOT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videocountry ("
"  intid int(10) unsigned NOT NULL AUTO_INCREMENT,"
"  country varchar(128) NOT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videogenre ("
"  intid int(10) unsigned NOT NULL AUTO_INCREMENT,"
"  genre varchar(128) NOT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videometadata ("
"  intid int(10) unsigned NOT NULL AUTO_INCREMENT,"
"  title varchar(128) NOT NULL,"
"  subtitle text NOT NULL,"
"  tagline varchar(255) DEFAULT NULL,"
"  director varchar(128) NOT NULL,"
"  studio varchar(128) DEFAULT NULL,"
"  plot text,"
"  rating varchar(128) NOT NULL,"
"  inetref varchar(255) NOT NULL,"
"  homepage text NOT NULL,"
"  `year` int(10) unsigned NOT NULL,"
"  releasedate date NOT NULL,"
"  userrating float NOT NULL,"
"  length int(10) unsigned NOT NULL,"
"  season smallint(5) unsigned NOT NULL DEFAULT '0',"
"  episode smallint(5) unsigned NOT NULL DEFAULT '0',"
"  showlevel int(10) unsigned NOT NULL,"
"  filename text NOT NULL,"
"  `hash` varchar(128) NOT NULL,"
"  coverfile text NOT NULL,"
"  childid int(11) NOT NULL DEFAULT '-1',"
"  browse tinyint(1) NOT NULL DEFAULT '1',"
"  watched tinyint(1) NOT NULL DEFAULT '0',"
"  processed tinyint(1) NOT NULL DEFAULT '0',"
"  playcommand varchar(255) DEFAULT NULL,"
"  category int(10) unsigned NOT NULL DEFAULT '0',"
"  trailer text,"
"  `host` text NOT NULL,"
"  screenshot text,"
"  banner text,"
"  fanart text,"
"  insertdate timestamp NULL DEFAULT CURRENT_TIMESTAMP,"
"  PRIMARY KEY (intid),"
"  KEY director (director),"
"  KEY title (title)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videometadatacast ("
"  idvideo int(10) unsigned NOT NULL,"
"  idcast int(10) unsigned NOT NULL,"
"  UNIQUE KEY idvideo (idvideo,idcast)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videometadatacountry ("
"  idvideo int(10) unsigned NOT NULL,"
"  idcountry int(10) unsigned NOT NULL,"
"  UNIQUE KEY idvideo_2 (idvideo,idcountry),"
"  KEY idvideo (idvideo),"
"  KEY idcountry (idcountry)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videometadatagenre ("
"  idvideo int(10) unsigned NOT NULL,"
"  idgenre int(10) unsigned NOT NULL,"
"  UNIQUE KEY idvideo_2 (idvideo,idgenre),"
"  KEY idvideo (idvideo),"
"  KEY idgenre (idgenre)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"CREATE TABLE videotypes ("
"  intid int(10) unsigned NOT NULL AUTO_INCREMENT,"
"  extension varchar(128) NOT NULL,"
"  playcommand varchar(255) NOT NULL,"
"  f_ignore tinyint(1) DEFAULT NULL,"
"  use_default tinyint(1) DEFAULT NULL,"
"  PRIMARY KEY (intid)"
") ENGINE=MyISAM DEFAULT CHARSET=utf8;",
"INSERT INTO dvdinput VALUES (1,720,480,16,9,1,1,'ntsc');",
"INSERT INTO dvdinput VALUES (2,720,480,16,9,1,0,'ntsc');",
"INSERT INTO dvdinput VALUES (3,720,480,4,3,1,1,'ntsc');",
"INSERT INTO dvdinput VALUES (4,720,480,4,3,1,0,'ntsc');",
"INSERT INTO dvdinput VALUES (5,720,576,16,9,3,1,'pal');",
"INSERT INTO dvdinput VALUES (6,720,576,16,9,3,0,'pal');",
"INSERT INTO dvdinput VALUES (7,720,576,4,3,3,1,'pal');",
"INSERT INTO dvdinput VALUES (8,720,576,4,3,3,0,'pal');",
"INSERT INTO dvdtranscode VALUES (1,1,'Good',2,1,16,16,0,0,2,0,0,0,0,0,32,32,8,8,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (2,2,'Excellent',2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'divx5',NULL,0,NULL,NULL,1,NULL);",
"INSERT INTO dvdtranscode VALUES (3,2,'Good',2,1,0,0,8,8,0,0,0,0,0,0,0,0,0,0,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (4,2,'Medium',2,1,0,0,8,8,5,5,0,0,0,0,0,0,0,0,'divx5',NULL,1200,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (5,3,'Good',2,1,0,0,0,0,0,0,0,0,2,0,80,80,8,8,'divx5',NULL,0,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (6,4,'Excellent',2,1,0,0,0,0,0,0,0,0,2,0,0,0,0,0,'divx5',NULL,0,NULL,NULL,1,NULL);",
"INSERT INTO dvdtranscode VALUES (7,4,'Good',2,1,0,0,8,8,0,2,0,0,0,0,0,0,0,0,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (8,5,'Good',1,1,16,16,0,0,5,0,0,0,0,0,40,40,8,8,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (9,6,'Good',1,1,0,0,16,16,5,0,0,0,0,0,0,0,0,0,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (10,7,'Good',1,1,0,0,0,0,1,0,0,0,0,0,76,76,8,8,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO dvdtranscode VALUES (11,8,'Good',1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,'divx5',NULL,1618,NULL,NULL,0,NULL);",
"INSERT INTO videotypes VALUES (1,'txt','',1,0);",
"INSERT INTO videotypes VALUES (2,'log','',1,0);",
"INSERT INTO videotypes VALUES (3,'mpg','Internal',0,0);",
"INSERT INTO videotypes VALUES (4,'avi','',0,1);",
"INSERT INTO videotypes VALUES (5,'vob','Internal',0,0);",
"INSERT INTO videotypes VALUES (6,'mpeg','Internal',0,0);",
"INSERT INTO videotypes VALUES (8,'iso','Internal',0,0);",
"INSERT INTO videotypes VALUES (9,'img','Internal',0,0);",
"INSERT INTO videotypes VALUES (10,'mkv','Internal',0,0);",
"INSERT INTO videotypes VALUES (11,'mp4','Internal',0,0);",
"INSERT INTO videotypes VALUES (12,'m2ts','Internal',0,0);",
"INSERT INTO videotypes VALUES (13,'evo','Internal',0,0);",
"INSERT INTO videotypes VALUES (14,'divx','Internal',0,0);",
"INSERT INTO videotypes VALUES (15,'mov','Internal',0,0);",
"INSERT INTO videotypes VALUES (16,'qt','Internal',0,0);",
"INSERT INTO videotypes VALUES (17,'wmv','Internal',0,0);",
"INSERT INTO videotypes VALUES (18,'3gp','Internal',0,0);",
"INSERT INTO videotypes VALUES (19,'asf','Internal',0,0);",
"INSERT INTO videotypes VALUES (20,'ogg','Internal',0,0);",
"INSERT INTO videotypes VALUES (21,'ogm','Internal',0,0);",
"INSERT INTO videotypes VALUES (22,'flv','Internal',0,0);",
"INSERT INTO videotypes VALUES (23,'ogv','Internal',0,0);",
"INSERT INTO videotypes VALUES (25,'nut','Internal',0,0);",
"INSERT INTO videotypes VALUES (26,'mxf','Internal',0,0);",
"INSERT INTO videotypes VALUES (27,'m4v','Internal',0,0);",
"INSERT INTO videotypes VALUES (28,'rm','Internal',0,0);",
"INSERT INTO videotypes VALUES (29,'ts','Internal',0,0);",
"INSERT INTO videotypes VALUES (30,'swf','Internal',0,0);",
"INSERT INTO videotypes VALUES (31,'f4v','Internal',0,0);",
"INSERT INTO videotypes VALUES (32,'nuv','Internal',0,0);",
NULL
};

    QString dbver = "";
    if (!performActualUpdate(updates, finalVideoDatabaseVersion, dbver,
                             MythVideoVersionName))
        return false;

    return true;
}

bool doUpgradeVideoDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("mythvideo.DBSchemaVer");
    if (dbver == finalVideoDatabaseVersion)
    {
        return true;
    }

    QString olddbver = gCoreContext->GetSetting("VideoDBSchemaVer");
    QString dvddbver = gCoreContext->GetSetting("DVDDBSchemaVer");
    if (dbver.isEmpty() && olddbver.isEmpty() && dvddbver.isEmpty())
    {
        if (!InitializeVideoSchema())
            return false;
        dbver = gCoreContext->GetSetting("mythvideo.DBSchemaVer");
    }

    if (dbver.isEmpty() || dbver.toInt() <  minimumVideoDatabaseVersion.toInt())
    {
        VERBOSE(VB_IMPORTANT, "Unrecognized video database schema version. "
                              "Unable to upgrade database.");
        VERBOSE(VB_IMPORTANT, "Please see mythplugins/mythvideo/README.database"
                              " for more information.");
        VERBOSE(VB_IMPORTANT, QString("mythvideo.DBSchemaVer: '%1', "
                "VideoDBSchemaVer: '%2', DVDDBSchemaVer: '%3'").arg(dbver)
                .arg(olddbver).arg(dvddbver));
        return false;
    }

    if (dbver == "1016")
    {
        const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
        .arg(gCoreContext->GetDatabaseParams().dbName),
"ALTER TABLE dvdbookmark"
"  MODIFY serialid varbinary(16) NOT NULL default '',"
"  MODIFY name varbinary(32) default NULL;",
"ALTER TABLE dvdinput"
"  MODIFY v_format varbinary(16) default NULL;",
"ALTER TABLE dvdtranscode"
"  MODIFY name varbinary(128) NOT NULL,"
"  MODIFY codec varbinary(128) NOT NULL,"
"  MODIFY codec_param varbinary(128) default NULL,"
"  MODIFY tc_param varbinary(128) default NULL;",
"ALTER TABLE filemarkup"
"  MODIFY filename blob NOT NULL;",
"ALTER TABLE videocast"
"  MODIFY cast varbinary(128) NOT NULL;",
"ALTER TABLE videocategory"
"  MODIFY category varbinary(128) NOT NULL;",
"ALTER TABLE videocountry"
"  MODIFY country varbinary(128) NOT NULL;",
"ALTER TABLE videogenre"
"  MODIFY genre varbinary(128) NOT NULL;",
"ALTER TABLE videometadata"
"  MODIFY title varbinary(128) NOT NULL,"
"  MODIFY director varbinary(128) NOT NULL,"
"  MODIFY plot blob,"
"  MODIFY rating varbinary(128) NOT NULL,"
"  MODIFY inetref varbinary(255) NOT NULL,"
"  MODIFY filename blob NOT NULL,"
"  MODIFY coverfile blob NOT NULL,"
"  MODIFY playcommand varbinary(255) default NULL;",
"ALTER TABLE videotypes"
"  MODIFY extension varbinary(128) NOT NULL,"
"  MODIFY playcommand varbinary(255) NOT NULL;",
""
};

        if (!performActualUpdate(updates, "1017", dbver,
                                 MythVideoVersionName))
            return false;
    }


    if (dbver == "1017")
    {
        const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
        .arg(gCoreContext->GetDatabaseParams().dbName),
"ALTER TABLE dvdbookmark"
"  DEFAULT CHARACTER SET default,"
"  MODIFY serialid varchar(16) CHARACTER SET utf8 NOT NULL default '',"
"  MODIFY name varchar(32) CHARACTER SET utf8 default NULL;",
"ALTER TABLE dvdinput"
"  DEFAULT CHARACTER SET default,"
"  MODIFY v_format varchar(16) CHARACTER SET utf8 default NULL;",
"ALTER TABLE dvdtranscode"
"  DEFAULT CHARACTER SET default,"
"  MODIFY name varchar(128) CHARACTER SET utf8 NOT NULL,"
"  MODIFY codec varchar(128) CHARACTER SET utf8 NOT NULL,"
"  MODIFY codec_param varchar(128) CHARACTER SET utf8 default NULL,"
"  MODIFY tc_param varchar(128) CHARACTER SET utf8 default NULL;",
"ALTER TABLE filemarkup"
"  DEFAULT CHARACTER SET default,"
"  MODIFY filename text CHARACTER SET utf8 NOT NULL;",
"ALTER TABLE videocast"
"  DEFAULT CHARACTER SET default,"
"  MODIFY cast varchar(128) CHARACTER SET utf8 NOT NULL;",
"ALTER TABLE videocategory"
"  DEFAULT CHARACTER SET default,"
"  MODIFY category varchar(128) CHARACTER SET utf8 NOT NULL;",
"ALTER TABLE videocountry"
"  DEFAULT CHARACTER SET default,"
"  MODIFY country varchar(128) CHARACTER SET utf8 NOT NULL;",
"ALTER TABLE videogenre"
"  DEFAULT CHARACTER SET default,"
"  MODIFY genre varchar(128) CHARACTER SET utf8 NOT NULL;",
"ALTER TABLE videometadata"
"  DEFAULT CHARACTER SET default,"
"  MODIFY title varchar(128) CHARACTER SET utf8 NOT NULL,"
"  MODIFY director varchar(128) CHARACTER SET utf8 NOT NULL,"
"  MODIFY plot text CHARACTER SET utf8,"
"  MODIFY rating varchar(128) CHARACTER SET utf8 NOT NULL,"
"  MODIFY inetref varchar(255) CHARACTER SET utf8 NOT NULL,"
"  MODIFY filename text CHARACTER SET utf8 NOT NULL,"
"  MODIFY coverfile text CHARACTER SET utf8 NOT NULL,"
"  MODIFY playcommand varchar(255) CHARACTER SET utf8 default NULL;",
"ALTER TABLE videometadatacast"
"  DEFAULT CHARACTER SET default;",
"ALTER TABLE videometadatacountry"
"  DEFAULT CHARACTER SET default;",
"ALTER TABLE videometadatagenre"
"  DEFAULT CHARACTER SET default;",
"ALTER TABLE videotypes"
"  DEFAULT CHARACTER SET default,"
"  MODIFY extension varchar(128) CHARACTER SET utf8 NOT NULL,"
"  MODIFY playcommand varchar(255) CHARACTER SET utf8 NOT NULL;",
""
};

        if (!performActualUpdate(updates, "1018", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1018")
    {
        QStringList updates;
        updates += "DELETE FROM settings WHERE value="
                   "'MovieListCommandLine' AND data LIKE '%imdb%';";
        updates += "DELETE FROM settings WHERE value="
                   "'MovieDataCommandLine' AND data LIKE '%imdb%';";
        updates += "DELETE FROM settings WHERE value="
                   "'MoviePosterCommandLine' AND data LIKE '%imdb%';";
        if (!performActualUpdate(updates, "1019", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1019")
    {
        QStringList updates(
                "ALTER TABLE videometadata ADD `trailer` TEXT;");
        if (!performActualUpdate(updates, "1020", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1020")
    {
        VERBOSE(VB_IMPORTANT, "Upgrading to MythVideo schema version 1021");

        AddFileType("mkv");
        AddFileType("mp4");
        AddFileType("m2ts");
        AddFileType("evo");
        AddFileType("divx");
        AddFileType("mov");
        AddFileType("qt");
        AddFileType("wmv");
        AddFileType("3gp");
        AddFileType("asf");
        AddFileType("ogg");
        AddFileType("ogm");
        AddFileType("flv");

        if (!UpdateDBVersionNumber(MythVideoVersionName, "1021"))
            return false;

        dbver = "1021";
    }

    if (dbver == "1021")
    {
         QStringList updates;
         updates += "ALTER TABLE videometadata ADD host text CHARACTER SET utf8 NOT NULL;";

         if (!performActualUpdate(updates, "1022", dbver,
                                  MythVideoVersionName))
            return false;
     }

    if (dbver == "1022")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD `screenshot` TEXT;";
        updates += "ALTER TABLE videometadata ADD `banner` TEXT;";
        updates += "ALTER TABLE videometadata ADD `fanart` TEXT;";
        if (!performActualUpdate(updates, "1023", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1023")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD `subtitle` TEXT "
                   "NOT NULL AFTER `title`;";
        updates += "ALTER TABLE videometadata ADD `season` SMALLINT "
                   "UNSIGNED NOT NULL DEFAULT '0' AFTER `length`;";
        updates += "ALTER TABLE videometadata ADD `episode` SMALLINT "
                   "UNSIGNED NOT NULL DEFAULT '0' AFTER `season`;";
        if (!performActualUpdate(updates, "1024", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1024")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD watched BOOL "
                   "NOT NULL DEFAULT 0 AFTER browse;";
        if (!performActualUpdate(updates, "1025", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1025")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD `insertdate` TIMESTAMP "
                   "NULL DEFAULT CURRENT_TIMESTAMP AFTER `fanart`;";
        if (!performActualUpdate(updates, "1026", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1026")
    {
        QStringList updates;
        updates += "DELETE FROM keybindings "
                   " WHERE action = 'DELETE' AND context = 'Video';";
        if (!performActualUpdate(updates, "1027", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1027")
    {
        VERBOSE(VB_IMPORTANT, "Upgrading to MythVideo schema version 1028");
        VERBOSE(VB_IMPORTANT, "Converting filenames in filemarkup table "
                "from absolute to relative paths.  This may take a long "
                "time if you have a large number of MythVideo seektables.");

        bool ok = true;
        MSqlQuery query(MSqlQuery::InitCon());
        MSqlQuery update(MSqlQuery::InitCon());

        query.prepare("SELECT DISTINCT filename FROM filemarkup;");
        update.prepare("UPDATE filemarkup SET filename = :RELPATH "
                       "    WHERE filename = :FULLPATH;");
        if (query.exec())
        {
            QString origPath;
            QString relPath;
            while (query.next())
            {
                origPath = query.value(0).toString();
                if (origPath.startsWith("dvd:"))
                    continue;

                relPath = StorageGroup::GetRelativePathname(origPath);
                if ((!relPath.isEmpty()) &&
                    (relPath != origPath))
                {
                    update.bindValue(":RELPATH", relPath);
                    update.bindValue(":FULLPATH", origPath);
                    if (!update.exec())
                    {
                        VERBOSE(VB_IMPORTANT,
                                QString("ERROR converting '%1' to '%2' in "
                                        "filemarkup table.")
                                        .arg(origPath).arg(relPath));
                        ok = false;
                    }
                }
            }
        }
        else
            ok = false;

        if (!ok)
            return false;

        if (!UpdateDBVersionNumber(MythVideoVersionName, "1028"))
            return false;

        dbver = "1028";
    }

    if (dbver == "1028")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD `releasedate` DATE "
                   "NOT NULL AFTER `year`;";
        updates += "ALTER TABLE videometadata ADD `homepage` TEXT "
                   "NOT NULL AFTER `inetref`;";
        if (!performActualUpdate(updates, "1029", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1029")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD `hash` VARCHAR(128) "
                   "NOT NULL AFTER `filename`;";
        if (!performActualUpdate(updates, "1030", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1030")
    {
        UpdateHashes();
        if (!UpdateDBVersionNumber(MythVideoVersionName, "1031"))
            return false;

        dbver = "1031";
    }

    if (dbver == "1031")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SHOW INDEX FROM videometadata");

        if (!query.exec())
        {
            MythDB::DBError("Unable to retrieve current indices on "
                            "videometadata.", query);
        }
        else
        {
            while (query.next())
            {
                QString index_name = query.value(2).toString();

                if ("title_2" == index_name)
                {
                    MSqlQuery update(MSqlQuery::InitCon());
                    update.prepare("ALTER TABLE videometadata "
                                   " DROP INDEX title_2");

                    if (!update.exec())
                         MythDB::DBError("Unable to drop duplicate index "
                                         "on videometadata. Ignoring.",
                                         update);
                    break;
                }
            }
        }

        if (!UpdateDBVersionNumber(MythVideoVersionName, "1032"))
            return false;

        dbver = "1032";
    }

    if (dbver == "1032")
    {
        QStringList updates;
        updates += "CREATE TEMPORARY TABLE bad_videometadatacast"
                    "       AS SELECT * FROM videometadatacast;";
        updates += "CREATE TEMPORARY TABLE bad_videometadatagenre"
                   "       AS SELECT * FROM videometadatagenre;";
        updates += "CREATE TEMPORARY TABLE bad_videometadatacountry"
                   "       AS SELECT * FROM videometadatacountry;";
        updates += "TRUNCATE TABLE videometadatacast;";
        updates += "TRUNCATE TABLE videometadatagenre;";
        updates += "TRUNCATE TABLE videometadatacountry;";
        updates += "INSERT videometadatacast SELECT idvideo,idcast"
                   "       FROM bad_videometadatacast GROUP BY idvideo,idcast;";
        updates += "INSERT videometadatagenre SELECT idvideo,idgenre"
                   "       FROM bad_videometadatagenre GROUP BY idvideo,idgenre;";
        updates += "INSERT videometadatacountry SELECT idvideo,idcountry"
                   "       FROM bad_videometadatacountry GROUP BY idvideo,idcountry;";
        updates += "DROP TEMPORARY TABLE bad_videometadatacast;";
        updates += "DROP TEMPORARY TABLE bad_videometadatagenre;";
        updates += "DROP TEMPORARY TABLE bad_videometadatacountry;";
        updates += "ALTER TABLE videometadatacast ADD UNIQUE INDEX (`idvideo`,`idcast`);";
        updates += "ALTER TABLE videometadatagenre ADD UNIQUE INDEX (`idvideo`,`idgenre`);";
        updates +="ALTER TABLE videometadatacountry ADD UNIQUE INDEX (`idvideo`,`idcountry`);";
        if (!performActualUpdate(updates, "1033", dbver,
                                 MythVideoVersionName))
            return false;

        dbver = "1033";
    }

    if (dbver == "1033")
    {
        AddFileType("ogv");
        AddFileType("BDMV");
        AddFileType("nut");
        AddFileType("mxf");
        AddFileType("m4v");
        AddFileType("rm");
        AddFileType("ts");
        AddFileType("swf");
        AddFileType("f4v");
        AddFileType("nuv");

        if (!UpdateDBVersionNumber(MythVideoVersionName, "1034"))
            return false;

        dbver = "1034";
    }

    if (dbver == "1034")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD `tagline` VARCHAR (255) "
                   "AFTER `subtitle`;";

        if (!performActualUpdate(updates, "1035", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1035")
    {
        QStringList updates;
        updates += "ALTER TABLE videometadata ADD processed BOOL "
                   "NOT NULL DEFAULT 0 AFTER watched;";
        if (!performActualUpdate(updates, "1036", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1036")
    {
        QStringList updates;
        updates += "ALTER TABLE  videometadata ADD  `studio` VARCHAR( 128 ) "
                   "AFTER `director`;";
        if (!performActualUpdate(updates, "1037", dbver,
                                 MythVideoVersionName))
            return false;
    }

    if (dbver == "1037")
    {
        QStringList updates;
        updates += "DELETE FROM videotypes WHERE extension = 'VIDEO_TS';";
        updates += "DELETE FROM videotypes WHERE extension = 'BDMV';";
        if (!performActualUpdate(updates, "1038", dbver,
                                 MythVideoVersionName))
            return false;
    }

    return true;
}

