#include <iostream> 
#include <qregexp.h> 
#include <qdatetime.h>
#include <qdir.h>

using namespace std;

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdbcon.h>

#include "metadata.h"

struct FieldSplitInfo {
   QString testStr;
   QString dispStr;
};

static FieldSplitInfo splitArray4[] =
{ 
  {"ABCDE", " (A B C D E)"},
  {"FGHIJ", " (F G H I J)"},
  {"KLMNO", " (K L M N O)"},
  {"PQRST", " (P Q R S T)"},
  {"UVWXYZ", " (U V W X Y Z)"}
};
const int kSplitArray4_Max = sizeof splitArray4 / sizeof splitArray4[0];

static FieldSplitInfo splitArray1[] =
{ 
  {"A", " (A)"},
  {"B", " (B)"},
  {"C", " (C)"},
  {"D", " (D)"},
  {"E", " (E)"},
  {"F", " (F)"},
  {"G", " (G)"},
  {"H", " (H)"},
  {"I", " (I)"},
  {"J", " (J)"},
  {"K", " (K)"},
  {"L", " (L)"},
  {"M", " (M)"},
  {"N", " (N)"},
  {"O", " (O)"},
  {"P", " (P)"},
  {"Q", " (Q)"},
  {"R", " (R)"},
  {"S", " (S)"},
  {"T", " (T)"},
  {"U", " (U)"},
  {"V", " (V)"},
  {"W", " (W)"},
  {"X", " (X)"},
  {"Y", " (Y)"},
  {"Z", " (Z)"},
};
const int kSplitArray1_Max = sizeof splitArray1 / sizeof splitArray1[0];

static QString thePrefix = "the ";

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
    compilation_artist = rhs->CompilationArtist();
    album = rhs->Album();
    title = rhs->Title();
    formattedartist = rhs->FormatArtist();
    formattedtitle = rhs->FormatTitle();
    genre = rhs->Genre();
    year = rhs->Year();
    tracknum = rhs->Track();
    length = rhs->Length();
    rating = rhs->Rating();
    lastplay = rhs->LastPlayStr();
    playcount = rhs->Playcount();
    compilation = rhs->Compilation();
    id = rhs->ID();
    filename = rhs->Filename();
    changed = rhs->hasChanged();

    return *this;
}

QString Metadata::m_startdir = "";

void Metadata::SetStartdir(const QString &dir)
{
    Metadata::m_startdir = dir;
}

void Metadata::persist()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE musicmetadata set rating = :RATING , "
                  "playcount = :PLAYCOUNT , lastplay = :LASTPLAY "
                  "where intid = :ID ;");
    query.bindValue(":RATING", rating);
    query.bindValue(":PLAYCOUNT", playcount);
    query.bindValue(":LASTPLAY", lastplay);
    query.bindValue(":ID", id);

    if (!query.exec() || query.numRowsAffected() < 1)
        MythContext::DBError("music persist", query);
}

bool Metadata::isInDatabase(QString startdir)
{
    bool retval = false;

    QString sqlfilename = filename;
    sqlfilename = filename.remove(0, startdir.length());

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT artist,compilation_artist,album,title,genre,year,tracknum,"
                  "length,intid,rating,playcount,lastplay,compilation FROM "
                  "musicmetadata WHERE filename = :FILENAME ;");
    query.bindValue(":FILENAME", sqlfilename.utf8());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
  
        artist = QString::fromUtf8(query.value(0).toString());
        compilation_artist = QString::fromUtf8(query.value(1).toString());
        album = QString::fromUtf8(query.value(2).toString());
        title = QString::fromUtf8(query.value(3).toString());
        genre = QString::fromUtf8(query.value(4).toString());
        year = query.value(5).toInt();
        tracknum = query.value(6).toInt();
        length = query.value(7).toInt();
        id = query.value(8).toUInt();
        rating = query.value(9).toInt();
        playcount = query.value(10).toInt();
        lastplay = query.value(11).toString();
        compilation = (query.value(12).toInt() > 0);
        
        retval = true;
    }

    return retval;
}

