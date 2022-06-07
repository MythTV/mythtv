
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
bool MythUserSession::CheckPermission(const QString &/*context*/,
                                      uint /*permission*/)
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

    return query.exec();
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

    return query.next();
}

/**
 * \public
 */
MythUserSession MythSessionManager::GetSession(const QString& sessionToken)
{
    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return {};
    }

    if (IsValidSession(sessionToken))
        return m_sessionList[sessionToken];

    return {};
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
        return {};
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

    return {};
}

/**
 * \public
 */
QString MythSessionManager::GetPasswordDigest(const QString& username)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT password_digest FROM users WHERE username = :USERNAME");
    query.bindValue(":USERNAME", username);

    if (!query.exec())
        MythDB::DBError("Error finding user", query);

    if (query.next())
        return query.value(0).toString();

    return {};
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
        return {};

    if (!gCoreContext->IsMasterBackend())
    {
        // TODO: Connect to master and do checking there
        return {};
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT userid, username FROM users WHERE "
                  "username = :USERNAME AND password_digest = :PWDIGEST");

    query.bindValue(":USERNAME", username);
    query.bindValue(":PWDIGEST", QString(digest));

    if (!query.exec())
        return {};

    if (query.size() > 1)
    {
        LOG(VB_GENERAL, LOG_CRIT, "LoginUser: Warning, multiple matching user records found.");
        return {};
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

    return {};
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

        clientIdentifier =
            QString("%1_%2").arg(type, gCoreContext->GetHostName());
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
 * \private
 */
bool MythSessionManager::AddDigestUser(const QString& username,
                                       const QString& password,
                                       const QString& adminPassword)
{
    if (adminPassword.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Admin password is missing."));
        return false;
    }

    if (IsValidUser(username))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Tried to add an existing user: %1.")
                                         .arg(username));
        return false;
    }

    if (CreateDigest("admin", adminPassword) != GetPasswordDigest("admin"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Incorrect password for user: %1.")
                                         .arg("admin"));
        return false;
    }

    MSqlQuery insert(MSqlQuery::InitCon());
    insert.prepare("INSERT INTO users SET "
                      "username = :USER_NAME, "
                      "password_digest = :PASSWORD_DIGEST");
    insert.bindValue(":USER_NAME", username);
    insert.bindValue(":PASSWORD_DIGEST", CreateDigest(username, password));

    bool bResult = insert.exec();
    if (!bResult)
        MythDB::DBError("Error adding digest user to database", insert);

    return bResult;
}

/**
 * \private
 */
bool MythSessionManager::RemoveDigestUser(const QString& username,
                                          const QString& password)
{
    if (!IsValidUser(username))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Tried to remove a non-existing "
                                         "user: %1.").arg(username));
        return false;
    }

    if (username == "admin")
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Tried to remove user: %1 (not "
                                         "permitted.)").arg("admin"));
        return false;
    }

    if (CreateDigest(username, password) != GetPasswordDigest(username))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Incorrect password for user: %1.")
                                         .arg(username));
        return false;
    }

    MSqlQuery deleteQuery(MSqlQuery::InitCon());
    deleteQuery.prepare("DELETE FROM users WHERE " "username = :USER_NAME ");
    deleteQuery.bindValue(":USER_NAME", username);

    bool bResult = deleteQuery.exec();
    if (!bResult)
        MythDB::DBError("Error removing digest user from database",
                        deleteQuery);

    return bResult;
}

/**
 * \private
 */
bool MythSessionManager::ChangeDigestUserPassword(const QString& username,
                                                  const QString& oldPassword,
                                                  const QString& newPassword)
{
    if (newPassword.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("New password is missing."));
        return false;
    }

    if (!IsValidUser(username))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Attempted to update non-existing"
                                         " user: %1.").arg(username));
        return false;
    }

    QByteArray oldPasswordDigest = CreateDigest(username, oldPassword);

    if (oldPasswordDigest != GetPasswordDigest(username))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Incorrect old password for "
                                         "user: %1.").arg(username));
        return false;
    }

    MSqlQuery update(MSqlQuery::InitCon());
    update.prepare("UPDATE users SET "
                      "password_digest = :NEW_PASSWORD_DIGEST WHERE "
                      "username = :USER_NAME AND "
                      "password_digest = :OLD_PASSWORD_DIGEST");
    update.bindValue(":NEW_PASSWORD_DIGEST", CreateDigest(username,
                                                          newPassword));
    update.bindValue(":USER_NAME", username);
    update.bindValue(":OLD_PASSWORD_DIGEST", oldPasswordDigest);

    bool bResult = update.exec();
    if (!bResult)
        MythDB::DBError("Error updating digest user in database", update);

    return bResult;
}

/**
 * \public
 */
QByteArray MythSessionManager::CreateDigest(const QString &username,
                                            const QString &password)
{
    // The realm is a constant, it's not private because it's included in the
    // plain text sent with an HTTP WWW-Authenticate header.
    QString plainText = QString("%1:MythTV:%2").arg(username, password);
    QByteArray digest = QCryptographicHash::hash(plainText.toLatin1(),
                                                 QCryptographicHash::Md5).toHex();
    return digest;
}

/**
 * \public
 */
bool MythSessionManager::ManageDigestUser(DigestUserActions action,
                                          const QString&    username,
                                          const QString&    password,
                                          const QString&    newPassword,
                                          const QString&    adminPassword)
{
    bool returnCode = false;

    if (username.isEmpty() || password.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, QString("Username and password required."));
    else if (action == DIGEST_USER_ADD)
        returnCode = AddDigestUser(username, password, adminPassword);
    else if (action == DIGEST_USER_REMOVE)
        returnCode = RemoveDigestUser(username, password);
    else if (action == DIGEST_USER_CHANGE_PW)
        returnCode = ChangeDigestUserPassword(username, password, newPassword);
    else
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown action."));

    return returnCode;
}
