
#include "mythsession.h"

#include "mythdbcon.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythdate.h"

#include <QCryptographicHash>
#include <QUuid>

/**
 * \public
 */
bool MythUserSession::IsValid(void) const
{
    if (m_userId == 0)
        return false;

    if (m_sessionToken.isEmpty() || m_sessionToken.length() != 40)
        return false;

    if (m_sessionCreated > MythDate::current())
        return false;

    if (m_sessionExpires < MythDate::current())
        return false;

    // NOTE: Check client string as well?

    return true;
}

/**
 * \public
 */
bool MythUserSession::CheckPermission(const QString &context,
                                      uint permission)
{
    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return false;
    }

    Update();

    // TODO: Implement perms checking here

    return false;
}

/**
 * \protected
 */
bool MythUserSession::Save(void)
{
    if (m_userId == 0)
        return false;

    if (m_sessionToken.isEmpty())
    {
        QByteArray randBytes = QUuid::createUuid().toByteArray();
        m_sessionToken = QCryptographicHash::hash(randBytes,
                                                  QCryptographicHash::Sha1).toHex();
    }

    if (m_sessionCreated.isNull())
        m_sessionCreated = MythDate::current();
    if (m_sessionLastActive.isNull())
        m_sessionLastActive = MythDate::current();
    if (m_sessionExpires.isNull())
        m_sessionExpires = MythDate::current().addDays(1);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("REPLACE INTO user_sessions SET "
                  "sessionToken = :SESSION_TOKEN, "
                  "userid = :USERID, "
                  "client = :CLIENT, "
                  "created = :CREATED, "
                  "lastactive = :LASTACTIVE, "
                  "expires = :EXPIRES");
    query.bindValue(":SESSION_TOKEN", m_sessionToken);
    query.bindValue(":USERID", m_userId);
    query.bindValue(":CLIENT", m_sessionClient);
    query.bindValue(":CREATED", m_sessionCreated);
    query.bindValue(":LASTACTIVE", m_sessionLastActive);
    // For now, fixed duration of 1 day, this should be different for the
    // WebFrontend (external client) vs the Frontend (local client). It should
    // perhaps be configurable. The expiry date is extended every time the
    // the session is validated, for the WebFrontend this is every http
    // connection for other clients this has yet to be defined
    query.bindValue(":EXPIRES", m_sessionExpires);

    if (query.exec())
        return true;

    return false;
}

/**
 * \protected
 */
bool MythUserSession::Update(void)
{
    m_sessionLastActive = MythDate::current();
    m_sessionExpires = MythDate::current().addDays(1);
    return Save();
}

/**
 * \public
 */
MythSessionManager::MythSessionManager()
{
    if (gCoreContext->IsMasterBackend())
        LoadSessions();
}

/**
 * \public
 */
MythSessionManager::~MythSessionManager()
{

}

/**
 * \private
 */
void MythSessionManager::LoadSessions(void)
{
    m_sessionList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT s.sessiontoken, s.created, s.lastactive, s.expires, "
                  "       s.client, u.userid, u.username "
                  "FROM user_sessions s, users u");

    if (!query.exec())
        MythDB::DBError("Error loading user sessions from database", query);

    while (query.next())
    {
        MythUserSession session;

        session.m_sessionToken = query.value(0).toString();
        session.m_sessionCreated = query.value(1).toDateTime();
        session.m_sessionLastActive = query.value(2).toDateTime();
        session.m_sessionExpires = query.value(3).toDateTime();
        session.m_sessionClient = query.value(4).toString();
        session.m_userId = query.value(5).toUInt();
        session.m_name = query.value(6).toString();

        m_sessionList.insert(session.m_sessionToken, session);
    }
}

/**
 * \public
 */
bool MythSessionManager::IsValidUser(const QString& username)
{
    if (username.isEmpty())
        return false;

    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return false;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT userid FROM users WHERE username = :USERNAME");
    query.bindValue(":USERNAME", username);

    if (!query.exec())
        MythDB::DBError("Error finding user", query);

    if (query.next())
        return true;

   return false;
}

/**
 * \public
 */
MythUserSession MythSessionManager::GetSession(const QString& sessionToken)
{
    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return MythUserSession();
    }

    if (IsValidSession(sessionToken))
        return m_sessionList[sessionToken];

    return MythUserSession();
}

/**
 * \private
 */
MythUserSession MythSessionManager::GetSession(const QString &username,
                                               const QString &client)
{
    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return MythUserSession();
    }

    QMap<QString, MythUserSession>::iterator it;
    for (it = m_sessionList.begin(); it != m_sessionList.end(); ++it)
    {
        if (((*it).m_name == username) &&
            ((*it).m_sessionClient == client))
        {
            if ((*it).IsValid())
            {
                (*it).Update();
                return *it;
            }

            DestroyUserSession((*it).m_sessionToken);
            break;
        }
    }

    return MythUserSession();
}

