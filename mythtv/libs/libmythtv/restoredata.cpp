/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Description:
 *     Restore data from deleted channels after scan for new channels
 *
 * When all channels are deleted and a new scan is done then
 * all the non-scanned data is lost.
 * This are the following fields:
 *     xmltvid    Channel identification for XMLTV including Schedules Direct
 *     iconpath   File name of icon for this channel
 *     visible    Visible status
 * When a channel is deleted it is not immediately deleted from the database;
 * it is kept for a while with the "deleted" field set to the date at which it
 * is deleted.
 * This makes it possible to retrieve this data from the deleted records
 * when the same channel is found again in a new scan.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// C++ includes
#include <vector>
#include <iostream>

// MythTV includes
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuitext.h"
#include "libmythui/themeinfo.h"

#include "restoredata.h"
#include "sourceutil.h"
#include "videosource.h"

#define LOC QString("RestoreData: ")

class RestoreXMLTVID : public TransMythUICheckBoxSetting
{
  public:
    RestoreXMLTVID()
    {
        setLabel(QObject::tr("Restore XMLTV ID"));
        setHelpText(
            QObject::tr(
                "If checked, copy the XMLTV ID in field \"xmltvid\" "
                "from a deleted channel "
                "or from a channel in another video source."));
        setValue(false);
    };
};

class RestoreVisible : public TransMythUICheckBoxSetting
{
  public:
    RestoreVisible()
    {
        setLabel(QObject::tr("Restore Visible status"));
        setHelpText(
            QObject::tr(
                "If checked, copy the Visible status in field \"visible\" "
                "from a deleted channel in this video source."));
        setValue(false);
    };
};

class RestoreIcon : public TransMythUICheckBoxSetting
{
  public:
    RestoreIcon()
    {
        setLabel(QObject::tr("Restore Icon filename"));
        setHelpText(
            QObject::tr(
                "If checked, copy the Icon filename in field \"icon\" "
                "from a deleted channel "
                "or from a channel in another video source."));
        setValue(false);
    };
};

RestoreData *RestoreData::s_Instance = nullptr;

RestoreData *RestoreData::getInstance(uint sourceid)
{
    if (s_Instance != nullptr)
    {
        if (sourceid != s_Instance->m_sourceid)
        {
            delete s_Instance;
            s_Instance = nullptr;
        }
    }
    if (s_Instance == nullptr)
        s_Instance = new RestoreData(sourceid, false);
    return s_Instance;
}

void RestoreData::freeInstance()
{
    if (s_Instance != nullptr)
    {
        delete s_Instance;
        s_Instance = nullptr;
    }
}

RestoreData::RestoreData(uint sourceid, bool useGUI) :
    m_sourceid(sourceid), m_useGUI(useGUI)
{
    if (!m_useGUI)
        return;
    setLabel(tr("Restore Data"));

    m_videosource = new VideoSourceShow(m_sourceid);
    m_videosource->setHelpText(
        QObject::tr(
            "The video source is selected in the Channel Editor page. "
            "Searching for non-scanned data is done for all channels in this video source."
            ));
    addChild(m_videosource);

    m_restoreXMLTVID = new RestoreXMLTVID();
    addChild(m_restoreXMLTVID);

    m_restoreVisible = new RestoreVisible();
    addChild(m_restoreVisible);

    m_restoreIcon = new RestoreIcon();
    addChild(m_restoreIcon);

    auto *newTransport = new ButtonStandardSetting(tr("Search"));
    newTransport->setHelpText(
        QObject::tr(
            "Start searching for non-scanned data. The data is written to the database "
            "when \'Save and Exit\' is selected in the \'Exit Settings?\' dialog box."
            ));
    addChild(newTransport);
    connect(newTransport, &ButtonStandardSetting::clicked, this, &RestoreData::Restore);
}

void RestoreData::Restore()
{
    if (!m_useGUI)
        return;
    bool do_xmltvid = m_restoreXMLTVID->boolValue();
    bool do_icon    = m_restoreIcon->boolValue();
    bool do_visible = m_restoreVisible->boolValue();
    doRestore(do_xmltvid, do_icon, do_visible);
}

