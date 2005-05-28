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

/* C++ includes */
#include <iostream>

/* MythTV includes */
#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/themedmenu.h>

using namespace std;

#include "mythcontrols.h"

extern "C" {
  int mythplugin_init(const char *libversion);
  int mythplugin_run(void);
  int mythplugin_config(void);
}


/**
 * @brief Initialize the MythControls plugin
 * @param libversion The library version, I assume.
 * @return zero.
 */
int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythcontrols", libversion,
				    MYTH_BINARY_VERSION))
    {

	cout << "Returning" << endl;
	return -1;
    }

    return 0;
}



/**
 * @brief Run the MythControls plugin.
 * @return zero.
 */
int mythplugin_run (void)
{
    MythControls controls(gContext->GetMainWindow(), "controls");

    cout << "Executing" << endl;
    controls.exec();
    return 0;    
}



/**
 * @brief Configure the MythNews plugin.
 * @return zero.
 */
int mythplugin_config (void)
{
    cout << "Configuring MythControls" << endl;

    return 0;
}



#endif /* MAIN_CPP */
