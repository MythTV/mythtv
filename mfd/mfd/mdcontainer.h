#ifndef MDCONTAINER_H_
#define MDCONTAINER_H_
/*
	mdcontainer.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for metadata container and the thread(s) that fill(s) it

*/

#include <qstring.h>
#include <qsqldatabase.h>
#include <qintdict.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>

#include "metadata.h"

class MFD;

class MetadataMonitor : public QThread
{
    //
    //  Run in the background, and keep checking for metadata changes.
    //
    
  public:
    
    MetadataMonitor(MFD *owner, QSqlDatabase *ldb);

    void run();
    bool sweepMetadata();
    void switchToNextGeneration();
    void forceSweep();
    void stop();
    int  bumpMetadataGeneration();
    int  bumpMetadataItem();
    QIntDict<AudioMetadata>* getCurrent(int *generation_value);

    
    bool sweepAudio();
    bool checkAudio();

    void log(const QString &log_message, int verbosity);
    void warning(const QString &warning_message);

    enum MetadataFileLocation
    {
        MFL_FileSystem,
        MFL_Database,
        MFL_Both
    };
    typedef QMap <QString, MetadataFileLocation> MetadataLoadedMap;

    void buildFileList(QString &directory);
    

  private:
  
    MFD          *parent;
    QSqlDatabase *db;
    bool         keep_going;
    QMutex       keep_going_mutex;    

    QIntDict<AudioMetadata>  *current_metadata;
    QIntDict<AudioMetadata>  *new_metadata;
    
    int     metadata_generation;
    int     current_metadata_generation;
    QMutex  metadata_generation_mutex;   

    int     metadata_item;
    QMutex  metadata_item_mutex;

    QTime   metadata_sweep_time;
    bool    force_sweep;
    QMutex  force_sweep_mutex;
    
    QWaitCondition  main_wait_condition;

    QMutex log_mutex;
    QMutex warning_mutex;
    
    MetadataLoadedMap metadata_files;

};


class MetadataContainer
{
    //
    //  An object that holds metadata
    //


  public:
  
    MetadataContainer(MFD *owner, QSqlDatabase *ldb);
    ~MetadataContainer();

    void    shutDown();

    AudioMetadata*           getMetadata(int *generation_value, int metadata_index);
    QIntDict<AudioMetadata>* getCurrentMetadata();
    void                     lockMetadata();
    void                     unlockMetadata();
    uint                     getCurrentGeneration(){return 3;}  // HACK
    uint                     getMetadataCount();
    uint                     getMP3Count();
    
  private:
  
    MFD                      *parent;
    QSqlDatabase             *db;
    QIntDict<AudioMetadata>  *current_audio_metadata;
    QMutex                   *current_metadata_mutex;
    MetadataMonitor          *metadata_monitor;
    int                      current_generation;

    uint                     mp3_count;
};


#endif
