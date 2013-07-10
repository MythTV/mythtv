#include <QSocketNotifier>
#include <QtEndian>

#include <stdlib.h>

#include "mythlogging.h"
#include "bonjourregister.h"

#define LOC QString("Bonjour: ")

QMutex BonjourRegister::g_lock;

BonjourRegister::BonjourRegister(QObject *parent)
    : QObject(parent), m_dnssref(0), m_socket(NULL), m_lock(NULL)
{
    setenv("AVAHI_COMPAT_NOWARN", "1", 1);
}

BonjourRegister::~BonjourRegister()
{
    if (m_socket)
        m_socket->setEnabled(false);

    if (m_dnssref)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("De-registering service '%1' on '%2'")
            .arg(m_type.data()).arg(m_name.data()));
        DNSServiceRefDeallocate(m_dnssref);
    }
    m_dnssref = 0;

    m_socket->deleteLater();
    m_socket = NULL;
    delete m_lock;
    m_lock = NULL;
}

bool BonjourRegister::Register(uint16_t port, const QByteArray &type,
                              const QByteArray &name, const QByteArray &txt)
{
    if (m_dnssref)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Service already registered.");
        return true;
    }

    m_lock = new QMutexLocker(&g_lock);

    uint16_t qport = qToBigEndian(port);
    DNSServiceErrorType res =
        DNSServiceRegister(&m_dnssref, 0, 0, (const char*)name.data(),
                           (const char*)type.data(),
                           NULL, 0, qport, txt.size(), (void*)txt.data(),
                           BonjourCallback, this);

    if (kDNSServiceErr_NoError != res)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error: %1").arg(res));
    }
    else
    {
        int fd = DNSServiceRefSockFD(m_dnssref);
        if (fd != -1)
        {
            m_socket = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            m_socket->setEnabled(true);
            connect(m_socket, SIGNAL(activated(int)),
                    this, SLOT(socketReadyRead()));
            delete m_lock; // would already have been deleted, but just in case
            m_lock = NULL;
            return true;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register service.");
    delete m_lock;
    m_lock = NULL;

    return false;
}


void BonjourRegister::socketReadyRead()
{
    DNSServiceErrorType res = DNSServiceProcessResult(m_dnssref);
    if (kDNSServiceErr_NoError != res)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Read Error: %1").arg(res));
}


void BonjourRegister::BonjourCallback(DNSServiceRef ref, DNSServiceFlags flags,
                                      DNSServiceErrorType errorcode,
                                      const char *name, const char *type,
                                      const char *domain, void *object)
{
    (void)ref;
    (void)flags;

    BonjourRegister *bonjour = static_cast<BonjourRegister *>(object);
    delete bonjour->m_lock;
    bonjour->m_lock = NULL;

    if (kDNSServiceErr_NoError != errorcode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Callback Error: %1")
            .arg(errorcode));
    }
    else if (bonjour)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Service registration complete: name '%1' type '%2' domain: '%3'")
            .arg(QString::fromUtf8(name)).arg(QString::fromUtf8(type))
            .arg(QString::fromUtf8(domain)));
        bonjour->m_name = name;
        bonjour->m_type = type;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("BonjourCallback for unknown object."));
    }
}

