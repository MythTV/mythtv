#include "v2config.h"
#include "libmythbase/http/mythhttpmetaservice.h"

#include "version.h"
#include "mythcorecontext.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "v2databaseInfo.h"
#include "v2countryList.h"
#include "v2languageList.h"
#include "v2databaseStatus.h"

#include "mythlocale.h"
#include "mythtranslation.h"
#include "iso3166.h"
#include "iso639.h"

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (CONFIG_HANDLE, V2Config::staticMetaObject, &V2Config::RegisterCustomTypes))

void V2Config::RegisterCustomTypes()
{
    qRegisterMetaType<V2ConnectionInfo*>("V2ConnectionInfo");
    qRegisterMetaType<V2CountryList*>("V2CountryList");
    qRegisterMetaType<V2Country*>("V2Country");
    qRegisterMetaType<V2LanguageList*>("V2LanguageList");
    qRegisterMetaType<V2Language*>("V2Language");
    qRegisterMetaType<V2DatabaseStatus*>("V2DatabaseStatus");
}


V2Config::V2Config()
  : MythHTTPService(s_service)
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
bool V2Config::SetDatabaseCredentials(const QString &Host, const QString &UserName, const QString &Password, const QString &Name, int Port, bool DoTest)
{
    bool bResult = false;

    QString db("mythconverg");
    int port = 3306;

    if (!Name.isEmpty())
        db = Name;

    if (Port != 0)
        port = Port;

    if (DoTest && !TestDatabase(Host, UserName, Password, db, port))
        throw( QString( "Database test failed. Not saving database connection information." ));

    DatabaseParams dbparms;
    dbparms.m_dbName = db;
    dbparms.m_dbUserName = std::move(UserName);
    dbparms.m_dbPassword = std::move(Password);
    dbparms.m_dbHostName = std::move(Host);
    dbparms.m_dbPort = port;

    // Just use some sane defaults for these values
    dbparms.m_wolEnabled = false;
    dbparms.m_wolReconnect = 1s;
    dbparms.m_wolRetry = 3;
    dbparms.m_wolCommand = QString();

    bResult = gContext->SaveDatabaseParams(dbparms);

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
V2DatabaseStatus* V2Config::GetDatabaseStatus()
{
    const DatabaseParams params = gCoreContext->GetDatabaseParams();

    auto *pInfo = new V2DatabaseStatus();

    pInfo->setHost(params.m_dbHostName);
    pInfo->setPing(params.m_dbHostPing);
    pInfo->setPort(params.m_dbPort);
    pInfo->setUserName(params.m_dbUserName);
    pInfo->setPassword(params.m_dbPassword);
    pInfo->setName(params.m_dbName);
    pInfo->setType(params.m_dbType);
    pInfo->setLocalEnabled(params.m_localEnabled);
    pInfo->setLocalHostName(params.m_localHostName);

    // are we connected to the database?
    bool connected = TestDatabase(params.m_dbHostName, params.m_dbUserName, params.m_dbPassword, params.m_dbName, params.m_dbPort);
    pInfo->setConnected(connected);

    // do we have a mythconverg database?
    if (connected)
    {
        bool haveSchema = GetMythDB()->HaveSchema();
        pInfo->setHaveDatabase(haveSchema);

        if (haveSchema)
        {
            // get schema version
            pInfo->setSchemaVersion(QString(MYTH_DATABASE_VERSION).toInt());
        }
        else
            pInfo->setSchemaVersion(0);
    }
    else
    {
        pInfo->setHaveDatabase(false);
        pInfo->setSchemaVersion(0);
    }

    return pInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
V2CountryList* V2Config::GetCountries()
{

    ISO3166ToNameMap localesMap = GetISO3166EnglishCountryMap();
    QStringList locales = localesMap.values();
    locales.sort();

    auto* pList = new V2CountryList();

    for (const auto& country : qAsConst(locales))
    {
        const QString code = localesMap.key(country);
        const QString nativeCountry = GetISO3166CountryName(code);

        V2Country *pCountry = pList->AddNewCountry();
        pCountry->setCode(code);
        pCountry->setCountry(country);
        pCountry->setNativeCountry(nativeCountry);
        pCountry->setImage(QString("%1.png").arg(code.toLower()));
     }

    return pList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
V2LanguageList* V2Config::GetLanguages()
{
    QMap<QString,QString> langMap = MythTranslation::getLanguages();
    QStringList langs = langMap.values();
    langs.sort();

    auto* pList = new V2LanguageList();

    for (const auto& nativeLang : qAsConst(langs))
    {
        const QString code = langMap.key(nativeLang);
        const QString language = GetISO639EnglishLanguageName(code);

        V2Language *pLanguage = pList->AddNewLanguage();
        pLanguage->setCode(code);
        pLanguage->setLanguage(language);
        pLanguage->setNativeLanguage(nativeLang);
        pLanguage->setImage(QString("%1.png").arg(code));
    }

    return pList;
}


