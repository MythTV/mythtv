/*
	settings.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A place to set settings

*/

#include "settings.h"

#ifdef MYTHLIB_SUPPORT 
    MythContext *gContext;
#endif


//
//  This is very straightforward. If we're linked with libmyth, then use
//  gContext for settings. If we aren't, well ... at the moment, hand back
//  the defaults.
//

Settings::Settings(const QString &binversion)
{
#ifdef MYTHLIB_SUPPORT 
    gContext = new MythContext(MYTH_BINARY_VERSION, false, false);
#endif
}

int Settings::openDatabase(QSqlDatabase *db)
{
#ifdef MYTHLIB_SUPPORT 
    return gContext->OpenDatabase(db);
#endif
    return 0;
}



QString Settings::GetSetting(const QString &key, const QString &defaultval)
{
#ifdef MYTHLIB_SUPPORT
    return gContext->GetSetting(key, defaultval);
#endif
    return defaultval;
}

QString Settings::getSetting(const QString &key, const QString &defaultval)
{
    return GetSetting(key, defaultval);
}



int Settings::GetNumSetting(const QString &key, int defaultval)
{
#ifdef MYTHLIB_SUPPORT
    return gContext->GetNumSetting(key, defaultval);
#endif
    return 3;
}

int Settings::getNumSetting(const QString &key, int defaultval)
{
    return GetNumSetting(key, defaultval);
}


QString Settings::GetHostName()
{
#ifdef MYTHLIB_SUPPORT
    return gContext->GetHostName();
#endif
    return "ihavenoidea";
}

QString Settings::getHostName()
{
    return GetHostName();
}


Settings::~Settings()
{
}
