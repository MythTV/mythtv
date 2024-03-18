#ifndef MUSICFILESCANNER_H
#define MUSICFILESCANNER_H

// MythTV
#include "mythmetaexp.h"

// Qt headers
#include <QCoreApplication>

using IdCache = QMap<QString, int>;

class META_PUBLIC MusicFileScanner
{
    Q_DECLARE_TR_FUNCTIONS(MusicFileScanner)

    enum MusicFileLocation : std::uint8_t
    {
        kFileSystem,
        kDatabase,
        kNeedUpdate,
        kBoth
    };

    struct MusicFileData
    {
        QString startDir;
        MusicFileLocation location {kFileSystem};
    };

    using MusicLoadedMap = QMap <QString, MusicFileData>;
    public:
        explicit MusicFileScanner(bool force = false);
        ~MusicFileScanner(void) = default;

        void SearchDirs(const QStringList &dirList);

        static bool IsRunning(void);

    private:
        void BuildFileList(QString &directory, MusicLoadedMap &music_files, MusicLoadedMap &art_files, int parentid);
        static int  GetDirectoryId(const QString &directory, int parentid);
        static bool HasFileChanged(const QString &filename, const QString &date_modified);
        void AddFileToDB(const QString &filename, const QString &startDir);
        void RemoveFileFromDB (const QString &filename, const QString &startDir);
        void UpdateFileInDB(const QString &filename, const QString &startDir);
        void ScanMusic(MusicLoadedMap &music_files);
        void ScanArtwork(MusicLoadedMap &music_files);
        static void cleanDB();
        static bool IsArtFile(const QString &filename);
        static bool IsMusicFile(const QString &filename);

        static void updateLastRunEnd(void);
        static void updateLastRunStart(void);
        static void updateLastRunStatus(QString &status);

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

        bool m_forceupdate       {false};
};

#endif // MUSICFILESCANNER_H
