#ifndef MYTHLOCALE_H
#define MYTHLOCALE_H

// QT
#include <QString>
#include <QMap>
#include <QLocale>

// libmythbase
#include "mythbaseexp.h"

class MBASE_PUBLIC MythLocale
{
  public:
    explicit MythLocale(const QString &localeName = QString());

    void ReInit();

    QString GetCountryCode() const; /// ISO3166 2-letter
    QString GetCountry() const; /// Name of country in English
    QString GetNativeCountry() const; /// Name of country in the native language

    QString GetLanguageCode() const; /// ISO639 2-letter
    QString GetLanguage() const; /// Name of language in English
    QString GetNativeLanguage() const; /// Name of language in that language

    QString GetLocaleCode() const { return m_localeCode; }

    QLocale ToQLocale() const { return m_qtLocale; }

    bool LoadDefaultsFromXML(void);
    void SaveLocaleDefaults(bool overwrite = false);
    void ResetToLocaleDefaults(void);
    void ResetToStandardDefaults(void);

    QString GetLocaleSetting(const QString &key);

  private:
    void Init(const QString &localeName = QString());

    QString m_localeCode;
    bool    m_defaultsLoaded {false};
    QLocale m_qtLocale;

    using SettingsMap = QMap<QString, QString>;
    SettingsMap m_globalSettings;
    SettingsMap m_hostSettings;
};

#endif
