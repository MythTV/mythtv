#ifndef VIDEOMETADATALISTMANAGER_H_
#define VIDEOMETADATALISTMANAGER_H_

#include <list>

#include "quicksp.h"
#include "videometadata.h"
#include "mythmetaexp.h"

class META_PUBLIC VideoMetadataListManager
{
  public:
    using VideoMetadataPtr = simple_ref_ptr<VideoMetadata>;
    using metadata_list = std::list<VideoMetadataPtr>;

  public:
    static VideoMetadataPtr loadOneFromDatabase(uint id);
    static void loadAllFromDatabase(metadata_list &items,
                                    const QString &sql = "",
                                    const QString &bindValue = "");

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
    class VideoMetadataListManagerImp *m_imp {nullptr};
};

class META_PUBLIC meta_node
{
  public:
    meta_node(meta_node *parent, bool is_path_root = false) :
            m_parent(parent), m_pathRoot(is_path_root) {}
    virtual ~meta_node() = default;

    virtual const QString &getName() const = 0;
    virtual const QString &getPath() const;
    const QString &getFQPath();
    void setParent(meta_node *parent);
    void setPathRoot(bool is_root = true);

  protected:
    meta_node *m_parent {nullptr};

  private:
    QString m_fqPath;
    bool m_pathRoot;
    static const QString kEmptyPath;
};

class META_PUBLIC meta_data_node : public meta_node
{
  public:
    meta_data_node(VideoMetadata *data, meta_node *parent = nullptr) :
                   meta_node(parent), m_data(data) {}
    const QString &getName() const override; // meta_node
    const VideoMetadata *getData() const;
    VideoMetadata *getData();

  private:
    VideoMetadata *m_data {nullptr};
    static const QString kMetaBug;
};

class meta_dir_node;

using smart_dir_node = simple_ref_ptr<meta_dir_node>;
using smart_meta_node = simple_ref_ptr<meta_data_node>;

using meta_dir_list = std::list<smart_dir_node>;
using meta_data_list = std::list<smart_meta_node>;

class META_PUBLIC meta_dir_node : public meta_node
{
  public:
    using dir_iterator = meta_dir_list::iterator;
    using const_dir_iterator = meta_dir_list::const_iterator;

    using entry_iterator = meta_data_list::iterator;
    using const_entry_iterator = meta_data_list::const_iterator;

  public:
    meta_dir_node(const QString &path, const QString &name = "",
                  meta_dir_node *parent = nullptr, bool is_path_root = false,
                  QString host = "", QString prefix = "",
                  QVariant data = QVariant());
    meta_dir_node() : meta_node(nullptr) { }

    void ensureSortFields();
    void setName(const QString &name);
    const QString &getName() const override; // meta_node
    void SetHost(const QString &host);
    const QString &GetHost() const;
    void SetPrefix(const QString &prefix);
    const QString &GetPrefix() const;
    const QString &getPath() const override; // meta_node
    const QString &getSortPath() const;
    void setPath(const QString &path, const QString &sortPath = nullptr);
    void SetData(const QVariant &data);
    const QVariant &GetData() const;
    bool DataIsValid(void) const;
    smart_dir_node addSubDir(const QString &subdir,
                             const QString &name = "",
                             const QString &host = "",
                             const QString &prefix = "",
                             const QVariant &data = QVariant());
    void addSubDir(const smart_dir_node &subdir);
    smart_dir_node getSubDir(const QString &subdir,
                             const QString &name = "",
                             bool create = true,
                             const QString &host = "",
                             const QString &prefix = "",
                             const QVariant &data = QVariant());
    void addEntry(const smart_meta_node &entry);
    void clear();
    bool empty() const;
    int  subdir_count() const;
    template <typename DirSort, typename EntrySort>
    void sort(DirSort dir_sort, EntrySort entry_sort)
    {
        m_subdirs.sort(dir_sort);
        m_entries.sort(entry_sort);

        for (auto & subdir : m_subdirs)
        {
            if (subdir == nullptr)
                continue;
            subdir->sort(dir_sort, entry_sort);
        }
    }
    dir_iterator dirs_begin();
    dir_iterator dirs_end();
    const_dir_iterator dirs_begin() const;
    const_dir_iterator dirs_end() const;
    entry_iterator entries_begin();
    entry_iterator entries_end();
    const_entry_iterator entries_begin() const;
    const_entry_iterator entries_end() const;
    bool has_entries() const;

  private:
    QString m_path;
    QString m_sortPath;
    QString m_name;
    QString m_host;
    QString m_prefix;
    meta_dir_list m_subdirs;
    meta_data_list m_entries;

    QVariant m_data;
};
#endif // VIDEOMETADATALISTMANAGER_H_
