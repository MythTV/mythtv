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

    bool saveSettingsCache();

    void SetDisableEventPopup(bool check);

  private:
    Q_DISABLE_COPY(MythContext)

    class Impl;
    Impl   *m_impl {nullptr}; ///< PIMPL idiom
    QString             m_appBinaryVersion;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
