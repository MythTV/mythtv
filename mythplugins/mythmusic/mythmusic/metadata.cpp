#include <iostream> 
#include <qapplication.h>
#include <qregexp.h> 
#include <qdatetime.h>
#include <qdir.h>

using namespace std;

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdbcon.h>

#include "metadata.h"
#include "treebuilders.h"

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

int Metadata::compare(Metadata *other) 
{
    if (format == "cast") 
    {
        int artist_cmp = Artist().lower().localeAwareCompare(other->Artist().lower());
        
        if (artist_cmp == 0) 
            return Title().lower().localeAwareCompare(other->Title().lower());
        
        return artist_cmp;
    } 
    else 
    {
        return (Track() - other->Track());
    }
}

bool Metadata::isInDatabase()
{
    bool retval = false;

    QString sqlfilename = filename.remove(0, m_startdir.length());

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT artist,compilation_artist,album,title,genre,year,tracknum,"
                  "length,intid,rating,playcount,lastplay,compilation,format FROM "
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
        format = query.value(13).toString();
        
        retval = true;
    }

    return retval;
}

void Metadata::dumpToDatabase()
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

    QString sqlfilename = filename.remove(0, m_startdir.length());

    // Don't update the database if a song with the exact same
    // metadata is already there
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT filename FROM musicmetadata WHERE "
                  "( ( artist = :ARTIST ) AND "
                  "( compilation_artist = :COMPILATION_ARTIST ) "
                  "( album = :ALBUM ) AND ( title = :TITLE ) "
                  "AND ( genre = :GENRE ) AND "
                  "( year = :YEAR ) AND ( tracknum = :TRACKNUM ) "
                  "AND ( length = :LENGTH ) "
                  "AND ( format = :FORMAT) );");
    query.bindValue(":ARTIST", artist.utf8());
    query.bindValue(":COMPILATION_ARTIST", compilation_artist.utf8());
    query.bindValue(":ALBUM", album.utf8());
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":GENRE", genre.utf8());
    query.bindValue(":YEAR", year);
    query.bindValue(":TRACKNUM", tracknum);
    query.bindValue(":LENGTH", length);
    query.bindValue(":FORMAT", format);

    if (query.exec() && query.isActive() && query.size() > 0)
        return;

    query.prepare("INSERT INTO musicmetadata "
                  "(artist,   compilation_artist, album,      title,  "
                  " genre,    year,               tracknum,   length, "
                  " filename, compilation,        date_added, date_modified, "
                  " format ) "
                  "VALUES "
                  "(:ARTIST,  :COMPILATION_ARTIST,:ALBUM,     :TITLE,   "
                  " :GENRE,   :YEAR,              :TRACKNUM,  :LENGTH,  "
                  " :FILENAME,:COMPILATION,       :DATE_ADDED,:DATE_MOD,"
                  " :FORMAT)");
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
    query.bindValue(":DATE_ADDED",  QDateTime::currentDateTime());
    query.bindValue(":DATE_MOD",    QDateTime::currentDateTime());
    query.bindValue(":FORMAT", format);
    
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
  rv.replace("COMPARTIST", compilation_artist);
  rv.replace("ARTIST", artist);
  rv.replace("TITLE", title);
  rv.replace("TRACK", QString("%1").arg(tracknum, 2));
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


