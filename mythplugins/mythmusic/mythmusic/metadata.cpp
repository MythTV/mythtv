#include <iostream> 
#include <qsqldatabase.h> 
#include <qregexp.h> 
#include <qdatetime.h>


using namespace std;

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>

#include "metadata.h"

bool operator==(const Metadata& a, const Metadata& b)
{
    if (a.Filename() == b.Filename())
        return true;
    return false;
}

bool operator!=(const Metadata& a, const Metadata& b)
{
    if (a.Filename() != b.Filename())
        return true;
    return false;
}

Metadata& Metadata::operator=(Metadata *rhs)
{
    artist = rhs->Artist();
    album = rhs->Album();
    title = rhs->Title();
    genre = rhs->Genre();
    year = rhs->Year();
    tracknum = rhs->Track();
    length = rhs->Length();
    rating = rhs->Rating();
    lastplay = rhs->Lastplay();
    playcount = rhs->Playcount();
    id = rhs->ID();
    filename = rhs->Filename();
    changed = rhs->hasChanged();

    return *this;
}

void Metadata::persist(QSqlDatabase *db)
{

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("UPDATE musicmetadata set "
                               "rating = %1 , "
                               "playcount = %2 , "
                               "lastplay = \"%3\" "
                               "where intid = %4 ;")
                               .arg(rating)
                               .arg(playcount)
                               .arg(lastplay, lastplay.length())
                               .arg(id);

    QSqlQuery query = db->exec(thequery);

    if (query.numRowsAffected() < 1)
    {
        cerr << "metadata.o: Had a problem updating metadata. Couldn't find row in database" << endl;
    }
    
}

bool Metadata::isInDatabase(QSqlDatabase *db)
{
    bool retval = false;

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("SELECT artist,album,title,genre,year,tracknum,"
                               "length,intid,rating,playcount,lastplay FROM "
                               "musicmetadata WHERE filename = \"%1\";")
                               .arg(sqlfilename);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
  
        artist = query.value(0).toString();
        album = query.value(1).toString();
        title = query.value(2).toString();
        genre = query.value(3).toString();
        year = query.value(4).toInt();
        tracknum = query.value(5).toInt();
        length = query.value(6).toInt();
        id = query.value(7).toUInt();
        rating = query.value(8).toInt();
        playcount = query.value(9).toInt();
        lastplay = query.value(10).toString();

        retval = true;
    }

    return retval;
}

void Metadata::dumpToDatabase(QSqlDatabase *db)
{
    if (artist == "")
        artist = "Unknown";
    if (album == "")
        album = "Unknown";
    if (title == "")
        title = filename;
    if (genre == "")
        genre = "Unknown";

    title.replace(QRegExp("\""), QString("\\\""));
    artist.replace(QRegExp("\""), QString("\\\""));
    album.replace(QRegExp("\""), QString("\\\""));
    genre.replace(QRegExp("\""), QString("\\\""));

    QString sqlfilename = filename;
    sqlfilename.replace(QRegExp("\""), QString("\\\""));

    QString thequery = QString("INSERT INTO musicmetadata (artist,album,title,"
                               "genre,year,tracknum,length,filename) VALUES "
                               "(\"%1\",\"%2\",\"%3\",\"%4\",%5,%6,%7,\"%8\");")
                              .arg(artist.latin1()).arg(album.latin1())
                              .arg(title.latin1()).arg(genre).arg(year)
                              .arg(tracknum).arg(length).arg(sqlfilename);
    db->exec(thequery);

    // easiest way to ensure we've got 'id' filled.
    fillData(db);
}

void Metadata::setField(const QString &field, const QString &data)
{
    if (field == "artist")
        artist = data;
    else if (field == "album")
        album = data;
    else if (field == "title")
        title = data;
    else if (field == "genre")
        genre = data;
    else if (field == "filename")
        filename = data;
    else if (field == "year")
        year = data.toInt();
    else if (field == "tracknum")
        tracknum = data.toInt();
    else if (field == "length")
        length = data.toInt();
    else
    {
        cerr << "metadata.o: Something asked me to return data about a field called " << field << endl ;
    }
}

