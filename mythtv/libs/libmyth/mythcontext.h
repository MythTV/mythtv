#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <QObject>
#include <QString>

#include "libmyth/mythexp.h"
#include "libmythbase/mythdbparams.h"

/** \class MythContext
 *  \brief Startup context for MythTV.
 *
 *   This class has methods used during startup for setting
 *   up database connections, checking for correct database
 *   version, locating the backend.
 *   After startup, context information is handled by
 *   MythCoreContext.
 */
class MPUBLIC MythContext
{
  public:
    explicit MythContext(QString binversion, bool needsBackend = false);
    virtual ~MythContext();

    bool Init(bool gui = true,
              bool promptForBackend = false,
              bool disableAutoDiscovery = false,
              bool ignoreDB = false);

    bool SaveDatabaseParams(const DatabaseParams &params, bool force = false);
    bool saveSettingsCache();

    void SetDisableEventPopup(bool check);

    enum WebOnlyStartup : std::uint8_t {
        kWebOnlyNone = 0,
        kWebOnlyDBSetup = 1,
        kWebOnlyDBTimezone = 2,
        kWebOnlyWebOnlyParm = 3,
        kWebOnlyIPAddress = 4,
        kWebOnlySchemaUpdate = 5
    };

    void setWebOnly(WebOnlyStartup w) {m_webOnly = w;}
    WebOnlyStartup getWebOnly() {return m_webOnly;}

  private:
    Q_DISABLE_COPY(MythContext)

    class Impl;
    Impl   *m_impl {nullptr}; ///< PIMPL idiom
    QString             m_appBinaryVersion;
    WebOnlyStartup      m_webOnly {kWebOnlyNone};
};

/// This global variable contains the MythContext instance for the application
extern MPUBLIC MythContext *gContext;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
