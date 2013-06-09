#ifndef MYTHDBCON_H_
#define MYTHDBCON_H_

#include <QSqlDatabase>
#include <QSqlRecord>
#include <QSqlError>
#include <QVariant>
#include <QSqlQuery>
#include <QRegExp>
#include <QDateTime>
#include <QMutex>
#include <QList>

#include "mythbaseexp.h"
#include "mythdbparams.h"

#define REUSE_CONNECTION 1

MBASE_PUBLIC bool TestDatabase(QString dbHostName,
                               QString dbUserName,
                               QString dbPassword,
                               QString dbName = "mythconverg",
                               int     dbPort = 3306);

/// \brief QSqlDatabase wrapper, used by MSqlQuery. Do not use directly.
class MSqlDatabase
{
  friend class MDBManager;
  friend class MSqlQuery;
  public:
    MSqlDatabase(const QString &name);
   ~MSqlDatabase(void);

    bool OpenDatabase(bool skipdb = false);
    void SetDBParams(const DatabaseParams &params) { m_dbparms = params; };

  private:
    bool isOpen(void);
    bool KickDatabase(void);
    QString GetConnectionName(void) const { return m_name; }
    QSqlDatabase db(void) const { return m_db; }
    bool Reconnect(void);
    void InitSessionVars(void);

  private:
    QString m_name;
    QSqlDatabase m_db;
    QDateTime m_lastDBKick;
    DatabaseParams m_dbparms;
};

/// \brief DB connection pool, used by MSqlQuery. Do not use directly.
class MBASE_PUBLIC MDBManager
{
  friend class MSqlQuery;
  public:
    MDBManager(void);
    ~MDBManager(void);

    void CloseDatabases(void);
    void PurgeIdleConnections(bool leaveOne = false);

  protected:
    MSqlDatabase *popConnection(bool reuse);
    void pushConnection(MSqlDatabase *db);

    MSqlDatabase *getSchedCon(void);
    MSqlDatabase *getDDCon(void);

  private:
    MSqlDatabase *getStaticCon(MSqlDatabase **dbcon, QString name);

    QMutex m_lock;
    typedef QList<MSqlDatabase*> DBList;
    QHash<QThread*, DBList> m_pool; // protected by m_lock
#if REUSE_CONNECTION 
    QHash<QThread*, MSqlDatabase*> m_inuse; // protected by m_lock
    QHash<QThread*, int> m_inuse_count; // protected by m_lock
#endif

    int m_nextConnID;
    int m_connCount;

    MSqlDatabase *m_schedCon;
    MSqlDatabase *m_DDCon;
    QHash<QThread*, DBList> m_static_pool;
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
 MBASE_PUBLIC  void MSqlAddMoreBindings(MSqlBindings &output, MSqlBindings &addfrom);

/// \brief Given a partial query string and a bindings object, escape the string
 MBASE_PUBLIC  void MSqlEscapeAsAQuery(QString &query, MSqlBindings &bindings);

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
class MBASE_PUBLIC MSqlQuery : private QSqlQuery
{
    MBASE_PUBLIC friend void MSqlEscapeAsAQuery(QString&, MSqlBindings&);
  public:
    /// \brief Get DB connection from pool
    MSqlQuery(const MSqlQueryInfo &qi);
    /// \brief Returns connection to pool
    ~MSqlQuery();

    /// \brief Only updated once during object creation
    bool isConnected(void) { return m_isConnected; }

    /// \brief Wrap QSqlQuery::exec() so we can display SQL
    bool exec(void);

    /// \brief Wrap QSqlQuery::next() so we can display the query results
    bool next(void);

    /// \brief Wrap QSqlQuery::previous() so we can display the query results
    bool previous(void);

    /// \brief Wrap QSqlQuery::first() so we can display the query results
    bool first(void);

    /// \brief Wrap QSqlQuery::last() so we can display the query results
    bool last(void);

    /// \brief Wrap QSqlQuery::seek(int,bool)
    //         so we can display the query results
    bool seek(int, bool relative = false);

    /// \brief Wrap QSqlQuery::exec(const QString &query) so we can display SQL
    bool exec(const QString &query);

    /// \brief QSqlQuery::prepare() is not thread safe in Qt <= 3.3.2
    bool prepare(const QString &query);

    void bindValue(const QString &placeholder, const QVariant &val);

    /// \brief Add all the bindings in the passed in bindings
    void bindValues(const MSqlBindings &bindings);

    /** \brief Return the id of the last inserted row
     *
     * Note: Currently, this function is only implemented in Qt4 (in QSqlQuery
     * and QSqlResult) 
     * The current implementation will only work for a DBMS that supports
     * the function LAST_INSERT_ID() (i.e. MySQL), and will _not_ work
     * in SQLite.
     */
    QVariant lastInsertId();

    /// Reconnects server and re-prepares and re-binds the last prepared
    /// query.
    bool Reconnect(void);

    // Thunks that allow us to make QSqlQuery private
    QVariant value(int i) const { return QSqlQuery::value(i); }
    QString executedQuery(void) const { return QSqlQuery::executedQuery(); }
    QMap<QString, QVariant> boundValues(void) const
        { return QSqlQuery::boundValues(); }
    QSqlError lastError(void) const { return QSqlQuery::lastError(); }
    int size(void) const { return QSqlQuery::size();}
    bool isActive(void) const { return  QSqlQuery::isActive(); }
    QSqlRecord record(void) const { return QSqlQuery::record(); }
    int numRowsAffected() const { return QSqlQuery::numRowsAffected(); }
    void setForwardOnly(bool f) { QSqlQuery::setForwardOnly(f); }
    bool isNull(int field) const { return QSqlQuery::isNull(field); }
    const QSqlDriver *driver(void) const { return QSqlQuery::driver(); }
    int at(void) const { return QSqlQuery::at(); }

    /// \brief Checks DB connection + login (login info via Mythcontext)
    static bool testDBConnection();

    typedef enum
    {
        kDedicatedConnection,
        kNormalConnection,
    } ConnectionReuse;
    /// \brief Only use this in combination with MSqlQuery constructor
    static MSqlQueryInfo InitCon(ConnectionReuse = kNormalConnection);

    /// \brief Returns dedicated connection. (Required for using temporary SQL tables.)
    static MSqlQueryInfo SchedCon();

    /// \brief Returns dedicated connection. (Required for using temporary SQL tables.)
    static MSqlQueryInfo DDCon();

  private:
    // Only QSql::In is supported as a param type and only named params...
    void bindValue(const QString&, const QVariant&, QSql::ParamType);
    void bindValue(int, const QVariant&, QSql::ParamType);
    void addBindValue(const QVariant&, QSql::ParamType = QSql::In);

    bool seekDebug(const char *type, bool result,
                   int where, bool relative) const;

    MSqlDatabase *m_db;
    bool m_isConnected;
    bool m_returnConnection;
    QString m_last_prepared_query; // holds a copy of the last prepared query
#ifdef DEBUG_QT4_PORT
    QRegExp m_testbindings;
#endif
};

#endif
