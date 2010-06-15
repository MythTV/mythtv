#ifndef _STORAGEGROUP_H
#define _STORAGEGROUP_H

#include <QStringList>
#include <QMutex>
#include <QHash>

#include "mythexp.h"

class MPUBLIC StorageGroup
{
  public:
    StorageGroup(const QString group = "", const QString hostname = "",
                 const bool allowFallback = true);

    void    Init(const QString group = "Default",
                 const QString hostname = "",
                 const bool allowFallback = true);

    QString getName(void) const
        { QString tmp = m_groupname; tmp.detach(); return tmp; }

    QStringList GetDirList(void) const
        { QStringList tmp = m_dirlist; tmp.detach(); return tmp; }

    QStringList GetFileList(QString Path);
    QStringList GetFileInfoList(QString Path);
    bool FileExists(QString filename);
    QStringList GetFileInfo(QString filename);
    static QString GetRelativePathname(const QString &filename);
    static bool FindDirs(const QString group = "Default",
                         const QString hostname = "",
                         QStringList *dirlist = NULL);

    QString FindRecordingFile(QString filename);
    QString FindRecordingDir(QString filename);

    QString FindNextDirMostFree(void);

    static void CheckAllStorageGroupDirs(void);

    static const char *kDefaultStorageDir;
    static const QStringList kSpecialGroups;

    static QStringList getRecordingsGroups(void);
    static QStringList getGroupDirs(QString groupname, QString host);

    static void ClearGroupToUseCache(void);
    static QString GetGroupToUse(
        const QString &host, const QString &sgroup);

  private:
    QString      m_groupname;
    QString      m_hostname;
    bool         m_allowFallback;
    QStringList  m_dirlist;

    static QMutex                 s_groupToUseLock;
    static QHash<QString,QString> s_groupToUseCache;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
