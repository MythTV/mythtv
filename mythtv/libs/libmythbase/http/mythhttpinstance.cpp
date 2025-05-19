// MythTV
#include "mthread.h"
#include "http/mythhttpinstance.h"
#include "http/mythhttproot.h"
#include "http/mythhttpserver.h"

MythHTTPInstance& MythHTTPInstance::Instance()
{
    static MythHTTPInstance s_instance;
    return s_instance;
}

MythHTTPInstance::MythHTTPInstance()
  : m_httpServer(new MythHTTPServer),
    m_httpServerThread(new MThread("HTTPServer"))
{
    // We need to register some types and this should always be hit at least once
    // before they are needed
    qRegisterMetaType<HTTPHandler>();
    qRegisterMetaType<HTTPHandlers>();
    qRegisterMetaType<HTTPServices>();
    qRegisterMetaType<DataPayload>();
    qRegisterMetaType<DataPayloads>();
    qRegisterMetaType<StringPayload>();

    m_httpServer->moveToThread(m_httpServerThread->qthread());
    m_httpServerThread->start();
    QThread::usleep(50);
    while (!m_httpServerThread->qthread()->isRunning())
    {
        QThread::usleep(50);
    }
}

MythHTTPInstance::~MythHTTPInstance()
{
    if (m_httpServerThread)
    {
        m_httpServerThread->quit();
        m_httpServerThread->wait();
    }
    delete m_httpServerThread;
    delete m_httpServer;
}

/*! \brief Stop and delete the MythHTTPServer instance.
 *
 * This should be used to cleanup the HTTP server when the application is terminating.
 * To start or stop the server during the application lifetime use EnableHTTPService.
*/
void MythHTTPInstance::StopHTTPService()
{
    if (Instance().m_httpServerThread)
    {
        Instance().m_httpServerThread->quit();
        Instance().m_httpServerThread->wait();
        delete Instance().m_httpServerThread;
        Instance().m_httpServerThread = nullptr;
    }

    delete Instance().m_httpServer;
    Instance().m_httpServer = nullptr;
}

/*! \brief Signals to the MythHTTPServer instance whether to start or stop listening.
 *
 * The server is not deleted.
*/
void MythHTTPInstance::EnableHTTPService(bool Enable)
{
    emit Instance().m_httpServer->EnableHTTP(Enable);
}

/*! \brief Add path(s) for the MythHTTPServer instance to handle.
 *
 * This is the default implementation. The server will serve any files in the
 * given path(s) without limit (i.e. no authorisation etc)
*/
void MythHTTPInstance::AddPaths(const QStringList &Paths)
{
    emit Instance().m_httpServer->AddPaths(Paths);
}

/*! \brief Remove path(s) from the MythHTTPServer instance.
*/
void MythHTTPInstance::RemovePaths(const QStringList &Paths)
{
    emit Instance().m_httpServer->RemovePaths(Paths);
}

/*! \brief Add function handlers for specific paths.
 *
 * The functions will provide *simple* dynamic content, returning either a valid response
 * to be processed or a null response to indicate that the path was not handled.
*/
void MythHTTPInstance::AddHandlers(const HTTPHandlers &Handlers)
{
    emit Instance().m_httpServer->AddHandlers(Handlers);
}

void MythHTTPInstance::RemoveHandlers(const HTTPHandlers &Handlers)
{
    emit Instance().m_httpServer->RemoveHandlers(Handlers);
}

void MythHTTPInstance::Addservices(const HTTPServices &Services)
{
    emit Instance().m_httpServer->AddServices(Services);
}

void MythHTTPInstance::RemoveServices(const HTTPServices &Services)
{
    emit Instance().m_httpServer->RemoveServices(Services);
}

void MythHTTPInstance::AddErrorPageHandler(const HTTPHandler &Handler)
{
    emit Instance().m_httpServer->AddErrorPageHandler(Handler);
}

/*! \brief A convenience class to manage the lifetime of a MythHTTPInstance
*/
MythHTTPScopedInstance::MythHTTPScopedInstance(const HTTPHandlers& Handlers)
{
    MythHTTPInstance::AddHandlers(Handlers);
    MythHTTPInstance::EnableHTTPService();
}

MythHTTPScopedInstance::~MythHTTPScopedInstance()
{
    MythHTTPInstance::StopHTTPService();
}
