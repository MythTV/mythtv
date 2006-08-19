#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <qmap.h>
#include <qstringlist.h> // optional

enum VideoFileLocation
{
    kFileSystem,
    kDatabase,
    kBoth
};

class MetadataListManager;

class VideoScanner
{
    public:
        VideoScanner();
        ~VideoScanner();

        void doScan(const QStringList &dirs);

    private:
        typedef QMap <QString, VideoFileLocation> VideoLoadedMap;

    private:
        bool m_ListUnknown;
        bool m_RemoveAll;
        bool m_KeepAll;
        VideoLoadedMap m_VideoFiles;

        MetadataListManager *m_dbmetadata;

    private:
        void promptForRemoval(const QString& filename);
        void verifyFiles();
        void updateDB();
        void buildFileList(const QString &directory,
                           const QStringList &imageExtensions);
};

#endif
