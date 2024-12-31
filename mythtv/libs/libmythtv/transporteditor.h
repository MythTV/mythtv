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

#ifndef TRANSPORT_EDITOR_H
#define TRANSPORT_EDITOR_H

#include <QObject>

#include "libmythui/standardsettings.h"

#include "mythtvexp.h"
#include "cardutil.h"

class VideoSourceShow;
class MultiplexID;

/*
 *  Objects added for Transport Editing section
 */
class TransportSetting : public GroupSetting
{
    Q_OBJECT

  public:
    TransportSetting(const QString &label, uint mplexid, uint sourceid,
                     CardUtil::INPUT_TYPES cardtype);

    bool keyPressEvent(QKeyEvent *event) override; // StandardSetting

    uint getMplexId() const;

  signals:
    void deletePressed();
    void openMenu();

  private:
    MultiplexID *m_mplexid {nullptr};
};

// Page for selecting a transport to be created/edited
class MTV_PUBLIC TransportListEditor : public GroupSetting
{
    Q_OBJECT

  public:
    explicit TransportListEditor(uint initial_sourceid);
    void Load(void) override; // StandardSetting

    void SetSourceID(uint sourceid);

  public slots:
    void SetSourceID(const QString &name);
    void Menu(TransportSetting *transport);
    void NewTransport(void);

  private:
    ~TransportListEditor() override = default;
    void Delete(TransportSetting *transport);

  private:
    VideoSourceShow *m_videosource {nullptr};
    QVector<StandardSetting*> m_list;
    uint m_sourceid  {0};
    CardUtil::INPUT_TYPES m_cardtype  {CardUtil::INPUT_TYPES::ERROR_PROBE};
    bool m_isLoading {false};
};

#endif // TRANSPORT_EDITOR_H