void Metadata::dumpToDatabase(QString startdir)
{
    if (artist == "")
        artist = QObject::tr("Unknown Artist");
    if (compilation_artist == "")
        compilation_artist = artist; // This should be the same as Artist if blank.
    if (album == "")
        album = QObject::tr("Unknown Album");
    if (title == "")
        title = filename;
    if (genre == "")
        genre = QObject::tr("Unknown Genre");

    QString sqlfilename = filename;
    sqlfilename = filename.remove(0, startdir.length());

    // Don't update the database if a song with the exact same
    // metadata is already there
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT filename FROM musicmetadata WHERE "
                  "( ( artist = :ARTIST ) AND "
                  "( compilation_artist = :COMPILATION_ARTIST ) "
                  "( album = :ALBUM ) AND ( title = :TITLE ) "
                  "AND ( genre = :GENRE ) AND "
                  "( year = :YEAR ) AND ( tracknum = :TRACKNUM ) "
                  "AND ( length = :LENGTH ) );");
    query.bindValue(":ARTIST", artist.utf8());
    query.bindValue(":COMPILATION_ARTIST", compilation_artist.utf8());
    query.bindValue(":ALBUM", album.utf8());
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":GENRE", genre.utf8());
    query.bindValue(":YEAR", year);
    query.bindValue(":TRACKNUM", tracknum);
    query.bindValue(":LENGTH", length);

    if (query.exec() && query.isActive() && query.size() > 0)
        return;

    query.prepare("INSERT INTO musicmetadata (artist,compilation_artist,album,title,"
                  "genre,year,tracknum,length,filename,compilation,date_added) VALUES "
                  "(:ARTIST, :COMPILATION_ARTIST, :ALBUM, :TITLE, :GENRE, :YEAR, :TRACKNUM, "
                  ":LENGTH, :FILENAME, :COMPILATION, :DATE_ADDED );");
    query.bindValue(":ARTIST", artist.utf8());
    query.bindValue(":COMPILATION_ARTIST", compilation_artist.utf8());
    query.bindValue(":ALBUM", album.utf8());
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":GENRE", genre.utf8());
    query.bindValue(":YEAR", year);
    query.bindValue(":TRACKNUM", tracknum);
    query.bindValue(":LENGTH", length);
    query.bindValue(":FILENAME", sqlfilename.utf8());
    query.bindValue(":COMPILATION", compilation);
    query.bindValue(":DATE_ADDED", QDate::currentDate());
    
    query.exec();

    // easiest way to ensure we've got 'id' filled.
    fillData();
}

// Default values for formats
// NB These will eventually be customizable....
QString Metadata::formatnormalfileartist      = "ARTIST";
QString Metadata::formatnormalfiletrack       = "TITLE";
QString Metadata::formatnormalcdartist        = "ARTIST";
QString Metadata::formatnormalcdtrack         = "TITLE";
QString Metadata::formatcompilationfileartist = "COMPARTIST";
QString Metadata::formatcompilationfiletrack  = "TITLE (ARTIST)";
QString Metadata::formatcompilationcdartist   = "COMPARTIST";
QString Metadata::formatcompilationcdtrack    = "TITLE (ARTIST)";

void Metadata::setArtistAndTrackFormats()
{
    QString tmp;
    
    tmp = gContext->GetSetting("MusicFormatNormalFileArtist");
    if (!tmp.isEmpty())
        formatnormalfileartist = tmp;
    
    tmp = gContext->GetSetting("MusicFormatNormalFileTrack");
    if (!tmp.isEmpty())
        formatnormalfiletrack = tmp;
        
    tmp = gContext->GetSetting("MusicFormatNormalCDArtist");
    if (!tmp.isEmpty())
        formatnormalcdartist = tmp;
        
    tmp = gContext->GetSetting("MusicFormatNormalCDTrack");
    if (!tmp.isEmpty())
        formatnormalcdtrack = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationFileArtist");
    if (!tmp.isEmpty())
        formatcompilationfileartist = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationFileTrack");
    if (!tmp.isEmpty())
        formatcompilationfiletrack = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationCDArtist");
    if (!tmp.isEmpty())
        formatcompilationcdartist = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationCDTrack");
    if (!tmp.isEmpty())
        formatcompilationcdtrack = tmp;
}


