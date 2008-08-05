// -*- Mode: c++ -*-

#ifndef _THUMBVIEW_H_
#define _THUMBVIEW_H_

// Qt headers
#include <q3deepcopy.h>
#include <QString>
#include <QList>
#include <QHash>
#include <QPixmap>

class MythMediaDevice;
class QPixmap;

class ThumbItem
{
  public:
    ThumbItem() :
        m_name(QString::null), m_caption(QString::null),
        m_path(QString::null), m_isDir(false),
        m_pixmap(NULL),        m_mediaDevice(NULL) { }
    ThumbItem(const QString &name, const QString &path, bool isDir,
              MythMediaDevice *dev = NULL) :
        m_name(Q3DeepCopy<QString>(name)), m_caption(QString::null),
        m_path(Q3DeepCopy<QString>(path)), m_isDir(isDir),
        m_pixmap(NULL),                   m_mediaDevice(dev) { }
    ~ThumbItem();

    // commands
    bool Remove(void);
    void InitCaption(bool get_caption);

    // sets
    void SetRotationAngle(int angle);
    void SetName(const QString &name)
        { m_name = Q3DeepCopy<QString>(name); }
    void SetCaption(const QString &caption)
        { m_caption = Q3DeepCopy<QString>(caption); }
    void SetPath(const QString &path, bool isDir)
        { m_path = Q3DeepCopy<QString>(path); m_isDir = isDir; }
    void SetPixmap(QPixmap *pixmap);
    void SetMediaDevice(MythMediaDevice *dev)
        { m_mediaDevice = dev; }

    // gets
    long    GetRotationAngle(void);
    QString GetName(void)    const { return m_name;               }
    bool    HasCaption(void) const { return !m_caption.isEmpty(); }
    QString GetCaption(void) const { return m_caption;            }
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
    QString  m_path;
    bool     m_isDir;
    QPixmap *m_pixmap;
    MythMediaDevice *m_mediaDevice;
};
typedef QList<ThumbItem*> ThumbList;
typedef QHash<QString, ThumbItem*>    ThumbHash;

#endif // _THUMBVIEW_H_
