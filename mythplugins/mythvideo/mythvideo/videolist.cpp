#include "videolist.h"

#include <qdir.h>

#include <mythtv/mythdbcon.h>
#include <mythtv/mythmedia.h>
#include <mythtv/mythmediamonitor.h>

VideoList::VideoList(const QString& _prefix)
{
    currentVideoFilter = new VideoFilterSettings(true, _prefix);
    video_tree_root = NULL;
    nitems = 0;

    m_ListUnknown = gContext->GetNumSetting("VideoListUnknownFileTypes", 1);

    m_LoadMetaData = gContext->GetNumSetting("VideoTreeLoadMetaData", 0);

    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery("SELECT extension,f_ignore FROM videotypes;");
    query.exec(thequery);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString ext = query.value(0).toString().lower();
            bool ignore = query.value(1).toBool();
            m_IgnoreList.insert(ext, ignore);
        }
    }

}

VideoList::~VideoList()
{
    delete video_tree_root;
    delete currentVideoFilter;
}

Metadata *VideoList::getVideoListMetadata(int index)
{
    if (index < 0)
        return NULL;    // Special node types

    if ((unsigned int)index < count())
        return(&metas[index]);

    cerr << __FILE__ << ": getVideoListMetadata: index out of bounds: "
        << index << endl;
    return NULL;
}

void VideoList::wantVideoListUpdirs(bool yes)
{
    if (yes) {
         if (! has_updirs)
             addUpnodes(video_tree_root);
    }
    else {
         if (has_updirs)
             removeUpnodes(video_tree_root);
    }
}

bool VideoList::ignoreExtension(const QString &extension) const
{
    QString ext = extension.lower();
    QMap<QString,bool>::const_iterator it = m_IgnoreList.find(ext);
    if (it == m_IgnoreList.end())
        return !m_ListUnknown;
    return it.data();
}


//
// Build a generic tree containing the video files. You can control the
// contents and the shape of the tree in de following ways:
//   filebrowser:
//      If true, the actual state of the filesystem is examined. If a video
//      is already known to the system, this info is retrieved. If not, some
//      basic info is provided.
//      If false, only video information already present in the database is
//      presented.
//   flatlist:
//      If true, the tree is reduced to a single level containing all the
//      videos found.
//      If false, the hierarchy present on the filesystem or in the database
//      is preserved. In this mode, both sub-dirs and updirs are present.

GenericTree *VideoList::buildVideoList(bool filebrowser, bool flatlist,
    int parental_level)
{
    // Clear out the old stuff
    browser_mode_files.clear();
    metas.clear();
    delete video_tree_root;

    video_tree_root = new GenericTree("video root", ROOT_NODE, false);

    if (filebrowser)
        buildFsysList(flatlist, parental_level);
    else
        buildDbList(flatlist, parental_level);

    //
    // Did some 'real' files get added?
    //
    if (nitems == 0) {
        delete video_tree_root;
        metas.clear();
        video_tree_root = new GenericTree("video root", ROOT_NODE, false);
        addDirNode(video_tree_root, QObject::tr("No files found"));
    }

    //
    // We build the tree with updirs. The idea is that it is cheaper to
    // delete afterward than to create them...
    //
    has_updirs = true;

    return(video_tree_root);
}



