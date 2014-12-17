// -*- Mode: c++ -*-

#ifndef _THUMBVIEW_H_
#define _THUMBVIEW_H_

// Qt headers
#include <QDateTime>
#include <QString>
#include <QList>
#include <QHash>
#include <QPixmap>
#include <QCoreApplication>

class MythMediaDevice;
class QPixmap;

class ThumbItem
{
    Q_DECLARE_TR_FUNCTIONS (ThumbItem)

  public:
    ThumbItem() :
        m_name(QString::null), m_caption(QString::null),
        m_path(QString::null), m_isDir(false),
        m_pixmap(NULL),        m_mediaDevice(NULL) { }
    ThumbItem(const QString &name, const QString &path, bool isDir,
              MythMediaDevice *dev = NULL);
    ~ThumbItem();

    // commands
    bool Remove(void);
    void InitCaption(bool get_caption);
    void InitTimestamp();

    // sets
    void SetRotationAngle(int angle);
    void SetName(const QString &name)
        { m_name = name; m_name.detach(); }
    void SetCaption(const QString &caption)
        { m_caption = caption; m_caption.detach(); }
    void SetTimestamp(const QDateTime &timestamp)
        { m_timestamp = timestamp; }
    void SetPath(const QString &path, bool isDir)
        { m_path = path; m_path.detach(), m_isDir = isDir; }
    void SetImageFilename(const QString &filename)
        { m_imageFilename = filename; m_imageFilename.detach(); }
    void SetPixmap(QPixmap *pixmap);
    void SetMediaDevice(MythMediaDevice *dev)
        { m_mediaDevice = dev; }

    // gets
    long    GetRotationAngle(void);
    QString GetName(void)    const { return m_name;               }
    bool    HasCaption(void) const { return !m_caption.trimmed().isEmpty(); }
    QString GetCaption(void) const { return m_caption;            }
    bool    HasTimestamp(void) const { return m_timestamp.isValid(); }
    QDateTime GetTimestamp(void) const { return m_timestamp;      }
    QString GetImageFilename(void) const { return m_imageFilename; }
    QString GetPath(void)    const { return m_path;               }
    bool    IsDir(void)      const { return m_isDir;              }
    QString GetDescription(const QString &status,
                           const QSize &sz, int angle) const;

    // non-const gets
    QPixmap         *GetPixmap(void)      { return m_pixmap;      }
    MythMediaDevice *GetMediaDevice(void) { return m_mediaDevice; }

  private:
    QString  m_name;
    QString  m_caption;
    QDateTime m_timestamp;
    QString  m_path;
    QString  m_imageFilename;
    bool     m_isDir;
    QPixmap *m_pixmap;
    MythMediaDevice *m_mediaDevice;
};
typedef QList<ThumbItem*> ThumbList;
typedef QHash<QString, ThumbItem*>    ThumbHash;

#endif // _THUMBVIEW_H_
