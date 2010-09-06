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

void VideoMetadataListManager::loadAllFromDatabase(metadata_list &items)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.setForwardOnly(true);
    const QString BaseMetadataQuery(
        "SELECT title, director, studio, plot, rating, year, releasedate,"
        "userrating, length, filename, hash, showlevel, "
        "coverfile, inetref, homepage, childid, browse, watched, "
        "playcommand, category, intid, trailer, screenshot, banner, fanart, "
        "subtitle, tagline, season, episode, host, insertdate, processed "
        " FROM videometadata");

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
