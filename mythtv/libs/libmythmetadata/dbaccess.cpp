#include <algorithm>
#include <vector>
#include <map>

#include "libmythbase/mythdb.h"
#include "libmythbase/stringutil.h"

#include "cleanup.h"
#include "dbaccess.h"

namespace
{
    template <typename T, typename arg_type>
    struct call_sort
    {
        explicit call_sort(T &c) : m_c(c) {}

        bool operator()(const arg_type &lhs, const arg_type &rhs)
        {
            return m_c.sort(lhs, rhs);
        }

        T &m_c;
    };
}

class SingleValueImp
{
  public:
    using entry = SingleValue::entry;
    using entry_list = std::vector<entry>;

  private:
    using entry_map = std::map<int, QString>;

  public:
    SingleValueImp(QString table_name, QString id_name, QString value_name)
        : m_tableName(std::move(table_name)), m_idName(std::move(id_name)),
          m_valueName(std::move(value_name)), m_cleanStub(this)
    {
        m_insertSql = QString("INSERT INTO %1 (%2) VALUES (:NAME)")
                .arg(m_tableName, m_valueName);
        m_fillSql = QString("SELECT %1, %2 FROM %3")
                .arg(m_idName, m_valueName, m_tableName);
        m_deleteSql = QString("DELETE FROM %1 WHERE %2 = :ID")
                .arg(m_tableName, m_idName);
    }

    virtual ~SingleValueImp() = default;

    mutable QMutex m_mutex;

    void load_data()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_ready)
        {
            fill_from_db();
            m_ready = true;
        }
    }

    int add(const QString &name)
    {
        int id = 0;

        if (!exists(name, &id))
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(m_insertSql);
            query.bindValue(":NAME", name);
            if (query.exec())
            {
                if (query.exec("SELECT LAST_INSERT_ID()") && query.next())
                {
                    id = query.value(0).toInt();
                    m_entries.insert(entry_map::value_type(id, name));
                    m_dirty = true;
                }
                else
                {
                    MythDB::DBError("get last id", query);
                }
            }
        }

        return id;
    }

    bool get(int id, QString &value)
    {
        auto p = m_entries.find(id);
        if (p != m_entries.end())
        {
            value = p->second;
            return true;
        }
        return false;
    }

    void remove(int id)
    {
        auto p = m_entries.find(id);
        if (p != m_entries.end())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(m_deleteSql);
            query.bindValue(":ID", p->first);
            if (query.exec())
            {
                m_dirty = true;
                m_entries.erase(p);
            }
        }
    }

    bool exists(int id)
    {
        return m_entries.find(id) != m_entries.end();
    }

    bool exists(const QString &name, int *id = nullptr)
    {
        auto p = find(name);
        if (p != m_entries.end())
        {
            if (id)
                *id = p->first;
            return true;
        }
        return false;
    }

    const entry_list &getList()
    {
        if (m_dirty)
        {
            m_dirty = false;
            m_retEntries.clear();

            for (auto & item : m_entries)
            {
                m_retEntries.emplace_back(item.first, item.second);
            }
            std::sort(m_retEntries.begin(), m_retEntries.end(),
                      call_sort<SingleValueImp, entry>(*this));
        }

        return m_retEntries;
    }

    virtual bool sort(const entry &lhs, const entry &rhs)
    {
        return StringUtil::naturalSortCompare(lhs.second, rhs.second);
    }

    void cleanup()
    {
        m_ready = false;
        m_dirty = true;
        m_retEntries.clear();
        m_entries.clear();
    }

  private:
    entry_map::iterator find(const QString &name)
    {
        for (auto p = m_entries.begin(); p != m_entries.end(); ++p)
        {
            if (p->second == name)
                return p;
        }
        return m_entries.end();
    }

    void fill_from_db()
    {
        m_entries.clear();

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.exec(m_fillSql))
        {
            while (query.next())
            {
                int id = query.value(0).toInt();
                QString val = query.value(1).toString();
                m_entries.insert(entry_map::value_type(id, val));
            }
        }
    }

  private:
    QString m_tableName;
    QString m_idName;
    QString m_valueName;

    QString m_insertSql;
    QString m_fillSql;
    QString m_deleteSql;

    bool m_ready {false};
    bool m_dirty {true};
    entry_list m_retEntries;
    entry_map m_entries;
    SimpleCleanup<SingleValueImp> m_cleanStub;
};

////////////////////////////////////////////

SingleValue::~SingleValue()
{
    delete m_imp;
}

int SingleValue::add(const QString &name)
{
    return m_imp->add(name);
}

