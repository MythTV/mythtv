#ifndef _MUSICFILESCANNER_H_
#define _MUSICFILESCANNER_H_

// MythTV
#include "mythmetaexp.h"

// Qt headers
#include <QCoreApplication>

typedef QMap<QString, int> IdCache;

class META_PUBLIC MusicFileScanner
{
    Q_DECLARE_TR_FUNCTIONS(MusicFileScanner)

    enum MusicFileLocation
    {
        kFileSystem,
        kDatabase,
        kNeedUpdate,
        kBoth
    };

    struct MusicFileData
    {
        QString startDir;
        MusicFileLocation location;
    };

    typedef QMap <QString, MusicFileData> MusicLoadedMap;
    public:
        MusicFileScanner(void);
        ~MusicFileScanner(void) = default;

        void SearchDirs(const QStringList &dirList);

        static bool IsRunning(void);

    private:
        void BuildFileList(QString &directory, MusicLoadedMap &music_files, MusicLoadedMap &art_files, int parentid);
        int  GetDirectoryId(const QString &directory, const int &parentid);
        bool HasFileChanged(const QString &filename, const QString &date_modified);
        void AddFileToDB(const QString &filename, const QString &startDir);
        void RemoveFileFromDB (const QString &filename, const QString &startDir);
        void UpdateFileInDB(const QString &filename, const QString &startDir);
        void ScanMusic(MusicLoadedMap &music_files);
        void ScanArtwork(MusicLoadedMap &music_files);
        void cleanDB();
        bool IsArtFile(const QString &filename);
        bool IsMusicFile(const QString &filename);

        void updateLastRunEnd(void);
        void updateLastRunStart(void);
        void updateLastRunStatus(QString &status);

        QStringList  m_startDirs;
        IdCache  m_directoryid;
        IdCache  m_artistid;
        IdCache  m_genreid;
        IdCache  m_albumid;

        uint m_tracksTotal       {0};
        uint m_tracksUnchanged   {0};
        uint m_tracksAdded       {0};
        uint m_tracksRemoved     {0};
        uint m_tracksUpdated     {0};
        uint m_coverartTotal     {0};
        uint m_coverartUnchanged {0};
        uint m_coverartAdded     {0};
        uint m_coverartRemoved   {0};
        uint m_coverartUpdated   {0};
};

#endif // _MUSICFILESCANNER_H_
