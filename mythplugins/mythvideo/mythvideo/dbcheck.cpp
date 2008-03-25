#include <QString>
#include <QRegExp>
#include <QStringList>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "dbcheck.h"
#include "videodlg.h"

namespace
{
    const QString lastMythDVDDBVersion = "1002";
    const QString lastMythVideoVersion = "1010";

    const QString currentDatabaseVersion = "1018";

    const QString OldMythVideoVersionName = "VideoDBSchemaVer";
    const QString OldMythDVDVersionName = "DVDDBSchemaVer";

    const QString MythVideoVersionName = "mythvideo.DBSchemaVer";

    void UpdateDBVersionNumber(const QString &field_name,
                               const QString &newnumber)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.exec(QString("DELETE FROM settings WHERE value='%1';")
                   .arg(field_name));
        query.exec(QString("INSERT INTO settings (value, data, hostname) "
                           "VALUES ('%1', %2, NULL);")
                   .arg(field_name).arg(newnumber));
    }

    void performActualUpdate(const QStringList &updates, const QString &version,
                             QString &dbver, const QString &field_name)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        VERBOSE(VB_IMPORTANT,
                QString("Upgrading to MythVideo schema version %1")
                .arg(version));

        for (QStringList::const_iterator p = updates.begin();
             p != updates.end(); ++p)
        {
            query.exec(*p);
        }

        UpdateDBVersionNumber(field_name, version);
        dbver = version;
    }

    void performActualUpdate(const QString updates[], const QString &version,
                             QString &dbver, const QString &field_name)
    {
        QStringList upQuery;
        for (int i = 0; ; ++i)
        {
            QString q = updates[i];
            if (q == "") break;
            upQuery.append(q);
        }
        performActualUpdate(upQuery, version, dbver, field_name);
    }

    void InitializeVideoDatabase()
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythVideo initial database information.");

        const QString updates[] = {
    "CREATE TABLE IF NOT EXISTS videometadata ("
    "    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
    "    title VARCHAR(128) NOT NULL,"
    "    director VARCHAR(128) NOT NULL,"
    "    plot VARCHAR(255) NOT NULL,"
    "    rating VARCHAR(128) NOT NULL,"
    "    inetref VARCHAR(32) NOT NULL,"
    "    year INT UNSIGNED NOT NULL,"
    "    userrating FLOAT NOT NULL,"
    "    length INT UNSIGNED NOT NULL,"
    "    showlevel INT UNSIGNED NOT NULL,"
    "    filename TEXT NOT NULL,"
    "    coverfile TEXT NOT NULL,"
    "    childid INT NOT NULL DEFAULT -1,"
    "    browse BOOL NOT NULL DEFAULT 1,"
    "    playcommand VARCHAR(255),"
    "    INDEX (director),"
    "    INDEX (title)"
    ");",
    "CREATE TABLE IF NOT EXISTS videotypes ("
    "    intid       INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
    "    extension   VARCHAR(128) NOT NULL,"
    "    playcommand VARCHAR(255) NOT NULL,"
    "    f_ignore    BOOL,"
    "    use_default BOOL"
    ");",
    ""
        };
        QString dbver = "";
        performActualUpdate(updates, "1000", dbver, OldMythVideoVersionName);

        MSqlQuery qQuery(MSqlQuery::InitCon());
        qQuery.exec("SELECT * FROM videotypes;");

        if (!qQuery.isActive() || qQuery.size() <= 0)
        {
            const QString updates2[] = {
    "INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
    "    VALUES ('txt', '', 1, 0);",
    "INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
    "    VALUES ('log', '', 1, 0);",
    "INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
    "    VALUES ('mpg', 'Internal', 0, 0);",
    "INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
    "    VALUES ('avi', '', 0, 1);",
    "INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
    "    VALUES ('vob', 'Internal', 0, 0);",
    "INSERT INTO videotypes (extension, playcommand, f_ignore, use_default)"
    "    VALUES ('mpeg', 'Internal', 0, 0);",
    ""
            };
            dbver = "";
            performActualUpdate(updates2, "1000", dbver,
                                OldMythVideoVersionName);
        }
    }

    bool IsCombinedSchema()
    {
        QString dbver = gContext->GetSetting(MythVideoVersionName);

        return dbver != "";
    }

    void DoOldVideoDatabaseSchemaUpgrade()
    {
        if (IsCombinedSchema()) return;

        QString dbver = gContext->GetSetting(OldMythVideoVersionName);

        if (dbver == lastMythVideoVersion)
            return;

        if (dbver == "")
        {
            InitializeVideoDatabase();
            dbver = "1000";
        }

        if (dbver == "1000")
        {
            const QString updates[] = {
"ALTER TABLE videometadata ADD playcommand VARCHAR(255);",
"ALTER TABLE videometadata ADD INDEX(title);",
"ALTER TABLE videometadata ADD browse BOOL NOT NULL DEFAULT 1;",
""
            };

            performActualUpdate(updates, "1001", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1001")
        {
            const QString updates[] = {
"ALTER TABLE videometadata CHANGE childid childid INT NOT NULL DEFAULT -1;",
""
            };

            performActualUpdate(updates, "1002", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1002")
        {
            const QString updates[] = {
"ALTER TABLE videometadata CHANGE plot plot TEXT;",
"ALTER TABLE videometadata ADD COLUMN category INT UNSIGNED NOT NULL DEFAULT 0;",
"CREATE TABLE IF NOT EXISTS videocategory ( intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, category VARCHAR(128) NOT NULL );",
"CREATE TABLE IF NOT EXISTS videocountry ( intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, country VARCHAR(128) NOT NULL ); ",
"CREATE TABLE IF NOT EXISTS videometadatacountry ( idvideo INT UNSIGNED NOT NULL, idcountry INT UNSIGNED NOT NULL );",
"CREATE TABLE IF NOT EXISTS videogenre ( intid INT UNSIGNED AUTO_INCREMENT NOT NULL  PRIMARY KEY, genre VARCHAR(128) NOT NULL);",
"CREATE TABLE IF NOT EXISTS videometadatagenre ( idvideo INT UNSIGNED NOT NULL,idgenre INT UNSIGNED NOT NULL );",
""
            };

            performActualUpdate(updates, "1003", dbver,
                                OldMythVideoVersionName);
        }


        if (dbver == "1003")
        {
            const QString updates[] = {
"CREATE TABLE IF NOT EXISTS filemarkup"
"("
"    filename TEXT NOT NULL,"
"    mark BIGINT(20) NOT NULL,"
"    offset VARCHAR(32) NULL,"
"    type INT NOT NULL"
");",
""
            };
            performActualUpdate(updates, "1004", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1004")
        {
            const QString updates[] = {
"UPDATE keybindings SET keylist = \"[,{,F10\" WHERE action = \"DECPARENT\" AND keylist = \"Left\";",
"UPDATE keybindings SET keylist = \"],},F11\" WHERE action = \"INCPARENT\" AND keylist = \"Right\";",
""
            };
            performActualUpdate(updates, "1005", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1005")
        {
            const QString updates[] = {
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default) "
"VALUES ('VIDEO_TS', 'Internal', 0, 0);",
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default) "
"VALUES ('iso', 'Internal', 0, 0);",
""
            };

            performActualUpdate(updates, "1006", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1006")
        {
            const QString updates[] = {
"ALTER TABLE videometadatacountry ADD INDEX(idvideo); ",
"ALTER TABLE videometadatacountry ADD INDEX(idcountry);",
"ALTER TABLE videometadatagenre ADD INDEX(idvideo);",
"ALTER TABLE videometadatagenre ADD INDEX(idgenre);",
""
            };

            performActualUpdate(updates, "1007", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1007")
        {
            const QString updates[] = {
"INSERT INTO filemarkup (filename, type, mark) SELECT filename,"
" '2', bookmark FROM videobookmarks;",
""
            };

            performActualUpdate(updates, "1008", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1008")
        {
            QStringList updates;

            MSqlQuery query(MSqlQuery::InitCon());
            query.exec("SELECT intid FROM videocategory;");

            if (query.isActive() && query.size())
            {
                QString categoryIDs = "'0'";
                while (query.next())
                {
                    categoryIDs += ",'" + query.value(0).toString() + "'";
                }
                updates.append(QString(
"UPDATE videometadata SET category = 0 WHERE category NOT IN (%1);")
                               .arg(categoryIDs));
            }
            else
            {
                updates.append("SELECT 1;");
            }

            performActualUpdate(updates, "1009", dbver,
                                OldMythVideoVersionName);
        }

        if (dbver == "1009")
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.exec("SELECT extension, playcommand FROM videotypes");

            QRegExp extChange("^(img|vob|mpeg|mpg|iso|VIDEO_TS)$", false);
            QStringList updates;
            if (query.isActive() && query.size())
            {
                while (query.next())
                {
                    QString extension = query.value(0).toString();
                    QString playcommand = query.value(1).toString();
                    if (playcommand != "Internal" &&
                        extension.find(extChange) == 0)
                    {
                        updates.append(QString(
"UPDATE videotypes SET extension = '%1_old' WHERE extension = '%2';")
                                       .arg(extension).arg(extension));
                        updates.append(QString(
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default) "
"VALUES ('%3', 'Internal', 0, 0);").arg(extension));
                    }
                }
            }
            updates.append(
"INSERT INTO videotypes (extension, playcommand, f_ignore, use_default) VALUES "
"('img', 'Internal', 0, 0);");

            performActualUpdate(updates, "1010", dbver,
                                OldMythVideoVersionName);
        }
    }

    void InitializeDVDDatabase()
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythDVD initial database information.");

        MSqlQuery qQuery(MSqlQuery::InitCon());
        qQuery.exec("SELECT * FROM dvdinput;");

        if (!qQuery.isActive() || qQuery.size() <= 0)
        {
            const QString updates[] = {
"CREATE TABLE IF NOT EXISTS dvdinput ("
"    intid       INT UNSIGNED NOT NULL PRIMARY KEY,"
"    hsize       INT UNSIGNED,"
"    vsize       INT UNSIGNED,"
"    ar_num      INT UNSIGNED,"
"    ar_denom    INT UNSIGNED,"
"    fr_code     INT UNSIGNED,"
"    letterbox   BOOL,"
"    v_format    VARCHAR(16)"
");",
// ntsc 16:9 letterbox
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (1, 720, 480, 16, 9, 1, 1, \"ntsc\");",
// ntsc 16:9
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (2, 720, 480, 16, 9, 1, 0, \"ntsc\");",
// ntsc 4:3 letterbox
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (3, 720, 480, 4, 3, 1, 1, \"ntsc\");",
// ntsc 4:3
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (4, 720, 480, 4, 3, 1, 0, \"ntsc\");",
// pal 16:9 letterbox
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (5, 720, 576, 16, 9, 3, 1, \"pal\");",
// pal 16:9
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (6, 720, 576, 16, 9, 3, 0, \"pal\");",
// pal 4:3 letterbox
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (7, 720, 576, 4, 3, 3, 1, \"pal\");",
// pal 4:3
"INSERT INTO dvdinput"
"    (intid, hsize, vsize, ar_num, ar_denom, fr_code, letterbox, v_format)"
"    VALUES"
"    (8, 720, 576, 4, 3, 3, 0, \"pal\");",
""
            };
            QString dbver = "";
            performActualUpdate(updates, "1000", dbver, OldMythDVDVersionName);
        }

        qQuery.exec("SELECT * FROM dvdtranscode;");
        if (!qQuery.isActive() || qQuery.size() <= 0)
        {
            const QString updates[] = {
"CREATE TABLE IF NOT EXISTS dvdtranscode ("
"    intid       INT AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    input       INT UNSIGNED,"
"    name        VARCHAR(128) NOT NULL,"
"    sync_mode   INT UNSIGNED,"
"    use_yv12    BOOL,"
"    cliptop     INT,"
"    clipbottom  INT,"
"    clipleft    INT,"
"    clipright   INT,"
"    f_resize_h  INT,"
"    f_resize_w  INT,"
"    hq_resize_h INT,"
"    hq_resize_w INT,"
"    grow_h      INT,"
"    grow_w      INT,"
"    clip2top    INT,"
"    clip2bottom INT,"
"    clip2left   INT,"
"    clip2right  INT,"
"    codec       VARCHAR(128) NOT NULL,"
"    codec_param VARCHAR(128),"
"    bitrate     INT,"
"    a_sample_r  INT,"
"    a_bitrate   INT,"
"    two_pass    BOOL"
");",
// ntsc 16:9 letterbox --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (1, \"Good\", 2, 0, 16, 16, 0, 0, 2, 0, 0, 0, 0, 0, 32, 32, 8, 8,"
"    \"divx5\", 1618, 0);",
// ntsc 16:9 --> Excellent
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (2, \"Excellent\", 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,"
"    \"divx5\", 0, 1);",
// ntsc 16:9 --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (2, \"Good\", 2, 1, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,"
"    \"divx5\", 1618, 0);",
// ntsc 16:9 --> Medium
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (2, \"Medium\", 2, 1, 0, 0, 8, 8, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,"
"    \"divx5\", 1200, 0);",
// ntsc 4:3 letterbox --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (3, \"Good\", 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 80, 80, 8, 8,"
"    \"divx5\", 0, 0);",
// ntsc 4:3 --> Excellent
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (4, \"Excellent\", 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,"
"    \"divx5\", 0, 1);",
// ntsc 4:3 --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (4, \"Good\", 2, 1, 0, 0, 8, 8, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,"
"    \"divx5\", 1618, 0);",
// pal 16:9 letterbox --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"    (5, \"Good\", 1, 1, 16, 16, 0, 0, 5, 0, 0, 0, 0, 0, 40, 40, 8, 8,"
"    \"divx5\", 1618, 0);",
// pal 16:9 --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (6, \"Good\", 1, 1, 0, 0, 16, 16, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0,"
"    \"divx5\", 1618, 0);",
// pal 4:3 letterbox --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (7, \"Good\", 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 76, 76, 8, 8,"
"    \"divx5\", 1618, 0);",
// pal 4:3 --> Good
"INSERT INTO dvdtranscode"
"  (input, name, sync_mode, use_yv12, cliptop, clipbottom, clipleft, clipright,"
"   f_resize_h, f_resize_w, hq_resize_h, hq_resize_w,"
"   grow_h, grow_w, clip2top, clip2bottom, clip2left, clip2right,"
"   codec, bitrate, two_pass)"
"  VALUES"
"   (8, \"Good\", 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,"
"    \"divx5\", 1618, 0);",
""
            };
            QString dbver = "";
            performActualUpdate(updates, "1000", dbver, OldMythDVDVersionName);
        }
    }

    void DoOldDVDDatabaseSchemaUpgrage()
    {
        if (IsCombinedSchema()) return;

        QString dbver = gContext->GetSetting(OldMythDVDVersionName);

        if (dbver == lastMythDVDDBVersion)
            return;

        if (dbver == "")
        {
            InitializeDVDDatabase();
            dbver = "1000";
        }

        if (dbver == "1000")
        {
            const QString updates[] = {
"UPDATE dvdtranscode SET use_yv12=1 WHERE (intid=1 OR intid=2 OR intid=12 OR intid=13);",
""
            };
            performActualUpdate(updates, "1001", dbver, OldMythDVDVersionName);
        }

        if (dbver == "1001")
        {
            const QString updates[] = {
"ALTER TABLE dvdtranscode ADD COLUMN tc_param VARCHAR(128);",
""
            };
            performActualUpdate(updates, "1002", dbver, OldMythDVDVersionName);
        }
    }

    void DoVideoDatabaseSchemaUpgrade()
    {
        QString dvdver = gContext->GetSetting(OldMythDVDVersionName);
        QString oldmythvideover = gContext->GetSetting(OldMythVideoVersionName);

        if (dvdver == lastMythDVDDBVersion &&
            oldmythvideover == lastMythVideoVersion)
        {
            QStringList updates;
            updates += QString("DELETE FROM settings WHERE value='%1';")
                    .arg(OldMythVideoVersionName);
            updates += QString("DELETE FROM settings WHERE value='%1';")
                    .arg(OldMythDVDVersionName);

            QString dbver;
            performActualUpdate(updates, "1011", dbver, MythVideoVersionName);

            VERBOSE(VB_IMPORTANT,
                    QString("Updated from old MythDVD/MythVideo schema to "
                            "combined version: %1.").arg(dbver));
        }

        QString dbver = gContext->GetSetting(MythVideoVersionName);

        if (dbver == currentDatabaseVersion)
            return;

        if (dbver == "1011")
        {
            const QString updates[] = {
"ALTER TABLE filemarkup MODIFY mark MEDIUMINT UNSIGNED NOT NULL DEFAULT 0, "
                       "MODIFY offset BIGINT UNSIGNED, "
                       "MODIFY type TINYINT NOT NULL DEFAULT 0;",
""
};
            performActualUpdate(updates, "1012", dbver, MythVideoVersionName);
        }

        if (dbver == "1012")
        {
            // handle DialogType value change
            const QString setting("Default MythVideo View");
            int view = gContext->GetNumSetting(setting, -1);
            if (view != -1)
            {
                switch (view)
                {
                    case 0: view = VideoDialog::DLG_BROWSER; break;
                    case 2: view = VideoDialog::DLG_TREE; break;
                    case 1:
                    default: view = VideoDialog::DLG_GALLERY; break;
                }
                gContext->SaveSetting(setting, view);
            }
            performActualUpdate(QStringList(), "1013", dbver,
                                MythVideoVersionName);
        }

        if (dbver == "1013")
        {
            QStringList updates;
            updates += "ALTER TABLE filemarkup ADD INDEX (filename(255));";
            performActualUpdate(updates, "1014", dbver, MythVideoVersionName);
        }

        if (dbver == "1014")
        {
            // Add Cast tables
            const QString updates[] = {
"CREATE TABLE videocast ("
    "intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
    "cast VARCHAR(128) NOT NULL);",
"CREATE TABLE videometadatacast ("
        "idvideo INT UNSIGNED NOT NULL,"
        "idcast INT UNSIGNED NOT NULL);",
""
            };

            performActualUpdate(updates, "1015", dbver, MythVideoVersionName);
        }

        if (dbver == "1015")
        {
            QStringList updates;
            updates +=
            "ALTER TABLE videometadata MODIFY inetref VARCHAR(255) NOT NULL;";
            performActualUpdate(updates, "1016", dbver, MythVideoVersionName);
        }

        if (dbver == "1016")
        {
            const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
        .arg(gContext->GetDatabaseParams().dbName),
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

            performActualUpdate(updates, "1017", dbver, MythVideoVersionName);
        }


        if (dbver == "1017")
        {
            const QString updates[] = {
QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
        .arg(gContext->GetDatabaseParams().dbName),
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

            performActualUpdate(updates, "1018", dbver, MythVideoVersionName);
        }

    }
}

void UpgradeVideoDatabaseSchema()
{
    if (!IsCombinedSchema())
    {
        DoOldVideoDatabaseSchemaUpgrade();
        DoOldDVDDatabaseSchemaUpgrage();
    }

    DoVideoDatabaseSchemaUpgrade();
}
