#ifndef MMUSIC_H_
#define MMUSIC_H_
/*
	mmusic.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	stay on top of mythmusic data

*/

#include <qdatetime.h>
#include <qmutex.h>
#include <qvaluelist.h>

#include "mfd_plugin.h"
#include "../../mdserver.h"


class MusicFile
{

  public:

    MusicFile();
    MusicFile(const QString &a_filename, QDateTime last_modified);

    bool      calculateMythDigest();    
    QDateTime lastModified(){ return last_modification; }    
    bool      checkedDatabase(){return checked_database;}
    void      checkedDatabase(bool y_or_n){ checked_database = y_or_n; }
    QString   getMythDigest(){ return myth_digest;}

    void      setMetadataId(int an_int){ metadata_id = an_int; }
    int       getMetadataId(){return metadata_id; }

    void      setDbId(int an_int){ database_id = an_int; }
    int       getDbId(){return database_id; }

    void      setMythDigest(QString a_digest){ myth_digest = a_digest; }

  private:
  
    QString     file_name;
    QDateTime   last_modification;
    QString     myth_digest;
    bool        checked_database;    
    int         metadata_id;
    int         database_id;
};

typedef QMap <QString, MusicFile> MusicFileMap;

class MMusicWatcher: public MFDServicePlugin
{

  public:

    MMusicWatcher(MFD *owner, int identity);
    ~MMusicWatcher();

    void            initialize();
    void            run();
    bool            sweepMetadata();
    void            checkForDeletions(MusicFileMap &music_files, const QString &startdir);
    bool            checkDataSources(const QString &startdir);
    void            removeAllMetadata();
    void            buildFileList(const QString &directory, MusicFileMap &music_files);
    void            compareToMasterList(MusicFileMap &music_files, const QString &startdir);
    void            checkDatabaseAgainstMaster(const QString &startdir);
    AudioMetadata*  loadFromDatabase(const QString &file_name, const QString &startdir);
    AudioMetadata*  checkNewFile(const QString &file_name, const QString &startdir);
    QDateTime       getQtTimeFromMySqlTime(QString timestamp);
    bool            updateMetadata(AudioMetadata* an_item);
    AudioMetadata*  getMetadataFromFile(QString file_path);
    void            persistMetadata(AudioMetadata* an_item);
    void            loadPlaylists();
    void            handleMetadataChange(int which_collection, bool external);
    void            possiblySaveToDb();
    void            persistPlaylist(Playlist *a_playlist);
    
  private:

    int             bumpMetadataId();
  
    QTime           metadata_sweep_time;
    bool            force_sweep;
    QMutex          force_sweep_mutex;

    QIntDict<Metadata>  *new_metadata;
    QIntDict<Playlist>  *new_playlists;
    
    MetadataServer      *metadata_server;
    MetadataContainer   *metadata_container;
    int                 container_id;

    QValueList<QString> files_to_ignore;

    QValueList<int>     metadata_additions;
    QValueList<int>     metadata_deletions;
    
    QValueList<int>     playlist_additions;
    QValueList<int>     playlist_deletions;

    bool    sent_directory_warning;
    bool    sent_dir_is_not_dir_warning;
    bool    sent_dir_is_not_readable_warning;
    bool    sent_musicmetadata_table_warning;
    bool    sent_playlist_table_warning;
    bool    sent_database_version_warning;
    
    QString desired_database_version;
    QTime   sweep_timer;

    MusicFileMap    master_list;
    MusicFileMap    latest_sweep;

    QMutex  current_metadata_id_mutex;
    int     current_metadata_id;

    bool    first_sweep_done;
};



#endif  // mmusic_h_
