#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1001";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("DELETE FROM settings WHERE value='DVDDBSchemaVer';");
    query.exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('DVDDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythDVD schema version ") + 
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.exec(thequery);
        counter++;
        thequery = updates[counter];
    }

    UpdateDBVersionNumber(version);
    dbver = version;
}

static void InitializeDatabase(void)
{
    VERBOSE(VB_IMPORTANT, "Inserting MythDVD initial database information.");

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
        performActualUpdate(updates, "1000", dbver);
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
        performActualUpdate(updates, "1000", dbver);
    }

    UpdateDBVersionNumber("1000");
}

void UpgradeDVDDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("DVDDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        InitializeDatabase();
        dbver = "1000";
    }


    if (dbver == "1000")
    {
      const QString updates[] = {
"UPDATE dvdtranscode SET use_yv12=1 WHERE (intid=1 OR intid=2 OR intid=12 OR intid=13);",
""
};
      performActualUpdate(updates, "1001", dbver);
    }
    
}
