/***************************************************************************
                          settings.h  -  description
                             -------------------
    begin                : Tuesday, October 03, 2000
    copyright            : (C) 2000 by Relatable, LLC
    written by           : Sean Ward
    email                : sward@relatable.com
 ***************************************************************************/

#ifndef OLDSETTINGS_H
#define OLDSETTINGS_H

#include <map>
using namespace std;

#include <QString>
#include <QPixmap>

#include "mythexp.h"

/**This class contains configuration information.
  *This object is threadsafe.
  *@author Sean Ward
  */

class QPixmap;
class MPUBLIC Settings {
public:
        Settings(QString strSettingFile = "settings.txt");
        ~Settings();

        // Setting retrieval functions
        /** Generic Setting Retrieval functions */
        QString GetSetting(QString strSetting, QString defaultvalue = "");
        /** Generic Setting Retrieval function for numeric values */
        int GetNumSetting(QString strSetting, int defaultvalue = 0);
        /** Generic Setting Retrieval function for float values */
        float GetFloatSetting(QString strSetting, float defaultvalue = 0);

        // Setting Setting functions
        /** Generic Setting Setting function */
        void SetSetting(QString strSetting, QString strNewVal);
        /** Generic Setting Setting function for int values */
        void SetSetting(QString strSetting, int nNewVal);
        /** Generic Setting Setting function for float values */
        void SetSetting(QString strSetting, float fNewVal);

        bool LoadSettingsFiles(QString filename, QString prefix, QString confdir);
        /** parse settings file */
        bool ReadSettings(QString pszFile);
private: // Private attributes
        /** main property-value mapping for strings */
        std::map<QString, QString> *m_pSettings;
};

void LoadSettingsFile(Settings *settings, QString filename);

#endif
