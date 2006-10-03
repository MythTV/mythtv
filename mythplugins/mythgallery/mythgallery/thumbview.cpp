// -*- Mode: c++ -*-

// Qt headers
#include <qpixmap.h>
#include <qfileinfo.h>
#include <qdir.h>

// MythTV plugin headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// MythGallery headers
#include "thumbview.h"
#include "galleryutil.h"

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
    query.bindValue(":PATH", m_path.utf8());

    if (!query.exec())
    {
        MythContext::DBError("thumb_item_remove", query);
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
    query.bindValue(":IMAGE", m_path.utf8());
    query.bindValue(":ANGLE", angle);

    if (!query.exec())
        MythContext::DBError("set_rotation_angle", query);

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
    query.prepare(
        "SELECT angle "
        "FROM gallerymetadata "
        "WHERE image = :PATH");
    query.bindValue(":PATH", m_path.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("get_rotation_angle", query);
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

    info += "\n\n" + QObject::tr("Folder: ") + fi.dir().dirName();
    info += "\n" + QObject::tr("Created: ") + fi.created().toString();
    info += "\n" + QObject::tr("Modified: ") +
        fi.lastModified().toString();
    info += "\n" + QString(QObject::tr("Bytes") + ": %1").arg(fi.size());
    info += "\n" + QString(QObject::tr("Width") + ": %1 " +
                           QObject::tr("pixels")).arg(sz.width());
    info += "\n" + QString(QObject::tr("Height") + ": %1 " +
                           QObject::tr("pixels")).arg(sz.height());
    info += "\n" + QString(QObject::tr("Pixel Count") + ": %1 " +
                           QObject::tr("megapixels"))
        .arg((float) sz.width() * sz.height() * (1.0f/1000000.0f), 0, 'f', 2);
    info += "\n" + QString(QObject::tr("Rotation Angle") + ": %1 " +
                           QObject::tr("degrees")).arg(angle);

    return info;
}
