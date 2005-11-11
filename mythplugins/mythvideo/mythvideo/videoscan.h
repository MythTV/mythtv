#ifndef VIDEO_SCANNER_H
#define  VIDEO_SCANNER_H
#include <qobject.h>
#include <qmap.h>

enum VideoFileLocation
{
    kFileSystem,
    kDatabase,
    kBoth
};



typedef QMap <QString, VideoFileLocation> VideoLoadedMap;

class VideoScanner
{
    public:
        VideoScanner();
        void doScan(const QString& dirs);
    
    private:
        bool m_ListUnknown;
        bool m_RemoveAll;
        bool m_KeepAll;
        VideoLoadedMap m_VideoFiles;
        QStringList m_IgnoreList;

        void promptForRemoval(const QString& filename);
        bool ignoreExtension(const QString& extension) const;
        void verifyFiles();
        void updateDB();
        void buildFileList(const QString &directory, 
                           const QStringList &imageExtensions);
};

#endif
