/*
	PhoneSettings.h

	(c) 2003 Paul Volkaerts
	
	Part of the mythTV project
	
	headers for settings for the mythPhone module
*/

#ifndef PHONESETTINGS_H
#define PHONESETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"



// The real work.

// Temporary dumping ground for things that have not been properly categorized yet
class MythPhoneSettings: virtual public ConfigurationWizard {
public:
    MythPhoneSettings();
};

#endif
