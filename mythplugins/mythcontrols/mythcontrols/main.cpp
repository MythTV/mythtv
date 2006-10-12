/** -*- Mode: c++ -*-
 * @file main.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief This file contains the plug-in glue functions
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */
#ifndef MAIN_CPP
#define MAIN_CPP

// MythTV headers
#include <mythtv/mythcontext.h> // for MYTH_BINARY_VERSION
#include <mythtv/mythdialogs.h>

// MythControls headers
#include "mythcontrols.h"

/** \brief Initializes the plugin.
 *  \param libversion The mythtv library version.
 *  \return 0 if all is well, otherwise -1
 */
extern "C" int mythplugin_init(const char *libversion)
{
    bool ok = gContext->TestPopupVersion(
        "mythcontrols", libversion, MYTH_BINARY_VERSION);

    return (ok) ? 0 : -1;
}

/** \brief Runs the plugin.
 *  \return 0 if all is well, otherwise -1
 */
extern "C" int mythplugin_run(void)
{
    bool ok = true;
    MythControls controls(gContext->GetMainWindow(), ok);

    if (ok)
    {
        controls.exec();
        return 0;    
    }

    MythPopupBox::showOkPopup(
        gContext->GetMainWindow(), QObject::tr("Theme Error"),
        QObject::tr("Could not load the keybinding plugin's theme. "
                    "See console for details."));

    return -1;
}

/// \brief Plug-in config handler, does nothing.
extern "C" int mythplugin_config(void)
{
    return 0;
}

#endif /* MAIN_CPP */
