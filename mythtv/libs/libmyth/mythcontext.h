#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <QObject>
#include <QString>

#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythexp.h"
#include "mythverbose.h"

class MythPluginManager;
class MythContextPrivate;
class UPnp;

class MythContextSlotHandler : public QObject
{
    Q_OBJECT

  public:
    MythContextSlotHandler(MythContextPrivate *x) : d(x) { }

  private slots:
    void ConnectFailurePopupClosed(void);
    void VersionMismatchPopupClosed(void);

  private:
    ~MythContextSlotHandler() {}

    MythContextPrivate *d;
};

/** \class MythContext
 *  \brief This class contains the runtime context for MythTV.
 *
 *   This class can be used to query for and set global and host
 *   settings, and is used to communicate between the frontends
 *   and backends. It also contains helper functions for theming
 *   and for getting system defaults, parsing the command line, etc.
 *   It also contains support for database error printing, and
 *   database message logging.
 */
class MPUBLIC MythContext
{
  public:
    MythContext(const QString &binversion);
    virtual ~MythContext();

    bool Init(const bool gui = true,
              UPnp *UPnPclient = NULL,
              const bool promptForBackend = false,
              const bool bypassAutoDiscovery = false,
              const bool ignoreDB = false);

    DatabaseParams GetDatabaseParams(void);
    bool SaveDatabaseParams(const DatabaseParams &params);

    bool TestPopupVersion(const QString &name, const QString &libversion,
                          const QString &pluginversion);

    void SetDisableEventPopup(bool check);
    void SetDisableLibraryPopup(bool check);

    void SetPluginManager(MythPluginManager *pmanager);
    MythPluginManager *getPluginManager(void);

  private:
    MythContextPrivate *d;
    QString app_binary_version;
};

/// This global variable contains the MythContext instance for the application
extern MPUBLIC MythContext *gContext;

/// Service type for the backend's UPnP server
extern MPUBLIC const QString gBackendURI;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
