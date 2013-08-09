#include "recordingprofile.h"
#include "fifowriter.h"
#include "transcodedefs.h"
#include "programtypes.h"
#include "playercontext.h"

class ProgramInfo;
class NuppelVideoRecorder;
class MythPlayer;
class RingBuffer;

typedef vector<struct kfatable_entry> KFATable;

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
        QString fifodir, bool fifo_info, bool cleanCut, frm_dir_map_t &deleteMap,
        int AudioTrackNo, bool passthru = false);
    void ShowProgress(bool val) { showprogress = val; }
    void SetRecorderOptions(QString options) { recorderOptions = options; }
    void SetAVFMode(void) { avfMode = true; }
    void SetHLSMode(void) { hlsMode = true; }
    void SetHLSStreamID(int streamid) { hlsStreamID = streamid; }
    void SetHLSMaxSegments(int segments) { hlsMaxSegments = segments; }
    void SetCMDContainer(QString container) { cmdContainer = container; }
    void SetCMDAudioCodec(QString codec) { cmdAudioCodec = codec; }
    void SetCMDVideoCodec(QString codec) { cmdVideoCodec = codec; }
    void SetCMDHeight(int height) { cmdHeight = height; }
    void SetCMDWidth(int width) { cmdWidth = width; }
    void SetCMDBitrate(int bitrate) { cmdBitrate = bitrate; }
    void SetCMDAudioBitrate(int bitrate) { cmdAudioBitrate = bitrate; }
    void DisableAudioOnlyHLS(void) { hlsDisableAudioOnly = true; }

  private:
    bool GetProfile(QString profileName, QString encodingType, int height,
                    int frameRate);
    void ReencoderAddKFA(long curframe, long lastkey, long num_keyframes);
    void SetPlayerContext(PlayerContext*);
    PlayerContext *GetPlayerContext(void) { return ctx; }
    MythPlayer *GetPlayer(void) { return (ctx) ? ctx->player : NULL; }

  private:
    ProgramInfo            *m_proginfo;
    RecordingProfile       *m_recProfile;
    int                     keyframedist;
    NuppelVideoRecorder    *nvr;
    PlayerContext          *ctx;
    RingBuffer             *outRingBuffer;
    FIFOWriter             *fifow;
    KFATable               *kfa_table;
    bool                    showprogress;
    QString                 recorderOptions;
    bool                    avfMode;
    bool                    hlsMode;
    int                     hlsStreamID;
    bool                    hlsDisableAudioOnly;
    int                     hlsMaxSegments;
    QString                 cmdContainer;
    QString                 cmdAudioCodec;
    QString                 cmdVideoCodec;
    int                     cmdWidth;
    int                     cmdHeight;
    int                     cmdBitrate;
    int                     cmdAudioBitrate;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
