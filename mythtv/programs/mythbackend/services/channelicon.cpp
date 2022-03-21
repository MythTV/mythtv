//////////////////////////////////////////////////////////////////////////////
// Program Name: channelicon.cpp
// Created     : Jun. 22, 2014
//
// Copyright (c) 2014 The MythTV Developers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

// Qt
#include <QList>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythdownloadmanager.h"

// MythBackend
#include "channelicon.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ChannelIconList* ChannelIcon::LookupChannelIcon ( const QString    &Query,
                                                       const QString    &FieldName )
{
    DTC::ChannelIconList *pChannelIcons = new DTC::ChannelIconList();
    LOG(VB_GENERAL, LOG_ERR,
            QString("ChannelIcon::LookupChannelIcon - Unexpected FieldName '%1'").arg(FieldName));
    // check the FieldName is valid
    if (FieldName != "callsign" || FieldName != "xmltv")
    {
        //throw( QString("FieldName appears invalid."));
        return pChannelIcons;
    }

    // query http://services.mythtv.org/channel-icon/lookup
    QByteArray data;
    QString lookupUrl = QString("http://services.mythtv.org/channel-icon/lookup?%1=%2").arg(FieldName).arg(Query);
    if (!GetMythDownloadManager()->download(lookupUrl, &data))
    {
        //throw( QString("Download from services.mythtv.org failed."));
        return pChannelIcons;
    }

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    QString response = QString(data.constData());
    QStringList lines = response.split('\n');

    for (int x = 0; x < lines.count(); x++)
    {
        QString line = lines.at(x);
        QStringList fields = line.split(',');

        if (fields.size() >= 4)
        {
            QString id = fields.at(2);
            QString name = fields.at(3);
            QString url = fields.at(1);

            DTC::ChannelIcon *pChannelIcon = pChannelIcons->AddNewChannelIcon();
            pChannelIcon->setChannelIconId(id.toUInt());
            pChannelIcon->setIconName(name);
            pChannelIcon->setURL(url);
        }
    }

    return pChannelIcons;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////


DTC::ChannelIconList* ChannelIcon::SearchChannelIcon ( const QString &Query )
{
    DTC::ChannelIconList *pChannelIcons = new DTC::ChannelIconList();

    // query http://services.mythtv.org/channel-icon/search
    QByteArray data;
    QString searchUrl = QString("http://services.mythtv.org/channel-icon/search?s=%1").arg(Query);
    if (!GetMythDownloadManager()->download(searchUrl, &data))
        return pChannelIcons;

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    QString response = QString(data.constData());
    QStringList lines = response.split('\n');

    for (int x = 0; x < lines.count(); x++)
    {
        QString line = lines.at(x);
        QStringList fields = line.split(',');

        if (fields.size() >= 3)
        {
            QString id = fields.at(0);
            QString name = fields.at(1);
            QString url = fields.at(2);

            DTC::ChannelIcon *pChannelIcon = pChannelIcons->AddNewChannelIcon();
            pChannelIcon->setChannelIconId(id.toUInt());
            pChannelIcon->setIconName(name);
            pChannelIcon->setURL(url);
        }
    }

    return pChannelIcons;
}