bool Metadata::determineIfCompilation(bool cd)
{ 
    compilation = (!compilation_artist.isEmpty() 
                   && artist != compilation_artist);
    setCompilationFormatting(cd);
    return compilation;
}


inline QString Metadata::formatReplaceSymbols(const QString &format)
{
  QString rv = format;
  rv.replace(QRegExp("COMPARTIST"), compilation_artist);
  rv.replace(QRegExp("ARTIST"), artist);
  rv.replace(QRegExp("TITLE"), title);
  rv.replace(QRegExp("TRACK"), QString("%1").arg(tracknum, 2));
  return rv;
}


inline void Metadata::setCompilationFormatting(bool cd)
{
    QString format_artist, format_title;
    
    if (!compilation
        || "" == compilation_artist
        || artist == compilation_artist)
    {
        if (!cd)
        {
          format_artist = formatnormalfileartist;
          format_title  = formatnormalfiletrack;
        }
        else
        {
          format_artist = formatnormalcdartist;
          format_title  = formatnormalcdtrack;          
        }
    }
    else
    {
        if (!cd)
        {
          format_artist = formatcompilationfileartist;
          format_title  = formatcompilationfiletrack;
        }
        else
        {
          format_artist = formatcompilationcdartist;
          format_title  = formatcompilationcdtrack;          
        }
    }

    // NB Could do some comparisons here to save memory with shallow copies...
    formattedartist = formatReplaceSymbols(format_artist);
      
    formattedtitle = formatReplaceSymbols(format_title);
}


QString Metadata::FormatArtist()
{
    if (formattedartist.isEmpty())
        setCompilationFormatting();
        
    return formattedartist;
}


QString Metadata::FormatTitle()
{
    if (formattedtitle.isEmpty())
        setCompilationFormatting();

    return formattedtitle;
}


void Metadata::updateDatabase(QString startdir)
{
    startdir = startdir; 
    // only save to DB if something changed
    //if (!hasChanged())
    //    return;

    if (artist == "")
        artist = QObject::tr("Unknown Artist");
    if (album == "")
        album = QObject::tr("Unknown Album");
    if (title == "")
        title = filename;
    if (genre == "")
        genre = QObject::tr("Unknown Genre");

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE musicmetadata SET artist = :ARTIST, album = :ALBUM, "
                  "compilation_artist = :COMPILATION_ARTIST, "
                  "title = :TITLE, genre = :GENRE, year = :YEAR, "
                  "tracknum = :TRACKNUM, rating = :RATING, " 
                  "compilation = :COMPILATION "
                  "WHERE intid = :ID;");
    query.bindValue(":ARTIST", artist.utf8());
    query.bindValue(":COMPILATION_ARTIST", compilation_artist.utf8());
    query.bindValue(":ALBUM", album.utf8());
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":GENRE", genre.utf8());
    query.bindValue(":YEAR", year);
    query.bindValue(":TRACKNUM", tracknum);
    query.bindValue(":RATING", rating);
    query.bindValue(":COMPILATION", compilation);
    query.bindValue(":ID", id);

    if (!query.exec())
         MythContext::DBError("Update musicmetadata", query);
}


void Metadata::setField(const QString &field, const QString &data)
{
    if (field == "artist")
        artist = data;
    // myth@colin.guthr.ie: Not sure what calls this method as I can't seem
    //                      to find anything that does!
    //                      I've added the compilation_artist stuff here for 
    //                      completeness.
    else if (field == "compilation_artist")
      compilation_artist = data;
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
    else if (field == "compilation")
        compilation = (data.toInt() > 0);
    
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
        *data = FormatArtist();
    else if (field == "album")
        *data = album;
    else if (field == "title")
        *data = FormatTitle();
    else if (field == "genre")
        *data = genre;
    else if (field == "splitartist")
    {
        bool set = false;
        QString firstchar;
        if (FormatArtist().left(4).lower() == thePrefix)
            firstchar = FormatArtist().mid(4, 1).upper();
        else
            firstchar = FormatArtist().left(1).upper();

        for (int i = 0; i < kSplitArray4_Max; i++)
        {
            if (splitArray4[i].testStr.contains(firstchar))
            {
                set = true;
                *data = QObject::tr("Artists") + splitArray4[i].dispStr;
            }
        }

        if (!set)
            *data = QObject::tr("Artists") + " (" + firstchar + ")";
    }
    else if (field == "splitartist1")
    {
        bool set = false;
        QString firstchar;
        if (FormatArtist().left(4).lower() == thePrefix)
            firstchar = FormatArtist().mid(4, 1).upper();
        else
            firstchar = FormatArtist().left(1).upper();

        for (int i = 0; i < kSplitArray1_Max; i++)
        {
            if (splitArray1[i].testStr.contains(firstchar))
            {
                set = true;
                *data = QObject::tr("Artists") + splitArray1[i].dispStr;
            }
        }

        if (!set)
            *data = QObject::tr("Artists") + " (" + firstchar + ")";
    }
    else
    {
        cerr << "metadata.o: Something asked me to return data about a field called " << field << endl ;
        *data = "I Dunno";
    }
}

