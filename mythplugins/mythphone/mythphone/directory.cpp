/*
	directory.cpp

	(c) 2004 Paul Volkaerts
	
  Directory class holding a simple contact database plus call history.
*/
#include <iostream>
using namespace std;
#include "directory.h"
#include "qdatetime.h"
#include "qdir.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>


static int counter = 0;

///////////////////////////////////////////////////////
//                  DirEntry Class
///////////////////////////////////////////////////////

DirEntry::DirEntry(QString nn, QString uri, QString fn, QString sn, QString ph, bool ohl)
{
    NickName = nn;
    FirstName = fn;
    Surname = sn;
    Uri = uri;
    PhotoFile = ph;
    id = counter++;
    SpeedDial = false;
    inDatabase = false;
    changed = true;
    onHomeLan = ohl;
    dbId = -1;
}


DirEntry::DirEntry(DirEntry *Original)
{
    NickName = Original->NickName;
    FirstName = Original->FirstName;
    Surname = Original->Surname;
    Uri = Original->Uri;
    PhotoFile = Original->PhotoFile;
    onHomeLan = Original->onHomeLan;
    id = counter++;
    inDatabase = false;
    changed = true;
    dbId = -1;
    TreeNode = 0;
    SpeeddialNode = 0;
}


DirEntry::~DirEntry()
{
}


bool DirEntry::urlMatches(QString s)
{
    // Check whether the URI passed matches the directory
    // They do not need to be identical; they just need to
    // refer to the same person
    return (Uri == s) ? true : false;
}



int getAlphaSortId(QString s)
{
    int v=0;
    s = s.lower();
    int len = s.length();

    // This just loads the first 4 bytes of a string into an int such that
    // AAA has a lower value than ZZZ
    v |= ((len>0) ? (s.at(0).unicode() << 24) : 0);
    v |= ((len>1) ? (s.at(1).unicode() << 16) : 0);
    v |= ((len>2) ? (s.at(2).unicode() << 8) : 0);
    v |= ((len>3) ? (s.at(3).unicode() << 0) : 0);
     
    return v;
}

void DirEntry::writeTree(GenericTree *tree_to_write_to, GenericTree *sdTree)
{
    GenericTree *sub_node;

    if (tree_to_write_to)
    {
        sub_node = tree_to_write_to->addNode(NickName, 0, true);
        sub_node->setAttribute(0, TA_DIRENTRY);
        sub_node->setAttribute(1, id);
        sub_node->setAttribute(2, getAlphaSortId(NickName));
        TreeNode = sub_node;
    }

    if ((SpeedDial) && (sdTree != 0))
    {
        // Default "selectable" to FALSE on speed-dials as it gets changed to true based on presence status
        sub_node = sdTree->addNode(NickName, 0, false); 
        sub_node->setAttribute(0, TA_SPEEDDIALENTRY);
        sub_node->setAttribute(1, id);
        sub_node->setAttribute(2, getAlphaSortId(NickName));
        sub_node->setAttribute(3, ICON_PRES_UNKNOWN);
        SpeeddialNode = sub_node;
    }
}


void DirEntry::updateYourselfInDB(QString Dir)
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    if (!inDatabase)
    {
        thequery = QString("INSERT INTO phonedirectory (nickname,firstname,surname,"
                               "url,directory,photofile,speeddial,onhomelan) VALUES "
                               "(\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",%7,%8);")
                              .arg(NickName.latin1()).arg(FirstName.latin1())
                              .arg(Surname.latin1()).arg(Uri.latin1())
                              .arg(Dir.latin1()).arg(PhotoFile.latin1())
                              .arg(SpeedDial).arg(onHomeLan);
        query.exec(thequery);

        thequery = QString("SELECT MAX(intid) FROM phonedirectory ;");
        query.exec(thequery);

        if ((query.isActive()) && (query.size() == 1))
        {
            query.next();
            dbId = query.value(0).toUInt();
            inDatabase = true;
            changed = false;
        }
        else
            cerr << "Mythphone: Something is up with the database\n";
    }
    else if (changed)
    {
        thequery = QString("UPDATE phonedirectory "
                           "SET nickname=\"%1\", "
                               "firstname=\"%2\", "
                               "surname=\"%3\", "
                               "directory=\"%4\", "
                               "url=\"%5\", "
                               "photofile=\"%6\", "
                               "speeddial=%7, "
                               "onhomelan=%8 "
                           "WHERE intid=%9 ;")
                           .arg(NickName.latin1()).arg(FirstName.latin1())
                           .arg(Surname.latin1()).arg(Dir.latin1()).arg(Uri.latin1())
                           .arg(PhotoFile.latin1()).arg(SpeedDial).arg(onHomeLan)
                           .arg(dbId);
        query.exec(thequery);
        changed = false;
    }
}


