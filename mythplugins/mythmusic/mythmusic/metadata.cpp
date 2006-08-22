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
    artist = rhs->artist;
    compilation_artist = rhs->compilation_artist;
    album = rhs->album;
    title = rhs->title;
    formattedartist = rhs->formattedartist;
    formattedtitle = rhs->formattedtitle;
    genre = rhs->genre;
    year = rhs->year;
    tracknum = rhs->tracknum;
    length = rhs->length;
    rating = rhs->rating;
    lastplay = rhs->lastplay;
    playcount = rhs->playcount;
    compilation = rhs->compilation;
    id = rhs->id;
    filename = rhs->filename;
    changed = rhs->changed;

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
    query.prepare("UPDATE music_songs set rating = :RATING , "
                  "numplays = :PLAYCOUNT , lastplay = :LASTPLAY "
                  "where song_id = :ID ;");
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

    QString sqlfilename(filename);
    if (!sqlfilename.contains("://"))
        sqlfilename.remove(0, m_startdir.length());

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT music_artists.artist_name, music_comp_artists.artist_name AS compilation_artist, "
                  "music_albums.album_name, music_songs.name, music_genres.genre, music_songs.year, "
                  "music_songs.track, music_songs.length, music_songs.song_id, music_songs.rating, "
                  "music_songs.numplays, music_songs.lastplay, music_albums.compilation, "
                  "music_songs.format "
                  "FROM music_songs "
                  "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                  "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                  "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
                  "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                  "WHERE music_songs.filename = :FILENAME ;");
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

    QString sqlfilename(filename);
    if (!sqlfilename.contains("://"))
        sqlfilename.remove(0, m_startdir.length());

    // Don't update the database if a song with the exact same
    // metadata is already there
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT music_songs.filename "
                  "FROM music_songs "
                  "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                  "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                  "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
                  "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                  "WHERE music_artists.artist_name = :ARTIST"
                  " AND music_comp_artists.artist_name = :COMPILATION_ARTIST"
                  " AND music_albums.album_name = :ALBUM"
                  " AND music_songs.name = :TITLE"
                  " AND music_genres.genre = :GENRE"
                  " AND music_songs.year = :YEAR"
                  " AND music_songs.track = :TRACKNUM"
                  " AND music_songs.length = :LENGTH"
                  " AND music_songs.format = :FORMAT ;");

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

    // Load the artist id or insert it and get the id
    unsigned int artistId;
    query.prepare("SELECT artist_id FROM music_artists "
                  "WHERE artist_name = :ARTIST ;");
    query.bindValue(":ARTIST", artist.utf8());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("music select artist id", query);
        return;
    }
    if (query.size() > 0)
    {
        query.next();
        artistId = query.value(0).toInt();
    }
    else
    {
        query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
        query.bindValue(":ARTIST", artist.utf8());

        if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
        {
            MythContext::DBError("music insert artist", query);
            return;
        }
        artistId = query.lastInsertId().toInt();
    }

    // Compilation Artist
    unsigned int compilationArtistId;
    query.prepare("SELECT artist_id FROM music_artists "
                  "WHERE artist_name = :ARTIST ;");
    query.bindValue(":ARTIST", compilation_artist.utf8());
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("music select compilation artist id", query);
        return;
    }
    if (query.size() > 0)
    {
        query.next();
        compilationArtistId = query.value(0).toInt();
    }
    else
    {
        query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
        query.bindValue(":ARTIST", compilation_artist.utf8());

        if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
        {
            MythContext::DBError("music insert compilation artist", query);
            return;
        }
        compilationArtistId = query.lastInsertId().toInt();
    }

    // Album
    unsigned int albumId;
    query.prepare("SELECT album_id FROM music_albums "
                  "WHERE artist_id = :COMP_ARTIST_ID "
                  " AND album_name = :ALBUM ;");
    query.bindValue(":COMP_ARTIST_ID", compilationArtistId);
    query.bindValue(":ALBUM", album.utf8());
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("music select album id", query);
        return;
    }
    if (query.size() > 0)
    {
        query.next();
        albumId = query.value(0).toInt();
    }
    else
    {
        query.prepare("INSERT INTO music_albums (artist_id, album_name, compilation, year) VALUES (:COMP_ARTIST_ID, :ALBUM, :COMPILATION, :YEAR);");
        query.bindValue(":COMP_ARTIST_ID", compilationArtistId);
        query.bindValue(":ALBUM", album.utf8());
        query.bindValue(":COMPILATION", compilation);
        query.bindValue(":YEAR", year);

        if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
        {
            MythContext::DBError("music insert album", query);
            return;
        }
        albumId = query.lastInsertId().toInt();
    }

    // Genres
    unsigned int genreId;
    query.prepare("SELECT genre_id FROM music_genres "
                  "WHERE genre = :GENRE ;");
    query.bindValue(":GENRE", genre.utf8());
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("music select genre id", query);
        return;
    }
    if (query.size() > 0)
    {
        query.next();
        genreId = query.value(0).toInt();
    }
    else
    {
        query.prepare("INSERT INTO music_genres (genre) VALUES (:GENRE);");
        query.bindValue(":GENRE", genre.utf8());

        if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
        {
            MythContext::DBError("music insert genre", query);
            return;
        }
        genreId = query.lastInsertId().toInt();
    }

    // We have all the id's now. We can insert it.
    QString strQuery;
    if (id < 1)
    {
        strQuery = "INSERT INTO music_songs ("
                   " artist_id, album_id,  name,         genre_id,"
                   " year,      track,     length,       filename,"
                   " rating,    format,    date_entered, date_modified ) "
                   "VALUES ("
                   " :ARTIST,   :ALBUM,    :TITLE,       :GENRE,"
                   " :YEAR,     :TRACKNUM, :LENGTH,      :FILENAME,"
                   " :RATING,   :FORMAT,   :DATE_ADD,    :DATE_MOD );";
    }
    else
    {
        strQuery = "UPDATE music_songs SET"
                   "  artist_id = :ARTIST"
                   ", album_id = :ALBUM"
                   ", name = :TITLE"
                   ", genre_id = :GENRE"
                   ", year = :YEAR"
                   ", track = :TRACKNUM"
                   ", length = :LENGTH"
                   ", filename = :FILENAME"
                   ", rating = :RATING"
                   ", format = :FORMAT"
                   ", date_modified = :DATE_MOD "
                   "WHERE song_id= :ID ;";
    }

    query.prepare(strQuery);
    /*
    query.prepare("INSERT INTO music_songs "
                  "  (artist_id, album_id,     name,"
                  "   genre_id,  year,         track,                length,"
                  "   filename,  date_entered, date_modified,"
                  "   format,    size,         bitrate) "
                  "VALUES "
                  "  (:ARTIST,   :ALBUM,       :TITLE,"
                  "   :GENRE,    :YEAR,        :TRACKNUM,            :LENGTH,"
                  "   :FILENAME, :DATE_ADDED,  :DATE_MOD,"
                  "   :FORMAT,   :FILESIZE,    :BITRATE)"
                  );
    */
    query.bindValue(":ARTIST", artistId);
    query.bindValue(":ALBUM", albumId);
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":GENRE", genreId);
    query.bindValue(":YEAR", year);
    query.bindValue(":TRACKNUM", tracknum);
    query.bindValue(":LENGTH", length);
    query.bindValue(":FILENAME", sqlfilename.utf8());
    query.bindValue(":RATING", rating);
    query.bindValue(":FORMAT", format);
    query.bindValue(":DATE_MOD", QDateTime::currentDateTime());

    if (id < 1)
        query.bindValue(":DATE_ADDED",  QDateTime::currentDateTime());
    else
        query.bindValue(":ID", id);

    query.exec();

    if (id < 1 && query.isActive() && 1 == query.numRowsAffected())
        id = query.lastInsertId().toInt();
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

