// Qt headers
#include <QFile>

// MythTV headers
#include "mythcontext.h"
#include "storagegroup.h"

#include "galleryviewhelper.h"



/** \fn     GalleryViewHelper::GalleryViewHelper(MythScreenType *)
 *  \brief  Constructor
 *  \param  parent The screen parent
 */
GalleryViewHelper::GalleryViewHelper(MythScreenType *parent)
{
    m_parent = parent;

    m_sgName    = gCoreContext->GetSetting("GalleryStorageGroupName");
    m_sgDirList = StorageGroup::getGroupDirs(m_sgName, "");

    m_dbHelper = new GalleryDatabaseHelper();
    m_thumbGenThread = new GalleryThumbGenThread();
    m_fileHelper  = new GalleryFileHelper();

    connect(m_thumbGenThread,  SIGNAL(ThumbnailCreated(ImageMetadata *, int)),
            m_parent, SLOT(UpdateThumbnail(ImageMetadata *, int)));

    connect(m_thumbGenThread,  SIGNAL(UpdateThumbnailProgress(int, int)),
            m_parent, SLOT(UpdateThumbnailProgress(int, int)));

    connect(m_thumbGenThread,  SIGNAL(finished()),
            m_parent, SLOT(ResetThumbnailProgress()));

    // these are the node trees that hold the data and
    // are used for navigating and finding files
    m_currentNode = new MythGenericTree("", kBaseDirectory, false);

    m_allNodesVisible = false;
}



/** \fn     GalleryViewHelper::~GalleryViewHelper()
 *  \brief  Destructor
 */
GalleryViewHelper::~GalleryViewHelper()
{
    if (m_dbHelper)
    {
        delete m_dbHelper;
        m_dbHelper = NULL;
    }

    if (m_currentNode)
    {
        delete m_currentNode;
        m_currentNode = NULL;
    }

    if (m_thumbGenThread)
    {
        m_thumbGenThread->cancel();
        delete m_thumbGenThread;
        m_thumbGenThread = NULL;
    }

    if (m_fileHelper)
    {
        delete m_fileHelper;
        m_fileHelper = NULL;
    }
}



/** \fn     GalleryViewHelper::LoadData()
 *  \brief  Checks the prerequisites are fulfilled
 *          before the data can be loaded
 *  \return void
 */
int GalleryViewHelper::LoadData()
{
    // clear all previously loaded data
    m_currentNode->deleteAllChildren();

    // get the base directories and check if they are valid
    if (m_sgDirList.isEmpty())
        return kStatusNoBaseDir;

    // Fill the MythGenericTree with the data of the first level
    // This method will be called recursively to further fill the tree
    LoadTreeData();

    // check if there is any data in the database
    if (m_currentNode->childCount() == 0)
        return kStatusNoFiles;

    return kStatusOk;
}


/** \fn     GalleryViewHelper::LoadTreeData()
 *  \brief  Load all available data from the database and populates the tree.
 *  \return void
 */
void GalleryViewHelper::LoadTreeData()
{
    QList<ImageMetadata *> *dirList = new QList<ImageMetadata *>;
    QList<ImageMetadata *> *fileList = new QList<ImageMetadata *>;

    // Stop generating thumbnails
    // when a new directory is loaded
    m_thumbGenThread->cancel();

    // The parent id is the database index of the
    // directories which subdirectories and files shall be loaded
    int id = 0;

    // Get the selected node. If there is no data available then the
    // plugin has been started for the first time, a synchronization
    // request was made or the settings have changed. In this case
    // use default parent id of the storage group directories.
    ImageMetadata *im = GetImageMetadataFromSelectedNode();
    if (im)
    {
        if (im->m_type == kUpDirectory)
            id = im->m_parentId;

        if (im->m_type == kSubDirectory)
            id = im->m_id;
    }

    // The data from the selected node has used.
    // Clear the list so that it can be populated with new data.
    m_currentNode->deleteAllChildren();

    // If the parentId is not one of the directories in the storage group
    // then add a additional directory at the beginning of the list that
    // is of the type kUpDirectory so that the user can navigate one level up.
    if (!m_dbHelper->GetStorageDirIDs().contains(id))
    {
        m_dbHelper->LoadParentDirectory(dirList, id);
        LoadTreeNodeData(dirList, m_currentNode);
    }

    m_dbHelper->LoadDirectories(dirList, id);
    LoadTreeNodeData(dirList, m_currentNode);

    // Load all files with the specified sorting criterias
    m_dbHelper->LoadFiles(fileList, id);
    LoadTreeNodeData(fileList, m_currentNode);

    // Start generating thumbnails if required
    m_thumbGenThread->start();

    // clean up
    delete dirList;
    delete fileList;
}



