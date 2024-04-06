// Qt
#include <QFile>

// MythTV
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythhttps.h"

#define LOC QString("SSL: ")

#ifndef QT_NO_OPENSSL
bool MythHTTPS::InitSSLServer(QSslConfiguration& Config)
{
    if (!QSslSocket::supportsSsl())
        return false;

    LOG(VB_HTTP, LOG_INFO, LOC + QSslSocket::sslLibraryVersionString());

    Config = QSslConfiguration::defaultConfiguration();
    Config.setProtocol(QSsl::SecureProtocols); // Includes SSLv3 which is insecure, but can't be helped
    Config.setSslOption(QSsl::SslOptionDisableLegacyRenegotiation, true); // Potential DoS multiplier
    Config.setSslOption(QSsl::SslOptionDisableCompression, true); // CRIME attack

    auto availableCiphers = QSslConfiguration::supportedCiphers();
    QList<QSslCipher> secureCiphers;
    for (const auto & cipher : std::as_const(availableCiphers))
    {
        // Remove weak ciphers from the cipher list
        if (cipher.usedBits() < 128)
            continue;

        if (cipher.name().startsWith("RC4") || // Weak cipher
            cipher.name().startsWith("EXP") || // Weak authentication
            cipher.name().startsWith("ADH") || // No authentication
            cipher.name().contains("NULL"))    // No encryption
            continue;

        secureCiphers.append(cipher);
    }
    Config.setCiphers(secureCiphers);

    // Fallback to the config directory if no cert settings
    auto configdir = GetConfDir();
    while (configdir.endsWith("/"))
        configdir.chop(1);
    configdir.append(QStringLiteral("/certificates/"));

    auto hostKeyPath = gCoreContext->GetSetting("hostSSLKey", "");
    if (hostKeyPath.isEmpty())
        hostKeyPath = configdir + "key.pem";

    QFile hostKeyFile(hostKeyPath);
    if (!hostKeyFile.exists() || !hostKeyFile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("SSL Host key file (%1) does not exist or is not readable").arg(hostKeyPath));
        return false;
    }

    auto rawHostKey = hostKeyFile.readAll();
    auto hostKey = QSslKey(rawHostKey, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (!hostKey.isNull())
    {
        Config.setPrivateKey(hostKey);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to load host key from file (%1)").arg(hostKeyPath));
        return false;
    }

    auto hostCertPath = gCoreContext->GetSetting("hostSSLCertificate", "");
    if (hostCertPath.isEmpty())
        hostCertPath = configdir + "cert.pem";

    QSslCertificate hostCert;
    auto certList = QSslCertificate::fromPath(hostCertPath);
    if (!certList.isEmpty())
        hostCert = certList.first();

    if (!hostCert.isNull())
    {
        if (hostCert.effectiveDate() > QDateTime::currentDateTime())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Host certificate start date in future (%1)").arg(hostCertPath));
            return false;
        }

        if (hostCert.expiryDate() < QDateTime::currentDateTime())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Host certificate has expired (%1)").arg(hostCertPath));
            return false;
        }

        Config.setLocalCertificate(hostCert);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to load host cert from file (%1)").arg(hostCertPath));
        return false;
    }

    auto caCertPath = gCoreContext->GetSetting("caSSLCertificate", "");
    auto CACertList = QSslCertificate::fromPath(caCertPath);
    if (!CACertList.isEmpty())
    {
        Config.setCaCertificates(CACertList);
    }
    else if (!caCertPath.isEmpty())
    {
        // Only warn if a path was actually configured, this isn't an error otherwise
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to load CA cert file (%1)").arg(caCertPath));
    }
    return true;
}

void MythHTTPS::InitSSLSocket(QSslSocket *Socket, QSslConfiguration& Config)
{
    if (!Socket)
        return;

    auto Encrypted = [](const QSslSocket* SslSocket)
    {
        LOG(VB_HTTP, LOG_INFO, LOC + "Socket encrypted");
        if (SslSocket)
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Cypher: %1").arg(SslSocket->sessionCipher().name()));
    };

    auto SSLErrors = [](const QList<QSslError>& Errors)
    {
        for (const auto & error : Errors)
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("SslError: %1").arg(error.errorString()));
    };

    QObject::connect(Socket, &QSslSocket::encrypted, [Encrypted, Socket] { Encrypted(Socket); } );
    QObject::connect(Socket, qOverload<const QList<QSslError> &>(&QSslSocket::sslErrors), SSLErrors);
    Socket->setSslConfiguration(Config);
    Socket->startServerEncryption();
}
#endif
