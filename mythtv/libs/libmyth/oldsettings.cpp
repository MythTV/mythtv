
/***************************************************************************
                          settings.cpp  -  description
                             -------------------
    begin                : Tuesday, October 03, 2000
    copyright            : (C) 2000 by Relatable, LLC
    written by           : Sean Ward
    email                : sward@relatable.com
 ***************************************************************************/


#include "oldsettings.h"
#include "mythcontext.h"
#include <qstring.h>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sys/time.h>

using namespace std;

#include <qdir.h>

Settings::Settings(QString strSettingsFile)
{
    if (strSettingsFile.length() == 0)
        strSettingsFile = "settings.txt";
    m_pSettings = new map<QString, QString>;

    ReadSettings(strSettingsFile);
}

Settings::~Settings()
{
    if (m_pSettings != NULL)
        delete m_pSettings;
}

// Setting retrieval functions
/** Generic Setting Retrieval functions */
QString Settings::GetSetting(QString strSetting, QString defaultvalue)
{
    map<QString, QString>::iterator i;
    if ((!m_pSettings->empty()) && 
        ((i = m_pSettings->find(strSetting)) != m_pSettings->end()))
    {
        return (*i).second;
    }
    return defaultvalue; // property doesn't exist
}

/** Generic Setting Retrieval function for numeric values */
int Settings::GetNumSetting(QString strSetting, int defaultvalue)
{
    int retval = defaultvalue;
    map<QString, QString>::iterator i;
    if ((!m_pSettings->empty()) && 
        ((i = m_pSettings->find(strSetting)) != m_pSettings->end()))
    {
        bool ok = false;
        retval = (*i).second.toInt(&ok);
        if (!ok)
            retval = defaultvalue;
    }
    return retval; // property doesn't exist
}

/** Generic Setting Retrieval function for float values */
float Settings::GetFloatSetting(QString strSetting, float defaultvalue)
{
    float retval = defaultvalue;

    map<QString, QString>::iterator i;
    if ((!m_pSettings->empty()) && 
        ((i = m_pSettings->find(strSetting)) != m_pSettings->end()))
    {
        bool ok = false;
        retval = ((*i).second).toFloat(&ok);
        if (!ok)
            retval = defaultvalue;
    }

    return defaultvalue; // property doesn't exist
}

// Setting Setting functions
/** Generic Setting Setting function */
void Settings::SetSetting(QString strSetting, QString strNewVal)
{
    (*m_pSettings)[strSetting] = strNewVal;
}

/** Generic Setting Setting function for int values */
void Settings::SetSetting(QString strSetting, int nNewVal)
{
    QString tmp;
    tmp = tmp.setNum(nNewVal);
    (*m_pSettings)[strSetting] = tmp;
}

/** Generic Setting Setting function for float values */
void Settings::SetSetting(QString strSetting, float fNewVal)
{
    QString tmp;
    tmp = tmp.setNum(fNewVal);
    (*m_pSettings)[strSetting] = tmp;
}

bool Settings::LoadSettingsFiles(QString filename, QString prefix)
{
    QString setname = prefix + "/share/mythtv/" + filename;
    bool result = false;

    if (ReadSettings(setname))
         result = true;

    setname = prefix + "/etc/mythtv/" + filename;
    if (ReadSettings(setname))
         result = true;

    setname = MythContext::GetConfDir() + "/" + filename;

    if (ReadSettings(setname))
         result = true;

    setname = "./" + filename;
    if (ReadSettings(setname))
         result = true;

    return result;
}

bool Settings::ReadSettings(QString pszFile)
{
    fstream fin(pszFile.ascii(), ios::in);
    if (!fin.is_open()) return false;

    string strLine;
    QString strKey;
    QString strVal;
    QString strType;
    QString line;
    int nSplitPoint = 0;

    while(!fin.eof())
    {
        getline(fin,strLine);
        line = strLine.c_str();

        if ((line[0] != '#') && (!line.isEmpty()))
        {
            nSplitPoint = strLine.find('=');
            if (nSplitPoint != -1)
            {
                strType = line.mid(0, 3);

                if (strType == "flt" || strType == "int" || strType == "str")
                    strKey = line.mid(4, nSplitPoint - 4);
                else
                    strKey = line.mid(0, nSplitPoint);

                strVal = line.mid(nSplitPoint + 1, strLine.size());

                (*m_pSettings)[strKey] = strVal;
            }
        }
    } // wend
    return true;
}
