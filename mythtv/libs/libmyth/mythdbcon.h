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

// Myth & database connections

// Rule #1: Never use QSqlQuery or QSqlDatabase directly.
// Rule #2: Never use QSqlQuery or QSqlDatabase directly.
// Rule #3: Use MSqlQuery for all DB stuff.

// MSqlQuery is tied to a connection pool in Mythcontext. DB connections are
// automatically set up by creating an MSqlQuery object. Use the helper 
// functions to create an MSqlQuery object e.g. 
// MSqlQuery query(MSqlQuery::InitCon());
// The MSqlQuery object gets exclusive access to the connection for its 
// lifetime. The connection is automatically returned when the MSqlQuery 
// object is destroyed.

// Note: Due to a bug in some Qt/MySql combinations, QSqlDatabase connections
// will crash if closed and reopend - so we never close them and keep them in
// a pool. 

// QSqlDatabase wrapper

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
};


// DB connection pool

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


typedef struct _MSqlQueryInfo
{
    MSqlDatabase *db;
    QSqlDatabase *qsqldb;
    bool returnConnection;
} MSqlQueryInfo;

// QSqlQuery wrapper

class MSqlQuery : public QSqlQuery
{
  public:
    // Get DB connection from pool 
    MSqlQuery(const MSqlQueryInfo &qi);
    // Returns conneciton to pool
    ~MSqlQuery();

    // Only updated once during object creation
    bool isConnected(void) { return m_isConnected; }

    // QSqlQuery::prepare() is not thread safe in Qt <= 3.3.2
    bool prepare(const QString &query);

    // Checks DB connection + login (login info via Mythcontext)
    static bool testDBConnection();

    // Only use this in combination with MSqlQuery constructor
    static MSqlQueryInfo InitCon();

    // Dedicated connections. (Requierd for using temporary SQL tables.)
    static MSqlQueryInfo SchedCon();
    static MSqlQueryInfo DDCon();

  private:
    MSqlDatabase *m_db;
    bool m_isConnected;
    bool m_returnConnection;
};

#endif    