void DirEntry::deleteYourselfFromDB()
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    if (inDatabase)
    {
        thequery = QString("DELETE FROM phonedirectory "
                           "WHERE intid=%1 ;").arg(dbId);
        query.exec(thequery);
    }
}




///////////////////////////////////////////////////////
//                Directory Class
///////////////////////////////////////////////////////

Directory::Directory(QString Name):QPtrList<DirEntry>()
{
    name = Name;
}

Directory::~Directory()
{
    DirEntry *p;
    while ((p = first()) != 0)
    {
        remove();
        delete p;   // auto-delete is disabled
    }
}

DirEntry *Directory::fetchById(int id)
{
    DirEntry *it;
    for (it=first(); it; it=next())
        if (it->getId() == id)
            return it;
    return 0;
}

void Directory::writeTree(GenericTree *tree_to_write_to, GenericTree *sdTree)
{
    DirEntry *it;
    for (it=first(); it; it=next())
        it->writeTree(tree_to_write_to, sdTree);
}


int Directory::compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2)
{
    DirEntry *d1 = (DirEntry *)s1;
    DirEntry *d2 = (DirEntry *)s2;

    return (getAlphaSortId(d1->getNickName()) - getAlphaSortId(d2->getNickName()));
}

DirEntry *Directory::getDirEntrybyDbId(int dbId)
{
    DirEntry *it;
    for (it=first(); it; it=next())
        if (it->getDbId() == dbId)
            return it;
    return 0;
}

DirEntry *Directory::getDirEntrybyUrl(QString Url)
{
    DirEntry *it;
    for (it=first(); it; it=next())
        if (it->getUri() == Url)
            return it;
    return 0;
}

void Directory::saveChangesinDB()
{
    DirEntry *it;
    for (it=first(); it; it=next())
    {
        it->updateYourselfInDB(name);
    }
}

void Directory::deleteEntry(DirEntry *Entry)
{
    Entry->deleteYourselfFromDB();

    if (find(Entry) != -1)
    {
        remove();
        delete Entry;
    }
}

void Directory::AddAllEntriesToList(QStrList &l, bool SpeeddialsOnly)
{
    DirEntry *it;
    for (it=first(); it; it=next())
        if ((!SpeeddialsOnly) || (it->isSpeedDial()))
            l.append(it->getUri());
    return;
}

void Directory::ChangePresenceStatus(QString Uri, int Status, QString StatusString, bool SpeeddialsOnly)
{
    DirEntry *it;
    for (it=first(); it; it=next())
        if ((it->urlMatches(Uri)) && 
            ((!SpeeddialsOnly) || (it->isSpeedDial())))
    {
        {
            if (!SpeeddialsOnly) 
            {
                (it->getTreeNode())->setSelectable(Status == ICON_PRES_OFFLINE ? false : true);
                (it->getTreeNode())->setString(it->getNickName() + "      (" + StatusString + ")");
            }
            (it->getSpeeddialNode())->setSelectable(Status == ICON_PRES_OFFLINE ? false : true);
            (it->getSpeeddialNode())->setAttribute(3, Status);
            (it->getSpeeddialNode())->setString(it->getNickName() + "      (" + StatusString + ")");
        }
    }
}



///////////////////////////////////////////////////////
//                  CallRecord Class
///////////////////////////////////////////////////////

