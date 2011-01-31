
/***************************************************************************
                          settings.cpp  -  description
                             -------------------
    begin                : Tuesday, October 03, 2000
    copyright            : (C) 2000 by Relatable, LLC
    written by           : Sean Ward
    email                : sward@relatable.com
 ***************************************************************************/

#include "compat.h"

#include <cstdlib>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <fstream>
#include <string>
using namespace std;

#include <QDir>

#include "oldsettings.h"
#include "mythverbose.h"

Settings::Settings(QString strSettingsFile)
{
    if (strSettingsFile.length() == 0)
        strSettingsFile = "settings.txt";
    m_pSettings = new map<QString, QString>;

    ReadSettings(strSettingsFile);
}

Settings::~Settings()
{
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

    return retval; // property doesn't exist
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

bool Settings::LoadSettingsFiles(QString filename, QString prefix,
                                 QString confdir)
{
    int result = ReadSettings(prefix + "/share/mythtv/" + filename);
    result += ReadSettings(prefix + "/etc/mythtv/" + filename);
    result += ReadSettings(confdir + '/' + filename);
    result += ReadSettings("./" + filename);
    return result;
}

bool Settings::ReadSettings(QString pszFile)
{
    QString LOC = "(old)Settings::ReadSettings(" + pszFile + ") - ";
    fstream fin(pszFile.toAscii(), ios::in);

    if (!fin.is_open())
    {
        VERBOSE(VB_FILE, LOC + "No such file");
        return false;
    }

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

                VERBOSE(VB_FILE, LOC + '\'' + strKey + "' = '" + strVal + "'.");
            }
        }
    } // wend
    return true;
}