bool Metadata::areYouFinished(uint depth, uint treedepth, const QString &paths, const QString &startdir)
{
    if(paths == "directory")
    {
        //  have we made it to directory just above the file name?

        QString working = filename;
        working.replace(QRegExp(startdir), QString(""));
        working = working.section('/', depth);
        if(working.contains('/') < 1)
        {
            return true;
        }
    }
    else
    {
        if(depth + 1 >= treedepth)
        {
            return true;
        }
    }
    return false;
}

void Metadata::getField(const QStringList &tree_levels, QString *data, const QString &paths, const QString &startdir, uint depth)
{
    if(paths == "directory")
    {
        //  Return directory values as if they were 
        //  real metadata/TAG values
        
        QString working = filename;
        working.replace(QRegExp(startdir), QString(""));
        working.replace(QRegExp("/[^/]*$"), QString(""));

        working = working.section('/', depth, depth);        
        
        *data = working;
    }
    else
    {
        getField(tree_levels[depth], data);
    }
}

void Metadata::getField(const QString &field, QString *data)
{
    if (field == "artist")
        *data = artist;
    else if (field == "album")
        *data = album;
    else if (field == "title")
        *data = title;
    else if (field == "genre")
        *data = genre;
    else
    {
        cerr << "metadata.o: Something asked me to return data about a field called " << field << endl ;
        *data = "I Dunno";
    }
}

void Metadata::fillData(QSqlDatabase *db)
{
    if (title == "")
        return;

    QString sqltitle = title;
    sqltitle.replace(QRegExp("\""), QString("\\\""));

    QString thequery = "SELECT artist,album,title,genre,year,tracknum,length,"
                       "filename,intid,rating,playcount,lastplay FROM "
                       "musicmetadata WHERE title=\"" + sqltitle + "\"";

    if (album != "")
    {
        QString temp = album;
        temp.replace(QRegExp("\""), QString("\\\""));
        thequery += " AND album=\"" + temp + "\"";
    }
    if (artist != "")
    {
        QString temp = artist;
        temp.replace(QRegExp("\""), QString("\\\""));
        thequery += " AND artist=\"" + temp + "\"";
    }

    thequery += ";";

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        artist = query.value(0).toString();
        album = query.value(1).toString();
        title = query.value(2).toString();
        genre = query.value(3).toString();
        year = query.value(4).toInt();
        tracknum = query.value(5).toInt();
        length = query.value(6).toInt();
        filename = query.value(7).toString();
        id = query.value(8).toUInt();
        rating = query.value(9).toInt();
        playcount = query.value(10).toInt();
        lastplay = query.value(11).toString();
    }
}

void Metadata::fillDataFromID(QSqlDatabase *db)
{       
    if (id == 0)
        return; 
        
    QString thequery;
    thequery = QString("SELECT title,artist,album,title,genre,year,tracknum,"
                       "length,filename,rating,playcount,lastplay FROM "
                       "musicmetadata WHERE intid=%1;").arg(id);
        
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        title = query.value(0).toString();
        artist = query.value(1).toString();
        album = query.value(2).toString();
        title = query.value(3).toString();
        genre = query.value(4).toString();
        year = query.value(5).toInt();
        tracknum = query.value(6).toInt();
        length = query.value(7).toInt();
        filename = query.value(8).toString();
        rating = query.value(9).toInt();
        playcount = query.value(10).toInt();
        lastplay = query.value(11).toString();
    }
}

void Metadata::decRating()
{
    if (rating > 0)
    {
        rating--;
    }
    changed = true;
}

void Metadata::incRating()
{
    if (rating < 10)
    {
        rating++;
    }
    changed = true;
}

