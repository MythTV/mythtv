#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

// Qt headers
#include <QFileInfo>
#include <QString>
#include <QImage>
#include <QList>

// MythTV headers
#include "mythmetaexp.h"



// We need to use other names to avoid
// getting coflicts with the videolist.h file
enum ImageTreeNodeType {
    kUnknown        = 0,
    kBaseDirectory  = 1,
    kSubDirectory   = 2,
    kUpDirectory    = 3,
    kImageFile      = 4,
    kVideoFile      = 5
};

enum ImageFileOrientationState {
    kFileRotateCW       = 0,
    kFileRotateCCW      = 1,
    kFileFlipHorizontal = 2,
    kFileFlipVertical   = 3,
    kFileZoomIn         = 4,
    kFileZoomOut        = 5
};

enum ImageFileSortOrder {
    kSortByNameAsc     = 0,
    kSortByNameDesc    = 1,
    kSortByModTimeAsc  = 2,
    kSortByModTimeDesc = 3,
    kSortByExtAsc      = 4,
    kSortByExtDesc     = 5,
    kSortBySizeAsc     = 6,
    kSortBySizeDesc    = 7,
    kSortByDateAsc     = 8,
    kSortByDateDesc    = 9
};

const static int kMaxFolderThumbnails = 4;


class META_PUBLIC ImageMetadata
{
public:
    ImageMetadata();
    ~ImageMetadata();

    // Database fields
    int         m_id;
    QString     m_fileName;
    QString     m_name;
    QString     m_path;
    int         m_parentId;
    int         m_dirCount;
    int         m_fileCount;
    int         m_type;
    int         m_modTime;
    int         m_size;
    QString     m_extension;
    double      m_date;
    int         m_isHidden;

    // Internal information
    bool        m_selected;

    int         GetAngle()  const   { return m_angle; }
    int         GetZoom()   const   { return m_zoom; }
    int         GetOrientation();
    void        SetAngle(int);
    void        SetZoom(int);
    void        SetOrientation(int, bool);

    // Internal thumbnail information
    QString         m_thumbPath;
    QList<QString> *m_thumbFileNameList;

private:
    int         m_zoom;
    int         m_angle;
    int         m_orientation;
};

Q_DECLARE_METATYPE(ImageMetadata*)

#endif // IMAGEMETADATA_H