/** \fn     GalleryViewHelper::LoadNodeTreeData(QList<ImageMetadata *> *, MythGenericTree *)
 *  \brief  Creates a new generic tree with the information from the database
 *  \param  list The list with the database file information
 *  \param  tree The tree that will be populated and shown
 *  \return void
 */
void GalleryViewHelper::LoadTreeNodeData(QList<ImageMetadata *> *list,
                                       MythGenericTree *tree)
{
    // Add all items in the list to the tree
    for (int i = 0; i < list->size(); ++i)
    {
        ImageMetadata *im = list->at(i);
        if (im)
        {
            m_thumbGenThread->AddToThumbnailList(im);

            // Create a new tree node that will hold the data
            MythGenericTree *treeItem =
                    new MythGenericTree(im->m_fileName,
                                        im->m_type, true);

            treeItem->SetData(qVariantFromValue<ImageMetadata *> (im));
            tree->addNode(treeItem);
        }
    }
}



/** \fn     GalleryViewHelper::RenameCurrentNode(QString &)
 *  \brief  Renames the file that belongs to the node and updates the database
 *  \param  New name of the file with the full path
 *  \return void
 */
void GalleryViewHelper::RenameCurrentNode(QString &newName)
{
    bool fileExist = false;

    // Check if the file with the new name already exists in the current directory
    QList<MythGenericTree *> *nodeTree = m_currentNode->getAllChildren();
    for (int i = 0; i < nodeTree->size(); i++)
    {
        ImageMetadata *im = nodeTree->at(i)->GetData().value<ImageMetadata *>();
        if (im)
        {
            if (im->m_name.compare(newName) == 0)
            {
                fileExist = true;
                break;
            }
        }
    }

    // The file with the given new name does not yet exist.
    // Continue with the renaming of the file
    if (!fileExist)
    {
        ImageMetadata *im = GetImageMetadataFromSelectedNode();
        if (!im)
            return;

        if (m_fileHelper->RenameFile(im, newName))
        {
            // replace the original filename with the
            // new one in the pull path + filename variable
            QString newFileName = im->m_fileName.replace(im->m_name, newName);

            im->m_fileName = newFileName;
            im->m_name = newName;

            m_dbHelper->UpdateData(im);
        }
    }
}



/** \fn     GalleryViewHelper::DeleteCurrentNode()
 *  \brief  Deletes the current node from the generic tree
 *  \return void
 */
void GalleryViewHelper::DeleteCurrentNode()
{
    ImageMetadata *im = GetImageMetadataFromSelectedNode();
    if (!im)
        return;

    // Delete the file and remove the database entry
    if (m_fileHelper->RemoveFile(im))
    {
        // Remove the entry from the node list
        m_currentNode->deleteNode(m_currentNode->getSelectedChild());

        // Only remove the first thumbnail when it is an image or video.
        // If its a folder, it will use thumbnails from other images
        if (im->m_type == kImageFile || im->m_type == kVideoFile)
            QFile::remove(im->m_thumbFileNameList->at(0));

        m_dbHelper->RemoveFile(im);
    }
}



/** \fn     GalleryViewHelper::DeleteSelectedNodes()
 *  \brief  Deletes multiple selected nodes from the generic tree
 *  \return void
 */
void GalleryViewHelper::DeleteSelectedNodes()
{
    QList<MythGenericTree *> *nodeTree = m_currentNode->getAllChildren();
    for (int i = 0; i < nodeTree->size(); i++)
    {
        ImageMetadata *im = nodeTree->at(i)->GetData().value<ImageMetadata *>();
        if (im && im->m_selected)
        {
            // Delete the file and remove the
            // database entry only if it was successful
            if (m_fileHelper->RemoveFile(im))
            {
                // Only remove the first thumbnail when it is an image or video.
                // If its a folder, it will use thumbnails from other images
                if (im->m_type == kImageFile || im->m_type == kVideoFile)
                    QFile::remove(im->m_thumbFileNameList->at(0));

                m_dbHelper->RemoveFile(im);

                // Remove the entry from the node list
                m_currentNode->deleteNode(nodeTree->at(i));
            }
        }
    }
}