/**
 * \public
 */
const QString MythSessionManager::GetPasswordDigest(const QString& username)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT password_digest FROM users WHERE username = :USERNAME");
    query.bindValue(":USERNAME", username);

    if (!query.exec())
        MythDB::DBError("Error finding user", query);

    if (query.next())
        return query.value(0).toString();

    return QString();
}

/**
 * \public
 */
bool MythSessionManager::IsValidSession(const QString& sessionToken)
{
    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return false;
    }

    if (m_sessionList.contains(sessionToken))
    {
        MythUserSession session = m_sessionList[sessionToken];
        if (session.IsValid())
        {
            // Accessing a session automatically extends it
            UpdateSession(sessionToken);
            return true;
        }

        DestroyUserSession(sessionToken);
    }

    return false;
}

/**
 * \private
 */
void MythSessionManager::UpdateSession(const QString& sessionToken)
{
    if (m_sessionList.contains(sessionToken))
    {
        MythUserSession session = m_sessionList[sessionToken];
        session.Update(); // Update the database
        m_sessionList[sessionToken] = session; // Update the cache
    }
}

/**
 * \public
 */
MythUserSession MythSessionManager::LoginUser(const QString &username,
                                              const QByteArray &digest,
                                              const QString &client)
{
    if (username.isEmpty() || digest.isEmpty() || digest.length() < 32 ||
        digest.length() > 32)
        return MythUserSession();

    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return MythUserSession();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT userid, username FROM users WHERE "
                  "username = :USERNAME AND password_digest = :PWDIGEST");

    query.bindValue(":USERNAME", username);
    query.bindValue(":PWDIGEST", QString(digest));

    if (!query.exec())
        return MythUserSession();

    if (query.size() > 1)
    {
        LOG(VB_GENERAL, LOG_CRIT, "LoginUser: Warning, multiple matching user records found.");
        return MythUserSession();
    }

    if (query.next())
    {
        // Having verified the password, check if there is an existing session
        // open that we should re-use. This is necessary for unencrypted HTTP
        // Auth where the session token isn't stored on the client and we must
        // re-auth on every request
        MythUserSession session = GetSession(username, client);

        if (session.IsValid())
            return session;

        // No pre-existing session, so create a new one
        uint userId = query.value(0).toUInt();
        QString userName = query.value(1).toString();

        return CreateUserSession(userId, userName, client);
    }

    LOG(VB_GENERAL, LOG_WARNING, QString("LoginUser: Failed login attempt for "
                                         "user %1").arg(username));

    return MythUserSession();
}

/**
 * \public
 */
MythUserSession MythSessionManager::LoginUser(const QString &username,
                                                    const QString &password,
                                                    const QString &client)
{
    QByteArray digest = CreateDigest(username, password);

    return LoginUser(username, digest, client);
}

/**
 * \private
 */
MythUserSession MythSessionManager::CreateUserSession(uint userId,
                                                      const QString &userName,
                                                      const QString &client)
{
    MythUserSession session;

    session.m_userId = userId;
    session.m_name = userName;

    QString clientIdentifier = client;
    if (clientIdentifier.isEmpty())
    {
        QString type = "Master";
        if (!gCoreContext->IsMasterBackend())
            type = "Slave";

        clientIdentifier = QString("%1_%2").arg(type)
                                            .arg(gCoreContext->GetHostName());
    }

    session.m_sessionClient = clientIdentifier;

    session.m_sessionCreated = MythDate::current();
    session.m_sessionLastActive = MythDate::current();
    session.m_sessionExpires = MythDate::current().addDays(1);

    if (session.Save()) // Sets the session token
    {
        m_sessionList.insert(session.m_sessionToken, session);
    }

    return session;
}

/**
 * \private
 */
void MythSessionManager::DestroyUserSession(const QString &sessionToken)
{
    if (sessionToken.isEmpty())
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM user_sessions WHERE "
                  "sessionToken = :SESSION_TOKEN");
    query.bindValue(":SESSION_TOKEN", sessionToken);

    if (!query.exec())
    {
        MythDB::DBError("Error deleting user session from database", query);
    }

    if (m_sessionList.contains(sessionToken))
        m_sessionList.remove(sessionToken);
}

/**
 * \public
 */
QByteArray MythSessionManager::CreateDigest(const QString &username,
                                            const QString &password)
{
    // The realm is a constant, it's not private because it's included in the
    // plain text sent with an HTTP WWW-Authenticate header.
    QString plainText = QString("%1:MythTV:%2").arg(username).arg(password);
    QByteArray digest = QCryptographicHash::hash(plainText.toLatin1(),
                                                 QCryptographicHash::Md5).toHex();
    return digest;
}