void VideoList::buildDbList(bool flatlist, int parental_level)
{
    //
    //  This just asks the database for a list
    //  of metadata, builds it into a tree and
    //  passes that tree structure to the GUI
    //  widget that handles navigation
    //

    QString thequery = QString("SELECT intid FROM %1 %2 %3")
                    .arg(currentVideoFilter->BuildClauseFrom())
                    .arg(currentVideoFilter->BuildClauseWhere(parental_level))
                    .arg(currentVideoFilter->BuildClauseOrderBy());
        
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);
    unsigned int numrows;

    if (!query.isActive())
    {
        cerr << __FILE__ << ": Your database sucks" << endl;
        return;
    }

    if ((numrows = query.numRowsAffected()) <= 0)
        return;

    QString prefix = gContext->GetSetting("VideoStartupDir");
    if (prefix.length() < 1)
    {
        cerr << __FILE__ << ": Seems unlikely that this is going to work" <<
            endl;
    }

    if (!flatlist)
        (void)addDirNode(video_tree_root, "videos");

    //
    //  Accumulate query results into the metaptrs list.
    //
    QPtrList<Metadata> metaptrs;
    while (query.next())
    {
        unsigned int intid = query.value(0).toUInt();
        Metadata *myData = new Metadata();
        myData->setID(intid);
        myData->fillDataFromID();
        metaptrs.append(myData);
    }

    //
    //  If sorting by title, re-sort by hand because the SQL sort doesn't
    //  ignore articles when sorting. This assumes all titles are in English.
    //  This means a movie with a foreign-language title like "A la carte" will
    //  be incorrectly alphabetized, because the "A" is actually significant.
    //  The set of ignored prefixes should be a database option, or each
    //  video's metadata should include a separate sort key.
    //
    if (currentVideoFilter->getOrderby() == VideoFilterSettings::kOrderByTitle)
    {
        //
        //  Pull things off the metaptrs list and put them into the
        //  "stringsort" qmap (which will automatically sort them by qmap key).
        //
        QMap<QString, Metadata*> stringsort;
        QRegExp prefixes = QObject::tr("^(The |A |An )");
        while (!metaptrs.isEmpty())
        {
            Metadata *myData = metaptrs.take(0);
            QString sTitle = myData->Title();
            sTitle.remove(prefixes);
            stringsort[sTitle] = myData;
        }

        //
        //  Now walk through the "stringsort" qmap and put them back on the
        //  list (in stringsort order).
        //
        for (QMap<QString, Metadata*>::iterator it = stringsort.begin();
                it != stringsort.end();
                it++)
        {
            metaptrs.append(*it);
        }
    }

    //
    //  Build list of videos.
    //
    for (unsigned int count = 0; !metaptrs.isEmpty(); count++)
    {
        Metadata *myData = metaptrs.take(0);
        GenericTree *where_to_add = video_tree_root->getChildAt(0);
        QString file_string = myData->Filename();
        file_string.remove(0, prefix.length());
        if (flatlist) 
        {
            video_tree_root->addNode(file_string, count, true);
            nitems++;
        }
        else 
        {
            where_to_add = addFileNode(where_to_add, file_string, count);
        }
        metas.append(*myData);
        delete myData;
    }
}

void VideoList::buildFsysList(bool flatlist, int parental_level)
{
    //
    //  Fill metadata from directory structure
    //

    QStringList nodesname;
    QStringList nodespath;

    QStringList dirs = QStringList::split(":",
                                        gContext->GetSetting("VideoStartupDir",
                                        "/share/Movies/dvd"));
    if (dirs.size() > 1 )
    {
        QStringList::iterator iter;

        for (iter = dirs.begin(); iter != dirs.end(); iter++)
        {
            int slashLoc = 0;
            QString pathname;

            nodespath.append( *iter );
            pathname = *iter;
            slashLoc = pathname.findRev("/", -2 ) + 1;

            if (pathname.right(1) == "/")
                    nodesname.append(pathname.mid(slashLoc, pathname.length()
                                                        - slashLoc - 2));
            else
                    nodesname.append(pathname.mid(slashLoc));
        }
    }
    else
    {
        nodespath.append( dirs[0] );
        nodesname.append(QObject::tr("videos"));
    }

    //
    // See if there are removable media available, so we can add them
    // to the tree.
    //
    MediaMonitor * mon = MediaMonitor::getMediaMonitor();
    if (mon)
    {
        QValueList <MythMediaDevice*> medias =
                                        mon->getMedias(MEDIATYPE_DATA);
        QValueList <MythMediaDevice*>::Iterator itr = medias.begin();
        MythMediaDevice *pDev;

        while(itr != medias.end())
        {
            pDev = *itr;
            if (pDev)
            {
                QString path = pDev->getMountPath();
                QString name = path.right(path.length()
                                                - path.findRev("/")-1);
                nodespath.append(path);
                nodesname.append(name);
            }
            itr++;
        }
    }


    //
    // Add all root-nodes to the tree.
    //
    for (uint j=0; j < nodesname.count(); j++)
    {
        if (!flatlist)
                (void)addDirNode(video_tree_root, nodesname[j]);
        buildFileList(nodespath[j]);
    }

    unsigned int mainnodeindex = 0;
    QString prefix = nodespath[mainnodeindex];
    GenericTree *where_to_add = video_tree_root->getChildAt(mainnodeindex);

    for(uint i=0; i < browser_mode_files.count(); i++)
    {
        QString file_string = *(browser_mode_files.at(i));
        if (prefix.compare(file_string.left(prefix.length())) != 0)
        {
            if (mainnodeindex++ < nodespath.count()) {
                prefix = nodespath[mainnodeindex];
            }
            else {
                cerr << __FILE__ << ": mainnodeindex out of bounds" << endl;
                break;
            }
        }
        where_to_add = video_tree_root->getChildAt(mainnodeindex);
        if(prefix.length() < 1)
        {
            cerr << __FILE__ << ": weird prefix.lenght" << endl;
            break;
        }

        Metadata *myData = new Metadata;
        // See if we can find this filename in DB
        myData->setFilename(file_string);
        if (m_LoadMetaData) {
            if(!myData->fillDataFromFilename()) {
                // No, fake it.
                QString base_name = file_string.section("/", -1);
                myData->setTitle(base_name.section(".", 0, -2));
            }
        }
        else
        {
            QString base_name = file_string.section("/", -1);
            myData->setTitle(base_name.section(".", 0, -2));

        }

        metas.append(*myData); 

        if (myData->ShowLevel() <= parental_level && myData->ShowLevel() != 0)
        {
            file_string.remove(0, prefix.length());
            if (!flatlist) {
                where_to_add = addFileNode(where_to_add, file_string, i);
            }
            else {
                video_tree_root->addNode(file_string, i, true);
                nitems++;
            }
        }
        delete myData;
    }
}

