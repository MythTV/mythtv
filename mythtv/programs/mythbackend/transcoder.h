#ifndef TRANSCODER_H_
#define TRANSCODER_H_

class ProgramInfo;
class QSqlDatabase;
class EncoderLink;

#include <qmap.h>
#include <list>
#include <vector>
#include <qobject.h>

using namespace std;

class Transcoder : public QObject
{
  public:
     Transcoder(QMap<int, EncoderLink *> *tvList, QSqlDatabase *ldb);
     ~Transcoder(void);
     void customEvent(QCustomEvent *e);

  protected:
    static void *TranscodePollThread(void *param);
    void TranscodePoll(void);

  private:
    void EnqueueTranscode(ProgramInfo *pginfo);
    void ClearTranscodeTable(bool skipPartial);
    void RestartTranscoding();
    pid_t Transcode(ProgramInfo *pginfo);
    void InitTranscoder(ProgramInfo *pginfo);
    void CheckDoneTranscoders(void);
    bool isFileInUse(ProgramInfo *pginfo);

    pthread_mutex_t transqlock;

    bool transcodePoll;
    int maxTranscoders;
    int useCutlist;

    QMap<int, EncoderLink *> *m_tvList;
    QSqlDatabase *db_conn;

    QPtrList <ProgramInfo> TranscodeQueue;
    QValueList <pid_t> Transcoders;

    pthread_t transpoll;
};

#endif

