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
#include <qsqldatabase.h>
#include <qvaluelist.h>

#include "mfd_plugin.h"
#include "../../mdserver.h"

enum MusicFileLocation
{
    MFL_on_file_system = 0,
    MFL_in_myth_database,
    MFL_both_file_and_database
};
    
typedef QMap <QString, MusicFileLocation> MusicLoadedMap;



class MMusicWatcher: public MFDServicePlugin
{

  public:

    MMusicWatcher(MFD *owner, int identity);
    ~MMusicWatcher();

    void    run();
    bool    checkDataSources(const QString &startdir, QSqlDatabase *a_db);
    bool    sweepMetadata();
    void    buildFileList(QString &directory, MusicLoadedMap &music_files);
    bool    checkNewMusicFile(const QString &filename, const QString &startdir);
    void    prepareNewData();
    bool    doDeltas();

  private:

    bool            first_time;
  
    QTime           metadata_sweep_time;
    bool            force_sweep;
    QMutex          force_sweep_mutex;
    QSqlDatabase    *db;

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

    QValueList<int>     previous_metadata;
    QValueList<int>     previous_playlists;

    bool    sent_directory_warning;
    bool    sent_dir_is_not_dir_warning;
    bool    sent_dir_is_not_readable_warning;
    bool    sent_musicmetadata_table_warning;
    bool    sent_playlist_table_warning;
    bool    sent_database_version_warning;
    
    QString desired_database_version;
    QTime   sweep_timer;
};



#endif  // mmusic_h_