bool SingleValue::get(int id, QString &category)
{
    return m_imp->get(id, category);
}

void SingleValue::remove(int id)
{
    m_imp->remove(id);
}

bool SingleValue::exists(int id)
{
    return m_imp->exists(id);
}

bool SingleValue::exists(const QString &name)
{
    return m_imp->exists(name);
}

const SingleValue::entry_list &SingleValue::getList()
{
    return m_imp->getList();
}

void SingleValue::load_data()
{
    m_imp->load_data();
}

////////////////////////////////////////////

class MultiValueImp
{
  public:
    using entry = MultiValue::entry;

  private:
    using id_map = std::map<int, entry>;

  public:
    MultiValueImp(QString table_name, QString id_name,
                  QString value_name) : m_tableName(std::move(table_name)),
        m_idName(std::move(id_name)), m_valueName(std::move(value_name)),
        m_cleanStub(this)
    {
        m_insertSql = QString("INSERT INTO %1 (%2, %3) VALUES (:ID, :VALUE)")
                .arg(m_tableName, m_idName, m_valueName);
        m_fillSql = QString("SELECT %1, %2 FROM %3 ORDER BY %4")
                .arg(m_idName, m_valueName, m_tableName, m_idName);
    }

    mutable QMutex m_mutex;

    void load_data()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_ready)
        {
            fill_from_db();
            m_ready = true;
        }
    }

    void cleanup()
    {
        m_ready = false;
        m_valMap.clear();
    }

    int add(int id, int value)
    {
        bool db_insert = false;
        auto p = m_valMap.find(id);
        if (p != m_valMap.end())
        {
            entry::values_type &va = p->second.values;
            auto v = std::find(va.begin(), va.end(), value);
            if (v == va.end())
            {
                va.push_back(value);
                db_insert = true;
            }
        }
        else
        {
            entry e;
            e.id = id;
            e.values.push_back(value);
            m_valMap.insert(id_map::value_type(id, e));
            db_insert = true;
        }

        if (db_insert)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(m_insertSql);
            query.bindValue(":ID", id);
            query.bindValue(":VALUE", value);
            if (!query.exec())
                MythDB::DBError("multi value insert", query);
        }

        return id;
    }

    bool get(int id, entry &values)
    {
        auto p = m_valMap.find(id);
        if (p != m_valMap.end())
        {
            values = p->second;
            return true;
        }
        return false;
    }

    void remove(int id, int value)
    {
        auto p = m_valMap.find(id);
        if (p != m_valMap.end())
        {
            auto vp = std::find(p->second.values.begin(),
                                p->second.values.end(), value);
            if (vp != p->second.values.end())
            {
                MSqlQuery query(MSqlQuery::InitCon());
                QString del_query = QString("DELETE FROM %1 WHERE %2 = :ID AND "
                                            "%3 = :VALUE")
                        .arg(m_tableName, m_idName, m_valueName);
                query.prepare(del_query);
                query.bindValue(":ID", p->first);
                query.bindValue(":VALUE", int(*vp));
                if (!query.exec() || !query.isActive())
                {
                    MythDB::DBError("multivalue remove", query);
                }
                p->second.values.erase(vp);
            }
        }
    }

    void remove(int id)
    {
        auto p = m_valMap.find(id);
        if (p != m_valMap.end())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            QString del_query = QString("DELETE FROM %1 WHERE %2 = :ID")
                    .arg(m_tableName, m_idName);
            query.prepare(del_query);
            query.bindValue(":ID", p->first);
            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("multivalue remove", query);
            }
            m_valMap.erase(p);
        }
    }

    bool exists(int id, int value)
    {
        auto p = m_valMap.find(id);
        if (p != m_valMap.end())
        {
            auto vp =
                    std::find(p->second.values.begin(), p->second.values.end(),
                              value);
            return vp != p->second.values.end();
        }
        return false;
    }

    bool exists(int id)
    {
        return m_valMap.find(id) != m_valMap.end();
    }

  private:
    void fill_from_db()
    {
        m_valMap.clear();

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.exec(m_fillSql) && query.size() > 0)
        {
            auto p = m_valMap.end();
            while (query.next())
            {
                int id = query.value(0).toInt();
                int val = query.value(1).toInt();

                if (p == m_valMap.end() ||
                        (p != m_valMap.end() && p->first != id))
                {
                    p = m_valMap.find(id);
                    if (p == m_valMap.end())
                    {
                        entry e;
                        e.id = id;
                        p = m_valMap.insert(id_map::value_type(id, e)).first;
                    }
                }
                p->second.values.push_back(val);
            }
        }
    }

  private:
    id_map m_valMap;

    QString m_tableName;
    QString m_idName;
    QString m_valueName;

    QString m_insertSql;
    QString m_fillSql;
    QString m_idSql;

    bool m_ready {false};
    SimpleCleanup<MultiValueImp> m_cleanStub;
};