double Metadata::LastPlay()
{
    QString timestamp = lastplay;


    if(timestamp.contains('-') < 1)
    {
        timestamp.insert(4, '-');
        timestamp.insert(7, '-');
        timestamp.insert(10, 'T');
        timestamp.insert(13, ':');
        timestamp.insert(16, ':');
    }

    QDateTime lTime = QDateTime::fromString(timestamp, Qt::ISODate);
    double lastDateTime = lTime.toString("yyyyMMddhhmmss").toDouble();
    return lastDateTime;
}

void Metadata::setLastPlay()
{
    QDateTime cTime = QDateTime::currentDateTime();
    lastplay = cTime.toString("yyyyMMddhhmmss");
    changed = true;
}

void Metadata::incPlayCount()
{
    if (playcount < 50)
    {
        playcount++;

    }
    changed = true;
}

MetadataLoadingThread::MetadataLoadingThread(AllMusic *parent_ptr)
{
    parent = parent_ptr;
}

void MetadataLoadingThread::run()
{
    //if you want to simulate a big music collection load
    //sleep(3); 
    parent->resync();
}

AllMusic::AllMusic(QSqlDatabase *ldb, QString path_assignment, QString a_startdir)
{
    db = ldb;
    startdir = a_startdir;
    done_loading = false;
    
    cd_title = "CD -- none";

    //  How should we sort?
    setSorting(path_assignment);
    root_node = new MusicNode("root", startdir, paths, tree_levels, 0);

    //
    //  Start a thread to do data
    //  loading and sorting
    //
    
    metadata_loader = new MetadataLoadingThread(this);
    metadata_loader->start();

    all_music.setAutoDelete(true);
    top_nodes.setAutoDelete(true);
    
    last_listed = -1;
}

AllMusic::~AllMusic()
{
    all_music.clear();
    top_nodes.clear();

    delete root_node;

    metadata_loader->wait();
    delete metadata_loader;
}

bool AllMusic::cleanOutThreads()
{
    //  If this is still running, the user
    //  probably selected mythmusic and then
    //  escaped out right away
    
    if(metadata_loader->finished())
    {
        return true;
    }

    metadata_loader->wait();
    return false;
}

void AllMusic::resync()
{
    done_loading = false;
    QString aquery =    "SELECT intid, artist, album, title, genre, "
                        "year, tracknum, length, filename, rating, "
                        "lastplay, playcount FROM musicmetadata "
                        "ORDER BY intid  ";

    QSqlQuery query = db->exec(aquery);

    all_music.clear();
    
    if(query.isActive() && query.numRowsAffected() > 0)
    {
        while(query.next())
        {
            Metadata *temp = new Metadata
                                    (
                                        query.value(8).toString(),
                                        query.value(1).toString(),
                                        query.value(2).toString(),
                                        query.value(3).toString(),
                                        query.value(4).toString(),
                                        query.value(5).toInt(),
                                        query.value(6).toInt(),
                                        query.value(7).toInt(),
                                        query.value(0).toInt(),
                                        query.value(9).toInt(),
                                        query.value(10).toInt(),
                                        query.value(11).toString()
                                    );
            all_music.append(temp); //  Don't delete temp, as PtrList now owns it
        }
    }
    else
    {
        if(query.isActive())
        {
            cerr << "metadata.o: Your database is out of whack. This is not good." << endl ;
        }
        else
        {
            cerr << "metadata.o: Your don't seem to have any tracks. That's ok with me if it's ok with you." << endl; 
        }
    }    
    
    //  To find this data quickly, build a map
    //  (a map to pointers!)
    
    QPtrListIterator<Metadata> an_iterator( all_music );
    Metadata *map_add;

    music_map.clear();
    while ( (map_add = an_iterator.current()) != 0 )
    {
        music_map[map_add->ID()] = map_add; 
        ++an_iterator;
    }
    
    //  Build a tree to reflect current state of 
    //  the metadata. Once built, sort it.
    
    buildTree(); 
    //printTree();
    sortTree();
    //printTree();
    done_loading = true;
}

