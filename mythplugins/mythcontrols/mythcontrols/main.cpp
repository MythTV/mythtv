/* -*- myth -*- */
/**
 * @file main.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief This is the main file.
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

using namespace std;

#include "mythcontrols.h"
#include <mythtv/mythdialogs.h>


/**
 * @brief Initializes the plugin.
 * @param libversion The mythtv library version.
 * @return zero if all is well, otherwise, less than zero
 */
extern "C" int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythcontrols", libversion,
                                    MYTH_BINARY_VERSION)) return -1;
    else return 0;
}



/**
 * @brief Runs the plugin.
 * @return zero if all is well, otherwise, less than zero
 */
extern "C" int mythplugin_run (void)
{
    bool uiloaded;
    MythMainWindow *window = gContext->GetMainWindow();

    /* create the keybinding plugin.  uiloaded will be set to false
     * if the theme was not correct */
    MythControls controls(window, uiloaded);

    /* if the UI is successfully loaded, just giv'er*/
    if (uiloaded)
    {
        controls.exec();
        return 0;    
    }
    else
    {
        MythPopupBox::showOkPopup(window, "Theme Error", "Could not load the "
                                  "keybinding plugin's theme.  Check the "
                                  "console for detailed output.");

        return -1;
    }
}



/**
 * @brief Configures the plugin.
 * @return zero.
 */
extern "C" int mythplugin_config (void) { return 0; }



#endif /* MAIN_CPP */
