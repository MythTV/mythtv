#include <algorithm>
#include <vector>
#include <map>

#include "mythdb.h"
#include "cleanup.h"
#include "dbaccess.h"
#include "mythmiscutil.h"

namespace
{
    template <typename T, typename arg_type>
    struct call_sort
    {
        call_sort(T &c) : m_c(c) {}

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
    typedef SingleValue::entry entry;
    typedef std::vector<entry> entry_list;

  private:
    typedef std::map<int, QString> entry_map;

  public:
    SingleValueImp(const QString &table_name, const QString &id_name,
                   const QString &value_name) : m_table_name(table_name),
        m_id_name(id_name), m_value_name(value_name), m_ready(false),
        m_dirty(true), m_clean_stub(this)
    {
        m_insert_sql = QString("INSERT INTO %1 (%2) VALUES (:NAME)")
                .arg(m_table_name).arg(m_value_name);
        m_fill_sql = QString("SELECT %1, %2 FROM %3").arg(m_id_name)
                .arg(m_value_name).arg(m_table_name);
        m_delete_sql = QString("DELETE FROM %1 WHERE %2 = :ID")
                .arg(m_table_name).arg(m_id_name);
    }

    virtual ~SingleValueImp() {}

    mutable QMutex mutex;

    void load_data()
    {
        QMutexLocker locker(&mutex);
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
            query.prepare(m_insert_sql);
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
                    MythDB::DBError("get last id", query);
            }
        }

        return id;
    }

    bool get(int id, QString &value)
    {
        entry_map::const_iterator p = m_entries.find(id);
        if (p != m_entries.end())
        {
            value = p->second;
            return true;
        }
        return false;
    }

    void remove(int id)
    {
        entry_map::iterator p = m_entries.find(id);
        if (p != m_entries.end())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(m_delete_sql);
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

    bool exists(const QString &name, int *id = 0)
    {
        entry_map::const_iterator p = find(name);
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
            m_ret_entries.clear();

            for (entry_map::const_iterator p = m_entries.begin();
                    p != m_entries.end(); ++p)
            {
                m_ret_entries.push_back(entry_list::value_type(p->first,
                                        p->second));
            }
            std::sort(m_ret_entries.begin(), m_ret_entries.end(),
                      call_sort<SingleValueImp, entry>(*this));
        }

        return m_ret_entries;
    }

    virtual bool sort(const entry &lhs, const entry &rhs)
    {
        return naturalCompare(lhs.second, rhs.second) < 0;
    }

    void cleanup()
    {
        m_ready = false;
        m_dirty = true;
        m_ret_entries.clear();
        m_entries.clear();
    }

  private:
    entry_map::iterator find(const QString &name)
    {
        for (entry_map::iterator p = m_entries.begin();
             p != m_entries.end(); ++p)
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

        if (query.exec(m_fill_sql))
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
    QString m_table_name;
    QString m_id_name;
    QString m_value_name;

    QString m_insert_sql;
    QString m_fill_sql;
    QString m_delete_sql;

    bool m_ready;
    bool m_dirty;
    entry_list m_ret_entries;
    entry_map m_entries;
    SimpleCleanup<SingleValueImp> m_clean_stub;
};

////////////////////////////////////////////

