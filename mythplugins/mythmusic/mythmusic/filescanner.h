#ifndef _FILESCANNER_H_
#define _FILESCANNER_H_

class Metadata;
class Decoder;

enum MusicFileLocation
{
    kFileSystem,
    kDatabase,
    kNeedUpdate,
    kBoth
};

typedef QMap <QString, MusicFileLocation> MusicLoadedMap;

class FileScanner
{
    public:
        FileScanner ();
        ~FileScanner ();

        void SearchDir(QString &directory);

    private:
        void BuildFileList(QString &directory, MusicLoadedMap &music_files, int parentid);
        bool HasFileChanged(const QString &filename, const QString &date_modified);
        void AddFileToDB(const QString &filename);
        void RemoveFileFromDB (const QString &directory, const QString &filename);
        void UpdateFileInDB(const QString &filename);

        QString         m_startdir;
};

#endif // _FILESCANNER_H_
