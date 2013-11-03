#ifndef GALLERYVIEWHELPER_H
#define GALLERYVIEWHELPER_H

// Qt headers
#include <QString>
#include <QList>
#include <QMap>
#include <QThread>

// MythTV headers
#include "mythgenerictree.h"
#include "imagemetadata.h"

#include "galleryfilehelper.h"
#include "gallerydatabasehelper.h"


class GalleryViewHelper : public QObject
{
    Q_OBJECT

public:
    GalleryViewHelper(MythScreenType *);
    ~GalleryViewHelper();

    MythGenericTree         *m_currentNode;

    QStringList m_sgDirList;
    QString     m_sgName;

    int     LoadData();
    void    LoadTreeData();
    void    LoadTreeNodeData(QList<ImageMetadata *>*, MythGenericTree*);
    void    UpdateAllData();
    void    RenameCurrentNode(QString &);
    void    DeleteCurrentNode();
    void    DeleteSelectedNodes();
    void    SetNodeSelectionState(int, bool);
    void    SetNodeVisibilityState(int);
    void    SetFileOrientation(int);
    void    SetFileZoom(int);
    void    SetPreviewImageSize(MythUIButtonList *);

    ImageMetadata*  GetImageMetadataFromSelectedNode();
    ImageMetadata*  GetImageMetadataFromNode(MythGenericTree *);
    ImageMetadata*  GetImageMetadataFromNode(int);

    GalleryFileHelper       *m_fileHelper;

private:
    void    SetNodeSelectionState(MythGenericTree *, int);

    MythScreenType          *m_parent;
    GalleryDatabaseHelper   *m_dbHelper;

    bool    m_allNodesVisible;
};



class GallerySyncStatusThread : public QThread
{
    Q_OBJECT

public:
    GallerySyncStatusThread();
    bool isSyncRunning();

protected:
    void run();

signals:
    void UpdateSyncProgress(int, int);
};

#endif // GALLERYVIEWHANDLER_H