void AllMusic::sortTree()
{
    root_node->sort();

    bool something_changed;
    
    //  sort top level nodes
    
    something_changed = false;
    if(top_nodes.count() > 1)
    {
        something_changed = true;
    }
    while(something_changed)
    {
        something_changed = false;
        for(uint i = 0; i < top_nodes.count() - 1;)
        {
            if(qstrcmp(top_nodes.at(i)->getTitle(), top_nodes.at(i+1)->getTitle()) > 0)
            {
                something_changed = true;
                MusicNode *temp = top_nodes.take(i + 1);
                top_nodes.insert(i, temp);
            }
            else
            {
                ++i;
            }
        }
    }
    
    //  tell top level nodes to sort from themselves 
    //  downwards

    QPtrListIterator<MusicNode> iter(top_nodes);
    MusicNode *crawler;
    while ( (crawler = iter.current()) != 0 )
    {
        crawler->sort();
        ++iter;
    }
}

void AllMusic::printTree()
{
    //  debugging

    cout << "Whole Music Tree" << endl;
    root_node->printYourself(0);
    QPtrListIterator<MusicNode> iter( top_nodes );
    MusicNode *printer;
    while ( (printer = iter.current()) != 0 )
    {
        printer->printYourself(1);
        ++iter;
    }
}

void AllMusic::buildTree()
{
    //
    //  Given "paths" and loaded metadata,
    //  build a tree (nodes, leafs, and all)
    //  that reflects the desired structure
    //  of the metadata. This is a structure
    //  that makes it easy (and QUICK) to 
    //  display metadata on (for example) a
    //  Select Music screen
    //
    
    top_nodes.clear();
    root_node->clearTracks();
    
    QPtrListIterator<Metadata> an_iterator( all_music );
    Metadata *inserter;
    while ( (inserter = an_iterator.current()) != 0 )
    {
        intoTree(inserter);
        ++an_iterator;
    }
}

void AllMusic::writeTree(GenericTree *tree_to_write_to)
{
    GenericTree *sub_node = tree_to_write_to->addNode("All My Music", 0);
    sub_node->setAttribute(0, 0);
    sub_node->setAttribute(1, 0);
    sub_node->setAttribute(2, 0);
    sub_node->setAttribute(3, 0);
    

    QPtrListIterator<MusicNode> iter( top_nodes );
    MusicNode *traverse;
    iter.toFirst();
    int a_counter = 0;
    while ( (traverse = iter.current()) != 0 )
    {
        traverse->writeTree(sub_node, a_counter);
        ++a_counter;
        ++iter;
    }
}

bool AllMusic::putYourselfOnTheListView(TreeCheckItem *where, int how_many)
{


    root_node->putYourselfOnTheListView(where, false);

    if(how_many < 0)
    {
    
        QPtrListIterator<MusicNode> iter( top_nodes );
        MusicNode *traverse;
        iter.toLast();
        while ( (traverse = iter.current()) != 0 )
        {
            traverse->putYourselfOnTheListView(where, true);
            --iter;
        }
        return true;
    }
    else
    {
        if(last_listed < 0)
        {
            last_listed = 0;
        }
        
        QPtrListIterator<MusicNode> iter( top_nodes );
        MusicNode *traverse;
        iter.toLast();
        iter -= last_listed;
        int numb_this_round = 0;

        while(true)
        {
            traverse = iter.current();
            if(traverse)
            {
                traverse->putYourselfOnTheListView(where, true);
                --iter;
                ++last_listed;
                ++numb_this_round;
                if(numb_this_round >= how_many)
                {
                    return false;
                }
            }
            else
            {
                return true;
            }
        }
    }
    cerr << "metadata.o: Control defied all possible logic and jumped way out here. World may end shortly. " << endl;
    return false;
}