CallRecord::CallRecord(QString dn, QString uri, bool callIn, QString ts)
{
    DisplayName  = dn;
    Uri          = uri;
    id           = counter++;
    timestamp    = ts;
    Duration     = 0;
    DirectionIn  = callIn;
    inDatabase   = false;
    changed      = true;
    dbId         = -1;
}


CallRecord::CallRecord(CallRecord *Original)
{
    DisplayName  = Original->DisplayName;
    Uri          = Original->Uri;
    timestamp    = Original->timestamp;
    Duration     = Original->Duration;
    DirectionIn  = Original->DirectionIn;
    id           = counter++;
    inDatabase   = false;
    changed      = true;
    dbId         = -1;
}


CallRecord::CallRecord(DirEntry *Original, bool callIn, QString ts)
{
    DisplayName = Original->getNickName();
    Uri         = Original->getUri();
    id          = counter++;
    timestamp   = ts;
    Duration    = 0;
    DirectionIn = callIn;
    inDatabase  = false;
    changed     = true;
    dbId        = -1;
}


CallRecord::~CallRecord()
{
}


void CallRecord::writeTree(GenericTree *tree_to_write_to)
{
    QString label = DisplayName;
    if (label.length() == 0)
        label = Uri;
    if (timestamp.length() > 0)
    {
        QDateTime dt = QDateTime::fromString(timestamp);
        QString ts = dt.toString("dd-MMM hh:mm");
        QString dur = QString(" (%1 min)").arg(Duration/60);

        // Left justify the name and right justify the timestamp
        // This doesn't work too well because it is a variable
        // length font; so really need to be part of the widget
        if (label.length() > 25)
            label.replace(22, 3, "...");
        label.leftJustify(25);
        ts.prepend("   ");
        label.replace(25, ts.length(), ts);
        label += dur;
    }
    GenericTree *sub_node = tree_to_write_to->addNode(label, 0, true);
    sub_node->setAttribute(0, TA_CALLHISTENTRY);
    sub_node->setAttribute(1, id);  // Unique ID
    sub_node->setAttribute(2, -id); // Sort newest first
}


void CallRecord::updateYourselfInDB()
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    if (!inDatabase)
    {
        thequery = QString("INSERT INTO phonecallhistory (displayname,url,timestamp,"
                               "duration, directionin, directoryref) VALUES "
                               "(\"%1\",\"%2\",\"%3\",%4,%5,%6);")
                              .arg(DisplayName.latin1()).arg(Uri.latin1())
                              .arg(timestamp.latin1()).arg(Duration)
                              .arg(DirectionIn).arg(0);
        query.exec(thequery);

        thequery = QString("SELECT MAX(recid) FROM phonecallhistory ;");
        query.exec(thequery);

        if ((query.isActive()) && (query.size() == 1))
        {
            query.next();
            dbId = query.value(0).toUInt();
            inDatabase = true;
            changed = false;
        }
        else
            cerr << "Mythphone: Something is up with the database\n";
    }
    else if (changed)
    {
        thequery = QString("UPDATE phonecallhistory "
                           "SET displayname=\"%1\", "
                               "url=\"%2\", "
                               "timestamp=\"%3\", "
                               "duration=%4, "
                               "directionin=%5, "
                               "directoryref=%6 "
                           "WHERE recid=%7 ;")
                           .arg(DisplayName.latin1()).arg(Uri.latin1())
                           .arg(timestamp.latin1()).arg(Duration)
                           .arg(DirectionIn).arg(0)
                           .arg(dbId);
        query.exec(thequery);
        changed = false;
    }
}


void CallRecord::deleteYourselfFromDB()
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    if (inDatabase)
    {
        thequery = QString("DELETE FROM phonecallhistory "
                           "WHERE recid=%1 ;").arg(dbId);
        query.exec(thequery);
    }
}







///////////////////////////////////////////////////////
//                CallHistory Class
///////////////////////////////////////////////////////

CallHistory::~CallHistory()
{
    CallRecord *p;
    while ((p = first()) != 0)
    {
        remove();
	delete p;   // auto-delete is disabled
    }
}

