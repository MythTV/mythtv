#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qmap.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "recordingprofile.h"
#include "fifowriter.h"
#include "programinfo.h"

#define REENCODE_OK              1
#define REENCODE_CUTLIST_CHANGE -1
#define REENCODE_ERROR           0

class Transcode : public QObject
{
  public:
     Transcode(QSqlDatabase *db, ProgramInfo *pginfo);
     ~Transcode();
     int TranscodeFile(char *inputname, char *outputname,
                              QString profileName,
                              bool honorCutList, bool framecontrol,
                              bool chkTranscodeDB, QString fifodir);
  private:
    bool GetProfile(QString profileName, QString encodingType);
    void ReencoderAddKFA(long curframe, long lastkey, long num_keyframes);
    ProgramInfo *m_proginfo;
    QSqlDatabase *m_db;
    RecordingProfile profile;
    int keyframedist;
    NuppelVideoRecorder *nvr;
    NuppelVideoPlayer *nvp;
    RingBuffer *inRingBuffer;
    RingBuffer *outRingBuffer;
    FIFOWriter::FIFOWriter *fifow;
    QPtrList<struct kfatable_entry> *kfa_table;
};