void Metadata::fillData()
{
    if (title == "")
        return;

    QString thequery = "SELECT artist,compilation_artist,album,title,genre,year,tracknum,length,"
                       "filename,intid,rating,playcount,lastplay,compilation "
                       "FROM musicmetadata WHERE title = :TITLE";

    if (album != "")
        thequery += " AND album = :ALBUM";
    if (artist != "")
        thequery += " AND artist = :ARTIST";
    if (compilation_artist != "")
        thequery += " AND compilation_artist = :COMPILATION_ARTIST";

    thequery += ";";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(thequery);
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":ALBUM", album.utf8());
    query.bindValue(":ARTIST", artist.utf8());
    query.bindValue(":COMPILATION_ARTIST", compilation_artist.utf8());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        artist = QString::fromUtf8(query.value(0).toString());
        compilation_artist = QString::fromUtf8(query.value(1).toString());
        album = QString::fromUtf8(query.value(2).toString());
        title = QString::fromUtf8(query.value(3).toString());
        genre = QString::fromUtf8(query.value(4).toString());
        year = query.value(5).toInt();
        tracknum = query.value(6).toInt();
        length = query.value(7).toInt();
        filename = QString::fromUtf8(query.value(8).toString());
        id = query.value(9).toUInt();
        rating = query.value(10).toInt();
        playcount = query.value(11).toInt();
        lastplay = query.value(12).toString();
        compilation = (query.value(13).toInt() > 0);

        if (!filename.contains("://"))
            filename = m_startdir + filename;
    }
}