CallRecord *CallHistory::fetchById(int id)
{
    CallRecord *it;
    for (it=first(); it; it=next())
        if (it->getId() == id)
            return it;
    return 0;
}

void CallHistory::writeTree(GenericTree *placed_tree, GenericTree *received_tree)
{
    CallRecord *it;
    for (it=first(); it; it=next())
    {
        if (it->isIncoming())
            it->writeTree(received_tree);
        else
            it->writeTree(placed_tree);
    }
}


int CallHistory::compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2)
{
    CallRecord *d1 = (CallRecord *)s1;
    CallRecord *d2 = (CallRecord *)s2;
    QDateTime dt1 = QDateTime::fromString(d1->getTimestamp());
    QDateTime dt2 = QDateTime::fromString(d2->getTimestamp());

    if (dt1 == dt2)
        return 0;
    return ((dt1 < dt2) ? 1 : -1);
}

void CallHistory::saveChangesinDB()
{
    CallRecord *it;
    for (it=first(); it; it=next())
    {
        it->updateYourselfInDB();
    }
}

void CallHistory::deleteRecords()
{
    CallRecord *it;
    for (it=first(); it; it=current())
    {
        it->deleteYourselfFromDB();
        remove();
        delete it;
    }
}



///////////////////////////////////////////////////////
//            Directory Container Class
///////////////////////////////////////////////////////


DirectoryContainer::DirectoryContainer()
{
    // Create the Call History directory
    callHistory = new CallHistory();
}


DirectoryContainer::~DirectoryContainer()
{
    saveChangesinDB();

    Directory *p;
    while ((p = AllDirs.first()) != 0)
    {
        AllDirs.remove();
        delete p;   // auto-delete is disabled
    }

    delete callHistory;
    callHistory = 0;
}


void DirectoryContainer::Load()
{
    // First load the phone directory
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("SELECT intid, nickname,firstname,surname,"
                               "url,directory,photofile,speeddial,onhomelan "
                               "FROM phonedirectory "
                               "ORDER BY intid ;");
    query.exec(thequery);

    if(query.isActive() && query.size() > 0)
    {
        while(query.next())
        {
            QString Dir(query.value(5).toString());
            if (fetch(Dir) == 0)
                AllDirs.append(new Directory(Dir));

            DirEntry *entry = new DirEntry(
                                query.value(1).toString(),  // Nickname
                                query.value(4).toString(),  // URL
                                query.value(2).toString(),  // Firstname
                                query.value(3).toString(),  // Surname
                                query.value(6).toString(),  // PhotoFile
                                query.value(8).toInt());    // OnHomeLan
            entry->setDbId(query.value(0).toInt());
            entry->setSpeedDial(query.value(7).toInt());
            entry->setDBUpToDate();
            AddEntry(entry, Dir, false);
        }
    }
    else
        cout << "mythphone: Nothing in your Directory -- ok?\n";

    // Then load the phone call history
    thequery = QString("SELECT recid, displayname,url,timestamp,"
                               "duration, directionin, directoryref "
                               "FROM phonecallhistory "
                               "ORDER BY recid ;");
    query.exec(thequery);

    if(query.isActive() && query.size() > 0)
    {
        while(query.next())
        {
            CallRecord *entry = new CallRecord(
                                query.value(1).toString(),  // DisplayName
                                query.value(2).toString(),  // URL
                                query.value(5).toInt(),     // callIncoming
                                query.value(3).toString()); // Timestamp
            entry->setDbId(query.value(0).toInt());
            entry->setDuration(query.value(4).toInt());
            entry->setDBUpToDate();
            AddToCallHistory(entry, false);
        }
    }
    else
        cout << "mythphone: Nothing in your Call History -- ok?\n";
}

void DirectoryContainer::createTree()
{
    TreeRoot = new GenericTree("directory root", 0);
    TreeRoot->setAttribute(0, TA_ROOT); // Node Type
    TreeRoot->setAttribute(1, 0); // Identifier to find object in callback
    TreeRoot->setAttribute(2, 0); // Sorting parameter
}



