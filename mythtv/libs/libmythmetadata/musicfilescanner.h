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

    typedef QMap <QString, MusicFileLocation> MusicLoadedMap;
    public:
        MusicFileScanner(void);
        ~MusicFileScanner(void);

        void SearchDir(QString &directory);

        static bool IsRunning(void);

    private:
        void BuildFileList(QString &directory, MusicLoadedMap &music_files, MusicLoadedMap &art_files, int parentid);
        int  GetDirectoryId(const QString &directory, const int &parentid);
        bool HasFileChanged(const QString &filename, const QString &date_modified);
        void AddFileToDB(const QString &filename);
        void RemoveFileFromDB (const QString &filename);
        void UpdateFileInDB(const QString &filename);
        void ScanMusic(MusicLoadedMap &music_files);
        void ScanArtwork(MusicLoadedMap &music_files);
        void cleanDB();
        bool IsArtFile(const QString &filename);
        bool IsMusicFile(const QString &filename);

        void updateLastRunEnd(void);
        void updateLastRunStart(void);
        void updateLastRunStatus(QString &status);

        QString  m_startdir;
        IdCache  m_directoryid;
        IdCache  m_artistid;
        IdCache  m_genreid;
        IdCache  m_albumid;

        uint m_tracksTotal, m_tracksUnchanged, m_tracksAdded, m_tracksRemoved, m_tracksUpdated;
        uint m_coverartTotal, m_coverartUnchanged, m_coverartAdded, m_coverartRemoved, m_coverartUpdated;
};

#endif // _MUSICFILESCANNER_H_
