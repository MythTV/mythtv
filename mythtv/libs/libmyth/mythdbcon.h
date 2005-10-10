#ifndef MYTHDBCON_H_
#define MYTHDBCON_H_

#include <qsqldatabase.h>
#include <qmutex.h>
#include <qsemaphore.h> 
#include <qstring.h>
#include <qptrlist.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"

/// \brief QSqlDatabase wrapper, used by MSqlQuery. Do not use directly.
class MSqlDatabase
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
    QSqlDatabase *db(void) { return m_db; }

  private:
    QString m_name;
    QSqlDatabase *m_db;
    QDateTime m_lastDBKick;
};

/// \brief DB connection pool, used by MSqlQuery. Do not use directly.
class MDBManager
{
  friend class MSqlQuery;
  public:
    MDBManager(void);
    ~MDBManager(void);

  protected:
    MSqlDatabase *popConnection(void);
    void pushConnection(MSqlDatabase *db);

    MSqlDatabase *getSchedCon(void);
    MSqlDatabase *getDDCon(void);

  private:
    QPtrList<MSqlDatabase> m_pool;
    QMutex m_lock;
    QSemaphore *m_sem;
    int m_connID;

    MSqlDatabase *m_schedCon;
    MSqlDatabase *m_DDCon;
};

/// \brief MSqlDatabase Info, used by MSqlQuery. Do not use directly.
typedef struct _MSqlQueryInfo
{
    MSqlDatabase *db;
    QSqlDatabase *qsqldb;
    bool returnConnection;
} MSqlQueryInfo;

/// \brief typedef for a map of string -> string bindings for generic queries.
typedef QMap<QString, QVariant> MSqlBindings;

/// \brief Add the entries in addfrom to the map in output
void MSqlAddMoreBindings(MSqlBindings &output, MSqlBindings &addfrom);

/// \brief Given a partial query string and a bindings object, escape the string
void MSqlEscapeAsAQuery(QString &query, MSqlBindings &bindings);

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
class MSqlQuery : public QSqlQuery
{
  public:
    /// \brief Get DB connection from pool 
    MSqlQuery(const MSqlQueryInfo &qi);
    /// \brief Returns conneciton to pool
    ~MSqlQuery();

    /// \brief Only updated once during object creation
    bool isConnected(void) { return m_isConnected; }

    /// \brief QSqlQuery::prepare() is not thread safe in Qt <= 3.3.2
    bool prepare(const QString &query);

    /// \brief Add all the bindings in the passed in bindings
    void bindValues(MSqlBindings &bindings);

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
};

#endif    