////////////////////////////////////////////

int MultiValue::add(int id, int value)
{
    return m_imp->add(id, value);
}

bool MultiValue::get(int id, entry &values)
{
    return m_imp->get(id, values);
}

void MultiValue::remove(int id, int value)
{
    m_imp->remove(id, value);
}

void MultiValue::remove(int id)
{
    m_imp->remove(id);
}

bool MultiValue::exists(int id, int value)
{
    return m_imp->exists(id, value);
}

bool MultiValue::exists(int id)
{
    return m_imp->exists(id);
}

void MultiValue::load_data()
{
    m_imp->load_data();
}

////////////////////////////////////////////

VideoCategory::VideoCategory() :
    SingleValue(new SingleValueImp("videocategory", "intid", "category"))
{
}

VideoCategory &VideoCategory::GetCategory()
{
    static VideoCategory s_vc;
    s_vc.load_data();
    return s_vc;
}

////////////////////////////////////////////

VideoCountry::VideoCountry() :
    SingleValue(new SingleValueImp("videocountry", "intid", "country"))
{
}

VideoCountry &VideoCountry::getCountry()
{
    static VideoCountry s_vc;
    s_vc.load_data();
    return s_vc;
}

////////////////////////////////////////////

VideoGenre::VideoGenre() :
    SingleValue(new SingleValueImp("videogenre", "intid", "genre"))
{
}

VideoGenre &VideoGenre::getGenre()
{
    static VideoGenre s_vg;
    s_vg.load_data();
    return s_vg;
}

////////////////////////////////////////////

VideoCast::VideoCast() :
    SingleValue(new SingleValueImp("videocast", "intid", "cast"))
{
}

VideoCast &VideoCast::GetCast()
{
    static VideoCast s_vc;
    s_vc.load_data();
    return s_vc;
}

////////////////////////////////////////////

VideoGenreMap::VideoGenreMap() :
    MultiValue(new MultiValueImp("videometadatagenre", "idvideo", "idgenre"))
{
}

VideoGenreMap &VideoGenreMap::getGenreMap()
{
    static VideoGenreMap s_vgm;
    s_vgm.load_data();
    return s_vgm;
}

////////////////////////////////////////////

VideoCountryMap::VideoCountryMap() :
    MultiValue(new MultiValueImp("videometadatacountry", "idvideo",
                                 "idcountry"))
{
}

VideoCountryMap &VideoCountryMap::getCountryMap()
{
    static VideoCountryMap s_vcm;
    s_vcm.load_data();
    return s_vcm;
}

////////////////////////////////////////////

VideoCastMap::VideoCastMap() :
    MultiValue(new MultiValueImp("videometadatacast", "idvideo",
                                 "idcast"))
{
}

VideoCastMap &VideoCastMap::getCastMap()
{
    static VideoCastMap s_vcm;
    s_vcm.load_data();
    return s_vcm;
}

////////////////////////////////////////////

class FileAssociationsImp
{
  public:
    using file_association = FileAssociations::file_association;
    using association_list = FileAssociations::association_list;
    using ext_ignore_list = FileAssociations::ext_ignore_list;

  public:
    FileAssociationsImp() = default;

    bool add(file_association &fa)
    {
        file_association ret_fa(fa);

        file_association *existing_fa = nullptr;
        MSqlQuery query(MSqlQuery::InitCon());

        auto p = find(ret_fa.extension);
        if (p != m_fileAssociations.end())
        {
            ret_fa.id = p->id;
            existing_fa = &(*p);

            query.prepare("UPDATE videotypes SET extension = :EXT, "
                          "playcommand = :PLAYCMD, f_ignore = :IGNORED, "
                          "use_default = :USEDEFAULT WHERE intid = :ID");
            query.bindValue(":ID", ret_fa.id);
        }
        else
        {
            query.prepare("INSERT INTO videotypes (extension, playcommand, "
                          "f_ignore, use_default) VALUES "
                          "(:EXT, :PLAYCMD, :IGNORED, :USEDEFAULT)");
        }

        query.bindValue(":EXT", ret_fa.extension);
        query.bindValue(":PLAYCMD", ret_fa.playcommand);
        query.bindValue(":IGNORED", ret_fa.ignore);
        query.bindValue(":USEDEFAULT", ret_fa.use_default);

        if (query.exec() && query.isActive())
        {
            if (!existing_fa)
            {
                if (query.exec("SELECT LAST_INSERT_ID()") && query.next())
                {
                    ret_fa.id = query.value(0).toUInt();
                    m_fileAssociations.push_back(ret_fa);
                }
                else
                {
                    return false;
                }
            }
            else
            {
                *existing_fa = ret_fa;
            }

            fa = ret_fa;
            return true;
        }

        return false;
    }

