#ifndef MDMONITOR_H_
#define MDMONITOR_H_
/*
	mdmonitor.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for threaded object(s) that try and keep
	metdata up to date

*/

#include <qdatetime.h>
#include <qintdict.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qsqldatabase.h>

#include "metadata.h"

class MFD;


enum MusicFileLocation
{
    MFL_on_file_system = 0,
    MFL_in_myth_database
};
    
typedef QMap <QString, MusicFileLocation> MusicLoadedMap;


class MetadataMonitor : public QThread
{
    //
    //  Run in the background, and keep checking for metadata changes.
    //
    
  public:
    
    MetadataMonitor(MFD *l_parent, int l_unique_identifer);

    virtual bool sweepMetadata();

    virtual bool getCurrentGeneration(QIntDict<Metadata>* &metadata_pointer, QIntDict<Playlist>* &playlists_pointer);
    void         run();
    void         stop();
    void         forceSweep();
    void         log(const QString& log_message, int verbosity);
    void         warning(const QString& warning_message);

  protected:
  
    MFD             *parent;
    int             unique_identifier;
    bool            keep_going;      
    QMutex          keep_going_mutex;
    QWaitCondition  main_wait_condition;
    QTime           metadata_sweep_time;
    bool            force_sweep;
    QMutex          force_sweep_mutex;
    QMutex          log_mutex;
    QMutex          warning_mutex;
    bool            first_time;

    QMutex              metadata_mutex;

    QIntDict<Metadata>  *current_metadata;
    QIntDict<Metadata>  *new_metadata;
    QIntDict<Playlist>  *current_playlists;
};



class MetadataMythMusicMonitor : public MetadataMonitor
{
  public:

    MetadataMythMusicMonitor(MFD *l_parent, int unique_identifier, QSqlDatabase *l_db);
    bool sweepMetadata();
    void buildFileList(QString &directory, MusicLoadedMap &music_files);
    bool checkNewMusicFile(const QString &filename);

  private:
  
    QSqlDatabase *db;
    
    
};


#endif
