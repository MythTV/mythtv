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

    using CleanupFunction = void (*)();
    void setCleanup(CleanupFunction cleanup) { m_cleanup = cleanup; }

    bool saveSettingsCache();

    void SetDisableEventPopup(bool check);

  private:
    Q_DISABLE_COPY(MythContext)

    class Impl;
    Impl   *m_impl {nullptr}; ///< PIMPL idiom
    QString             m_appBinaryVersion;
    /**
    This is used to destroy global state before main() returns.  It is called
    first before anything else is done in ~MythContext() if it is not nullptr.
    */
    CleanupFunction     m_cleanup {nullptr};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
