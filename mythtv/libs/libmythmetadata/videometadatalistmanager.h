#ifndef VIDEOMETADATALISTMANAGER_H_
#define VIDEOMETADATALISTMANAGER_H_

#include <list>

#include "quicksp.h"
#include "videometadata.h"
#include "mythexp.h"

class MPUBLIC VideoMetadataListManager
{
  public:
    typedef simple_ref_ptr<VideoMetadata> VideoMetadataPtr;
    typedef std::list<VideoMetadataPtr> metadata_list;

  public:
    static void loadAllFromDatabase(metadata_list &items);

  public:
    VideoMetadataListManager();
    ~VideoMetadataListManager();

    void setList(metadata_list &list);
    const metadata_list &getList() const;

    VideoMetadataPtr byFilename(const QString &file_name) const;
    VideoMetadataPtr byID(unsigned int db_id) const;

    bool purgeByFilename(const QString &file_name);
    bool purgeByID(unsigned int db_id);

  private:
    class VideoMetadataListManagerImp *m_imp;
};

#endif // VIDEOMETADATALISTMANAGER_H_