Directory *DirectoryContainer::fetch(QString Dir)
{
    Directory *it;
    for (it=AllDirs.first(); it; it=AllDirs.next())
        if (it->getName() == Dir)
            return it;
    return 0;
}


DirEntry *DirectoryContainer::fetchDirEntryById(int id)
{
    Directory *it;
    DirEntry  *p;
    for (it=AllDirs.first(); it; it=AllDirs.next())
        if ((p = it->fetchById(id)) != 0)
            return p;
    return 0;
}

CallRecord *DirectoryContainer::fetchCallRecordById(int id)
{
    return (callHistory->fetchById(id));
}

void DirectoryContainer::AddEntry(DirEntry *entry, QString Dir, bool addToUITree)
{
    Directory *dp = fetch(Dir);
    if (dp == 0)
    {
        dp = new Directory(Dir);
        AllDirs.append(dp);
    }
    dp->append(entry);

    if (addToUITree)
        addToTree(entry, Dir);
}


void DirectoryContainer::ChangeEntry(DirEntry *entry, QString nn, QString Url, QString fn, QString sn, QString ph, bool OnHomeLan)
{
    if (nn != 0)
        entry->setNickName(nn);
    if (Url != 0)
        entry->setUri(Url);
    if (fn != 0)
        entry->setFirstName(fn);
    if (sn != 0)
        entry->setSurname(sn);
    if (ph != 0)
        entry->setPhotoFile(ph);

    entry->setOnHomeLan(OnHomeLan);

    // Change the entry in the GUI
    findInTree(TreeRoot, 0, TA_DIRENTRY, 1, entry->getId());

    // TODO -- not yet allowed to change the name in the tree
    // because there is no GenericTree 'delete' fn to do so
}


void DirectoryContainer::AddToCallHistory(CallRecord *entry, bool addToUITree)
{
    GenericTree* Tree;
    callHistory->append(entry);
    if (addToUITree)
    {
        Tree = (entry->isIncoming()) ? receivedcallsTree : placedcallsTree;
        entry->writeTree(Tree);
        Tree->reorderSubnodes(2); // Sort the new node
    }
}


void DirectoryContainer::clearCallHistory()
{
    // Remove from memory and SQL databases
    callHistory->deleteRecords();

    // Now remove from tree
    receivedcallsTree->deleteAllChildren();
    placedcallsTree->deleteAllChildren();
}


QStrList DirectoryContainer::getDirectoryList(void)
{
    QStrList l;
    Directory *it;
    for (it=AllDirs.first(); it; it=AllDirs.next())
    {
        l.append(it->getName());
    }
    return l;
}


void DirectoryContainer::writeTree()
{

    // First create the special trees
    speeddialTree     = TreeRoot->addNode(QObject::tr("Speed Dials"), 0, true);
    speeddialTree->setAttribute(0, TA_DIR);
    speeddialTree->setAttribute(1, 0); // No identifier required
    speeddialTree->setAttribute(2, 0); // Sort Order
    voicemailTree   = TreeRoot->addNode(QObject::tr("Voicemail"), 0, true);
    voicemailTree->setAttribute(0, TA_VMAIL);
    voicemailTree->setAttribute(1, 0);
    voicemailTree->setAttribute(2, 1); 
    placedcallsTree   = TreeRoot->addNode(QObject::tr("Placed Calls"), 0, true);
    placedcallsTree->setAttribute(0, TA_DIR);
    placedcallsTree->setAttribute(1, 0);
    placedcallsTree->setAttribute(2, 2);
    receivedcallsTree = TreeRoot->addNode(QObject::tr("Received Calls"), 0, true);
    receivedcallsTree->setAttribute(0, TA_DIR);
    receivedcallsTree->setAttribute(1, 0);
    receivedcallsTree->setAttribute(2, 2);

    // Write the placed/received calls tree
    callHistory->writeTree(placedcallsTree, receivedcallsTree);

    // Write Voicemail to the tree
    PutVoicemailInTree(voicemailTree);

    // Now add the normal directories into the tree
    Directory *it;
    for (it=AllDirs.first(); it; it=AllDirs.next())
    {
        GenericTree *sub_node = TreeRoot->addNode(it->getName(), 0, true);
        sub_node->setAttribute(0, TA_DIR);
        sub_node->setAttribute(1, 0); // No identifier required
        sub_node->setAttribute(2, 3); // Place last in sort

        it->writeTree(sub_node, speeddialTree);
    }
}