void Metadata::fillDataFromID()
{       
    if (id == 0)
        return; 
        
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title,artist,compilation_artist,album,title,genre,year,tracknum,"
                  "length,filename,rating,playcount,lastplay,compilation FROM "
                  "musicmetadata WHERE intid = :ID ;");
    query.bindValue(":ID", id);
        
    if (query.exec() && query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        title = QString::fromUtf8(query.value(0).toString());
        artist = QString::fromUtf8(query.value(1).toString());
        compilation_artist = QString::fromUtf8(query.value(2).toString());
        album = QString::fromUtf8(query.value(3).toString());
        title = QString::fromUtf8(query.value(4).toString());
        genre = QString::fromUtf8(query.value(5).toString());
        year = query.value(6).toInt();
        tracknum = query.value(7).toInt();
        length = query.value(8).toInt();
        filename = QString::fromUtf8(query.value(9).toString());
        rating = query.value(10).toInt();
        playcount = query.value(11).toInt();
        lastplay = query.value(12).toString();
        compilation = (query.value(13).toInt() > 0);

        if (!filename.contains("://"))
            filename = m_startdir + filename;
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
    playcount++;
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

AllMusic::AllMusic(QString path_assignment, QString a_startdir)
{
    startdir = a_startdir;
    done_loading = false;
    
    cd_title = QObject::tr("CD -- none");

    //  How should we sort?
    setSorting(path_assignment);

    MusicNode::SetStaticData(startdir, paths);

    root_node = new MusicNode("root", tree_levels, 0);

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
    QString aquery =    "SELECT intid, artist, compilation_artist, album, title, genre, "
                        "year, tracknum, length, filename, rating, "
                        "lastplay, playcount, compilation FROM musicmetadata "
                        "ORDER BY intid;";

    QString filename;
    QString startdir = gContext->GetSetting("MusicLocation");
    startdir = QDir::cleanDirPath(startdir);
    if (!startdir.endsWith("/"));
        startdir += "/";

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(aquery);

    all_music.clear();
    
    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            filename = QString::fromUtf8(query.value(9).toString());
            if (!filename.contains("://"))
                filename = startdir + filename;

            Metadata *temp = new Metadata
                                    (
                                        filename,
                                        QString::fromUtf8(query.value(1).toString()),
                                        QString::fromUtf8(query.value(2).toString()),
                                        QString::fromUtf8(query.value(3).toString()),
                                        QString::fromUtf8(query.value(4).toString()),
                                        QString::fromUtf8(query.value(5).toString()),
                                        query.value(6).toInt(),
                                        query.value(7).toInt(),
                                        query.value(8).toInt(),
                                        query.value(0).toInt(),
                                        query.value(10).toInt(),
                                        query.value(12).toInt(),
                                        query.value(11).toString(),
                                        (query.value(13).toInt() > 0)
                                    );
            all_music.append(temp); //  Don't delete temp, as PtrList now owns it

            // compute max/min playcount,lastplay for all music
            if (query.at() == 0) { // first song
                playcountMin = playcountMax = temp->PlayCount();
                lastplayMin = lastplayMax = temp->LastPlay();
            } else {
                if (temp->PlayCount() < playcountMin) { playcountMin = temp->PlayCount(); }
                else if (temp->PlayCount() > playcountMax) { playcountMax = temp->PlayCount(); }

                if (temp->LastPlay() < lastplayMin) { lastplayMin = temp->LastPlay(); }
                else if (temp->LastPlay() > lastplayMax) { lastplayMax = temp->LastPlay(); }
            }
        }
    }
    else
    {
        cerr << "metadata.o: You don't seem to have any tracks. That's ok with me if it's ok with you." << endl; 
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

    //  sort top level nodes
    
    top_nodes.sort();
    
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
        if (inserter->isVisible())
            intoTree(inserter);
        ++an_iterator;
    }
}

void AllMusic::writeTree(GenericTree *tree_to_write_to)
{
    GenericTree *sub_node = tree_to_write_to->addNode(QObject::tr("All My Music"), 0);
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
        traverse->setPlayCountMin(playcountMin);
        traverse->setPlayCountMax(playcountMax);
        traverse->setLastPlayMin(lastplayMin);
        traverse->setLastPlayMax(lastplayMax);
        traverse->writeTree(sub_node, a_counter);
        ++a_counter;
        ++iter;
    }
}

