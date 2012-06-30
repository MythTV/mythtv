// -*- Mode: c++ -*-

// Qt headers
#include <QPixmap>
#include <QFileInfo>
#include <QDir>

// MythTV plugin headers
#include <mythdb.h>

// MythGallery headers
#include "thumbview.h"
#include "galleryutil.h"

ThumbItem::ThumbItem(const QString &name, const QString &path, bool isDir,
                     MythMediaDevice *dev) :
        m_name(name), m_caption(QString::null),
        m_path(path), m_isDir(isDir),
        m_pixmap(NULL), m_mediaDevice(dev)
{
    m_name.detach();
    m_path.detach();
}

ThumbItem::~ThumbItem()
{
    if (m_pixmap)
    {
        delete m_pixmap;
        m_pixmap = NULL;
    }
}

bool ThumbItem::Remove(void)
{
    if (!QFile::exists(m_path) || !QFile::remove(m_path))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM gallerymetadata "
        "WHERE image = :PATH");
    query.bindValue(":PATH", m_path);

    if (!query.exec())
    {
        MythDB::DBError("thumb_item_remove", query);
        return false;
    }

    return true;
}

void ThumbItem::InitCaption(bool get_caption)
{
    if (!HasCaption() && get_caption)
        SetCaption(GalleryUtil::GetCaption(m_path));
    if (!HasCaption())
        SetCaption(m_name);
}

void ThumbItem::SetRotationAngle(int angle)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "REPLACE INTO gallerymetadata "
        "SET image = :IMAGE, "
        "    angle = :ANGLE");
    query.bindValue(":IMAGE", m_path);
    query.bindValue(":ANGLE", angle);

    if (!query.exec())
        MythDB::DBError("set_rotation_angle", query);

    SetPixmap(NULL);
}

void ThumbItem::SetPixmap(QPixmap *pixmap)
{
    if (m_pixmap)
        delete m_pixmap;
    m_pixmap = pixmap;
}

long ThumbItem::GetRotationAngle(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // first try to find the exact file
    query.prepare(
        "SELECT angle "
        "FROM gallerymetadata "
        "WHERE image = :PATH");
    query.bindValue(":PATH", m_path);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_rotation_angle", query);
    else if (query.next())
        return query.value(0).toInt();

    // second try to find the first image in the same directory
    query.prepare(
        "SELECT angle, image "
        "FROM gallerymetadata "
        "WHERE image LIKE :PATH "
        "ORDER BY image");
    query.bindValue(":PATH", m_path + '%');

    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_rotation_angle", query);
    else if (query.next())
        return query.value(0).toInt();

    return GalleryUtil::GetNaturalRotation(m_path);
}

QString ThumbItem::GetDescription(const QString &status,
                                  const QSize &sz, int angle) const
{
    QFileInfo fi(GetPath());

    QString info = GetName();

    if (!status.isEmpty())
        info += status;

    info += "\n\n" + QString(tr("Folder: %1")).arg(fi.dir().dirName());
    //: %1 is the creation date
    info += "\n" + QString(tr("Created: %1")).arg(fi.created().toString());
    //: %1 is the modification date
    info += "\n" + QString(tr("Modified: %1")).arg(fi.lastModified().toString());
    info += "\n" + tr("Size: %n bytes", "", fi.size());
    info += "\n" + tr("Width: %n pixel(s)", "", sz.width());
    info += "\n" + tr("Height: %n pixel(s)", "", sz.height());
    info += "\n" + QString(tr("Pixel Count: %1 megapixels"))
        .arg((float) sz.width() * sz.height() * (1.0f/1000000.0f), 0, 'f', 2);
    info += "\n" + tr("Rotation Angle: %n degree(s)", "", angle);

    return info;
}