SingleValue::SingleValue(SingleValueImp *imp) : m_imp(imp)
{
}

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
    typedef MultiValue::entry entry;

  private:
    typedef std::map<int, entry> id_map;

  public:
    MultiValueImp(const QString &table_name, const QString &id_name,
                  const QString &value_name) : m_table_name(table_name),
        m_id_name(id_name), m_value_name(value_name), m_ready(false),
        m_clean_stub(this)
    {
        m_insert_sql = QString("INSERT INTO %1 (%2, %3) VALUES (:ID, :VALUE)")
                .arg(m_table_name).arg(m_id_name).arg(m_value_name);
        m_fill_sql = QString("SELECT %1, %2 FROM %3 ORDER BY %4").arg(m_id_name)
                .arg(m_value_name).arg(m_table_name).arg(m_id_name);
    }

    mutable QMutex mutex;

    void load_data()
    {
        QMutexLocker locker(&mutex);
        if (!m_ready)
        {
            fill_from_db();
            m_ready = true;
        }
    }

    void cleanup()
    {
        m_ready = false;
        m_val_map.clear();
    }

    int add(int id, int value)
    {
        bool db_insert = false;
        id_map::iterator p = m_val_map.find(id);
        if (p != m_val_map.end())
        {
            entry::values_type &va = p->second.values;
            entry::values_type::iterator v =
                    std::find(va.begin(), va.end(), value);
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
            m_val_map.insert(id_map::value_type(id, e));
            db_insert = true;
        }

        if (db_insert)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(m_insert_sql);
            query.bindValue(":ID", id);
            query.bindValue(":VALUE", value);
            if (!query.exec())
                MythDB::DBError("multi value insert", query);
        }

        return id;
    }

    bool get(int id, entry &values)
    {
        id_map::iterator p = m_val_map.find(id);
        if (p != m_val_map.end())
        {
            values = p->second;
            return true;
        }
        return false;
    }

    void remove(int id, int value)
    {
        id_map::iterator p = m_val_map.find(id);
        if (p != m_val_map.end())
        {
            entry::values_type::iterator vp =
                    std::find(p->second.values.begin(), p->second.values.end(),
                              value);
            if (vp != p->second.values.end())
            {
                MSqlQuery query(MSqlQuery::InitCon());
                QString del_query = QString("DELETE FROM %1 WHERE %2 = :ID AND "
                                            "%3 = :VALUE")
                        .arg(m_table_name).arg(m_id_name).arg(m_value_name);
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
        id_map::iterator p = m_val_map.find(id);
        if (p != m_val_map.end())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            QString del_query = QString("DELETE FROM %1 WHERE %2 = :ID")
                    .arg(m_table_name).arg(m_id_name);
            query.prepare(del_query);
            query.bindValue(":ID", p->first);
            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("multivalue remove", query);
            }
            m_val_map.erase(p);
        }
    }

    bool exists(int id, int value)
    {
        id_map::iterator p = m_val_map.find(id);
        if (p != m_val_map.end())
        {
            entry::values_type::iterator vp =
                    std::find(p->second.values.begin(), p->second.values.end(),
                              value);
            return vp != p->second.values.end();
        }
        return false;
    }

    bool exists(int id)
    {
        return m_val_map.find(id) != m_val_map.end();
    }

  private:
    void fill_from_db()
    {
        m_val_map.clear();

        MSqlQuery query(MSqlQuery::InitCon());

        if (query.exec(m_fill_sql) && query.size() > 0)
        {
            id_map::iterator p = m_val_map.end();
            while (query.next())
            {
                int id = query.value(0).toInt();
                int val = query.value(1).toInt();

                if (p == m_val_map.end() ||
                        (p != m_val_map.end() && p->first != id))
                {
                    p = m_val_map.find(id);
                    if (p == m_val_map.end())
                    {
                        entry e;
                        e.id = id;
                        p = m_val_map.insert(id_map::value_type(id, e)).first;
                    }
                }
                p->second.values.push_back(val);
            }
        }
    }

  private:
    id_map m_val_map;

    QString m_table_name;
    QString m_id_name;
    QString m_value_name;

    QString m_insert_sql;
    QString m_fill_sql;
    QString m_id_sql;

    bool m_ready;
    SimpleCleanup<MultiValueImp> m_clean_stub;
};

////////////////////////////////////////////

MultiValue::MultiValue(MultiValueImp *imp) : m_imp(imp)
{
}

MultiValue::~MultiValue()
{
}

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

VideoCategory::~VideoCategory()
{
}

VideoCategory &VideoCategory::GetCategory()
{
    static VideoCategory vc;
    vc.load_data();
    return vc;
}

////////////////////////////////////////////

VideoCountry::VideoCountry() :
    SingleValue(new SingleValueImp("videocountry", "intid", "country"))
{
}

VideoCountry::~VideoCountry()
{
}

VideoCountry &VideoCountry::getCountry()
{
    static VideoCountry vc;
    vc.load_data();
    return vc;
}

////////////////////////////////////////////

VideoGenre::VideoGenre() :
    SingleValue(new SingleValueImp("videogenre", "intid", "genre"))
{
}

VideoGenre::~VideoGenre()
{
}

VideoGenre &VideoGenre::getGenre()
{
    static VideoGenre vg;
    vg.load_data();
    return vg;
}

////////////////////////////////////////////

VideoCast::VideoCast() :
    SingleValue(new SingleValueImp("videocast", "intid", "cast"))
{
}

VideoCast::~VideoCast()
{
}

VideoCast &VideoCast::GetCast()
{
    static VideoCast vc;
    vc.load_data();
    return vc;
}

////////////////////////////////////////////

VideoGenreMap::VideoGenreMap() :
    MultiValue(new MultiValueImp("videometadatagenre", "idvideo", "idgenre"))
{
}

VideoGenreMap::~VideoGenreMap()
{
}

VideoGenreMap &VideoGenreMap::getGenreMap()
{
    static VideoGenreMap vgm;
    vgm.load_data();
    return vgm;
}

////////////////////////////////////////////

VideoCountryMap::VideoCountryMap() :
    MultiValue(new MultiValueImp("videometadatacountry", "idvideo",
                                 "idcountry"))
{
}

VideoCountryMap::~VideoCountryMap()
{
}

VideoCountryMap &VideoCountryMap::getCountryMap()
{
    static VideoCountryMap vcm;
    vcm.load_data();
    return vcm;
}

////////////////////////////////////////////

VideoCastMap::VideoCastMap() :
    MultiValue(new MultiValueImp("videometadatacast", "idvideo",
                                 "idcast"))
{
}

VideoCastMap::~VideoCastMap()
{
}