bool AllMusic::putYourselfOnTheListView(TreeCheckItem *where, int how_many)
{
    root_node->putYourselfOnTheListView(where, false);

    if (how_many < 0)
    {
        QPtrListIterator<MusicNode> iter(top_nodes);
        MusicNode *traverse;
        while ((traverse = iter.current()) != 0)
        {
            traverse->putYourselfOnTheListView(where, true);
            ++iter;
        }
        return true;
    }
    else
    {
        if (last_listed < 0)
            last_listed = 0;
        
        QPtrListIterator<MusicNode> iter(top_nodes);
        MusicNode *traverse;
        iter += last_listed;
        int numb_this_round = 0;

        while (true)
        {
            traverse = iter.current();
            if (traverse)
            {
                traverse->putYourselfOnTheListView(where, true);
                ++iter;
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
            title_string = (*anit).FormatTitle();
        }
        else
        {
            title_string = QObject::tr("Unknown");
        }
        QString title_temp = QString(QObject::tr("%1 - %2")).arg((*anit).Track()).arg(title_string);
        QString level_temp = QObject::tr("title");
        CDCheckItem *new_item = new CDCheckItem(where, title_temp, level_temp, 
                                                -(*anit).Track());
        new_item->setCheck(false); //  Avoiding -Wall     
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
    
    MusicNode *new_one = new MusicNode(a_field, tree_levels, 0);
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
            a_label = QString(QObject::tr("Missing database entry: %1")).arg(an_id);
            *error_flag = true;
            return a_label;
        }
      
        a_label += music_map[an_id]->FormatArtist();
        a_label += QObject::tr(" ~ ");
        a_label += music_map[an_id]->FormatTitle();
    

        if(a_label.length() < 1)
        {
            a_label = QObject::tr("Ooops");
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
                a_label = QString(QObject::tr("CD: %1 ~ %2 - %3")).arg((*anit).FormatArtist()).arg((*anit).Track()).arg((*anit).FormatTitle());
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

bool AllMusic::updateMetadata(int an_id, Metadata *the_track)
{
    if(an_id > 0)
    {
        Metadata *mdata = getMetadata(an_id);
        if (mdata)
        {
            *mdata = the_track;
            return true;    
        }
    }
    return false;
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
            searcher->persist();
        }
        ++an_iterator;
    }
}

void AllMusic::clearCDData()
{
    cd_data.clear();
    cd_title = QObject::tr("CD -- none");
}

void AllMusic::addCDTrack(Metadata *the_track)
{
    cd_data.append(*the_track);
}

bool AllMusic::checkCDTrack(Metadata *the_track)
{
    if (cd_data.count() < 1)
    {
        return false;
    }
    if (cd_data.last().FormatTitle() == the_track->FormatTitle())
    {
        return true;
    }
    return false;
}

bool AllMusic::getCDMetadata(int the_track, Metadata *some_metadata)
{
    ValueMetadata::iterator anit;
    for (anit = cd_data.begin(); anit != cd_data.end(); ++anit)
    {
        if ((*anit).Track() == the_track)
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
    if (paths == "directory")
    {
        return;
    }
    else
    {
        tree_levels = QStringList::split(" ", paths);
    }
    //  Error checking
    for (QStringList::Iterator it = tree_levels.begin(); it != tree_levels.end(); ++it )
    {
        if( *it != "genre"  &&
            *it != "artist" &&
            *it != "splitartist" && 
            *it != "album"  &&
            *it != "title")
        {
            cerr << "metadata.o: I don't understand the expression \"" << *it 
                 << "\" as a tree level in a music hierarchy " << endl ; 
        }
            
    }
}

void AllMusic::setAllVisible(bool visible)
{
    QPtrListIterator<Metadata> an_iterator( all_music );
    Metadata *md;
    while ( (md = an_iterator.current()) != 0 )
    {
        md->setVisible(visible);
        ++an_iterator;
    }
}

MusicNode::MusicNode(QString a_title, QStringList tree_levels, uint depth)
{
    my_title = a_title;
    if (m_paths == "directory")
    {
        my_level = "directory";
    }
    else
    {
        if (depth < tree_levels.count())
            my_level = tree_levels[depth];
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

// static member vars

QString MusicNode::m_startdir = "";
QString MusicNode::m_paths = "";
int MusicNode::m_RatingWeight = 2;
int MusicNode::m_PlayCountWeight = 2;
int MusicNode::m_LastPlayWeight = 2;
int MusicNode::m_RandomWeight = 2;

void MusicNode::SetStaticData(const QString &startdir, const QString &paths)
{
    m_startdir = startdir;
    m_paths = paths;
    m_RatingWeight = gContext->GetNumSetting("IntelliRatingWeight", 2);
    m_PlayCountWeight = gContext->GetNumSetting("IntelliPlayCountWeight", 2);
    m_LastPlayWeight = gContext->GetNumSetting("IntelliLastPlayWeight", 2);
    m_RandomWeight = gContext->GetNumSetting("IntelliRandomWeight", 2);
}
    
void MusicNode::insert(Metadata* inserter)
{
    my_tracks.append(inserter);
}

MusicNode* MusicNode::findRightNode(QStringList tree_levels, 
                                    Metadata *inserter, uint depth)
{
    QString a_field = "";
    QString a_lowercase_field = "";
    QString a_title = "";

    //
    //  Search and create from my node downards
    //


    if(inserter->areYouFinished(depth, tree_levels.count(), m_paths, m_startdir))
    {
        return this;
    }
    else
    {
        inserter->getField(tree_levels, &a_field, m_paths, m_startdir, depth);

        a_lowercase_field = a_field.lower();
        if (a_lowercase_field.left(4) == "the ")
            a_lowercase_field = a_lowercase_field.mid(4);

        QPtrListIterator<MusicNode> iter(my_subnodes);
        MusicNode *search;
        while ((search = iter.current() ) != 0)
        {
            a_title = search->getTitle().lower();
            if (a_title.left(4) == thePrefix)
                a_title = a_title.mid(4);

            if (a_lowercase_field == a_title)
            {
                return( search->findRightNode(tree_levels, inserter, depth + 1) );
            }
            ++iter;
        }
        MusicNode *new_one = new MusicNode(a_field, tree_levels, depth);
        my_subnodes.append(new_one);
        return (new_one->findRightNode(tree_levels, inserter, depth + 1) );                
    }
}

void MusicNode::putYourselfOnTheListView(TreeCheckItem *parent, bool show_node)
{
    TreeCheckItem *current_parent;

    if (show_node)
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
    while ((a_track = anit.current() ) != 0)
    {
        QString title_temp = QString(QObject::tr("%1 - %2"))
                                  .arg(a_track->Track()).arg(a_track->Title());
        QString level_temp = QObject::tr("title");
        TreeCheckItem *new_item = new TreeCheckItem(current_parent, title_temp,
                                                    level_temp, a_track->ID());
        ++anit;
        new_item->setCheck(false); //  Avoiding -Wall     
    }  

    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *sub_traverse;
    while ((sub_traverse = iter.current() ) != 0)
    {
        sub_traverse->putYourselfOnTheListView(current_parent, true);
        ++iter;
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
        QString title_temp = QString(QObject::tr("%1 - %2")).arg(a_track->Track()).arg(a_track->Title());
        GenericTree *subsub_node = sub_node->addNode(title_temp, a_track->ID(), true);
        subsub_node->setAttribute(0, 1);
        subsub_node->setAttribute(1, track_counter);    // regular order
        subsub_node->setAttribute(2, rand());           // random order

        //
        //  "Intelligent" ordering
        //
        int rating = a_track->Rating();
        int playcount = a_track->PlayCount();
        double lastplaydbl = a_track->LastPlay();
        double ratingValue = (double)(rating) / 10;
        double playcountValue, lastplayValue;
        if (playcountMax == playcountMin) { playcountValue = 0; }
        else { playcountValue = ((playcountMin - (double)playcount) / (playcountMax - playcountMin) + 1); }
        if (lastplayMax == lastplayMin) { lastplayValue = 0; }
        else { lastplayValue = ((lastplayMin - lastplaydbl) / (lastplayMax - lastplayMin) + 1); }
        double rating_value =  (m_RatingWeight * ratingValue + m_PlayCountWeight * playcountValue +
                                m_LastPlayWeight * lastplayValue + m_RandomWeight * (double)rand() /
                                (RAND_MAX + 1.0));
        int integer_rating = (int) (4000001 - rating_value * 10000);
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
        sub_traverse->setPlayCountMin(playcountMin);
        sub_traverse->setPlayCountMax(playcountMax);
        sub_traverse->setLastPlayMin(lastplayMin);
        sub_traverse->setLastPlayMax(lastplayMax);
        sub_traverse->writeTree(sub_node, another_counter);
        ++another_counter;
        ++iter;
    }
}


void MusicNode::sort()
{
    //  Sort any tracks
    my_tracks.sort();

    //  Sort any subnodes
    my_subnodes.sort();
    
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

/**************************************************************************/

int MetadataPtrList::compareItems(QPtrCollection::Item item1, 
                                  QPtrCollection::Item item2)
{
    return ((Metadata*)item1)->Track() - ((Metadata*)item2)->Track();
}

int MusicNodePtrList::compareItems (QPtrCollection::Item item1, 
                                    QPtrCollection::Item item2)
{
    MusicNode *itemA = (MusicNode*)item1;
    MusicNode *itemB = (MusicNode*)item2;

    QString title1 = itemA->getTitle().lower();
    QString title2 = itemB->getTitle().lower();
    
    // Cut "the " off the front of titles
    title1 = (title1.lower().left(4) == thePrefix) ? title1.mid(4) : title1;
    title2 = (title2.lower().left(4) == thePrefix) ? title2.mid(4) : title2;

    return qstrcmp(title1, title2);
}
