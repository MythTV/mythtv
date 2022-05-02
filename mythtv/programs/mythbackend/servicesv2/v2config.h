#ifndef V2CONFIG_H
#define V2CONFIG_H

#include "libmythbase/http/mythhttpservice.h"
#include "v2connectionInfo.h"
#include "v2databaseStatus.h"
#include "v2languageList.h"
#include "v2countryList.h"

#define CONFIG_SERVICE QString("/Config/")
#define CONFIG_HANDLE  QString("Config")

class V2Config : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "0.1" )
    Q_CLASSINFO( "SetDatabaseCredentials",     "methods=POST"                )


  public:
    V2Config();
   ~V2Config() override = default;
    static void RegisterCustomTypes();

  public slots:

    static bool              SetDatabaseCredentials    ( const QString &Host,
                                                         const QString &UserName,
                                                         const QString &Password,
                                                         const QString &Name,
                                                         int   Port,
                                                         bool  DoTest );

    static V2DatabaseStatus* GetDatabaseStatus         ( void );

    static V2CountryList*    GetCountries              ( void );
    static V2LanguageList*   GetLanguages              ( void );

    static QStringList       GetIPAddresses            ( const QString &Protocol );

  private:
    Q_DISABLE_COPY(V2Config)

};

#endif // V2CONFIG_H
