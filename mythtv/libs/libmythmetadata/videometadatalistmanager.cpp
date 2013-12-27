#include <map>

#include "mythdb.h"
#include "videometadatalistmanager.h"

class VideoMetadataListManagerImp
{
  public:
    typedef VideoMetadataListManager::VideoMetadataPtr VideoMetadataPtr;
    typedef VideoMetadataListManager::metadata_list metadata_list;

  private:
    typedef std::map<unsigned int, metadata_list::iterator> int_to_meta;
    typedef std::map<QString, metadata_list::iterator> string_to_meta;

  public:
    void setList(metadata_list &list)
    {
        m_id_map.clear();
        m_file_map.clear();
        m_meta_list.swap(list);

        for (metadata_list::iterator p = m_meta_list.begin();
             p != m_meta_list.end(); ++p)
        {
            m_id_map.insert(int_to_meta::value_type((*p)->GetID(), p));
            m_file_map.insert(
                    string_to_meta::value_type((*p)->GetFilename(), p));
        }
    }

    const metadata_list &getList() const
    {
        return m_meta_list;
    }


    VideoMetadataPtr byFilename(const QString &file_name) const
    {
        string_to_meta::const_iterator p = m_file_map.find(file_name);
        if (p != m_file_map.end())
        {
            return *(p->second);
        }
        return VideoMetadataPtr();
    }

    VideoMetadataPtr byID(unsigned int db_id) const
    {
        int_to_meta::const_iterator p = m_id_map.find(db_id);
        if (p != m_id_map.end())
        {
            return *(p->second);
        }
        return VideoMetadataPtr();
    }

    bool purgeByFilename(const QString &file_name)
    {
        return purge_entry(byFilename(file_name));
    }

    bool purgeByID(unsigned int db_id)
    {
        return purge_entry(byID(db_id));
    }

  private:
    bool purge_entry(VideoMetadataPtr metadata)
    {
        if (metadata)
        {
            int_to_meta::iterator im = m_id_map.find(metadata->GetID());

            if (im != m_id_map.end())
            {
                metadata_list::iterator mdi = im->second;
                (*mdi)->DeleteFromDatabase();

                m_id_map.erase(im);
                string_to_meta::iterator sm =
                        m_file_map.find(metadata->GetFilename());
                if (sm != m_file_map.end())
                    m_file_map.erase(sm);
                m_meta_list.erase(mdi);
                return true;
            }
        }

        return false;
    }

  private:
    metadata_list m_meta_list;
    int_to_meta m_id_map;
    string_to_meta m_file_map;
};

VideoMetadataListManager::VideoMetadataListManager()
{
    m_imp = new VideoMetadataListManagerImp();
}

VideoMetadataListManager::~VideoMetadataListManager()
{
    delete m_imp;
}

VideoMetadataListManager::VideoMetadataPtr
VideoMetadataListManager::loadOneFromDatabase(uint id)
{
    QString sql = QString("WHERE intid = %1 LIMIT 1").arg(id);
    metadata_list item;
    loadAllFromDatabase(item, sql);
    if (!item.empty())
    {
        return item.front();
    }

    return VideoMetadataPtr(new VideoMetadata());
}

void VideoMetadataListManager::loadAllFromDatabase(metadata_list &items,
                                                   const QString &sql)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.setForwardOnly(true);
    QString BaseMetadataQuery(
        "SELECT title, director, studio, plot, rating, year, releasedate,"
        "userrating, length, playcount, filename, hash, showlevel, "
        "coverfile, inetref, collectionref, homepage, childid, browse, watched, "
        "playcommand, category, intid, trailer, screenshot, banner, fanart, "
        "subtitle, tagline, season, episode, host, insertdate, processed, "
        "contenttype FROM videometadata ");

    if (!sql.isEmpty())
        BaseMetadataQuery.append(sql);

    query.prepare(BaseMetadataQuery);

    if (query.exec() && query.isActive())
    {
        while (query.next())
        {
            items.push_back(VideoMetadataPtr(new VideoMetadata(query)));
        }
    }
    else
    {
        MythDB::DBError("Querying video metadata", query);
    }
}

void VideoMetadataListManager::setList(metadata_list &list)
{
    m_imp->setList(list);
}

const VideoMetadataListManager::metadata_list &
VideoMetadataListManager::getList() const
{
    return m_imp->getList();
}

VideoMetadataListManager::VideoMetadataPtr
VideoMetadataListManager::byFilename(const QString &file_name) const
{
    return m_imp->byFilename(file_name);
}

VideoMetadataListManager::VideoMetadataPtr
VideoMetadataListManager::byID(unsigned int db_id) const
{
    return m_imp->byID(db_id);
}

bool VideoMetadataListManager::purgeByFilename(const QString &file_name)
{
    return m_imp->purgeByFilename(file_name);
}

bool VideoMetadataListManager::purgeByID(unsigned int db_id)
{
    return m_imp->purgeByID(db_id);
}

const QString meta_node::m_empty_path;

const QString& meta_node::getPath() const
{
    return m_empty_path;
}