/** \fn     GalleryViewHelper::SetNodeSelectionState(int, bool)
 *  \brief  Sets either one or all nodes to the
            nodeState that the user has specified
 *  \param  nodeState Can be either selected or unselected
 *  \param  allNodes Set the selection state for all nodes or not
 *  \return void
 */
void GalleryViewHelper::SetNodeSelectionState(int nodeState, bool allNodes)
{
    if (!m_currentNode)
        return;

    if (!allNodes)
    {
        SetNodeSelectionState(m_currentNode->getSelectedChild(), nodeState);
    }
    else
    {
        QList<MythGenericTree *> *nodeTree = m_currentNode->getAllChildren();
        for (int i = 0; i < nodeTree->size(); i++)
            SetNodeSelectionState(nodeTree->at(i), nodeState);
    }
}



/** \fn     GalleryViewHelper::SetNodeSelectionState(MythGenericTree *, int)
 *  \brief  Sets the given node to the given nodeState
 *  \param  node The single node that shall be changed
 *  \param  nodeState Can be either selected or unselected
 *  \return void
 */
void GalleryViewHelper::SetNodeSelectionState(MythGenericTree *node,
                                            int nodeState)
{
    // set the given node as selected / unselected
    if (node)
    {
        ImageMetadata *im = node->GetData().value<ImageMetadata *>();

        // Allow a selection / deselection only for images
        if (im &&
            im->m_type == kImageFile)
        {
            if (nodeState == kNodeStateSelect)
                im->m_selected = true;

            if (nodeState == kNodeStateDeselect)
                im->m_selected = false;

            if (nodeState == kNodeStateInvert)
                im->m_selected = !im->m_selected;
        }
    }
}



/** \fn     GalleryViewHelper::SetNodeVisibilityState(int)
 *  \brief  Sets the selected not either to the
            nodeState that the user has specified
 *  \param  nodeState Can be either visible or invisible
 *  \return void
 */
void GalleryViewHelper::SetNodeVisibilityState(int nodeState)
{
    // set the given node as visible / invisible
    ImageMetadata *im = GetImageMetadataFromSelectedNode();
    if (im)
    {
        if (nodeState == kNodeStateVisible)
            im->m_isHidden = false;

        if (nodeState == kNodeStateInvisible)
            im->m_isHidden = true;

        m_dbHelper->UpdateData(im);
    }
}



/** \fn     GalleryViewHelper::SetFileOrientation(int)
 *  \brief  Saves the orientation information of the selected node
 *  \param  fileOrientation The orientation value 1-8
 *  \return void
 */
void GalleryViewHelper::SetFileOrientation(int fileOrientation)
{
    ImageMetadata *im = GetImageMetadataFromSelectedNode();
    if (!im)
        return;

    int oldFileOrientation = im->GetOrientation();

    // Update the orientation, the new value will
    // be calculated with this method. This new
    // value will then be saved in the exif header tag.
    im->SetOrientation(fileOrientation, false);

    // Update the exif tag, if that fails we can restore the original
    // orientation so that the database and image file are not out of sync
    if (m_fileHelper->SetImageOrientation(im))
    {
        m_dbHelper->UpdateData(im);
        m_thumbGenThread->RecreateThumbnail(im);
        m_thumbGenThread->start();
    }
    else
    {
        im->SetOrientation(oldFileOrientation, false);

        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not write the angle %1 into the file %2. "
                    "The database value has not been updated.")
            .arg(im->GetAngle()).arg(im->m_fileName));
    }
}



/** \fn     GalleryViewHelper::SetFileZoom(int)
 *  \brief  Saves the zoom information of the selected node
 *  \param  zoom The zoom value in percent
 *  \return void
 */
