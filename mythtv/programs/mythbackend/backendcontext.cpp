
#include "backendcontext.h"

#include "mythlogging.h"

QMap<int, EncoderLink *> tvList;
AutoExpire  *expirer      = NULL;
JobQueue    *jobqueue     = NULL;
HouseKeeper *housekeeping = NULL;
MediaServer *g_pUPnp      = NULL;
BackendContext *gBackendContext = NULL;
QString      pidfile;
QString      logfile;

BackendContext::BackendContext()
{

}

BackendContext::~BackendContext()
{
    QMap<QString, Frontend*>::iterator it = m_knownFrontends.begin();
    while (it != m_knownFrontends.end())
    {
        Frontend *fe = (*it);
        delete fe;
        fe = NULL;
        ++it;
    }

    m_connectedFrontends.clear();
    m_knownFrontends.clear();
}

void BackendContext::SetFrontendConnected(Frontend *frontend)
{
    if (!frontend || frontend->name.isEmpty())
        return;

    if (m_knownFrontends.contains(frontend->name))
    {
        Frontend *fe = m_knownFrontends.value(frontend->name);
        // Frontend may have changed IP since we last saw it
        fe->ip = frontend->ip;
        delete frontend;
        frontend = NULL;

        if (!m_connectedFrontends.contains(fe->name))
            m_connectedFrontends.insert(fe->name, fe);

        fe->connectionCount++;
        LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Increasing "
                                           "connection count for (%1) to %2 ")
                                            .arg(fe->name)
                                            .arg(fe->connectionCount));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("BackendContext: Frontend '%1' "
                                      "connected.").arg(frontend->name));

    frontend->connectionCount++;
    m_connectedFrontends.insert(frontend->name, frontend);

    // TODO: We want to store this information in the database so that
    //       it persists between backend restarts. We can then give users
    //       an overview of the number of frontends on the network, those
    //       connected at any given moment and in the future give them the
    //       option to forget clients including associated settings, deny
    //       unknown clients from connecting and other cool stuff
    m_knownFrontends.insert(frontend->name, frontend);

}

void BackendContext::SetFrontendDisconnected(const QString& name)
{
    if (m_connectedFrontends.contains(name))
    {
        Frontend *frontend = m_connectedFrontends.value(name);
        frontend->connectionCount--;
        LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Decreasing "
                                           "connection count for (%1) to %2 ")
                                            .arg(frontend->name)
                                            .arg(frontend->connectionCount));
        if (frontend->connectionCount <= 0)
        {
            // Will still be referenced in knownFrontends, so no leak here
            m_connectedFrontends.remove(name);
            LOG(VB_GENERAL, LOG_INFO, QString("BackendContext: Frontend '%1' "
                                              "disconnected.").arg(frontend->name));
        }

        return;
    }
    LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Disconnect requested "
                                           "for frontend (%1) which isn't "
                                           "registered. ").arg(name));
}
