#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <QObject>
#include <QString>

#include "mythcorecontext.h"
#include "mythevent.h"
#include "mythexp.h"
#include "mythlogging.h"

class MythContextPrivate;

class MythContextSlotHandler : public QObject
{
    Q_OBJECT

  public:
    explicit MythContextSlotHandler(MythContextPrivate *x) : d(x) { }

  private slots:
    void VersionMismatchPopupClosed(void);

  public slots:
    void OnCloseDialog(void);

  private:
    ~MythContextSlotHandler() = default;

    MythContextPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)
};

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
    MythContext(QString binversion, bool needsBackend = false);
    virtual ~MythContext();

    bool Init(const bool gui = true,
              const bool promptForBackend = false,
              const bool disableAutoDiscovery = false,
              const bool ignoreDB = false);

    DatabaseParams GetDatabaseParams(void);
    bool SaveDatabaseParams(const DatabaseParams &params);
    bool saveSettingsCache(void);

    void SetDisableEventPopup(bool check);

  private:
    MythContextPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)
    QString             m_appBinaryVersion;
};

/// This global variable contains the MythContext instance for the application
extern MPUBLIC MythContext *gContext;

/// Service type for the backend's UPnP server
extern const QString gBackendURI;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
