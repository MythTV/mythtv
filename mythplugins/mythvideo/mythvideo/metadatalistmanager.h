#ifndef METADATALISTMANAGER_H_
#define METADATALISTMANAGER_H_

#include "quicksp.h"

class MetadataListManager
{
  public:
    typedef simple_ref_ptr<Metadata> MetadataPtr;
    typedef std::list<MetadataPtr> metadata_list;

  public:
    static void loadAllFromDatabase(metadata_list &items);

  public:
    MetadataListManager();
    ~MetadataListManager();

    void setList(metadata_list &list);
    const metadata_list &getList() const;

    MetadataPtr byFilename(const QString &file_name) const;
    MetadataPtr byID(unsigned int db_id) const;

    bool purgeByFilename(const QString &file_name);
    bool purgeByID(unsigned int db_id);

  private:
    class MetadataListManagerImp *m_imp;
};

#endif // METADATALISTMANAGER_H_
