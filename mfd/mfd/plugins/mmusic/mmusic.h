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
    MFL_in_myth_database
};
    
typedef QMap <QString, MusicFileLocation> MusicLoadedMap;



class MMusicWatcher: public MFDServicePlugin
{

  public:

    MMusicWatcher(MFD *owner, int identity);
    ~MMusicWatcher();

    void    run();
    bool    sweepMetadata();
    void    buildFileList(QString &directory, MusicLoadedMap &music_files);
    bool    checkNewMusicFile(const QString &filename, const QString &startdir);

  private:
  
    QTime           metadata_sweep_time;
    bool            first_time;
    bool            force_sweep;
    QMutex          force_sweep_mutex;
    QSqlDatabase    *db;

    QIntDict<Metadata>  *current_metadata;
    QIntDict<Metadata>  *new_metadata;
    QIntDict<Playlist>  *current_playlists;
    
    MetadataServer      *metadata_server;
    MetadataContainer   *metadata_container;
    int                  container_id;

    QValueList<QString> files_to_ignore;

    QValueList<int>     metadata_additions;
    QValueList<int>     metadata_deletions;

};



#endif  // mmusic_h_
