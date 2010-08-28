#include <map>

#include "mythdb.h"
#include "metadatalistmanager.h"

class MetadataListManagerImp
{
  public:
    typedef MetadataListManager::MetadataPtr MetadataPtr;
    typedef MetadataListManager::metadata_list metadata_list;

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


    MetadataPtr byFilename(const QString &file_name) const
    {
        string_to_meta::const_iterator p = m_file_map.find(file_name);
        if (p != m_file_map.end())
        {
            return *(p->second);
        }
        return MetadataPtr();
    }

    MetadataPtr byID(unsigned int db_id) const
    {
        int_to_meta::const_iterator p = m_id_map.find(db_id);
        if (p != m_id_map.end())
        {
            return *(p->second);
        }
        return MetadataPtr();
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
    bool purge_entry(MetadataPtr metadata)
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

MetadataListManager::MetadataListManager()
{
    m_imp = new MetadataListManagerImp();
}

MetadataListManager::~MetadataListManager()
{
    delete m_imp;
}

void MetadataListManager::loadAllFromDatabase(metadata_list &items)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.setForwardOnly(true);
    const QString BaseMetadataQuery(
        "SELECT title, director, plot, rating, year, releasedate,"
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
            items.push_back(MetadataPtr(new Metadata(query)));
        }
    }
    else
    {
        MythDB::DBError("Querying video metadata", query);
    }
}

void MetadataListManager::setList(metadata_list &list)
{
    m_imp->setList(list);
}

const MetadataListManager::metadata_list &
MetadataListManager::getList() const
{
    return m_imp->getList();
}

MetadataListManager::MetadataPtr
MetadataListManager::byFilename(const QString &file_name) const
{
    return m_imp->byFilename(file_name);
}

MetadataListManager::MetadataPtr
MetadataListManager::byID(unsigned int db_id) const
{
    return m_imp->byID(db_id);
}

bool MetadataListManager::purgeByFilename(const QString &file_name)
{
    return m_imp->purgeByFilename(file_name);
}

bool MetadataListManager::purgeByID(unsigned int db_id)
{
    return m_imp->purgeByID(db_id);
}
