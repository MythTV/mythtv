/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *      Taylor Jacob (rtjacob@earthlink.net)
 *
 * Description:
 *     Collection of classes to provide dvb a transport editor
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

#ifndef _TRANSPORT_EDITOR_H_
#define _TRANSPORT_EDITOR_H_

#include <QObject>

#include "mythtvexp.h"
#include "settings.h"

class VideoSourceSelector;
class MultiplexID;

/*
 *  Objects added for Transport Editing section
 */

class TransportList : public ListBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    TransportList() : ListBoxSetting(this), sourceid(0), cardtype(0) { }

    virtual void Load(void) { fillSelections(); }
    virtual void fillSelections(void);

    void SetSourceID(uint _sourceid);

  public slots:
    void SetSourceID(const QString &_sourceid)
        { SetSourceID(_sourceid.toUInt()); }

  private:
    ~TransportList() { }

  private:
    uint sourceid;
    uint cardtype;
};

// Page for selecting a transport to be created/edited
class MTV_PUBLIC TransportListEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    TransportListEditor(uint initial_sourceid);

    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }

  public slots:
    void Menu(void);
    void Delete(void);
    void Edit(void);

  private:
    ~TransportListEditor() { }

  private:
    VideoSourceSelector *m_videosource;
    TransportList       *m_list;
};

#endif // _TRANSPORT_EDITOR_H_