GenericTree *DirectoryContainer::addToTree(QString DirName)
{
    Directory *dp = fetch(DirName);
    if (dp != 0)
    {
        GenericTree *sub_node = TreeRoot->addNode(DirName, 0, true);
        sub_node->setAttribute(0, TA_DIR);
        sub_node->setAttribute(1, 0); // No identifier required
        sub_node->setAttribute(2, 3); // No sorting required
        return sub_node;
    }

    cerr << "No directory called " << DirName << endl;
    return 0;
}



void DirectoryContainer::addToTree(DirEntry *newEntry, QString Dir)
{
    GenericTree* Tree = TreeRoot->getChildByName(Dir);
    if (Tree == 0)
        Tree = addToTree(Dir);

    if (newEntry)
    {
        newEntry->writeTree(Tree, speeddialTree);
        Tree->reorderSubnodes(2);
    }
}


void DirectoryContainer::setSpeedDial(DirEntry *entry)
{
    if ((entry) && (!entry->isSpeedDial()))
    {
        entry->setSpeedDial(true);

        entry->writeTree(0, speeddialTree);
        speeddialTree->reorderSubnodes(2);
    }
}


void DirectoryContainer::removeSpeedDial(DirEntry *entry)
{
    if ((entry) && (entry->isSpeedDial()))
    {
        entry->setSpeedDial(false);

        // Need to delete all speeddials then re-add them --- no delete-on-item function
        speeddialTree->deleteAllChildren();

        Directory *it;
        for (it=AllDirs.first(); it; it=AllDirs.next())
            it->writeTree(0, speeddialTree);
    }
}


GenericTree *DirectoryContainer::findInTree(GenericTree *Root, int at1, int atv1, int at2, int atv2)
{
    // Should really be a generic UI function ...
    GenericTree *temp;
    GenericTree *Tree = Root;

    while ((Tree) && (Tree->getAttribute(at1) != atv1) && (Tree->getAttribute(at2) != atv2))
    {
        // Get next node; go deep first then travese siblings
        if (Tree->childCount() > 0)
        {
            Tree = Tree->getChildAt(0);
            continue;
        }

        if (Tree == Root) // Tree root has no children
            return 0;

        // Go to siblings first then recursively ask for parents next siblings
        if ((temp = Tree->nextSibling(1)) == 0)
        {
            do {
                Tree = Tree->getParent();
                if (Tree == Root)
                    return 0; // Not found 

            } while ((temp = Tree->nextSibling(1)) == 0);
            Tree = temp;
        }
        Tree = temp;
    }
    return Tree;
}

void DirectoryContainer::deleteFromTree(GenericTree *treeObject, DirEntry *entry)
{
    QString DirName = 0;
    if (entry)
    {
        if (entry->isSpeedDial())
            removeSpeedDial(entry);

        // Find which directory the entry is in then delete the entry from the DB
        Directory *it;
        for (it=AllDirs.first(); it; it=AllDirs.next())
        {
            if (it->fetchById(entry->getId()))
            {
                it->deleteEntry(entry);

                // Cannot delete single items in the tree; so delete all items under this items parent 
                // then add the remaining items back in
                GenericTree *itemDir = treeObject->getParent();
                itemDir->deleteAllChildren();
                it->writeTree(itemDir, 0);
                break;
            }
        }
    }
}


void DirectoryContainer::getRecentCalls(DirEntry *source, CallHistory &RecentCalls)
{
    CallRecord *cr;

    for (cr=callHistory->first(); cr; cr=callHistory->next())
    {
        if (source->urlMatches(cr->getUri()))
        {
            CallRecord *crCopy = new CallRecord(cr);
            RecentCalls.append(crCopy);
        }
    }
}


