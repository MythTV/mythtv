#ifndef _STORAGEGROUP_H
#define _STORAGEGROUP_H

#include <QStringList>
#include <QMutex>
#include <QHash>
#include <QMap>

#include "mythbaseexp.h"

class MBASE_PUBLIC StorageGroup
{
  public:
    StorageGroup(const QString &group = "", const QString &hostname = "",
                 const bool allowFallback = true);

    void    Init(const QString &group = "Default",
                 const QString &hostname = "",
                 const bool allowFallback = true);

    QString getName(void) const
        { QString tmp = m_groupname; tmp.detach(); return tmp; }

    QStringList GetDirList(void) const
        { QStringList tmp = m_dirlist; tmp.detach(); return tmp; }
    QString GetFirstDir(bool appendSlash = false) const;

    QStringList GetDirFileList(const QString &dir, const QString &base,
                               bool recursive = false, bool onlyDirs = false);
    QStringList GetDirList(const QString &Path, bool recursive = false);
    QStringList GetFileList(const QString &Path, bool recursive = false);
    QStringList GetFileInfoList(const QString &Path);
    bool FileExists(const QString &filename);
    QStringList GetFileInfo(const QString &filename);
    static QString GetRelativePathname(const QString &filename);
    static bool FindDirs(const QString &group = "Default",
                         const QString &hostname = "",
                         QStringList *dirlist = NULL);

    QString FindFile(const QString &filename);
    QString FindFileDir(const QString &filename);

    QString FindNextDirMostFree(void);

    static void CheckAllStorageGroupDirs(void);

    static const char *kDefaultStorageDir;
    static const QStringList kSpecialGroups;

    static QStringList getRecordingsGroups(void);
    static QStringList getGroupDirs(const QString &groupname,
                                    const QString &host);

    static void ClearGroupToUseCache(void);
    static QString GetGroupToUse(
        const QString &host, const QString &sgroup);

  private:
    static void    StaticInit(void);
    static bool    m_staticInitDone;
    static QMutex  m_staticInitLock;

    QString      m_groupname;
    QString      m_hostname;
    bool         m_allowFallback;
    QStringList  m_dirlist;

    static QMap<QString, QString> m_builtinGroups;

    static QMutex                 s_groupToUseLock;
    static QHash<QString,QString> s_groupToUseCache;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
