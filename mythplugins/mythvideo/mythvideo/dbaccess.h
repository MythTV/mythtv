#ifndef DBACCESS_H_
#define DBACCESS_H_

class SingleValueImp;

class SingleValue
{
  public:
    typedef std::pair<int, QString> entry;
    typedef std::vector<entry> entry_list;

  public:
    int add(const QString &name);
    bool get(int id, QString &value);
    void remove(int id);
    bool exists(int id);
    bool exists(const QString &name);
    const entry_list &getList();

    void load_data();

  protected:
    SingleValue(SingleValueImp *imp);
    virtual ~SingleValue();

  private:
    SingleValueImp *m_imp;
};

class MultiValueImp;
class MultiValue
{
  public:
    struct entry
    {
        int id;
        typedef std::vector<int> values_type;
        values_type values;
    };
    typedef std::vector<entry> entry_list;

  public:
    int add(int id, int value);
    bool get(int id, entry &values);
    void remove(int id, int value);
    void remove(int id);
    bool exists(int id, int value);
    bool exists(int id);

    void load_data();

  protected:
    MultiValue(MultiValueImp *imp);
    virtual ~MultiValue();

  private:
    MultiValueImp *m_imp;
};

class VideoCategory : public SingleValue
{
  public:
    static VideoCategory &getCategory();

  private:
    VideoCategory();
    ~VideoCategory();
};

class VideoCountry : public SingleValue
{
  public:
    static VideoCountry &getCountry();

  private:
    VideoCountry();
    ~VideoCountry();
};

class VideoGenre : public SingleValue
{
  public:
    static VideoGenre &getGenre();

  private:
    VideoGenre();
    ~VideoGenre();
};

class VideoGenreMap : public MultiValue
{
  public:
    static VideoGenreMap &getGenreMap();

  private:
    VideoGenreMap();
    ~VideoGenreMap();
};

class VideoCountryMap : public MultiValue
{
  public:
    static VideoCountryMap &getCountryMap();

  private:
    VideoCountryMap();
    ~VideoCountryMap();
};

class FileAssociations
{
  public:
    struct file_association
    {
        unsigned int id;
        QString extension;
        QString playcommand;
        bool ignore;
        bool use_default;

        file_association();
        file_association(unsigned int l_id, const QString &ext,
                         const QString &playcmd, bool l_ignore,
                         bool l_use_default);
    };
    typedef std::vector<file_association> association_list;
    typedef std::vector<std::pair<QString, bool> > ext_ignore_list;

  public:
    static FileAssociations &getFileAssociation();

  public:
    unsigned int add(const QString &ext, const QString &playcommand,
                     bool ignore, bool use_default);
    bool get(unsigned int id, file_association &val) const;
    bool get(const QString &ext, file_association &val) const;
    bool remove(unsigned int id);

    const association_list &getList() const;

    void getExtensionIgnoreList(ext_ignore_list &ext_ignore) const;

    void load_data();

  private:
    FileAssociations();
    ~FileAssociations();

  private:
    class FileAssociationsImp *m_imp;
};

#endif // DBACCESS_H_
