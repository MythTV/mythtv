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


class GlobalSetting: public SimpleDBStorage, virtual public Configurable
{

  public:

    GlobalSetting(QString name):
        SimpleDBStorage("settings", "data"){setName(name);};

  protected:

    virtual QString whereClause(void) { return QString("value = '%1' AND hostname = '%2'")
        .arg(getName()).arg(gContext->GetHostName());
        };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%2', hostname = '%3'")
                       .arg(getName()).arg(getValue())
                       .arg(gContext->GetHostName());
    };
};


// The real work.

// Temporary dumping ground for things that have not been properly categorized yet
class MythPhoneSettings: virtual public ConfigurationWizard {
public:
    MythPhoneSettings();
};

#endif
