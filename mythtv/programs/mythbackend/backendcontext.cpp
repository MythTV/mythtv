
#include "backendcontext.h"

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythcorecontext.h"

QMap<int, EncoderLink *> gTVList;
AutoExpire  *gExpirer      = nullptr;
BackendContext *gBackendContext = nullptr;

BackendContext::~BackendContext()
{
    QMap<QString, Frontend*>::iterator it = m_knownFrontends.begin();
    while (it != m_knownFrontends.end())
    {
        Frontend *fe = (*it);
        delete fe;
        fe = nullptr;
        ++it;
    }

    m_connectedFrontends.clear();
    m_knownFrontends.clear();
}

void BackendContext::SetFrontendConnected(Frontend *frontend)
{
    if (!frontend || frontend->m_name.isEmpty())
        return;

    gCoreContext->SendSystemEvent(
                QString("CLIENT_CONNECTED HOSTNAME %1").arg(frontend->m_name));

    if (m_knownFrontends.contains(frontend->m_name))
    {
        Frontend *fe = m_knownFrontends.value(frontend->m_name);
        // Frontend may have changed IP since we last saw it
        fe->m_ip = frontend->m_ip;
        delete frontend;
        frontend = nullptr;

        if (!m_connectedFrontends.contains(fe->m_name))
        {
            m_connectedFrontends.insert(fe->m_name, fe);
            LOG(VB_GENERAL, LOG_INFO, QString("BackendContext: Frontend '%1' "
                                      "connected.").arg(fe->m_name));
        }

        fe->m_connectionCount++;
        LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Increasing "
                                           "connection count for (%1) to %2 ")
                                            .arg(fe->m_name)
                                            .arg(fe->m_connectionCount));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("BackendContext: Frontend '%1' "
                                      "connected.").arg(frontend->m_name));

    frontend->m_connectionCount++;
    m_connectedFrontends.insert(frontend->m_name, frontend);

    // TODO: We want to store this information in the database so that
    //       it persists between backend restarts. We can then give users
    //       an overview of the number of frontends on the network, those
    //       connected at any given moment and in the future give them the
    //       option to forget clients including associated settings, deny
    //       unknown clients from connecting and other cool stuff
    m_knownFrontends.insert(frontend->m_name, frontend);

}

void BackendContext::SetFrontendDisconnected(const QString& name)
{
    if (!m_connectedFrontends.contains(name))
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Disconnect requested "
                                           "for frontend (%1) which isn't "
                                           "registered. ").arg(name));
        return;
    }

    Frontend *frontend = m_connectedFrontends.value(name);
    if (frontend == nullptr)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Disconnect requested "
                                           "for frontend (%1) with null pointer.")
                                           .arg(name));
        return;
    }

    frontend->m_connectionCount--;
    LOG(VB_GENERAL, LOG_DEBUG, QString("BackendContext: Decreasing "
                                       "connection count for (%1) to %2 ")
                                        .arg(frontend->m_name)
                                        .arg(frontend->m_connectionCount));
    if (frontend->m_connectionCount <= 0)
    {
        // Will still be referenced in knownFrontends, so no leak here
        m_connectedFrontends.remove(name);

        gCoreContext->SendSystemEvent(
                QString("CLIENT_DISCONNECTED HOSTNAME %1")
                        .arg(frontend->m_name));
        LOG(VB_GENERAL, LOG_INFO, QString("BackendContext: Frontend '%1' "
                                          "disconnected.").arg(frontend->m_name));
    }
}