QStringList Metadata::fillFieldList(QString field)
{
    QStringList searchList;
    searchList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    if ("artist" == field || "compilation_artist" == field)
    {
        query.prepare("SELECT artist_name FROM music_artists ORDER BY artist_name;");
    }
    else if ("album" == field)
    {
        query.prepare("SELECT album_name FROM music_albums ORDER BY album_name;");
    }
    else if ("title" == field)
    {
        query.prepare("SELECT name FROM music_songs ORDER BY name;");
    }
    else if ("genre" == field)
    {
        query.prepare("SELECT genre FROM music_genres ORDER BY genre;");
    }
    else
    {
        return searchList;
    }

    if (query.exec() && query.isActive() && query.size())
    {
        while (query.next())
        {
            searchList << QString::fromUtf8(query.value(0).toString());
        }
    }
    return searchList;
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

    QString aquery = "SELECT music_songs.song_id, music_artists.artist_name, music_comp_artists.artist_name AS compilation_artist, "
                     "music_albums.album_name, music_songs.name, music_genres.genre, music_songs.year, "
                     "music_songs.track, music_songs.length, music_songs.filename, "
                     "music_songs.rating, music_songs.numplays, music_songs.lastplay, music_albums.compilation, "
                     "music_songs.format "
                     "FROM music_songs "
                     "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                     "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                     "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
                     "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                     "ORDER BY music_songs.song_id;";

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
                query.value(10).toInt(), //rating
                query.value(11).toInt(), //playcount
                query.value(12).toString(), //lastplay
                (query.value(13).toInt() > 0), //compilation
                query.value(14).toString()); //format

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
