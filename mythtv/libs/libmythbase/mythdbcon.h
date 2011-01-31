#ifndef MYTHDBCON_H_
#define MYTHDBCON_H_

#include <QSqlDatabase>
#include <QVariant>
#include <QSqlQuery>
#include <QRegExp>
#include <QDateTime>
#include <QMutex>
#include <QList>

#include "mythexp.h"

class QSemaphore;

/// \brief QSqlDatabase wrapper, used by MSqlQuery. Do not use directly.
class MPUBLIC MSqlDatabase
{
  friend class MDBManager;
  friend class MSqlQuery;
  public:
    MSqlDatabase(const QString &name);
   ~MSqlDatabase(void);

  private:
    bool isOpen(void);
    bool OpenDatabase(void);
    bool KickDatabase(void);
    QString GetConnectionName(void) const { return m_name; }
    QSqlDatabase db(void) const { return m_db; }
    bool Reconnect(void);

  private:
    QString m_name;
    QSqlDatabase m_db;
    QDateTime m_lastDBKick;
};

/// \brief DB connection pool, used by MSqlQuery. Do not use directly.
class MPUBLIC MDBManager
{
  friend class MSqlQuery;
  public:
    MDBManager(void);
    ~MDBManager(void);

    void CloseDatabases(void);
    void PurgeIdleConnections(void);

  protected:
    MSqlDatabase *popConnection(void);
    void pushConnection(MSqlDatabase *db);

    MSqlDatabase *getSchedCon(void);
    MSqlDatabase *getDDCon(void);

  private:
    QList<MSqlDatabase*> m_pool;
    QMutex m_lock;
    QSemaphore *m_sem;
    int m_nextConnID;
    int m_connCount;

    MSqlDatabase *m_schedCon;
    MSqlDatabase *m_DDCon;
};

/// \brief MSqlDatabase Info, used by MSqlQuery. Do not use directly.
typedef struct _MSqlQueryInfo
{
    MSqlDatabase *db;
    QSqlDatabase qsqldb;
    bool returnConnection;
} MSqlQueryInfo;

/// \brief typedef for a map of string -> string bindings for generic queries.
typedef QMap<QString, QVariant> MSqlBindings;

/// \brief Add the entries in addfrom to the map in output
MPUBLIC void MSqlAddMoreBindings(MSqlBindings &output, MSqlBindings &addfrom);

/// \brief Given a partial query string and a bindings object, escape the string
MPUBLIC void MSqlEscapeAsAQuery(QString &query, MSqlBindings &bindings);

/** \brief QSqlQuery wrapper that fetches a DB connection from the connection pool.
 *
 *   Myth & database connections
 *   Rule #1: Never use QSqlQuery or QSqlDatabase directly.
 *   Rule #2: Never use QSqlQuery or QSqlDatabase directly.
 *   Rule #3: Use MSqlQuery for all DB stuff.
 *
 *   MSqlQuery is tied to a connection pool in MythContext. DB connections are
 *   automatically set up by creating an MSqlQuery object. Use the helper
 *   functions to create an MSqlQuery object e.g.
 *   MSqlQuery query(MSqlQuery::InitCon());
 *   The MSqlQuery object gets exclusive access to the connection for its
 *   lifetime. The connection is automatically returned when the MSqlQuery
 *   object is destroyed.
 *
 *   Note: Due to a bug in some Qt/MySql combinations, QSqlDatabase connections
 *   will crash if closed and reopend - so we never close them and keep them in
 *   a pool.
 */
class MPUBLIC MSqlQuery : public QSqlQuery
{
  public:
    /// \brief Get DB connection from pool
    MSqlQuery(const MSqlQueryInfo &qi);
    /// \brief Returns conneciton to pool
    ~MSqlQuery();

    /// \brief Only updated once during object creation
    bool isConnected(void) { return m_isConnected; }

    /// \brief Wrap QSqlQuery::exec() so we can display SQL
    bool exec(void);

    /// \brief Wrap QSqlQuery::next() so we can display the query results
    bool next(void);

    /// \brief Wrap QSqlQuery::exec(const QString &query) so we can display SQL
    bool exec(const QString &query);

    /// \brief QSqlQuery::prepare() is not thread safe in Qt <= 3.3.2
    bool prepare(const QString &query);

    /// \brief Wrap QSqlQuery::bindValue so we can convert null QStrings to empty QStrings
    void bindValue ( const QString & placeholder, const QVariant & val, QSql::ParamType paramType = QSql::In );
    /// \brief Wrap QSqlQuery::bindValue so we can convert null QStrings to empty QStrings
    void bindValue ( int pos, const QVariant & val, QSql::ParamType paramType = QSql::In );

    /// \brief Add all the bindings in the passed in bindings
    void bindValues(MSqlBindings &bindings);

    /** \brief Return the id of the last inserted row
     *
     * Note: Currently, this function is only implemented in Qt4 (in QSqlQuery
     * and QSqlResult), and is implemented here until the switch to Qt4.  Also,
     * the current implementation will only work for a DBMS that supports
     * the function LAST_INSERT_ID() (i.e. MySQL), and will _not_ work
     * in SQLite.
     */
    QVariant lastInsertId();

    /// \brief Checks DB connection + login (login info via Mythcontext)
    static bool testDBConnection();

    /// \brief Only use this in combination with MSqlQuery constructor
    static MSqlQueryInfo InitCon();

    /// \brief Returns dedicated connection. (Required for using temporary SQL tables.)
    static MSqlQueryInfo SchedCon();

    /// \brief Returns dedicated connection. (Required for using temporary SQL tables.)
    static MSqlQueryInfo DDCon();

  private:
    MSqlDatabase *m_db;
    bool m_isConnected;
    bool m_returnConnection;
    QString m_last_prepared_query; // holds a copy of the last prepared query
#ifdef DEBUG_QT4_PORT
    QRegExp m_testbindings;
#endif
};

#endif
