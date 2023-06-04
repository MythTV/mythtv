#include "bonjourregister.h"

#include <cstdlib>
#include <limits> // workaround QTBUG-90395

#include <QSocketNotifier>
#include <QtEndian>

#include "mythrandom.h"
#include "mythlogging.h"

#define LOC QString("Bonjour: ")

QMutex BonjourRegister::g_lock;

BonjourRegister::BonjourRegister(QObject *parent)
    : QObject(parent)
{
    setenv("AVAHI_COMPAT_NOWARN", "1", 1);
}

BonjourRegister::~BonjourRegister()
{
    if (m_socket) {
        m_socket->setEnabled(false);
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_dnssref)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("De-registering service '%1' on '%2'")
            .arg(m_type.data(), m_name.data()));
        DNSServiceRefDeallocate(m_dnssref);
    }
    m_dnssref = nullptr;

    delete m_lock;
    m_lock = nullptr;
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
    m_data = txt;

    uint16_t qport = qToBigEndian(port);
    DNSServiceErrorType res =
        DNSServiceRegister(&m_dnssref, 0, 0, name.data(), type.data(),
                           nullptr, nullptr, qport, txt.size(), (void*)txt.data(),
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
            connect(m_socket, &QSocketNotifier::activated,
                    this, &BonjourRegister::socketReadyRead);
            delete m_lock; // would already have been deleted, but just in case
            m_lock = nullptr;
            return true;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register service.");
    delete m_lock;
    m_lock = nullptr;

    return false;
}


void BonjourRegister::socketReadyRead()
{
    DNSServiceErrorType res = DNSServiceProcessResult(m_dnssref);
    if (kDNSServiceErr_NoError != res)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Read Error: %1").arg(res));
}


void BonjourRegister::BonjourCallback([[maybe_unused]] DNSServiceRef ref,
                                      [[maybe_unused]] DNSServiceFlags flags,
                                      DNSServiceErrorType errorcode,
                                      const char *name, const char *type,
                                      const char *domain, void *object)
{
    auto *bonjour = static_cast<BonjourRegister *>(object);
    if (bonjour)
    {
        delete bonjour->m_lock;
        bonjour->m_lock = nullptr;
    }

    if (kDNSServiceErr_NoError != errorcode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Callback Error: %1")
            .arg(errorcode));
    }
    else if (bonjour)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Service registration complete: name '%1' type '%2' domain: '%3'")
            .arg(QString::fromUtf8(name), QString::fromUtf8(type),
                 QString::fromUtf8(domain)));
        bonjour->m_name = name;
        bonjour->m_type = type;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("BonjourCallback for unknown object."));
    }
}

QByteArray BonjourRegister::RandomizeData(void)
{
    QByteArray data = m_data;
    QByteArray rnd;

    data.append(7);
    data.append("_rnd=");
    rnd.append(MythRandom(33, 33 + 80 - 1));
    data.append(rnd.toHex());

    return data;
}

bool BonjourRegister::ReAnnounceService(void)
{
    if (!m_dnssref)
    {
        // nothing to refresh
        return false;
    }

    QByteArray data = RandomizeData();

    DNSServiceErrorType res =
        DNSServiceUpdateRecord(m_dnssref,           /* DNSServiceRef sdRef */
                               nullptr,             /* DNSRecordRef RecordRef */
                               0,                   /* DNSServiceFlags flags */
                               data.size(),         /* uint16_t rdlen */
                               (void*)data.data(),  /* const void *rdata */
                               0);                  /* uint32_t ttl */

    if (kDNSServiceErr_NoError != res)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error ReAnnounceService(%1): %2")
            .arg(m_name.data()).arg(res));
    }
    return kDNSServiceErr_NoError != res;
}
