// libmyth* headers
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

// local headers
#include "eitutils.h"

static int ClearEIT(const MythUtilCommandLineParser &cmdline)
{
    int result = GENERIC_EXIT_OK;
    int sourceid = -1;

    if (cmdline.toBool("sourceid"))
    {
        sourceid = cmdline.toInt("sourceid");
    }

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        // truncate EIT cache
        QString sql = "TRUNCATE eit_cache;";
        query.prepare(sql);
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Truncating EIT cache"));
        if (!query.exec())
        {
            MythDB::DBError("Truncate eit_cache table", query);
            result = GENERIC_EXIT_NOT_OK;
        }

        // delete program for all channels that use EIT on sources that use EIT
        sql = "DELETE FROM program WHERE chanid IN ("
              "SELECT chanid FROM channel "
              "WHERE deleted IS NULL AND "
              "      useonairguide = 1 AND "
              "      sourceid IN ("
              "SELECT sourceid FROM videosource WHERE useeit=1";
        if (-1 != sourceid)
        {
            sql += " AND sourceid = :SOURCEID";
        }
        sql += "));";
        query.prepare(sql);
        if (-1 != sourceid)
        {
            query.bindValue(":SOURCEID", sourceid);
        }
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Deleting program entries from EIT."));
        if (!query.exec())
        {
            MythDB::DBError("Delete program entries from EIT", query);
            result = GENERIC_EXIT_NOT_OK;
        }

        // delete programrating for all channels that use EIT on sources that use EIT
        sql = "DELETE FROM programrating WHERE chanid IN ("
              "SELECT chanid FROM channel "
              "WHERE deleted IS NULL AND "
              "      useonairguide = 1 AND "
              "      sourceid IN ("
              "SELECT sourceid FROM videosource WHERE useeit=1";
        if (-1 != sourceid)
        {
            sql += " AND sourceid = :SOURCEID";
        }
        sql += "));";
        query.prepare(sql);
        if (-1 != sourceid)
        {
            query.bindValue(":SOURCEID", sourceid);
        }
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Deleting program rating entries from EIT."));
        if (!query.exec())
        {
            MythDB::DBError("Delete program rating entries from EIT", query);
            result = GENERIC_EXIT_NOT_OK;
        }

        // delete credits for all channels that use EIT on sources that use EIT
        sql = "DELETE FROM credits WHERE chanid IN ("
              "SELECT chanid FROM channel "
              "WHERE deleted IS NULL AND "
              "      useonairguide = 1 AND "
              "      sourceid IN ("
              "SELECT sourceid FROM videosource WHERE useeit=1";
        if (-1 != sourceid)
        {
            sql += " AND sourceid = :SOURCEID";
        }
        sql += "));";
        query.prepare(sql);
        if (-1 != sourceid)
        {
            query.bindValue(":SOURCEID", sourceid);
        }
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Deleting credits from EIT."));
        if (!query.exec())
        {
            MythDB::DBError("Delete credits from EIT", query);
            result = GENERIC_EXIT_NOT_OK;
        }

        // delete program genres for all channels that use EIT on sources that use EIT
        sql = "DELETE FROM programgenres WHERE chanid IN ("
              "SELECT chanid FROM channel "
              "WHERE deleted IS NULL AND "
              "      useonairguide = 1 AND "
              "      sourceid IN ("
              "SELECT sourceid FROM videosource WHERE useeit=1";
        if (-1 != sourceid)
        {
            sql += " AND sourceid = :SOURCEID";
        }
        sql += "));";
        query.prepare(sql);
        if (-1 != sourceid)
        {
            query.bindValue(":SOURCEID", sourceid);
        }
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Deleting program genre entries from EIT."));
        if (!query.exec())
        {
            MythDB::DBError("Delete program genre entries from EIT", query);
            result = GENERIC_EXIT_NOT_OK;
        }
    }

    return result;
}

void registerEITUtils(UtilMap &utilMap)
{
    utilMap["cleareit"]             = &ClearEIT;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
