#ifndef DBACCESS_H_
#define DBACCESS_H_

#include <vector>
#include <utility> // for std::pair

#include "mythmetaexp.h"

class SingleValueImp;

class META_PUBLIC SingleValue
{
  public:
    using entry = std::pair<int, QString>;
    using entry_list = std::vector<entry>;

  public:
    int add(const QString &name);
    bool get(int id, QString &category);
    void remove(int id);
    bool exists(int id);
    bool exists(const QString &name);
    const entry_list &getList();

    void load_data();

  protected:
    explicit SingleValue(SingleValueImp *imp) : m_imp(imp) {}
    virtual ~SingleValue();

  private:
    SingleValueImp *m_imp {nullptr};
};

class MultiValueImp;
class META_PUBLIC MultiValue
{
  public:
    struct entry
    {
        int id;
        using values_type = std::vector<long>;
        values_type values;
    };
    using entry_list = std::vector<entry>;

  public:
    int add(int id, int value);
    bool get(int id, entry &values);
    void remove(int id, int value);
    void remove(int id);
    bool exists(int id, int value);
    bool exists(int id);

    void load_data();

  protected:
    explicit MultiValue(MultiValueImp *imp)  : m_imp(imp) {}
    virtual ~MultiValue() = default;

  private:
    MultiValueImp *m_imp {nullptr};
};

class META_PUBLIC VideoCategory : public SingleValue
{
  public:
    static VideoCategory &GetCategory();

  private:
    VideoCategory();
    ~VideoCategory() = default;
};

class META_PUBLIC VideoCountry : public SingleValue
{
  public:
    static VideoCountry &getCountry();

  private:
    VideoCountry();
    ~VideoCountry() = default;
};

class META_PUBLIC VideoGenre : public SingleValue
{
  public:
    static VideoGenre &getGenre();

  private:
    VideoGenre();
    ~VideoGenre() = default;
};

class META_PUBLIC VideoGenreMap : public MultiValue
{
  public:
    static VideoGenreMap &getGenreMap();

  private:
    VideoGenreMap();
    ~VideoGenreMap() = default;
};

class META_PUBLIC VideoCountryMap : public MultiValue
{
  public:
    static VideoCountryMap &getCountryMap();

  private:
    VideoCountryMap();
    ~VideoCountryMap() = default;
};

class META_PUBLIC VideoCast : public SingleValue
{
  public:
    static VideoCast &GetCast();

  private:
    VideoCast();
    ~VideoCast() = default;
};

class META_PUBLIC VideoCastMap : public MultiValue
{
  public:
    static VideoCastMap &getCastMap();

  private:
    VideoCastMap();
    ~VideoCastMap() = default;
};

class META_PUBLIC FileAssociations
{
  public:
    struct META_PUBLIC file_association
    {
        unsigned int id     {0};
        QString extension;
        QString playcommand;
        bool ignore         {false};
        bool use_default    {false};

        file_association() = default;
        file_association(unsigned int l_id, const QString &ext,
                         const QString &playcmd, bool l_ignore,
                         bool l_use_default)
            : id(l_id), extension(ext), playcommand(playcmd),
              ignore(l_ignore), use_default(l_use_default) {}
    };
    using association_list = std::vector<file_association>;
    using ext_ignore_list = std::vector<std::pair<QString, bool> >;

  public:
    static FileAssociations &getFileAssociation();

  public:
    bool add(file_association &fa);
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
    class FileAssociationsImp *m_imp {nullptr};
};

#endif // DBACCESS_H_