VideoCastMap &VideoCastMap::getCastMap()
{
    static VideoCastMap vcm;
    vcm.load_data();
    return vcm;
}

////////////////////////////////////////////

class FileAssociationsImp
{
  public:
    typedef FileAssociations::file_association file_association;
    typedef FileAssociations::association_list association_list;
    typedef FileAssociations::ext_ignore_list ext_ignore_list;

  public:
    FileAssociationsImp() : m_ready(false) {}

    bool add(file_association &fa)
    {
        file_association ret_fa(fa);

        file_association *existing_fa = 0;
        MSqlQuery query(MSqlQuery::InitCon());

        association_list::iterator p = find(ret_fa.extension);
        if (p != m_file_associations.end())
        {
            ret_fa.id = p->id;
            existing_fa = &(*p);

            query.prepare("UPDATE videotypes SET extension = :EXT, "
                          "playcommand = :PLAYCMD, f_ignore = :IGNORED, "
                          "use_default = :USEDEFAULT WHERE intid = :ID");
            query.bindValue(":ID", ret_fa.id);
        }
        else
            query.prepare("INSERT INTO videotypes (extension, playcommand, "
                          "f_ignore, use_default) VALUES "
                          "(:EXT, :PLAYCMD, :IGNORED, :USEDEFAULT)");

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
                    m_file_associations.push_back(ret_fa);
                }
                else
                    return false;
            }
            else
                *existing_fa = ret_fa;

            fa = ret_fa;
            return true;
        }

        return false;
    }

    bool get(unsigned int id, file_association &val) const
    {
        association_list::const_iterator p = find(id);
        if (p != m_file_associations.end())
        {
            val = *p;
            return true;
        }
        return false;
    }

    bool get(const QString &ext, file_association &val) const
    {
        association_list::const_iterator p = find(ext);
        if (p != m_file_associations.end())
        {
            val = *p;
            return true;
        }
        return false;
    }

    bool remove(unsigned int id)
    {
        association_list::iterator p = find(id);
        if (p != m_file_associations.end())
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM videotypes WHERE intid = :ID");
            query.bindValue(":ID", p->id);
            if (query.exec())
            {
                m_file_associations.erase(p);
                return true;
            }
        }
        return false;
    }

    const association_list &getList() const
    {
        return m_file_associations;
    }

    void getExtensionIgnoreList(ext_ignore_list &ext_ignore) const
    {
        for (association_list::const_iterator p = m_file_associations.begin();
             p != m_file_associations.end(); ++p)
        {
            ext_ignore.push_back(std::make_pair(p->extension, p->ignore));
        }
    }

    mutable QMutex mutex;

    void load_data()
    {
        QMutexLocker locker(&mutex);
        if (!m_ready)
        {
            fill_from_db();
            m_ready = true;
        }
    }

    void cleanup()
    {
        m_ready = false;
        m_file_associations.clear();
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
                m_file_associations.push_back(fa);
            }
        }
    }

    association_list::iterator find(const QString &ext)
    {
        for (association_list::iterator p = m_file_associations.begin();
             p != m_file_associations.end(); ++p)
        {
            if (p->extension.length() == ext.length() &&
                ext.indexOf(p->extension) == 0)
            {
                return p;
            }
        }
        return m_file_associations.end();
    }

    association_list::iterator find(unsigned int id)
    {
        for (association_list::iterator p = m_file_associations.begin();
             p != m_file_associations.end(); ++p)
        {
            if (p->id == id) return p;
        }
        return m_file_associations.end();
    }

    association_list::const_iterator find(const QString &ext) const
    {
        for (association_list::const_iterator p = m_file_associations.begin();
             p != m_file_associations.end(); ++p)
        {
            if (p->extension.length() == ext.length() &&
                ext.indexOf(p->extension) == 0)
            {
                return p;
            }
        }
        return m_file_associations.end();
    }

    association_list::const_iterator find(unsigned int id) const
    {
        for (association_list::const_iterator p = m_file_associations.begin();
             p != m_file_associations.end(); ++p)
        {
            if (p->id == id) return p;
        }
        return m_file_associations.end();
    }

  private:
    association_list m_file_associations;
    bool m_ready;
};


FileAssociations::file_association::file_association() : id(0), ignore(false),
    use_default(false)
{
}

FileAssociations::file_association::file_association(unsigned int l_id,
                                                     const QString &ext,
                                                     const QString &playcmd,
                                                     bool l_ignore,
                                                     bool l_use_default) :
    id(l_id), extension(ext), playcommand(playcmd), ignore(l_ignore),
    use_default(l_use_default)
{
}

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
    return m_imp->getExtensionIgnoreList(ext_ignore);
}

void FileAssociations::load_data()
{
    m_imp->load_data();
}

FileAssociations::FileAssociations()
{
    m_imp = new FileAssociationsImp;
}

FileAssociations::~FileAssociations()
{
    delete m_imp;
}

FileAssociations &FileAssociations::getFileAssociation()
{
    static FileAssociations fa;
    fa.load_data();
    return fa;
}