QString RestoreData::doRestore(bool do_xmltvid, bool do_icon, bool do_visible)
{
    if (do_xmltvid || do_icon || do_visible)
    {
        QString msg = QString("Restore data from deleted channels for fields ");
        if (do_xmltvid)
        {
            msg += "xmltvid ";
        }
        if (do_icon)
        {
            msg += "icon ";
        }
        if (do_visible)
        {
            msg += "visible ";
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    }

    // Old Channel Data
    m_ocd.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    // For all channels in this videosource
    query.prepare(
        "SELECT chanid, channum, name, serviceid, transportid, networkid, "
        "       channel.xmltvid, "
        "       channel.icon, "
        "       channel.visible "
        "FROM channel, dtv_multiplex "
        "WHERE channel.sourceid = :SOURCEID "
        "  AND channel.mplexid = dtv_multiplex.mplexid "
        "  AND deleted IS NULL ");
    query.bindValue(":SOURCEID", m_sourceid);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("RestoreData::Restore(1)", query);
        return {};
    }
    while (query.next())
    {
        OldChannelData cd;
        cd.chanid      = query.value(0).toUInt();
        cd.channum     = query.value(1).toString();
        cd.name        = query.value(2).toString();
        cd.serviceid   = query.value(3).toUInt();
        cd.transportid = query.value(4).toUInt();
        cd.networkid   = query.value(5).toUInt();
        cd.xmltvid     = query.value(6).toString();
        cd.icon        = query.value(7).toString();
        cd.visible     = query.value(8).toInt();

        bool nullNetwork = query.isNull(5);
        cd.found_xmltvid = false;
        cd.found_icon    = false;
        cd.found_visible = false;

        QString networkCheck;
        if (nullNetwork)
            networkCheck = " networkid IS NULL";
        else
            networkCheck = " networkid = :NETWORKID ";

        // Get xmltvid from the last deleted channel
        // from any video source or from a channel
        // on a different video source
        if (do_xmltvid && cd.xmltvid.isEmpty())
        {
            query2.prepare(
                "SELECT xmltvid "
                "FROM channel, dtv_multiplex "
                "WHERE serviceid   = :SERVICEID "
                "  AND transportid = :TRANSPORTID "
                "  AND " + networkCheck +
                "  AND channel.mplexid = dtv_multiplex.mplexid "
                "  AND xmltvid != ''"
                "  AND (deleted IS NOT NULL OR "
                "       channel.sourceid != :SOURCEID)"
                "ORDER BY deleted DESC;");
            query2.bindValue(":SERVICEID",   cd.serviceid);
            query2.bindValue(":TRANSPORTID", cd.transportid);
            if (!nullNetwork)
                query2.bindValue(":NETWORKID",   cd.networkid);
            query2.bindValue(":SOURCEID",    m_sourceid);
            if (!query2.exec() || !query2.isActive())
            {
                MythDB::DBError("RestoreData::Restore(2)", query);
                return {};
            }
            if (query2.next())
            {
                cd.xmltvid = query2.value(0).toString();
                cd.found_xmltvid = true;
            }
        }

        // Get icon from the last deleted channel
        // from any video source or from a channel
        // on a different video source
        if (do_icon && cd.icon.isEmpty())
        {
            query2.prepare(
                "SELECT icon "
                "FROM channel, dtv_multiplex "
                "WHERE serviceid   = :SERVICEID "
                "  AND transportid = :TRANSPORTID "
                "  AND channel.mplexid = dtv_multiplex.mplexid "
                "  AND icon != ''"
                "  AND (deleted IS NOT NULL OR "
                "       channel.sourceid != :SOURCEID)"
                "ORDER BY deleted DESC;");
            query2.bindValue(":SERVICEID",   cd.serviceid);
            query2.bindValue(":TRANSPORTID", cd.transportid);
            if (!nullNetwork)
                query2.bindValue(":NETWORKID",   cd.networkid);
            query2.bindValue(":SOURCEID",    m_sourceid);
            if (!query2.exec() || !query2.isActive())
            {
                MythDB::DBError("RestoreData::Restore(3)", query);
                return {};
            }
            if (query2.next())
            {
                cd.icon = query2.value(0).toString();
                cd.found_icon = true;
            }
        }

        // Get visible from the last deleted channel
        // but only from the same video source
        if (do_visible)
        {
            query2.prepare(
                "SELECT channel.visible "
                "FROM channel, dtv_multiplex "
                "WHERE serviceid   = :SERVICEID "
                "  AND transportid = :TRANSPORTID "
                "  AND " + networkCheck +
                "  AND channel.sourceid = :SOURCEID "
                "  AND channel.mplexid  = dtv_multiplex.mplexid "
                "  AND deleted IS NOT NULL "
                "ORDER BY deleted DESC;");
            query2.bindValue(":SERVICEID",   cd.serviceid);
            query2.bindValue(":TRANSPORTID", cd.transportid);
            if (!nullNetwork)
                query2.bindValue(":NETWORKID",   cd.networkid);
            query2.bindValue(":SOURCEID",    m_sourceid);
            if (!query2.exec() || !query2.isActive())
            {
                MythDB::DBError("RestoreData::Restore(4)", query);
                return {};
            }
            if (query2.next())
            {
                int visible = query2.value(0).toInt();
                if (visible != cd.visible)
                {
                    cd.visible = visible;
                    cd.found_visible = true;
                }
            }
        }

        if (cd.found_xmltvid || cd.found_icon || cd.found_visible)
        {
            m_ocd.push_back(cd);
        }
    }

    // Header line for log of all changes
    if (m_ocd.empty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No data found in deleted channels or no data needed");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Restoring data in %1 channels from deleted channels:")
                .arg(m_ocd.size()));
    }

    // Log of all channels that will be updated
    m_num_xmltvid = 0;
    m_num_icon    = 0;
    m_num_visible = 0;
    for (auto & cd : m_ocd)
    {
        QString msg = QString("Channel %1 \'%2\' update ").arg(cd.channum, cd.name);
        if (cd.found_xmltvid)
        {
            msg += QString("xmltvid(%1) ").arg(cd.xmltvid);
            m_num_xmltvid++;
        }
        if (cd.found_icon)
        {
            msg += QString("icon(%1) ").arg(cd.icon);
            m_num_icon++;
        }
        if (cd.found_visible)
        {
            msg += QString("visible(%1) ").arg(cd.visible);
            m_num_visible++;
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    }
    m_num_channels = m_ocd.size();

    // Show totals of what has been found.
    {
        QString msg;
        if (m_ocd.empty())
        {
            msg = "No data found";
        }
        else
        {
            msg = QString("Found data for %1 channels\n").arg(m_ocd.size());
            if (m_num_xmltvid > 0)
            {
                msg += QString("xmltvid: %1  ").arg(m_num_xmltvid);
            }
            if (m_num_icon > 0)
            {
                msg += QString("icon: %1  ").arg(m_num_icon);
            }
            if (m_num_visible > 0)
            {
                msg += QString("visible: %1").arg(m_num_visible);
            }
        }
        if (m_useGUI)
            WaitFor(ShowOkPopup(msg));
        return msg;
    }

}

// Load value of selected video source (for display only)
void RestoreData::Load(void)
{
    GroupSetting::Load();
}

// Do the actual updating if "Save and Exit" is selected in the "Exit Settings?" dialog.
void RestoreData::Save(void)
{
    doSave();
}

bool RestoreData::doSave(void)
{
    if (m_ocd.empty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("No data to Restore"));
        return false;
    }
    MSqlQuery query(MSqlQuery::InitCon());
    for (auto & cd : m_ocd)
    {
        query.prepare(
            "UPDATE channel "
            "SET icon    = :ICON, "
            "    xmltvid = :XMLTVID, "
            "    visible = :VISIBLE "
            "WHERE chanid = :CHANID");
        query.bindValue(":ICON",    cd.icon);
        query.bindValue(":XMLTVID", cd.xmltvid);
        query.bindValue(":VISIBLE", cd.visible);
        query.bindValue(":CHANID",  cd.chanid);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("RestoreData::Restore(5)", query);
            return false;
        }
    }
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Restored data for %1 channels").arg(m_ocd.size()));
    return true;
}