void AllMusic::putCDOnTheListView(CDCheckItem *where)
{
    ValueMetadata::iterator anit;
    for(anit = cd_data.begin(); anit != cd_data.end(); ++anit)
    {
        QString title_string = "";
        if((*anit).Title().length() > 0)
        {
            title_string = (*anit).Title();
        }
        else
        {
            title_string = "Unknown";
        }
        QString title_temp = QString("%1 - %2").arg((*anit).Track()).arg(title_string);
        QString level_temp = "title";
        CDCheckItem *new_item = new CDCheckItem(where, title_temp, level_temp, (*anit).Track());
        new_item->setOn(false); //  Avoiding -Wall     
    }  
}


void AllMusic::intoTree(Metadata* inserter)
{
    MusicNode *insertion_point = findRightNode(inserter, 0);
    insertion_point->insert(inserter);
}

MusicNode* AllMusic::findRightNode(Metadata* inserter, uint depth)
{
    QString a_field = "";

    //  Use metadata to find pre-exisiting insertion
    //  point or (recursively) create nodes as needed
    //  and return ultimate insertion point
    
    if(inserter->areYouFinished(depth, tree_levels.count(), paths, startdir))
    {
        //  special case, track is at root level
        //  e.g. an mp3 in the root directory and
        //  paths=directory
        return root_node;
    }
    
    inserter->getField(tree_levels.first(), &a_field, paths, startdir, depth);
    QPtrListIterator<MusicNode> iter( top_nodes );
    MusicNode *search;
    while ( (search = iter.current()) != 0 )
    {
        if(a_field == search->getTitle())
        {
            return ( search->findRightNode(tree_levels, inserter, depth + 1) );
        }
        ++iter;
    }
    //  If we made it here, no appropriate top level node exists
    
    MusicNode *new_one = new MusicNode(a_field, startdir, paths, tree_levels, 0);
    top_nodes.append(new_one);
    return ( new_one->findRightNode(tree_levels, inserter, depth + 1) );
}

QString AllMusic::getLabel(int an_id, bool *error_flag)
{
    QString a_label = "";
    if(an_id > 0)
    {
   
        if (!music_map.contains(an_id))
        {
            a_label = QString("Missing database entry: %1").arg(an_id);
            *error_flag = true;
            return a_label;
        }
      
        a_label += music_map[an_id]->Artist();
        a_label += " ~ ";
        a_label += music_map[an_id]->Title();
    

        if(a_label.length() < 1)
        {
            a_label = "Ooops";
            *error_flag = true;
        }
        else
        {
            *error_flag = false;
        }
        return a_label;
    }
    else
    {
        ValueMetadata::iterator anit;
        for(anit = cd_data.begin(); anit != cd_data.end(); ++anit)
        {
            if( (*anit).Track() == an_id * -1)
            {
                a_label = QString("%1 ~ %2").arg((*anit).Artist()).arg((*anit).Title());
                *error_flag = false;
                return a_label;
            }
        }
    }

    a_label = "";
    *error_flag = true;
    return a_label;
}

Metadata* AllMusic::getMetadata(int an_id)
{
    if(an_id > 0)
    {
        if (music_map.contains(an_id))
        {
            return music_map[an_id];    
        }
    }
    else if(an_id < 0)
    {
        ValueMetadata::iterator anit;
        for(anit = cd_data.begin(); anit != cd_data.end(); ++anit)
        {
            if( (*anit).Track() == an_id * -1)
            {
                return &(*anit);
            }
        }
    }
    return NULL;
}

void AllMusic::save()
{
    //  Check each Metadata entry and save those that 
    //  have changed (ratings, etc.)
    
    
    QPtrListIterator<Metadata> an_iterator( all_music );
    Metadata *searcher;
    while ( (searcher = an_iterator.current()) != 0 )
    {
        if(searcher->hasChanged())
        {
            searcher->persist(db);
        }
        ++an_iterator;
    }
}

void AllMusic::clearCDData()
{
    cd_data.clear();
    cd_title = "CD -- none";
}

void AllMusic::addCDTrack(Metadata *the_track)
{
    cd_data.append(*the_track);
}

bool AllMusic::checkCDTrack(Metadata *the_track)
{
    if(cd_data.count() < 1)
    {
        return false;
    }
    if(cd_data.first().Title() == the_track->Title())
    {
        return true;
    }
    return false;
}

