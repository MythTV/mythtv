#ifndef MYTHLOCALE_H
#define MYTHLOCALE_H

// QT
#include <QString>
#include <QMap>
#include <QLocale>

// Libmythdb
#include "iso3166.h"
#include "iso639.h"
#include "mythexp.h"

class MPUBLIC MythLocale
{
  public:
    MythLocale(QString localeName = QString());
    ~MythLocale() { };

    QString GetCountryCode() const; /// ISO3166 2-letter
    QString GetCountry() const /// Name of country in English
    {
        return QLocale::countryToString(m_country);
    }
    QString GetNativeCountry() const; /// Name of country in the native language

    QString GetLanguageCode() const; /// ISO639 2-letter
    QString GetLanguage() const /// Name of language in English
    {
        return QLocale::languageToString(m_language);
    }
    QString GetNativeLanguage() const; /// Name of language in that language

    QString GetLocaleCode() const { return m_localeCode; }

    bool LoadDefaultsFromXML(void);
    void SaveLocaleDefaults(bool overwrite = false);
    void ResetToLocaleDefaults(void);
    void ResetToStandardDefaults(void);

    QString GetLocaleSetting(const QString &key);

  private:
    QLocale::Country m_country;
    QLocale::Language m_language;
    QString m_localeCode;
    bool m_defaultsLoaded;

    typedef QMap<QString, QString> SettingsMap;
    SettingsMap m_globalSettings;
    SettingsMap m_hostSettings;
};

#endif

