// -*- Mode: c++ -*-

#ifndef MYTHSTORAGE_H
#define MYTHSTORAGE_H

// Qt headers
#include <qstring.h>
#include <qdeepcopy.h>

// MythTV headers
#include "mythexp.h"
#include "mythdbcon.h"

class Setting;

class MPUBLIC Storage
{
  public:
    Storage() { }
    virtual ~Storage() { }

    virtual void load(void) = 0;
    virtual void save(void) = 0;
    virtual void save(QString /*destination*/) { }
};

class MPUBLIC DBStorage : public Storage
{
  public:
    DBStorage(Setting *_setting, QString _table, QString _column) :
        setting(_setting), table(QDeepCopy<QString>(_table)),
        column(QDeepCopy<QString>(_column)) { }

    virtual ~DBStorage() { }

  protected:
    QString getColumn(void) const { return QDeepCopy<QString>(column); }
    QString getTable(void)  const { return QDeepCopy<QString>(table);  }

    Setting *setting;
    QString table;
    QString column;
};

class MPUBLIC SimpleDBStorage : public DBStorage
{
  public:
    SimpleDBStorage(Setting *_setting,
                    QString _table, QString _column) :
        DBStorage(_setting, _table, _column) {}
    virtual ~SimpleDBStorage() { }

    virtual void load(void);
    virtual void save(void);
    virtual void save(QString destination);

  protected:
    virtual QString whereClause(MSqlBindings&) = 0;
    virtual QString setClause(MSqlBindings& bindings);
};

class MPUBLIC TransientStorage : public Storage
{
  public:
    TransientStorage() { }
    virtual ~TransientStorage() { }

    virtual void load(void) { }
    virtual void save(void) { }
};

class MPUBLIC HostDBStorage : public SimpleDBStorage
{
  public:
    HostDBStorage(Setting *_setting, QString name);

  protected:
    virtual QString whereClause(MSqlBindings &bindings);
    virtual QString setClause(MSqlBindings &bindings);
};

class MPUBLIC GlobalDBStorage : public SimpleDBStorage
{
  public:
    GlobalDBStorage(Setting *_setting, QString name);

  protected:
    virtual QString whereClause(MSqlBindings &bindings);
    virtual QString setClause(MSqlBindings &bindings);
};

///////////////////////////////////////////////////////////////////////////////

#endif // MYTHSTORAGE_H
