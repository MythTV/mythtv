#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <pthread.h>

#include <list>
#include <vector>

#include <qmap.h> 
#include <qmutex.h>
#include <qobject.h>

using namespace std;
class ProgramInfo;
class EncoderLink;
typedef vector<ProgramInfo*> pginfolist_t;
typedef vector<EncoderLink*> enclinklist_t;

class AutoExpire : public QObject
{
  public:
    AutoExpire(bool runthread, bool master);
   ~AutoExpire();

    void CalcParams(vector<EncoderLink*>);
    void FillExpireList();
    void PrintExpireList();

    static void Update(QMap<int, EncoderLink*>*, bool immediately);
  protected:
    void RunExpirer(void);
    static void *ExpirerThread(void *param);

  private:
    void ExpireEpisodesOverMax(void);

    void FillDBOrdered(int expMethod);
    void SendDeleteMessages(size_t, size_t);
    void ClearExpireList(void);
    void Sleep();

    // main expire info
    pginfolist_t  expire_list;
    pthread_t     expire_thread;
    QMutex        instance_lock;
    QString       record_file_prefix;
    size_t        desired_space;
    uint          desired_freq;
    bool          expire_thread_running;
    bool          is_master_backend;
    bool          disable_expire;

    // update info
    bool          update_pending;
    pthread_t     update_thread;
    enclinklist_t encoder_links;

    friend void *SpawnUpdateThread(void *param);
};

#endif
