#ifndef TRANSCODER_H_
#define TRANSCODER_H_

class ProgramInfo;
class QSqlDatabase;
class EncoderLink;

#include <qmap.h>
#include <qmutex.h>
#include <list>
#include <vector>
#include <qobject.h>

using namespace std;

struct TranscodeData
{
    ProgramInfo *pinfo;
    bool useCutlist;
};

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
    struct TranscodeData *CheckTranscodeTable(bool skipPartial);
    pid_t Transcode(ProgramInfo *pginfo, bool useCutlist);
    void EnqueueTranscode(ProgramInfo *pginfo, bool useCutlist);
    void UpdateTranscoder(ProgramInfo *pginfo, int status);
    void StopTranscoder(ProgramInfo *pinfo);
    void DeleteTranscode(ProgramInfo *pinfo);
    void CheckDoneTranscoders(void);
    void importMapFile(ProgramInfo *pinfo, QString mapfile);
    bool isFileInUse(ProgramInfo *pginfo);

    bool transcodePoll;

    QMap<int, EncoderLink *> *m_tvList;
    QMutex *dblock;
    QSqlDatabase *db_conn;

    QValueList <pid_t> Transcoders;

    pthread_t transpoll;
};

#endif

