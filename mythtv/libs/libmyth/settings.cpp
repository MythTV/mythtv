
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
#include <qstring.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <string>
using namespace std;

Settings::Settings(QString strSettingsFile)
{
	if (strSettingsFile.length() == 0)
	{
		strSettingsFile = "settings.txt";
	}
	m_pStringSettings = new map<QString, QString>;
	m_pIntSettings = new map<QString, int>;
	m_pFloatSettings = new map<QString, float>;
	m_pVoidSettings = new map<QString, void*>;
	
	this->ReadSettings(strSettingsFile);	// load configuration values from settings.txt in this dir	
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
QString Settings::GetSetting(QString strSetting)
{
	map<QString, QString>::iterator i;
	if ((!m_pStringSettings->empty()) && ((i = m_pStringSettings->find(strSetting)) != m_pStringSettings->end()))
	{
		return (*i).second;
	}
	return ""; // property doesn't exist
}

/** Generic Setting Retrieval function for numeric values */
int Settings::GetNumSetting(QString strSetting)
{
	map<QString, int>::iterator i;
	if ((!m_pIntSettings->empty()) && ((i = m_pIntSettings->find(strSetting)) != m_pIntSettings->end()))
	{
		return (*i).second;
	}
	return 0; // property doesn't exist
}

/** Generic Setting Retrieval function for float values */
float Settings::GetFloatSetting(QString strSetting)
{
	map<QString, float>::iterator i;
	if ((!m_pFloatSettings->empty()) && ((i = m_pFloatSettings->find(strSetting)) != m_pFloatSettings->end()))
	{
		return (*i).second;
	}
	return 0; // property doesn't exist
}

/** Generic Setting Retrieval functions for pointers */
void* Settings::GetPointer(QString strSetting)
{
	map<QString, void*>::iterator i;
	if ((!m_pVoidSettings->empty()) && ((i = m_pVoidSettings->find(strSetting)) != m_pVoidSettings->end()))
	{
		return (*i).second;
	}
	return NULL; // property doesn't exist
}

// Setting Setting functions
/** Generic Setting Setting function */
void Settings::SetSetting(QString strSetting, QString strNewVal)
{
	(*m_pStringSettings)[strSetting] = strNewVal;
}

/** Generic Setting Setting function for int values */
void Settings::SetSetting(QString strSetting, int nNewVal)
{
	(*m_pIntSettings)[strSetting] = nNewVal;
}

/** Generic Setting Setting function for float values */
void Settings::SetSetting(QString strSetting, float fNewVal)
{
	(*m_pFloatSettings)[strSetting] = fNewVal;
}

/** Generic Setting Setting function for pointer values */
void Settings::SetSetting(QString strSetting, void* pNewVal)
{
	(*m_pVoidSettings)[strSetting] = pNewVal;
}

int Settings::ReadSettings(QString pszFile)
{
	fstream fin(pszFile.ascii(), ios::in);
	if (!fin.is_open()) return -1;
	
	string strLine;
	QString strKey;
	QString strVal;
	QString strType;
	int nSplitPoint = 0;
	while(!fin.eof())
	{
		getline(fin,strLine);
		if ((strLine[0] != '#') && (!strLine.empty()))
		{
			nSplitPoint = strLine.find('=');
			if (nSplitPoint != -1)
			{
				strType = strLine.substr(0, 3).c_str();
				strKey = strLine.substr(4, nSplitPoint  - 4).c_str();
				strVal = strLine.substr(nSplitPoint + 1, strLine.size()).c_str();
				if (strType == "flt")
				{
					(*m_pFloatSettings)[strKey] = atof(strVal.ascii());
				}
				else if (strType == "int")
				{
					(*m_pIntSettings)[strKey] = atoi(strVal.ascii());
				}
				else if (strType == "str")
				{
					(*m_pStringSettings)[strKey] = strVal;				
				}
			}		
		}
	} // wend
	return 0;
}
