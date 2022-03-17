#ifndef MYTHHTTPINSTANCE_H
#define MYTHHTTPINSTANCE_H

// MythTV
#include "libmythbase/http/mythhttpservice.h"
#include "libmythbase/http/mythhttptypes.h"

class MThread;
class MythHTTPServer;

class MBASE_PUBLIC MythHTTPInstance
{
  public:
    static void EnableHTTPService(bool Enable = true);
    static void StopHTTPService  ();
    static void AddPaths         (const QStringList& Paths);
    static void RemovePaths      (const QStringList& Paths);
    static void AddHandlers      (const HTTPHandlers& Handlers);
    static void RemoveHandlers   (const HTTPHandlers& Handlers);
    static void Addservices      (const HTTPServices& Services);
    static void RemoveServices   (const HTTPServices& Services);
    static void AddErrorPageHandler (const HTTPHandler& Handler);

  private:
    Q_DISABLE_COPY(MythHTTPInstance)

    static MythHTTPInstance& Instance();
    MythHTTPInstance();
   ~MythHTTPInstance();

    MythHTTPServer* m_httpServer { nullptr };
    MThread* m_httpServerThread  { nullptr };
};

class MBASE_PUBLIC MythHTTPScopedInstance
{
  public:
    explicit MythHTTPScopedInstance(const HTTPHandlers& Handlers);
   ~MythHTTPScopedInstance();
};

#endif
