// -*- Mode: c++ -*-

#ifndef MYTHSTORAGE_H
#define MYTHSTORAGE_H

// Qt headers
#include <QString>

// MythTV headers
#include "mythbaseexp.h"
#include "mythdbcon.h"

class StorageUser
{
  public:
    virtual void SetDBValue(const QString&) = 0;
    virtual QString GetDBValue(void) const = 0;
    virtual ~StorageUser() = default;
};

class MBASE_PUBLIC Storage
{
  public:
    Storage() = default;
    virtual ~Storage() = default;

    virtual void Load(void) = 0;
    virtual void Save(void) = 0;
    virtual void Save(const QString &/*destination*/) { }
    virtual bool IsSaveRequired(void) const { return true; };
    virtual void SetSaveRequired(void) { };
};

class MBASE_PUBLIC DBStorage : public Storage
{
  public:
    DBStorage(StorageUser *user, const QString &table, const QString &column) :
        m_user(user), m_tablename(table), m_columnname(column) { }

    virtual ~DBStorage() = default;

  protected:
    QString GetColumnName(void) const { return m_columnname; }
    QString GetTableName(void)  const { return m_tablename;  }

    StorageUser *m_user {nullptr};
    QString      m_tablename;
    QString      m_columnname;
};

class MBASE_PUBLIC SimpleDBStorage : public DBStorage
{
  public:
    SimpleDBStorage(StorageUser *user,
                    const QString &table, const QString &column) :
        DBStorage(user, table, column) { m_initval.clear(); }
    virtual ~SimpleDBStorage() = default;

    void Load(void) override; // Storage
    void Save(void) override; // Storage
    void Save(const QString &table) override; // Storage
    bool IsSaveRequired(void) const override; // Storage
    void SetSaveRequired(void) override; // Storage

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const = 0;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString m_initval;
};

class MBASE_PUBLIC GenericDBStorage : public SimpleDBStorage
{
  public:
    GenericDBStorage(StorageUser *user,
                     const QString &table, const QString &column,
                     const QString &keycolumn, const QString &keyvalue = QString()) :
        SimpleDBStorage(user, table, column),
        m_keycolumn(keycolumn), m_keyvalue(keyvalue) {}
    virtual ~GenericDBStorage() = default;

    void SetKeyValue(const QString &val) { m_keyvalue = val; }
    void SetKeyValue(long long val) { m_keyvalue = QString::number(val); }

  protected:
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage

  protected:
    QString m_keycolumn;
    QString m_keyvalue;
};

class MBASE_PUBLIC TransientStorage : public Storage
{
  public:
    TransientStorage() = default;
    virtual ~TransientStorage() = default;

    void Load(void) override { } // Storage
    void Save(void) override { } // Storage
    void Save(const QString &/*destination*/) override { } // Storage
};

class MBASE_PUBLIC HostDBStorage : public SimpleDBStorage
{
  public:
    HostDBStorage(StorageUser *_user, QString name);
    using SimpleDBStorage::Save; // prevent compiler warning
    void Save(void) override; // SimpleDBStorage

  protected:
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage

  protected:
    QString m_settingname;
};

class MBASE_PUBLIC GlobalDBStorage : public SimpleDBStorage
{
  public:
    GlobalDBStorage(StorageUser *_user, QString name);
    using SimpleDBStorage::Save; // prevent compiler warning
    void Save(void) override; // SimpleDBStorage

  protected:
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage

  protected:
    QString m_settingname;
};

///////////////////////////////////////////////////////////////////////////////

#endif // MYTHSTORAGE_H
