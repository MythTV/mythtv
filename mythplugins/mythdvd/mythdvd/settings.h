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



// The real work.

// Temporary dumping ground for things that have not been properly categorized yet
class DVDGeneralSettings: virtual public ConfigurationWizard {
public:
    DVDGeneralSettings();
};

class DVDPlayerSettings: virtual public ConfigurationWizard {
public:
    DVDPlayerSettings();
};

class DVDRipperSettings: virtual public ConfigurationWizard {
public:
    DVDRipperSettings();
};

#endif
