/***************************************************************************
                          settings.h  -  description
                             -------------------
    begin                : Tuesday, October 03, 2000
    copyright            : (C) 2000 by Relatable, LLC
    written	by		     : Sean Ward
    email                : sward@relatable.com
 ***************************************************************************/

#ifndef SETTINGS_H
#define SETTINGS_H
#include <iostream>
#include <qstring.h>
#include <map>

using namespace std;

/**This class contains configuration information.  
  *This object is threadsafe.
  *@author Sean Ward
  */

class Settings {
public: 
	Settings(QString strSettingFile = "settings.txt");
	~Settings();
	
	// Setting retrieval functions
	/** Generic Setting Retrieval functions */
	QString GetSetting(QString strSetting);
	/** Generic Setting Retrieval function for numeric values */
	int GetNumSetting(QString strSetting);
	/** Generic Setting Retrieval function for float values */
	float GetFloatSetting(QString strSetting);
	/** Generic Setting Retrieval functions for pointers */
	void* GetPointer(QString strSetting);
	
	// Setting Setting functions
	/** Generic Setting Setting function */
	void SetSetting(QString strSetting, QString strNewVal);
	/** Generic Setting Setting function for int values */
	void SetSetting(QString strSetting, int nNewVal);
	/** Generic Setting Setting function for float values */
	void SetSetting(QString strSetting, float fNewVal);
	/** Generic Setting Setting function for pointer values */
	void SetSetting(QString strSetting, void* pNewVal);

        void LoadSettingsFiles(QString filename, QString prefix);
        /** parse settings file */
        int ReadSettings(QString pszFile);
private: // Private attributes
	/** main property-value mapping for strings */
	map<QString, QString>* m_pStringSettings;
	/** main property-value mapping for ints */
	map<QString, int>* m_pIntSettings;
	/** main property-value mapping for floats */
	map<QString, float>* m_pFloatSettings;
	/** main property-value mapping for pointers */
	map<QString, void*>* m_pVoidSettings;
};

void LoadSettingsFile(Settings *settings, QString filename);

#endif