void Metadata::updateDatabase()
{
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

    query.prepare("UPDATE musicmetadata    "
                  "SET artist   = :ARTIST,   "
                  "    album    = :ALBUM,    "
                  "    title    = :TITLE,    "
                  "    genre    = :GENRE,    "
                  "    year     = :YEAR,     "
                  "    tracknum = :TRACKNUM, "
                  "    rating   = :RATING,   " 
                  "    date_modified      = :DATE_MODIFIED, "
                  "    compilation        = :COMPILATION,   "
                  "    compilation_artist = :COMPILATION_ARTIST, "
                  "    format             = :FORMAT "
                  "WHERE intid = :ID;");
    query.bindValue(":ARTIST",             artist.utf8());
    query.bindValue(":ALBUM",              album.utf8());
    query.bindValue(":TITLE",              title.utf8());
    query.bindValue(":GENRE",              genre.utf8());
    query.bindValue(":YEAR",               year);
    query.bindValue(":TRACKNUM",           tracknum);
    query.bindValue(":RATING",             rating);
    query.bindValue(":DATE_MODIFIED",      QDateTime::currentDateTime());
    query.bindValue(":COMPILATION",        compilation);
    query.bindValue(":COMPILATION_ARTIST", compilation_artist.utf8());
    query.bindValue(":FORMAT", format);
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
                       "filename,intid,rating,playcount,lastplay,compilation,format "
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
        format = query.value(14).toString();

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
                  "length,filename,rating,playcount,lastplay,compilation,format FROM "
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
        format = query.value(14).toString();

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
    timestamp = timestamp.replace(':', "");
    timestamp = timestamp.replace('T', "");
    timestamp = timestamp.replace('-', "");

    return timestamp.toDouble();
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
    numPcs = numLoaded = 0;
    
    cd_title = QObject::tr("CD -- none");

    //  How should we sort?
    setSorting(path_assignment);

    root_node = new MusicNode(QObject::tr("All My Music"), paths);

    //
    //  Start a thread to do data
    //  loading and sorting
    //
    
    metadata_loader = NULL;
    startLoading();

    all_music.setAutoDelete(true);
    
    last_listed = -1;
}

