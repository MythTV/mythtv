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
    int g_context_value = gContext->GetNumSetting(key, defaultval);
    
    if(key == "mfd_main_port" && g_context_value == 0)
    {
        g_context_value = MFD_MAIN_PORT;  
    }
    else if(key == "mfd_audio_port" && g_context_value == 0)
    {
        g_context_value = MFD_AUDIO_PORT;  
    }
    else if(key == "mfd_metadata_port" && g_context_value == 0)
    {
        g_context_value = MFD_METADATA_PORT;  
    }
    else if(key == "mfd_httpufpi_port" && g_context_value == 0)
    {
        g_context_value = MFD_HTTPUFPI_PORT;  
    }
    else if(key == "mfd_audiortsp_port" && g_context_value == 0)
    {
        g_context_value = MFD_AUDIORTSP_PORT;  
    }
    else if(key == "mfd_audiortspcontrol_port" && g_context_value == 0)
    {
        g_context_value = MFD_AUDIORTSPCONTROL_PORT;  
    }
    else if(key == "mfd_speakercontrol_port" && g_context_value == 0)
    {
        g_context_value = MFD_SPEAKERCONTROL_PORT;  
    }
    else if(key == "mfd_transcoder_port" && g_context_value == 0)
    {
        g_context_value = MFD_TRANSCODER_PORT;
    }
    else if(key == "mfd_daapserver_port" && g_context_value == 0)
    {
        g_context_value = MFD_DAAPSERVER_PORT;
    }
    
    return g_context_value;
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
