#ifndef MFDINTERFACE_H_
#define MFDINTERFACE_H_
/*
	mfdinterface.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <qobject.h>
#include <qptrlist.h>
#include <qintdict.h>

#include <mythtv/uilistbtntype.h>
#include "mfdcontent.h"

class DiscoveryThread;
class MfdInstance;

class MfdInterface : public QObject
{

  Q_OBJECT

  public:

    MfdInterface();
    ~MfdInterface();

    //
    //  Methods that the linking client application can call to make an mfd
    //  do something
    //

    void playAudio(int which_mfd, int container, int type, int which_id, int index=0);
    void stopAudio(int which_mfd);
    void pauseAudio(int which_mfd, bool on_or_off);
    void seekAudio(int which_mfd, int how_much);
    void nextAudio(int which_mfd);
    void prevAudio(int which_mfd);
    void askForStatus(int which_mfd);
    
    
  signals:

    //
    //  Signals we send out when something happens. Linking client wants to
    //  connect to all or some of these
    //

    void mfdDiscovery(int, QString, QString, bool);
    void audioPluginDiscovery(int);
    void audioPaused(int, bool);
    void audioStopped(int);
    void audioPlaying(int, int, int, int, int, int, int, int, int);
    void metadataChanged(int, MfdContentCollection*);

  protected:
  
    void customEvent(QCustomEvent *ce);
 

  private:

    int             bumpMfdId(){ ++mfd_id_counter; return mfd_id_counter;}
    MfdInstance*    findMfd(
                            const QString &a_host,
                            const QString &an_ip_addesss,
                            int a_port
                           );
    void            swapMetadata(int which_mfd, MfdContentCollection *new_collection);
  
    DiscoveryThread       *discovery_thread;
    QPtrList<MfdInstance> *mfd_instances;
    int                   mfd_id_counter;
    
    QIntDict<MfdContentCollection>   mfd_collections;
};

#endif