//
// Remove UP_FOLDER nodes from the tree
//
void VideoList::removeUpnodes(GenericTree *parent)
{
    QPtrListIterator<GenericTree> it = parent->getFirstChildIterator();

    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        ++it;
        if (child->getInt() == UP_FOLDER)
            parent->removeNode(child);
        else if (child->getInt() == SUB_FOLDER)
                removeUpnodes(child);
    }
}

//
// Add UP_FOLDER nodes to the tree
//
void VideoList::addUpnodes(GenericTree *parent)
{
    QPtrListIterator<GenericTree> it = parent->getFirstChildIterator();

    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        ++it;
        if (child->getInt() == SUB_FOLDER) {
                child->addNode(parent->getString(),  UP_FOLDER, true);
                addUpnodes(child);
        }
    }
}

GenericTree *VideoList::addDirNode(GenericTree *where_to_add,
                                                        const QString& dname)
{
    GenericTree *sub_node, *up_node;

    // Add the subdir node...
    sub_node = where_to_add->addNode(dname, SUB_FOLDER, false);
    sub_node->setAttribute(0, ORDER_SUB);
    sub_node->setOrderingIndex(0);

    // ...and the updir node.
    up_node = sub_node->addNode(where_to_add->getString(), UP_FOLDER, true);
    up_node->setAttribute(0, ORDER_UP);
    up_node->setOrderingIndex(0);
    return(sub_node);
}

GenericTree *VideoList::addFileNode(GenericTree *where_to_add,
                                                const QString& fname, int index)
{
    int a_counter = 0;
    GenericTree *sub_node;

    QStringList list(QStringList::split("/", fname));
    QStringList::Iterator an_it = list.begin();
    for( ; an_it != list.end(); ++an_it)
    {
        if(a_counter + 1 >= (int) list.count()) // video
        {
            QString title = (*an_it);
            sub_node = where_to_add->addNode(title.section(".",0,-2),
                    index, true);
            sub_node->setAttribute(0, ORDER_ITEM);
            sub_node->setOrderingIndex(0);
            nitems++;
        }
        else
        {
            QString dirname = *an_it + "/";
            sub_node = where_to_add->getChildByName(dirname);
            if(!sub_node)
                where_to_add = addDirNode(where_to_add, dirname);
            else where_to_add = sub_node;
        }
        ++a_counter;
    }
    return(where_to_add);
}

void VideoList::buildFileList(const QString& directory)
{
    QDir d(directory);

    d.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;
    QRegExp r;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || 
            fi->fileName() == ".." ||
            fi->fileName() == "Thumbs.db")
        {
            continue;
        }
        
        if (!fi->isDir() && ignoreExtension(fi->extension(false)))
                continue;
        
        QString filename = fi->absFilePath();
        if (fi->isDir())
            buildFileList(filename);
        else browser_mode_files.append(filename);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
