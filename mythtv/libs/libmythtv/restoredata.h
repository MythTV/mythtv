/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 *
 * Description:
 *     Postprocessing of channelscan result
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

#ifndef RESTOREDATA_H
#define RESTOREDATA_H

#include <vector>

#include "mythuigroup.h"
#include "mythtvexp.h"
#include "standardsettings.h"
#include "cardutil.h"
#include "xmlparsebase.h"
#include "videosource.h"

class VideoSourceShow;
class MythUIType;
class RestoreXMLTVID;
class RestoreVisible;
class RestoreIcon;

struct OldChannelData
{
    uint chanid        {0};
    uint sourceid      {0};
    uint serviceid     {0};
    uint transportid   {0};
    uint networkid     {0};
    QString channum;
    QString name;
    QString xmltvid;
    QString icon;
    int visible        {0};
    QString deleted;
    bool found_xmltvid {false};
    bool found_icon    {false};
    bool found_visible {false};
};

// Page for updating the fields with non-scanned data
// from deleted channel data.
//
class MTV_PUBLIC RestoreData : public GroupSetting
{
  Q_OBJECT
  public:
    explicit RestoreData(uint sourceid);
    void Load(void) override; // StandardSetting
    void Save(void) override; // StandardSetting

  public slots:
    void Restore(void);

  private:
    VideoSourceShow       *m_videosource      {nullptr};
    RestoreXMLTVID        *m_restoreXMLTVID   {nullptr};
    RestoreVisible        *m_restoreVisible   {nullptr};
    RestoreIcon           *m_restoreIcon      {nullptr};

    uint m_sourceid {0};

    std::vector<OldChannelData> m_ocd;
};

#endif // RESTOREDATA_H