void GalleryViewHelper::SetFileZoom(int zoom)
{
    ImageMetadata *im = GetImageMetadataFromSelectedNode();
    if (!im)
        return;

    if (zoom == kFileZoomIn)
        im->SetZoom(20);

    if (zoom == kFileZoomOut)
        im->SetZoom(-20);

    m_dbHelper->UpdateData(im);
}



/** \fn     GalleryViewHelper::GetImageMetadataFromSelectedNode()
 *  \brief  Returns the data selected node
 *  \return ImageMetadata
 */
ImageMetadata *GalleryViewHelper::GetImageMetadataFromSelectedNode()
{
    if (!m_currentNode)
        return NULL;

    MythGenericTree *node = m_currentNode->getSelectedChild();
    if (!node)
        return NULL;

    ImageMetadata *im = node->GetData().value<ImageMetadata *>();
    if (!im)
        return NULL;

    return im;
}



/** \fn     GalleryViewHelper::GetImageMetadataFromNode(MythGenericTree *)
 *  \brief  Returns the data of the given node
 *  \param  node The specified node that shall be used
 *  \return ImageMetadata
 */
ImageMetadata *GalleryViewHelper::GetImageMetadataFromNode(MythGenericTree *node)
{
    if (!node)
        return NULL;

    ImageMetadata *im = node->GetData().value<ImageMetadata *>();
    if (!im)
        return NULL;

    return im;
}



/** \fn     GalleryViewHelper::GetImageMetadataFromNode(int)
 *  \brief  Returns the data of the given node id
 *  \param  id The specified node id that shall be used
 *  \return ImageMetadata
 */
ImageMetadata *GalleryViewHelper::GetImageMetadataFromNode(int i)
{
    if (!m_currentNode)
        return NULL;

    MythGenericTree *node = m_currentNode->getChildAt(i);
    if (!node)
        return NULL;

    ImageMetadata *im = node->GetData().value<ImageMetadata *>();
    if (!im)
        return NULL;

    return im;
}



/** \fn     GalleryViewHelper::SetPreviewImageSize(MythUIButtonList *)
 *  \brief  Gets and saves the size of the shown image previews which
 *          will be used to create thumbnails of the given dimensions.
 *  \param  imageList The widget where the themer has specified the images sizes
 *  \return void
 */
void GalleryViewHelper::SetPreviewImageSize(MythUIButtonList *imageList)
{
    float width  = (float)imageList->GetArea().width();
    float height = width / ((float)imageList->ItemWidth() /
                            (float)imageList->ItemHeight());

    m_thumbGenThread->SetThumbnailSize((int)width, (int)height);
}



/** \fn     GallerySyncStatusThread::GallerySyncStatusThread()
 *  \brief  Constructor
 *  \return void
 */
GallerySyncStatusThread::GallerySyncStatusThread()
{

}

/**
 * @brief GallerySyncStatusThread::isSyncRunning
 * @return void
 */
bool GallerySyncStatusThread::isSyncRunning()
{
    GalleryFileHelper *fh = new GalleryFileHelper();
    GallerySyncStatus status = fh->GetSyncStatus();

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("GallerySyncStatusThread: Sync status is running: %1").arg(status.running));

    delete fh;

    return status.running;
}

/** \fn     GallerySyncStatusThread::run()
 *  \brief  Called when the thread is started. Calls the service
            api to start the syncing and checks the status every 2s.
 *  \return void
 */
void GallerySyncStatusThread::run()
{
    volatile bool exit = false;
    GalleryFileHelper *fh = new GalleryFileHelper();

    // Internal counter that tracks how many
    // times we have been in the while loop
    int loopCounter = 0;

    while (!exit)
    {
        GallerySyncStatus status = fh->GetSyncStatus();

        LOG(VB_GENERAL, LOG_DEBUG,
            QString("GallerySyncStatusThread: Sync status is running: %1, Syncing image '%2' of '%3'")
            .arg(status.running).arg(status.current).arg(status.total));

        // Only update the progress text
        // if the sync is still running
        if (status.running)
            emit UpdateSyncProgress(status.current, status.total);

        // Try at least one time to get the sync
        // status before checking for the exit condition
        if (loopCounter >= 1)
        {
            if (status.running == false)
                exit = true;
        }

        // Wait some time before trying to get and update the status
        // This also avoids too many calls to the service api.
        usleep(1000000);

        ++loopCounter;
    }

    delete fh;
}