bool AllMusic::getCDMetadata(int the_track, Metadata *some_metadata)
{
    ValueMetadata::iterator anit;
    for(anit = cd_data.begin(); anit != cd_data.end(); ++anit)
    {
        if( (*anit).Track() == the_track)
        {
            *some_metadata = (*anit);
            return true;
        }

    }  
    return false;
}

void AllMusic::setSorting(QString a_paths)
{
    paths = a_paths;
    if(paths == "directory")
    {
        return;
    }
    else
    {
        tree_levels = QStringList::split(" ", paths);
    }
    //  Error checking
    for(QStringList::Iterator it = tree_levels.begin(); it != tree_levels.end(); ++it )
    {
        if( *it != "genre"  &&
            *it != "artist" &&
            *it != "album"  &&
            *it != "title")
        {
            cerr << "metadata.o: I don't understand the expression \"" << *it 
                 << "\" as a tree level in a music hierarchy " << endl ; 
        }
            
    }
}

MusicNode::MusicNode(QString a_title, const QString& a_startdir, 
                     const QString& a_paths, QStringList tree_levels, 
                     uint depth)
{
    my_title = a_title;
    startdir = a_startdir;
    paths = a_paths;
    if(paths == "directory")
    {
        my_level = "directory";
    }
    else
    {
        if(depth < tree_levels.count())
        {
            my_level = tree_levels[depth];
        }
        else
        {
            my_level = "I am confused";
            cerr << "metadata.o: Something asked me to look up a StringList entry that doesn't exist" << endl ;
        }
       
    }

    my_subnodes.setAutoDelete(true);
}

MusicNode::~MusicNode()
{
    my_subnodes.clear();
}

void MusicNode::insert(Metadata* inserter)
{
    my_tracks.append(inserter);
}

MusicNode* MusicNode::findRightNode(QStringList tree_levels, 
                                    Metadata *inserter, uint depth)
{
    QString a_field = "";

    //
    //  Search and create from my node downards
    //


    if(inserter->areYouFinished(depth, tree_levels.count(), paths, startdir))
    {
        return this;
    }
    else
    {
        inserter->getField(tree_levels, &a_field, paths, startdir, depth);
        QPtrListIterator<MusicNode> iter(my_subnodes);
        MusicNode *search;
        while( (search = iter.current() ) != 0)
        {
            if(a_field == search->getTitle())
            {
                return( search->findRightNode(tree_levels, inserter, depth + 1) );
            }
            ++iter;
        }
        MusicNode *new_one = new MusicNode(a_field, startdir, paths, tree_levels, depth);
        my_subnodes.append(new_one);
        return (new_one->findRightNode(tree_levels, inserter, depth + 1) );                
    }
}

void MusicNode::putYourselfOnTheListView(TreeCheckItem *parent, bool show_node)
{
    TreeCheckItem *current_parent;

    if(show_node)
    {
        QString title_temp = my_title;
        QString level_temp = my_level;
        current_parent = new TreeCheckItem(parent, title_temp, level_temp, 0);
    }
    else
    {
        current_parent = parent;
    }


    QPtrListIterator<Metadata>  anit(my_tracks);
    Metadata *a_track;
    anit.toLast();
    while( (a_track = anit.current() ) != 0)
    {
        QString title_temp = QString("%1 - %2").arg(a_track->Track()).arg(a_track->Title());
        QString level_temp = "title";
        TreeCheckItem *new_item = new TreeCheckItem(current_parent, title_temp, level_temp, a_track->ID());
        --anit;
        new_item->setOn(false); //  Avoiding -Wall     
    }  

    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *sub_traverse;
    iter.toLast();
    while( (sub_traverse = iter.current() ) != 0)
    {
        sub_traverse->putYourselfOnTheListView(current_parent, true);
        --iter;
    }
    
}

