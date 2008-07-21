


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
    int TranscodeFile(
        const QString &inputname,
        const QString &outputname,
        const QString &profileName,
        bool honorCutList, bool framecontrol, int jobID,
        QString fifodir, QMap<long long, int> deleteMap);
    void ShowProgress(bool val) { showprogress = val; }
    void SetRecorderOptions(QString options) { recorderOptions = options; }
  private:
    bool GetProfile(QString profileName, QString encodingType, int height,
                    int frameRate);
    void ReencoderAddKFA(long curframe, long lastkey, long num_keyframes);
    ProgramInfo *m_proginfo;
    RecordingProfile profile;
    int keyframedist;
    NuppelVideoRecorder *nvr;
    NuppelVideoPlayer *nvp;
    RingBuffer *inRingBuffer;
    RingBuffer *outRingBuffer;
    FIFOWriter::FIFOWriter *fifow;
    Q3PtrList<struct kfatable_entry> *kfa_table;
    bool showprogress;

    QString recorderOptions;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
