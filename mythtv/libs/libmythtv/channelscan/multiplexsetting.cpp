/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
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

#include "multiplexsetting.h"
#include "frequencies.h"

void MultiplexSetting::Load(void)
{
    clearSelections();

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT mplexid,   networkid,  transportid, "
        "       frequency, symbolrate, modulation "
        "FROM dtv_multiplex "
        "WHERE sourceid = :SOURCEID "
        "ORDER by frequency, networkid, transportid");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive() || query.size() <= 0)
        return;

    while (query.next())
    {
        QString DisplayText;
        if (query.value(5).toString() == "8vsb")
        {
            QString ChannelNumber =
                QString("Freq %1").arg(query.value(3).toInt());
            struct CHANLIST* curList = chanlists[0].list;
            int totalChannels = chanlists[0].count;
            int findFrequency = (query.value(3).toInt() / 1000) - 1750;
            for (int x = 0 ; x < totalChannels ; x++)
            {
                if ((curList[x].freq <= findFrequency + 200) &&
                    (curList[x].freq >= findFrequency - 200))
                {
                    ChannelNumber = QString("%1").arg(curList[x].name);
                }
            }

            //: %1 is the channel number
            DisplayText = tr("ATSC Channel %1").arg(ChannelNumber);
        }
        else
        {
            DisplayText = QString("%1 Hz (%2) (%3) (%4)")
                .arg(query.value(3).toString())
                .arg(query.value(4).toString())
                .arg(query.value(1).toInt())
                .arg(query.value(2).toInt());
        }
        addSelection(DisplayText, query.value(0).toString());
    }
}

void MultiplexSetting::SetSourceID(uint _sourceid)
{
    sourceid = _sourceid;
    Load();
}