DirEntry *DirectoryContainer::getDirEntrybyDbId(int dbId)
{
    DirEntry *entry = 0;
    Directory *it;
    for (it=AllDirs.first(); (it) && (entry == 0); it=AllDirs.next())
        entry = it->getDirEntrybyDbId(dbId);
    return entry;
}

void DirectoryContainer::saveChangesinDB()
{
    Directory *it;
    for (it=AllDirs.first(); it; it=AllDirs.next())
        it->saveChangesinDB();
    callHistory->saveChangesinDB();
}


DirEntry *DirectoryContainer::FindMatchingDirectoryEntry(QString url)
{
    // See if a call-record matches a directory entry URL
    DirEntry *entry = 0;
    Directory *it;

    for (it=AllDirs.first(); (it) && (entry == 0); it=AllDirs.next())
        entry = it->getDirEntrybyUrl(url);

    return entry;
}


void DirectoryContainer::PutVoicemailInTree(GenericTree *tree_to_write_to)
{
    QString dirName = MythContext::GetConfDir() + "/MythPhone/Voicemail";
    QDir dir(dirName, "*.wav", QDir::Time, QDir::Files);
    if (!dir.exists())
    {
        cout << MythContext::GetConfDir() << "/MythPhone/Voicemail does not exist -- its meant to get created earlier so this is wrong\n";
        return;
    }

    // Fill tree from directory listing, using just the base name, which should be formatted nicely
    const QFileInfoList *il = dir.entryInfoList();
    if (il)
    {
        QFileInfoListIterator it(*il);
        QFileInfo *fi;
        for (int i=0; (fi = it.current()) != 0; ++it, i++)
        {
            GenericTree *sub_node = tree_to_write_to->addNode(fi->baseName(), 0, true);
            sub_node->setAttribute(0, TA_VMAIL_ENTRY);
            sub_node->setAttribute(1, i);
            sub_node->setAttribute(2, i);
        }
    }
}


void DirectoryContainer::deleteVoicemail(QString vmailName)
{
    // Get Voicemail Directory
    QString dirName = MythContext::GetConfDir() + "/MythPhone/Voicemail";
    QDir dir(dirName, "*.wav", QDir::Time, QDir::Files);
    if (!dir.exists())
    {
        cout << MythContext::GetConfDir() << "/MythPhone/Voicemail does not exist -- its meant to get created earlier so this is wrong\n";
        return;
    }

    // Delete that voicemail file
    dir.remove(vmailName + ".wav");

    // Now clear the voicemail tree and re-read it
    voicemailTree->deleteAllChildren();
    PutVoicemailInTree(voicemailTree);
}



void DirectoryContainer::clearAllVoicemail()
{
    // Get Voicemail Directory
    QString dirName = MythContext::GetConfDir() + "/MythPhone/Voicemail";
    QDir dir(dirName, "*.wav", QDir::Time, QDir::Files);
    if (!dir.exists())
    {
        cout << MythContext::GetConfDir() << "/MythPhone/Voicemail does not exist -- its meant to get created earlier so this is wrong\n";
        return;
    }


    // Delete only the filenames that were listed in the tree; so we don't delete any new ones
    // Should really be a generic UI function ...
    GenericTree *Node = voicemailTree->getChildAt(0);
    while (Node)
    {
        dir.remove(Node->getString() + ".wav");

        Node = Node->nextSibling(1);
    }

    // Now remove from tree
    voicemailTree->deleteAllChildren();
}


QStrList DirectoryContainer::ListAllEntries(bool SpeeddialsOnly)
{
    QStrList l;
    Directory *it;
    for (it=AllDirs.first(); it; it=AllDirs.next())
    {
        it->AddAllEntriesToList(l, SpeeddialsOnly);
    }
    return l;
}


void DirectoryContainer::ChangePresenceStatus(QString Uri, int Status, QString StatusString, bool SpeeddialsOnly)
{
    Directory *it;
    for (it=AllDirs.first(); it; it=AllDirs.next())
    {
        it->ChangePresenceStatus(Uri, Status, StatusString, SpeeddialsOnly);
    }
}

