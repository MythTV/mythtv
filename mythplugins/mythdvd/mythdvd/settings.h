/*
	settings.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	
	Part of the mythTV project
	
	headers for settings for the mythDVD module
*/

#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class DVDGeneralSettings :  public ConfigurationWizard
{
  public:
    DVDGeneralSettings();
};

class DVDPlayerSettings :  public ConfigurationWizard
{
  public:
    DVDPlayerSettings();
};

class DVDRipperSettings :  public ConfigurationWizard
{
  public:
    DVDRipperSettings();
};

#endif
