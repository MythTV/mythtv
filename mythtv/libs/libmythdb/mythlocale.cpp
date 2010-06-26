
#include "mythlocale.h"

// QT
#include <QLocale>
#include <QDomDocument>
#include <QFile>
#include <QIODevice>

// Libmythdb
#include "mythverbose.h"
#include "mythdb.h"
#include "mythdirs.h"

MythLocale::MythLocale(QString localeName)
{
    QLocale locale;

    if (!localeName.isEmpty())
        locale = QLocale(localeName);
    else
        locale = QLocale::system();

    m_localeCode = locale.name();
    m_country = locale.country();
    m_language = locale.language();

    if (m_localeCode.isEmpty())
        m_localeCode = "en_us";
}

QString MythLocale::GetCountryCode(void) const
{
    QString isoCountry = m_localeCode.section("_", 1, 1);

    return isoCountry;
}

QString MythLocale::GetLanguageCode(void) const
{
    QString isoLanguage = m_localeCode.section("_", 0, 0);

    return isoLanguage;
}

bool MythLocale::LoadDefaultsFromXML(void)
{
    m_globalSettings.clear();
    QDomDocument doc;

    QString path = QString("/locales/%1.xml").arg(m_localeCode.toLower());

    QFile file(path.prepend(GetShareDir()));
    if (!file.exists())
    {
        file.setFileName(path.prepend(GetConfDir()));

        if (!file.exists())
        {
            VERBOSE(VB_GENERAL, QString("No locale defaults file for %1, "
                                        "skipping").arg(m_localeCode));
            return false;
        }
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to open %1")
                                                        .arg(file.fileName()));
        return false;
    }

    VERBOSE(VB_GENERAL, QString("Reading locale defaults from %1")
                                                        .arg(file.fileName()));

    if (!doc.setContent(&file))
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to parse %1")
                                                        .arg(file.fileName()));

        file.close();
        return false;
    }
    file.close();

    QDomElement docElem = doc.documentElement();

    for (QDomNode n = docElem.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "setting")
            {
                QString name = e.attribute("name", "");
                bool global = (e.attribute("global", "false") == "true");
                QString value = e.firstChild().toText().data();

                // TODO Assumes no setting accepts an empty value, which may not
                // be the case
                if (!name.isEmpty() && !value.isEmpty())
                {
                    if (global)
                        m_globalSettings[name] = value;
                    else
                        m_hostSettings[name] = value;
                }
            }
        }
    }

    if (m_globalSettings.isEmpty() && m_hostSettings.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("No locale defaults specified in %1, "
                                    "skipping").arg(file.fileName()));
        return false;
    }

    return true;
}

void MythLocale::SaveLocaleDefaults(bool overwrite)
{
    if (!LoadDefaultsFromXML())
        return;

    SettingsMap::iterator it;
    for (it = m_globalSettings.begin(); it != m_globalSettings.end(); ++it)
    {
        MythDB *mythDB = MythDB::getMythDB();
        if (overwrite || mythDB->GetSetting(it.key()).isEmpty())
        {
            mythDB->SetSetting(it.key(), it.value());
            mythDB->SaveSettingOnHost(it.key(), it.value(), "");
        }
    }

    for (it = m_hostSettings.begin(); it != m_hostSettings.end(); ++it)
    {
        MythDB *mythDB = MythDB::getMythDB();
        if (overwrite || mythDB->GetSetting(it.key()).isEmpty())
        {
            mythDB->SetSetting(it.key(), it.value());
            mythDB->SaveSetting(it.key(), it.value());
        }
    }
}

void MythLocale::ResetToLocaleDefaults(void)
{
    SaveLocaleDefaults(true);
}

void MythLocale::ResetToStandardDefaults(void)
{
    // TODO Not implemented yet, delete everything in m_globalSettings
    //      from the database then let the standard defaults populate them
    //      again. Used if the user wants to revert the changes
    return;
}

