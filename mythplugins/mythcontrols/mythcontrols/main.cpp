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



/**
 * @name Plugin API
 *
 * The following methods are required by all MythTV plugins.
 */
//@{
extern "C" {

  /**
   * @brief Called by mythtv to initialize the plugin.
   * @param libversion Used to make sure that the requried version of
   * the library is being used.
   * @return I dont really know what difference your return values makes.
   * @todo Find out the difference this return values makes.
   */
  int mythplugin_init(const char *libversion);

  /**
   * @brief Runs the plugin.
   * @return I dont really know what difference your return values makes.
   * @todo Find out the difference this return values makes.
   */
  int mythplugin_run(void);

  /**
   * @brief Called to configure the plugin.
   * @return I dont really know what difference your return values makes.
   * @todo Find out the difference this return values makes.
   */
  int mythplugin_config(void);
}
//@}


/* documented above */
int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythcontrols", libversion,
				    MYTH_BINARY_VERSION)) return -1;
    else return 0;
}



/* documented above */
int mythplugin_run (void)
{
    MythControls controls(gContext->GetMainWindow(), "controls");

    /* if the UI is successfully loaded, just giv'er*/
    if (controls.loadUI())
    {
	controls.exec();
	return 0;    
    }
    else
    {
	VERBOSE(VB_ALL, "Unable to load theme, exiting");
	return -1;
    }
}



/* documented above*/
int mythplugin_config (void) { return 0; }



#endif /* MAIN_CPP */
