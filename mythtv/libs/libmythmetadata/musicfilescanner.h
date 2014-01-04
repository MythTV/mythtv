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

    private:
        void BuildFileList(QString &directory, MusicLoadedMap &music_files, int parentid);
        int  GetDirectoryId(const QString &directory, const int &parentid);
        bool HasFileChanged(const QString &filename, const QString &date_modified);
        void AddFileToDB(const QString &filename);
        void RemoveFileFromDB (const QString &filename);
        void UpdateFileInDB(const QString &filename);
        void ScanMusic(MusicLoadedMap &music_files);
        void ScanArtwork(MusicLoadedMap &music_files);
        void cleanDB();

        QString  m_startdir;
        IdCache  m_directoryid;
        IdCache  m_artistid;
        IdCache  m_genreid;
        IdCache  m_albumid;
};

#endif // _MUSICFILESCANNER_H_
