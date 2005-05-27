#ifndef SETTINGS_H_
#define SETTINGS_H_
/*
	settings.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A place to set settings

*/

#include <qstring.h>
#include <qstringlist.h>

#include "../config.h"

#include <mythtv/mythcontext.h>

//
//  Default port assignments
//

#define MFD_MAIN_PORT               2342
#define MFD_AUDIO_PORT              2343
#define MFD_METADATA_PORT           2344
#define MFD_HTTPUFPI_PORT           2345
#define MFD_AUDIORTSP_PORT          2346
#define MFD_AUDIORTSPCONTROL_PORT   2347
#define MFD_SPEAKERCONTROL_PORT     2348
#define MFD_TRANSCODER_PORT         2349
#define MFD_DAAPSERVER_PORT         3689    // same as iTunes, totally arbitrary, can be changed




class Settings
{
  public:

    Settings();
    ~Settings();

    QString GetSetting(const QString &key, const QString &defaultval = "");
    QString getSetting(const QString &key, const QString &defaultval = "");

    int     GetNumSetting(const QString &key, int defaultval = 0);
    int     getNumSetting(const QString &key, int defaultval = 0);

    QStringList GetListSetting(const QString &key, const QStringList &defaultval = QStringList());
    QStringList getListSetting(const QString &key, const QStringList &defaultval = QStringList());

    QString GetHostName();
    QString getHostName();

  private:
  
    
};

extern Settings *mfdContext;

#endif  // settings_h_