AllMusic::~AllMusic()
{
    all_music.clear();

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

/** \fn AllMusic::startLoading(void)
 *  \brief Start loading metadata.
 *
 *  Makes the AllMusic object run it's resync in a thread.
 *  Once done, the doneLoading() method will return true.
 *
 *  \note Alternatively, this could be made to emit a signal
 *        so the caller won't have to poll for completion.
 *
 *  \returns true if the loader thread was started
 */
bool AllMusic::startLoading(void)
{
    // Set this to false early rather than letting it be
    // delayed till the thread calls resync.
    done_loading = false;

    if (metadata_loader)
    {
        cleanOutThreads();
        delete metadata_loader;
    }

    metadata_loader = new MetadataLoadingThread(this);
    metadata_loader->start();

    return true;
}

void AllMusic::resync()
{
    done_loading = false;
    QString aquery =    "SELECT intid, artist, compilation_artist, album, title, genre, "
                        "year, tracknum, length, filename, rating, "
                        "lastplay, playcount, compilation, format "
                        "FROM musicmetadata "
                        "ORDER BY intid;";

    QString filename, artist, album, title;

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(aquery);

    root_node->clear();
    all_music.clear();

    numPcs = query.size() * 2;
    numLoaded = 0;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            filename = QString::fromUtf8(query.value(9).toString());
            if (!filename.contains("://"))
                filename = startdir + filename;

            artist = QString::fromUtf8(query.value(1).toString());
            if (artist.isEmpty())
                artist = QObject::tr("Unknown Artist");

            album = QString::fromUtf8(query.value(3).toString());
            if (album.isEmpty())
                album = QObject::tr("Unknown Album");

            title = QString::fromUtf8(query.value(4).toString());
            if (title.isEmpty())
                title = QObject::tr("Unknown Title");

            Metadata *temp = new Metadata(
                filename,
                artist,
                QString::fromUtf8(query.value(2).toString()),
                album,
                title,
                QString::fromUtf8(query.value(5).toString()),
                query.value(6).toInt(),
                query.value(7).toInt(),
                query.value(8).toInt(),
                query.value(0).toInt(),
                query.value(10).toInt(),
                query.value(12).toInt(),
                query.value(11).toString(),
                (query.value(13).toInt() > 0),
                query.value(14).toString());
            
            //  Don't delete temp, as PtrList now owns it
            all_music.append(temp);

            // compute max/min playcount,lastplay for all music
            if (query.at() == 0)
            { // first song
                playcountMin = playcountMax = temp->PlayCount();
                lastplayMin  = lastplayMax  = temp->LastPlay();
            }
            else
            {
                int playCount = temp->PlayCount();
                double lastPlay = temp->LastPlay();

                playcountMin = min(playCount, playcountMin);
                playcountMax = max(playCount, playcountMax);
                lastplayMin  = min(lastPlay,  lastplayMin);
                lastplayMax  = max(lastPlay,  lastplayMax);
            }
            numLoaded++;
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
}

void AllMusic::printTree()
{
    //  debugging

    cout << "Whole Music Tree" << endl;
    root_node->printYourself(0);
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
    
    QPtrListIterator<Metadata> an_iterator( all_music );
    Metadata *inserter;
    MetadataPtrList list;

    while ( (inserter = an_iterator.current()) != 0 )
    {
        if (inserter->isVisible())
            list.append(inserter);
        ++an_iterator;

        numLoaded++;
    }

    MusicTreeBuilder *builder = MusicTreeBuilder::createBuilder (paths);
    builder->makeTree (root_node, list);
    delete builder;
}

void AllMusic::writeTree(GenericTree *tree_to_write_to)
{
    root_node->writeTree(tree_to_write_to, 0);
}

bool AllMusic::putYourselfOnTheListView(TreeCheckItem *where)
{
    root_node->putYourselfOnTheListView(where, false);
    return true;
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
        QString title_temp = QString("%1 - %2").arg((*anit).Track()).arg(title_string);
        QString level_temp = QObject::tr("title");
        CDCheckItem *new_item = new CDCheckItem(where, title_temp, level_temp, 
                                                -(*anit).Track());
        new_item->setCheck(false); //  Avoiding -Wall     
    }  
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
        a_label += " ~ ";
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
                a_label = QString("CD: %1 ~ %2 - %3").arg((*anit).FormatArtist()).arg((*anit).Track()).arg((*anit).FormatTitle());
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
    MusicNode::SetStaticData(startdir, paths);

    if (paths == "directory")
        return;

    //  Error checking
    QStringList tree_levels = QStringList::split(" ", paths);
    QStringList::const_iterator it = tree_levels.begin();
    for (; it != tree_levels.end(); ++it)
    {
        if (*it != "genre"        &&
            *it != "artist"       &&
            *it != "splitartist"  && 
            *it != "splitartist1" && 
            *it != "album"        &&
            *it != "title")
        {
            VERBOSE(VB_IMPORTANT, "AllMusic::setSorting()" +
                    QString("Unknown tree level '%1'").arg(*it));
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

MusicNode::MusicNode(const QString &a_title, const QString &tree_level)
{
    my_title = a_title;
    my_level = tree_level;
    my_subnodes.setAutoDelete(true);
    setPlayCountMin(0);
    setPlayCountMax(0);
    setLastPlayMin(0);
    setLastPlayMax(0);
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

        if (playcountMax == playcountMin) 
            playcountValue = 0; 
        else 
            playcountValue = ((playcountMin - (double)playcount) / (playcountMax - playcountMin) + 1); 
        if (lastplayMax == lastplayMin) 
            lastplayValue = 0;
        else 
            lastplayValue = ((lastplayMin - lastplaydbl) / (lastplayMax - lastplayMin) + 1);

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
    return ((Metadata*)item1)->compare((Metadata*)item2);
}

int MusicNodePtrList::compareItems (QPtrCollection::Item item1, 
                                    QPtrCollection::Item item2)
{
    MusicNode *itemA = (MusicNode*)item1;
    MusicNode *itemB = (MusicNode*)item2;

    QString title1 = itemA->getTitle().lower();
    QString title2 = itemB->getTitle().lower();
    
    // Cut "the " off the front of titles
    if (title1.left(4) == thePrefix) 
        title1 = title1.mid(4);
    if (title2.left(4) == thePrefix) 
        title2 = title2.mid(4);

    return title1.localeAwareCompare(title2);
}
