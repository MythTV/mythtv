// -*- Mode: c++ -*-

#ifndef MYTHSTORAGE_H
#define MYTHSTORAGE_H

// Qt headers
#include <QString>

// MythTV headers
#include "mythexp.h"
#include "mythdbcon.h"

class StorageUser
{
  public:
    virtual void SetDBValue(const QString&) = 0;
    virtual QString GetDBValue(void) const = 0;
    virtual ~StorageUser() { }
};

class MPUBLIC Storage
{
  public:
    Storage() { }
    virtual ~Storage() { }

    virtual void Load(void) = 0;
    virtual void Save(void) = 0;
    virtual void Save(QString /*destination*/) { }
    virtual bool IsSaveRequired(void) const { return true; };
    virtual void SetSaveRequired(void) { };
};

class MPUBLIC DBStorage : public Storage
{
  public:
    DBStorage(StorageUser *_user, QString _table, QString _column) :
        user(_user), tablename(_table), columnname(_column) { }

    virtual ~DBStorage() { }

  protected:
    QString GetColumnName(void) const { return columnname; }
    QString GetTableName(void)  const { return tablename;  }

    StorageUser *user;
    QString      tablename;
    QString      columnname;
};

class MPUBLIC SimpleDBStorage : public DBStorage
{
  public:
    SimpleDBStorage(StorageUser *_user,
                    QString _table, QString _column) :
        DBStorage(_user, _table, _column) { initval.clear(); }
    virtual ~SimpleDBStorage() { }

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString destination);
    virtual bool IsSaveRequired(void) const;
    virtual void SetSaveRequired(void);

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const = 0;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString initval;
};

class MPUBLIC GenericDBStorage : public SimpleDBStorage
{
  public:
    GenericDBStorage(StorageUser *_user,
                     QString _table, QString _column,
                     QString _keycolumn, QString _keyvalue = QString::null) :
        SimpleDBStorage(_user, _table, _column),
        keycolumn(_keycolumn), keyvalue(_keyvalue) {}
    virtual ~GenericDBStorage() { }

    void SetKeyValue(const QString val) { keyvalue = val; }
    void SetKeyValue(long long val) { keyvalue = QString::number(val); }

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString keycolumn;
    QString keyvalue;
};

class MPUBLIC TransientStorage : public Storage
{
  public:
    TransientStorage() { }
    virtual ~TransientStorage() { }

    virtual void Load(void) { }
    virtual void Save(void) { }
};

class MPUBLIC HostDBStorage : public SimpleDBStorage
{
  public:
    HostDBStorage(StorageUser *_user, const QString &name);

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString settingname;
};

class MPUBLIC GlobalDBStorage : public SimpleDBStorage
{
  public:
    GlobalDBStorage(StorageUser *_user, const QString &name);

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString settingname;
};

///////////////////////////////////////////////////////////////////////////////

#endif // MYTHSTORAGE_H
