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

#include "inputselectorsetting.h"

#include <utility>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"

#include "cardutil.h"

InputSelector::InputSelector(uint default_cardid,
                             QString default_inputname) :
    m_defaultCardId(default_cardid),
    m_defaultInputName(std::move(default_inputname))
{
    setLabel(tr("Input"));
    setHelpText(
        QObject::tr(
            "Select a capture card from the capture cards that are "
            "connected to the currently selected video source."
            ));
}

void InputSelector::Load(void)
{
    clearSelections();

    if (!m_sourceId)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid, cardtype, videodevice, inputname "
        "FROM capturecard, videosource "
        "WHERE capturecard.sourceid = videosource.sourceid AND "
        "      hostname             = :HOSTNAME            AND "
        "      capturecard.sourceid = :SOURCEID            AND "
        "      capturecard.parentid = 0");

    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    query.bindValue(":SOURCEID", m_sourceId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("InputSelector::load()", query);
        return;
    }

    uint which = 0;
    uint cnt = 0;
    for (; query.next(); ++cnt)
    {
        uint    cardid     = query.value(0).toUInt();
        QString inputname  = query.value(3).toString();

        QString desc = CardUtil::GetDeviceLabel(
            query.value(1).toString(), query.value(2).toString());

        desc += QString(" (%1)").arg(inputname);

        QString key = QString("%1:%2").arg(cardid).arg(inputname);

        addSelection(desc, key);

        which = (m_defaultCardId == cardid) ? cnt : which;
    }

    if (cnt)
        setValue(which);
}

void InputSelector::SetSourceID(const QString &sourceid)
{
    if (m_sourceId != sourceid.toUInt())
    {
        m_sourceId = sourceid.toUInt();
        Load();
    }
}

uint InputSelector::GetCardID(void) const
{
    uint    cardid    = 0;
    QString inputname;

    Parse(getValue(), cardid, inputname);

    return cardid;
}

QString InputSelector::GetInputName(void) const
{
    uint    cardid    = 0;
    QString inputname;

    Parse(getValue(), cardid, inputname);

    return inputname;
}

bool InputSelector::Parse(const QString &cardid_inputname,
                          uint          &cardid,
                          QString       &inputname)
{
    cardid    = 0;
    inputname.clear();

    int sep0 = cardid_inputname.indexOf(':');
    if (sep0 < 1)
        return false;

    cardid    = cardid_inputname.left(sep0).toUInt();
    inputname = cardid_inputname.mid(sep0 + 1);

    return true;
}