void MusicNode::writeTree(GenericTree *tree_to_write_to, int a_counter)
{
    
    GenericTree *sub_node = tree_to_write_to->addNode(my_title);
    sub_node->setAttribute(0, 0);
    sub_node->setAttribute(1, a_counter);
    sub_node->setAttribute(2, rand());
    sub_node->setAttribute(3, rand());
    
    QPtrListIterator<Metadata>  anit(my_tracks);
    Metadata *a_track;
    int track_counter = 0;
    anit.toFirst();
    while( (a_track = anit.current() ) != 0)
    {
        QString title_temp = QString("%1 - %2").arg(a_track->Track()).arg(a_track->Title());
        GenericTree *subsub_node = sub_node->addNode(title_temp, a_track->ID(), true);
        subsub_node->setAttribute(0, 1);
        subsub_node->setAttribute(1, track_counter);    // regular order
        subsub_node->setAttribute(2, rand());           // random order

        //
        //  "Intelligent" ordering
        //
        QDateTime cTime = QDateTime::currentDateTime();
        double currentDateTime = cTime.toString("yyyyMMddhhmmss").toDouble();
        int rating = a_track->Rating();
        int playcount = a_track->PlayCount();
        double lastplay = a_track->LastPlay();
        double ratingValue = (double)rating / 10;
        double playcountValue = (double)playcount / 50;
        double lastplayValue = (currentDateTime - lastplay) / currentDateTime * 2000;
        double rating_value =  (35 * ratingValue - 25 * playcountValue + 25 * lastplayValue + 
                                15 * (double)rand() / (RAND_MAX + 1.0));
        int integer_rating = (int) rating_value * 100000;
        subsub_node->setAttribute(3, integer_rating);   //  "intelligent" order
        ++track_counter;
        ++anit;
    }  

    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *sub_traverse;
    int another_counter = 0;
    iter.toFirst();
    while( (sub_traverse = iter.current() ) != 0)
    {
        sub_traverse->writeTree(sub_node, another_counter);
        ++another_counter;
        ++iter;
    }
}


void MusicNode::sort()
{
    bool something_changed;

    //  Sort any tracks
    
    something_changed = false;
    if(my_tracks.count() > 1)
    {
        something_changed = true;
    }
    while(something_changed)
    {
        something_changed = false;
        for(uint i = 0; i < my_tracks.count() - 1;)
        {

            if(my_tracks.at(i)->Track() > my_tracks.at(i+1)->Track())
            {
                something_changed = true;
                Metadata *temp = my_tracks.take(i + 1);
                my_tracks.insert(i, temp);
            }
            else
            {
                ++i;
            }
        }
    }

    //  Sort any subnodes
    
    something_changed = false;
    if(my_subnodes.count() > 1)
    {
        something_changed = true;
    }
    while(something_changed)
    {
        something_changed = false;
        for(uint i = 0; i < my_subnodes.count() - 1;)
        {
            if(qstrcmp(my_subnodes.at(i)->getTitle(), my_subnodes.at(i+1)->getTitle()) > 0)
            {
                something_changed = true;
                MusicNode *temp = my_subnodes.take(i + 1);
                my_subnodes.insert(i, temp);
            }
            else
            {
                ++i;
            }
        }
    }
    
    //  Tell any subnodes to sort themselves

    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *crawler;
    while ( (crawler = iter.current()) != 0 )
    {
        crawler->sort();
        ++iter;
    }
}


void MusicNode::printYourself(int indent_level)
{

    for(int i = 0; i < (indent_level) * 4; ++i)
    {
        cout << " " ;
    }
    cout << my_title << endl;

    QPtrListIterator<Metadata>  anit(my_tracks);
    Metadata *a_track;
    while( (a_track = anit.current() ) != 0)
    {
        for(int j = 0; j < (indent_level + 1) * 4; j++)
        {
            cout << " " ;
        } 
        cout << a_track->Title() << endl ;
        ++anit;
    }       
    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *print;
    while( (print = iter.current() ) != 0)
    {
        print->printYourself(indent_level + 1);
        ++iter;
    }
}

