#ifndef VIDEOMETADATALISTMANAGER_H_
#define VIDEOMETADATALISTMANAGER_H_

#include <list>

#include "quicksp.h"
#include "videometadata.h"
#include "mythmetaexp.h"

class META_PUBLIC VideoMetadataListManager
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

class META_PUBLIC meta_node
{
  public:
    meta_node(meta_node *parent, bool is_path_root = false) :
            m_parent(parent), m_path_root(is_path_root) {}
    virtual ~meta_node() {}

    virtual const QString &getName() const = 0;
    virtual const QString &getPath() const;
    const QString &getFQPath();
    void setParent(meta_node *parent);
    void setPathRoot(bool is_root = true);

  protected:
    meta_node *m_parent;

  private:
    QString m_fq_path;
    bool m_path_root;
    static const QString m_empty_path;
};

class META_PUBLIC meta_data_node : public meta_node
{
  public:
    meta_data_node(VideoMetadata *data, meta_node *parent = NULL) :
                   meta_node(parent), m_data(data) {}
    const QString &getName() const;
    const VideoMetadata *getData() const;
    VideoMetadata *getData();

  private:
    VideoMetadata *m_data;
    static const QString m_meta_bug;
};

class meta_dir_node;

typedef simple_ref_ptr<meta_dir_node> smart_dir_node;
typedef simple_ref_ptr<meta_data_node> smart_meta_node;

typedef std::list<smart_dir_node> meta_dir_list;
typedef std::list<smart_meta_node> meta_data_list;

class META_PUBLIC meta_dir_node : public meta_node
{
  public:
    typedef meta_dir_list::iterator dir_iterator;
    typedef meta_dir_list::const_iterator const_dir_iterator;

    typedef meta_data_list::iterator entry_iterator;
    typedef meta_data_list::const_iterator const_entry_iterator;

  public:
    meta_dir_node(const QString &path, const QString &name = "",
                  meta_dir_node *parent = NULL, bool is_path_root = false,
                  const QString &host = "", const QString &prefix = "",
                  const QVariant &data = QVariant());
    meta_dir_node() : meta_node(NULL) { }

    void setName(const QString &name);
    const QString &getName() const;
    void SetHost(const QString &host);
    const QString &GetHost() const;
    void SetPrefix(const QString &prefix);
    const QString &GetPrefix() const;
    const QString &getPath() const;
    void setPath(const QString &path);
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

        for (meta_dir_list::iterator p = m_subdirs.begin();
        p != m_subdirs.end(); ++p)
        {
            (*p)->sort(dir_sort, entry_sort);
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
    QString m_name;
    QString m_host;
    QString m_prefix;
    meta_dir_list m_subdirs;
    meta_data_list m_entries;

    QVariant m_data;
};
#endif // VIDEOMETADATALISTMANAGER_H_
