#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qmap.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "recordingprofile.h"
#include "fifowriter.h"
#include "programinfo.h"
#include "mythdbcon.h"
#include "transcodedefs.h"

class Transcode : public QObject
{
  public:
    Transcode(ProgramInfo *pginfo);
    ~Transcode();
    int TranscodeFile(char *inputname, char *outputname, QString profileName,
                      bool honorCutList, bool framecontrol, int jobID,
                      QString fifodir);
    void ShowProgress(bool val) { showprogress = val; }
  private:
    bool GetProfile(QString profileName, QString encodingType);
    void ReencoderAddKFA(long curframe, long lastkey, long num_keyframes);
    ProgramInfo *m_proginfo;
    RecordingProfile profile;
    int keyframedist;
    NuppelVideoRecorder *nvr;
    NuppelVideoPlayer *nvp;
    RingBuffer *inRingBuffer;
    RingBuffer *outRingBuffer;
    FIFOWriter::FIFOWriter *fifow;
    QPtrList<struct kfatable_entry> *kfa_table;
    bool showprogress;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
