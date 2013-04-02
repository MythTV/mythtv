#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

// Qt headers
#include <QDirIterator>

// Other headers
#include <exiv2/exiv2.hpp>

// MythTV headers
#include "mythdbcon.h"
#include "imagemetadata.h"
#include "mythmetaexp.h"



class META_PUBLIC ImageUtils
{
public:
    static ImageUtils* getInstance();

    void LoadDirectoriesFromDB(QMap<QString, ImageMetadata *>*);
    void LoadFilesFromDB(QMap<QString, ImageMetadata *>*);
    void LoadFileFromDB(ImageMetadata * im, int id);

    int  InsertDirectoryIntoDB(ImageMetadata *);
    int  InsertFileIntoDB(ImageMetadata *);

    bool UpdateDirectoryInDB(ImageMetadata *);
    bool UpdateFileInDB(ImageMetadata *);

    bool RemoveFromDB(ImageMetadata *);
    bool RemoveDirectoryFromDB(ImageMetadata *);
    bool RemoveDirectoryFromDB(int);
    bool RemoveFileFromDB(ImageMetadata *);
    bool RemoveFileFromDB(int);

    void LoadDirectoryData(QFileInfo &, ImageMetadata *, int);
    void LoadFileData(QFileInfo &, ImageMetadata *);

    QStringList  GetStorageDirs();

    long                GetExifDate(const QString &, bool *);
    int                 GetExifOrientation(const QString &, bool *);
    QString             GetExifValue(const QString &, const QString &, bool *);
    QList<QStringList>  GetAllExifValues(const QString &fileName);

    void    SetExifDate(const QString &, const long, bool *);
    void    SetExifOrientation(const QString &, const int, bool *);
    void    SetExifValue(const QString &, const QString &, const QString &, bool *);

private:
    ImageUtils();
    ~ImageUtils();
    static ImageUtils   *m_instance;

    QStringList          m_imageFileExt;
    QStringList          m_videoFileExt;

    void LoadDirectoryValues(MSqlQuery &, ImageMetadata *);
    void LoadFileValues(MSqlQuery &, ImageMetadata *);

    bool HasExifKey(Exiv2::ExifData, const QString &);
};

#endif // IMAGEUTILS_H
