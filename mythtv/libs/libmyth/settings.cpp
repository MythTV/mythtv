
/***************************************************************************
                          settings.cpp  -  description
                             -------------------
    begin                : Tuesday, October 03, 2000
    copyright            : (C) 2000 by Relatable, LLC
    written	by		     : Sean Ward
    email                : sward@relatable.com
 ***************************************************************************/


#include "settings.h"
#include <fstream.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

Settings::Settings(string strSettingsFile)
{
	if (strSettingsFile.empty())
	{
		strSettingsFile = "settings.txt";
	}
	m_pStringSettings = new map<string, string>;
	m_pIntSettings = new map<string, int>;
	m_pFloatSettings = new map<string, float>;
	m_pVoidSettings = new map<string, void*>;
	
	this->ReadSettings(strSettingsFile.c_str());	// load configuration values from settings.txt in this dir	
}

Settings::~Settings()
{
	if (m_pStringSettings != NULL)
	{
		delete m_pStringSettings;
	}
	
	if (m_pIntSettings != NULL)
	{
		delete m_pIntSettings;
	}

	if (m_pFloatSettings != NULL)
	{
		delete m_pFloatSettings;
	}

	if (m_pVoidSettings != NULL)
	{
		delete m_pVoidSettings;
	}
}
		
// Setting retrieval functions
/** Generic Setting Retrieval functions */
string Settings::GetSetting(string strSetting)
{
	map<string, string>::iterator i;
	if ((!m_pStringSettings->empty()) && ((i = m_pStringSettings->find(strSetting)) != m_pStringSettings->end()))
	{
		return (*i).second;
	}
	return ""; // property doesn't exist
}

/** Generic Setting Retrieval function for numeric values */
int Settings::GetNumSetting(string strSetting)
{
	map<string, int>::iterator i;
	if ((!m_pIntSettings->empty()) && ((i = m_pIntSettings->find(strSetting)) != m_pIntSettings->end()))
	{
		return (*i).second;
	}
	return 0; // property doesn't exist
}

/** Generic Setting Retrieval function for float values */
float Settings::GetFloatSetting(string strSetting)
{
	map<string, float>::iterator i;
	if ((!m_pFloatSettings->empty()) && ((i = m_pFloatSettings->find(strSetting)) != m_pFloatSettings->end()))
	{
		return (*i).second;
	}
	return 0; // property doesn't exist
}

/** Generic Setting Retrieval functions for pointers */
void* Settings::GetPointer(string strSetting)
{
	map<string, void*>::iterator i;
	if ((!m_pVoidSettings->empty()) && ((i = m_pVoidSettings->find(strSetting)) != m_pVoidSettings->end()))
	{
		return (*i).second;
	}
	return NULL; // property doesn't exist
}

// Setting Setting functions
/** Generic Setting Setting function */
void Settings::SetSetting(string strSetting, string strNewVal)
{
	(*m_pStringSettings)[strSetting] = strNewVal;
}

/** Generic Setting Setting function for int values */
void Settings::SetSetting(string strSetting, int nNewVal)
{
	(*m_pIntSettings)[strSetting] = nNewVal;
}

/** Generic Setting Setting function for float values */
void Settings::SetSetting(string strSetting, float fNewVal)
{
	(*m_pFloatSettings)[strSetting] = fNewVal;
}

/** Generic Setting Setting function for pointer values */
void Settings::SetSetting(string strSetting, void* pNewVal)
{
	(*m_pVoidSettings)[strSetting] = pNewVal;
}

int Settings::ReadSettings(const char* pszFile)
{
	fstream fin(pszFile, ios::in);
	if (!fin.is_open()) return -1;
	
	string strLine;
	string strKey;
	string strVal;
	string strType;
	int nSplitPoint = 0;
	while(!fin.eof())
	{
		getline(fin,strLine);
		if ((strLine[0] != '#') && (!strLine.empty()))
		{
			nSplitPoint = strLine.find('=');
			if (nSplitPoint != -1)
			{
				strType = strLine.substr(0, 3);
				strKey = strLine.substr(4, nSplitPoint  - 4);
				strVal = strLine.substr(nSplitPoint + 1, strLine.size());
				if (strType == "flt")
				{
					(*m_pFloatSettings)[strKey] = atof(strVal.c_str());
				}
				else if (strType == "int")
				{
					(*m_pIntSettings)[strKey] = atoi(strVal.c_str());
				}
				else if (strType == "str")
				{
					(*m_pStringSettings)[strKey] = strVal;				
				}
			}		
		}
	} // wend
/*
	// debug code
	map<string, string>::iterator i;
	map<string, int>::iterator j;
	map<string, float>::iterator k;
	// displaying read in settings
	for (i = m_pStringSettings->begin(); i != m_pStringSettings->end(); i++)
	{
		cout << (*i).first << " : " << (*i).second << endl;
	}
	
	for (j = m_pIntSettings->begin(); j != m_pIntSettings->end(); j++)
	{
		cout << (*j).first << " : " << (*j).second << endl;
	}
	
	for (k = m_pFloatSettings->begin(); k != m_pFloatSettings->end(); k++)
	{
		cout << (*k).first << " : " << (*k).second << endl;
	}
	*/
	return 0;
}
