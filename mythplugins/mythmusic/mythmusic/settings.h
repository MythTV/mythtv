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
#include <string>
#include <map>

using namespace std;

/**This class contains configuration information.  
  *This object is threadsafe.
  *@author Sean Ward
  */

class Settings {
public: 
	Settings(string strSettingFile = "settings.txt");
	~Settings();
	
	// Setting retrieval functions
	/** Generic Setting Retrieval functions */
	string GetSetting(string strSetting);
	/** Generic Setting Retrieval function for numeric values */
	int GetNumSetting(string strSetting);
	/** Generic Setting Retrieval function for float values */
	float GetFloatSetting(string strSetting);
	/** Generic Setting Retrieval functions for pointers */
	void* GetPointer(string strSetting);
	
	// Setting Setting functions
	/** Generic Setting Setting function */
	void SetSetting(string strSetting, string strNewVal);
	/** Generic Setting Setting function for int values */
	void SetSetting(string strSetting, int nNewVal);
	/** Generic Setting Setting function for float values */
	void SetSetting(string strSetting, float fNewVal);
	/** Generic Setting Setting function for pointer values */
	void SetSetting(string strSetting, void* pNewVal);

        /** parse settings file */
        int ReadSettings(const char *pszFile);
private: // Private attributes
	/** main property-value mapping for strings */
	map<string, string>* m_pStringSettings;
	/** main property-value mapping for ints */
	map<string, int>* m_pIntSettings;
	/** main property-value mapping for floats */
	map<string, float>* m_pFloatSettings;
	/** main property-value mapping for pointers */
	map<string, void*>* m_pVoidSettings;
};

#endif