const QString& meta_node::getFQPath()
{
    if (m_fq_path.length())
        return m_fq_path;

    if (m_parent && !m_path_root)
        m_fq_path = m_parent->getFQPath() + "/" + getPath();
    else
    {
        QString p = getPath();
        if (p.startsWith("myth://"))
            m_fq_path = p;
        else
            m_fq_path = ((p.length() && p[0] != '/') ? "/" : "") + p;
    }

    return m_fq_path;
}

void meta_node::setParent(meta_node *parent)
{
    m_parent = parent;
}

void meta_node::setPathRoot(bool is_root)
{
    m_path_root = is_root;
}

const QString meta_data_node::m_meta_bug = "Bug";

const QString& meta_data_node::getName() const
{
    if (m_data)
    {
        return m_data->GetTitle();
    }

    return m_meta_bug;
}

const VideoMetadata* meta_data_node::getData() const
{
    return m_data;
}

VideoMetadata* meta_data_node::getData()
{
    return m_data;
}

meta_dir_node::meta_dir_node(const QString &path, const QString &name,
                            meta_dir_node *parent, bool is_path_root,
                            const QString &host, const QString &prefix,
                            const QVariant &data)
  : meta_node(parent, is_path_root), m_path(path), m_name(name),
    m_host(host), m_prefix(prefix), m_data(data)
{
    if (!name.length())
        m_name = path;
}

void meta_dir_node::setName(const QString &name)
{
    m_name = name;
}

const QString &meta_dir_node::getName() const
{
    return m_name;
}

void meta_dir_node::SetHost(const QString &host)
{
    m_host = host;
}

const QString &meta_dir_node::GetHost() const
{
    return m_host;
}

void meta_dir_node::SetPrefix(const QString &prefix)
{
    m_prefix = prefix;
}

const QString &meta_dir_node::GetPrefix() const
{
    return m_prefix;
}

const QString &meta_dir_node::getPath() const
{
    return m_path;
}

void meta_dir_node::setPath(const QString &path)
{
    m_path = path;
}

void meta_dir_node::SetData(const QVariant &data)
{
    m_data = data;
}

const QVariant &meta_dir_node::GetData() const
{
    return m_data;
}

bool meta_dir_node::DataIsValid() const
{
    return m_data.isValid();
}

smart_dir_node meta_dir_node::addSubDir(const QString &subdir,
                                        const QString &name,
                                        const QString &host,
                                        const QString &prefix,
                                        const QVariant &data)
{
    return getSubDir(subdir, name, true, host, prefix, data);
}

void meta_dir_node::addSubDir(const smart_dir_node &subdir)
{
    m_subdirs.push_back(subdir);
}

smart_dir_node meta_dir_node::getSubDir(const QString &subdir,
                                        const QString &name,
                                        bool create,
                                        const QString &host,
                                        const QString &prefix,
                                        const QVariant &data)
{
    for (meta_dir_list::const_iterator p = m_subdirs.begin();
    p != m_subdirs.end(); ++p)
    {
        if (subdir == (*p)->getPath())
        {
            return *p;
        }
    }

    if (create)
    {
        smart_dir_node node(new meta_dir_node(subdir, name, this, false,
                                              host, prefix, data));
        m_subdirs.push_back(node);
        return node;
    }

    return smart_dir_node();
}

void meta_dir_node::addEntry(const smart_meta_node &entry)
{
    entry->setParent(this);
    m_entries.push_back(entry);
}

void meta_dir_node::clear()
{
    m_subdirs.clear();
    m_entries.clear();
}

bool meta_dir_node::empty() const
{
    return m_subdirs.empty() && m_entries.empty();
}

int meta_dir_node::subdir_count() const
{
    return m_subdirs.size();
}

meta_dir_list::iterator meta_dir_node::dirs_begin()
{
    return m_subdirs.begin();
}

meta_dir_list::iterator meta_dir_node::dirs_end()
{
    return m_subdirs.end();
}

meta_dir_list::const_iterator meta_dir_node::dirs_begin() const
{
    return m_subdirs.begin();
}

meta_dir_list::const_iterator meta_dir_node::dirs_end() const
{
    return m_subdirs.end();
}

meta_data_list::iterator meta_dir_node::entries_begin()
{
    return m_entries.begin();
}

meta_data_list::iterator meta_dir_node::entries_end()
{
    return m_entries.end();
}

meta_data_list::const_iterator meta_dir_node::entries_begin() const
{
    return m_entries.begin();
}

meta_data_list::const_iterator meta_dir_node::entries_end() const
{
    return m_entries.end();
}

// Returns true if this directory or any of its subdirectories
// have entries. TODO: cache this value
bool meta_dir_node::has_entries() const
{
    bool ret = m_entries.size();

    if (!ret)
    {
        for (meta_dir_list::const_iterator p = m_subdirs.begin();
        p != m_subdirs.end(); ++p)
        {
            ret = (*p)->has_entries();
            if (ret) break;
        }
    }

    return ret;
}
