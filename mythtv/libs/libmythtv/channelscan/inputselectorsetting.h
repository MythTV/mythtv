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

#ifndef _INPUT_SELECTOR_SETTING_H_
#define _INPUT_SELECTOR_SETTING_H_

#include "settings.h"

class InputSelector : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    InputSelector(uint _default_cardid, const QString &_default_inputname);

    virtual void Load(void);

    uint GetCardID(void) const;

    QString GetInputName(void) const;

    static bool Parse(const QString &cardids_inputname,
                      uint          &cardid,
                      QString       &inputname);

  public slots:
    void SetSourceID(const QString &_sourceid);

  private:
    uint    sourceid;
    uint    default_cardid;
    QString default_inputname;
};

#endif // _INPUT_SELECTOR_SETTING_H_