    bool get(unsigned int id, file_association &val) const
    {
        auto p = cfind(id);
        if (p != m_fileAssociations.end())
        {
            val = *p;
            return true;
        }
        return false;
    }

    bool get(const QString &ext, file_association &val) const
    {
        auto p = cfind(ext);
        if (p != m_fileAssociations.end())
        {
            val = *p;
            return true;
        }
        return false;
    }

    bool remove(unsigned int id)
    {
        auto p = find(id);
        if (p != m_fileAssociations.end())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM videotypes WHERE intid = :ID");
            query.bindValue(":ID", p->id);
            if (query.exec())
            {
                m_fileAssociations.erase(p);
                return true;
            }
        }
        return false;
    }

    const association_list &getList() const
    {
        return m_fileAssociations;
    }

    void getExtensionIgnoreList(ext_ignore_list &ext_ignore) const
    {
        for (const auto & fa : m_fileAssociations)
            ext_ignore.emplace_back(fa.extension, fa.ignore);
    }

    mutable QMutex m_mutex;

    void load_data()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_ready)
        {
            fill_from_db();
            m_ready = true;
        }
    }

    void cleanup()
    {
        m_ready = false;
        m_fileAssociations.clear();
    }

  private:
    void fill_from_db()
    {
        MSqlQuery query(MSqlQuery::InitCon());
        if (query.exec("SELECT intid, extension, playcommand, f_ignore, "
                       "use_default FROM videotypes"))
        {
            while (query.next())
            {
                file_association fa(query.value(0).toUInt(),
                                    query.value(1).toString(),
                                    query.value(2).toString(),
                                    query.value(3).toBool(),
                                    query.value(4).toBool());
                m_fileAssociations.push_back(fa);
            }
        }
    }

    association_list::iterator find(const QString &ext)
    {
        for (auto p = m_fileAssociations.begin();
             p != m_fileAssociations.end(); ++p)
        {
            if (p->extension.length() == ext.length() &&
                ext.indexOf(p->extension) == 0)
            {
                return p;
            }
        }
        return m_fileAssociations.end();
    }

    association_list::iterator find(unsigned int id)
    {
        for (auto p = m_fileAssociations.begin();
             p != m_fileAssociations.end(); ++p)
        {
            if (p->id == id) return p;
        }
        return m_fileAssociations.end();
    }

    association_list::const_iterator cfind(const QString &ext) const
    {
        for (auto p = m_fileAssociations.cbegin();
             p != m_fileAssociations.cend(); ++p)
        {
            if (p->extension.length() == ext.length() &&
                ext.indexOf(p->extension) == 0)
            {
                return p;
            }
        }
        return m_fileAssociations.cend();
    }

    association_list::const_iterator cfind(unsigned int id) const
    {
        for (auto p = m_fileAssociations.cbegin();
             p != m_fileAssociations.cend(); ++p)
        {
            if (p->id == id) return p;
        }
        return m_fileAssociations.cend();
    }

  private:
    association_list m_fileAssociations;
    bool             m_ready {false};
};


bool FileAssociations::add(file_association &fa)
{
    return m_imp->add(fa);
}

bool FileAssociations::get(unsigned int id, file_association &val) const
{
    return m_imp->get(id, val);
}

bool FileAssociations::get(const QString &ext, file_association &val) const
{
    return m_imp->get(ext, val);
}

bool FileAssociations::remove(unsigned int id)
{
    return m_imp->remove(id);
}

const FileAssociations::association_list &FileAssociations::getList() const
{
    return m_imp->getList();
}

void FileAssociations::getExtensionIgnoreList(ext_ignore_list &ext_ignore) const
{
    m_imp->getExtensionIgnoreList(ext_ignore);
}

void FileAssociations::load_data()
{
    m_imp->load_data();
}

FileAssociations::FileAssociations()
  : m_imp(new FileAssociationsImp)
{
}

FileAssociations::~FileAssociations()
{
    delete m_imp;
}

FileAssociations &FileAssociations::getFileAssociation()
{
    static FileAssociations s_fa;
    s_fa.load_data();
    return s_fa;
}
