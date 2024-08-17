#include <map>

#include "libmythbase/mythdb.h"
#include "libmythbase/mythsorthelper.h"
#include "videometadatalistmanager.h"

class VideoMetadataListManagerImp
{
  public:
    using VideoMetadataPtr = VideoMetadataListManager::VideoMetadataPtr;
    using metadata_list = VideoMetadataListManager::metadata_list;

  private:
    using int_to_meta = std::map<unsigned int, metadata_list::iterator>;
    using string_to_meta = std::map<QString, metadata_list::iterator>;

  public:
    void setList(metadata_list &list)
    {
        m_idMap.clear();
        m_fileMap.clear();
        m_metaList.swap(list);

        for (auto p = m_metaList.begin(); p != m_metaList.end(); ++p)
        {
            m_idMap.insert(int_to_meta::value_type((*p)->GetID(), p));
            m_fileMap.insert(
                    string_to_meta::value_type((*p)->GetFilename(), p));
        }
    }

    const metadata_list &getList() const
    {
        return m_metaList;
    }


    VideoMetadataPtr byFilename(const QString &file_name) const
    {
        //NOLINTNEXTLINE(modernize-use-auto)
        string_to_meta::const_iterator p = m_fileMap.find(file_name);
        if (p != m_fileMap.end())
        {
            return *(p->second);
        }
        return {};
    }

    VideoMetadataPtr byID(unsigned int db_id) const
    {
        //NOLINTNEXTLINE(modernize-use-auto)
        int_to_meta::const_iterator p = m_idMap.find(db_id);
        if (p != m_idMap.end())
        {
            return *(p->second);
        }
        return {};
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
    bool purge_entry(const VideoMetadataPtr& metadata)
    {
        if (metadata)
        {
            auto im = m_idMap.find(metadata->GetID());

            if (im != m_idMap.end())
            {
                auto mdi = im->second;
                (*mdi)->DeleteFromDatabase();

                m_idMap.erase(im);
                auto sm = m_fileMap.find(metadata->GetFilename());
                if (sm != m_fileMap.end())
                    m_fileMap.erase(sm);
                m_metaList.erase(mdi);
                return true;
            }
        }

        return false;
    }

  private:
    metadata_list  m_metaList;
    int_to_meta    m_idMap;
    string_to_meta m_fileMap;
};

VideoMetadataListManager::VideoMetadataListManager()
  : m_imp(new VideoMetadataListManagerImp())
{
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

    return {new VideoMetadata()};
}

/// Load videometadata database into memory
///
/// Query consumed in VideoMetadataImp::fromDBRow
///
void VideoMetadataListManager::loadAllFromDatabase(metadata_list &items,
                                                   const QString &sql,
                                                   const QString &bindValue)
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
    if (!bindValue.isEmpty())
        query.bindValue(":BINDVALUE", bindValue);

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

const QString meta_node::kEmptyPath;

const QString& meta_node::getPath() const
{
    return kEmptyPath;
}

const QString& meta_node::getFQPath()
{
    if (!m_fqPath.isEmpty())
        return m_fqPath;

    if (m_parent && !m_pathRoot)
        m_fqPath = m_parent->getFQPath() + "/" + getPath();
    else
    {
        QString p = getPath();
        if (p.startsWith("myth://"))
            m_fqPath = p;
        else
            m_fqPath = ((!p.isEmpty() && p[0] != '/') ? "/" : "") + p;
    }

    return m_fqPath;
}

void meta_node::setParent(meta_node *parent)
{
    m_parent = parent;
}

void meta_node::setPathRoot(bool is_root)
{
    m_pathRoot = is_root;
}

const QString meta_data_node::kMetaBug = "Bug";

const QString& meta_data_node::getName() const
{
    if (m_data)
    {
        return m_data->GetTitle();
    }

    return kMetaBug;
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
                            QString host, QString prefix,
                            QVariant data)
  : meta_node(parent, is_path_root), m_path(path), m_name(name),
    m_host(std::move(host)), m_prefix(std::move(prefix)), m_data(std::move(data))
{
    if (name.isEmpty())
        m_name = path;
    ensureSortFields();
}

void meta_dir_node::ensureSortFields()
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();

    if (m_sortPath.isEmpty() and not m_path.isEmpty())
        m_sortPath = sh->doPathname(m_path);
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

const QString &meta_dir_node::getSortPath() const
{
    return m_sortPath;
}

void meta_dir_node::setPath(const QString &path, const QString &sortPath)
{
    m_path = path;
    m_sortPath = sortPath;
    ensureSortFields();
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
    for (auto & entry : m_subdirs)
    {
        if (entry && (subdir == entry->getPath()))
        {
            return entry;
        }
    }

    if (create)
    {
        smart_dir_node node(new meta_dir_node(subdir, name, this, false,
                                              host, prefix, data));
        m_subdirs.push_back(node);
        return node;
    }

    return {};
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
    bool ret = !m_entries.empty();

    if (!ret)
    {
        for (const auto & subdir : m_subdirs)
        {
            if (subdir == nullptr)
                continue;
            ret = subdir->has_entries();
            if (ret) break;
        }
    }

    return ret;
}
