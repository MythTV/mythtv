/*
	settings.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A place to set settings

*/

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "settings.h"

MythContext *gContext;


//
//  Use libmyth's gContext for settings. 
//

Settings::Settings()
{
    gContext = new MythContext(MYTH_BINARY_VERSION);
    gContext->Init(false);
}

QString Settings::GetSetting(const QString &key, const QString &defaultval)
{
    return gContext->GetSetting(key, defaultval);
}

QString Settings::getSetting(const QString &key, const QString &defaultval)
{
    return GetSetting(key, defaultval);
}



int Settings::GetNumSetting(const QString &key, int defaultval)
{
    return gContext->GetNumSetting(key, defaultval);
}

int Settings::getNumSetting(const QString &key, int defaultval)
{
    return GetNumSetting(key, defaultval);
}


QStringList Settings::GetListSetting(const QString &key, const QStringList &defaultval)
{
    if(key){;}
    
    return defaultval;
}

QStringList Settings::getListSetting(const QString &key, const QStringList &defaultval)
{
    return GetListSetting(key, defaultval);
}


QString Settings::GetHostName()
{
    return gContext->GetHostName();
}

QString Settings::getHostName()
{
    return GetHostName();
}


Settings::~Settings()
{
}
