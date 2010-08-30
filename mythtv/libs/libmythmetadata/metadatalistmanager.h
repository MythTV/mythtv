#ifndef METADATALISTMANAGER_H_
#define METADATALISTMANAGER_H_

#include <list>

#include "quicksp.h"
#include "videometadata.h"
#include "mythexp.h"

class MPUBLIC MetadataListManager
{
  public:
    typedef simple_ref_ptr<VideoMetadata> VideoMetadataPtr;
    typedef std::list<VideoMetadataPtr> metadata_list;

  public:
    static void loadAllFromDatabase(metadata_list &items);

  public:
    MetadataListManager();
    ~MetadataListManager();

    void setList(metadata_list &list);
    const metadata_list &getList() const;

    VideoMetadataPtr byFilename(const QString &file_name) const;
    VideoMetadataPtr byID(unsigned int db_id) const;

    bool purgeByFilename(const QString &file_name);
    bool purgeByID(unsigned int db_id);

  private:
    class MetadataListManagerImp *m_imp;
};

#endif // METADATALISTMANAGER_H_
