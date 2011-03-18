
#include "mythlocale.h"

// QT
#include <QLocale>
#include <QDomDocument>
#include <QFile>
#include <QIODevice>

// libmythbase
#include "mythverbose.h"
#include "mythdb.h"
#include "mythdirs.h"

MythLocale::MythLocale(QString localeName) :
    m_defaultsLoaded(false)
{
    QString dbLanguage = GetMythDB()->GetSetting("Language", "");
    QString dbCountry = GetMythDB()->GetSetting("Country", "");

    if (!localeName.isEmpty())
    {
        m_localeCode = localeName;
    }
    else if (!dbLanguage.isEmpty() &&
             !dbCountry.isEmpty())
    {
        QString langcode = dbLanguage.section('_',0,0);
        m_localeCode = QString("%1_%2").arg(langcode)
                                       .arg(dbCountry.toUpper());
    }
    else
    {
        QLocale locale = QLocale::system();

        if (locale.name().isEmpty() || locale.name() == "C")
        {
            // If all else has failed use the US locale
            m_localeCode = "en_US";
        }
        else
            m_localeCode = locale.name();
    }
}

QString MythLocale::GetCountryCode(void) const
{
    QString isoCountry = m_localeCode.section('_', 1, 1);

    return isoCountry;
}

QString MythLocale::GetCountry() const
{
    return GetISO3166EnglishCountryName(GetCountryCode());
}

QString MythLocale::GetNativeCountry(void) const
{
    return GetISO3166CountryName(GetCountryCode());
}

QString MythLocale::GetLanguageCode(void) const
{
    QString isoLanguage = m_localeCode.section('_', 0, 0);

    return isoLanguage;
}

QString MythLocale::GetLanguage() const
{
    return GetISO639EnglishLanguageName(GetLanguageCode());
}

QString MythLocale::GetNativeLanguage(void) const
{
    return GetISO639LanguageName(GetLanguageCode());
}

bool MythLocale::LoadDefaultsFromXML(void)
{
    m_defaultsLoaded = true;
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
    if (!m_defaultsLoaded &&
        !LoadDefaultsFromXML())
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

QString MythLocale::GetLocaleSetting(const QString &key)
{
    if (!m_defaultsLoaded &&
        !LoadDefaultsFromXML())
        return QString();

    QString value = m_globalSettings.value(key);
    if (m_hostSettings.contains(key))
        value = m_hostSettings.value(key);

    return value;
}
