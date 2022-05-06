// MythTV
#include "libmythbase/programtypes.h"
#include "libmythtv/io/mythfifowriter.h"
#include "libmythtv/playercontext.h"
#include "libmythtv/recordingprofile.h"

// MythTranscode
#include "transcodedefs.h"

class ProgramInfo;
class NuppelVideoRecorder;
class MythPlayer;
class MythMediaBuffer;

using KFATable = std::vector<struct kfatable_entry>;

class Transcode : public QObject
{
  public:
    explicit Transcode(ProgramInfo *pginfo);
    ~Transcode() override;
    int TranscodeFile(
        const QString &inputname,
        const QString &outputname,
        const QString &profileName,
        bool honorCutList, bool framecontrol, int jobID,
        const QString& fifodir, bool fifo_info, bool cleanCut, frm_dir_map_t &deleteMap,
        int AudioTrackNo, bool passthru = false);
    void ShowProgress(bool val) { m_showProgress = val; }
    void SetRecorderOptions(const QString& options) { m_recorderOptions = options; }
    void SetAVFMode(void) { m_avfMode = true; }
    void SetHLSMode(void) { m_hlsMode = true; }
    void SetHLSStreamID(int streamid) { m_hlsStreamID = streamid; }
    void SetHLSMaxSegments(int segments) { m_hlsMaxSegments = segments; }
    void SetCMDContainer(const QString& container) { m_cmdContainer = container; }
    void SetCMDAudioCodec(const QString& codec) { m_cmdAudioCodec = codec; }
    void SetCMDVideoCodec(const QString& codec) { m_cmdVideoCodec = codec; }
    void SetCMDHeight(int height) { m_cmdHeight = height; }
    void SetCMDWidth(int width) { m_cmdWidth = width; }
    void SetCMDBitrate(int bitrate) { m_cmdBitrate = bitrate; }
    void SetCMDAudioBitrate(int bitrate) { m_cmdAudioBitrate = bitrate; }
    void DisableAudioOnlyHLS(void) { m_hlsDisableAudioOnly = true; }

  private:
    bool GetProfile(const QString& profileName, const QString& encodingType, int height,
                    int frameRate);
    void ReencoderAddKFA(long curframe, long lastkey, long num_keyframes);
    void SetPlayerContext(PlayerContext* player_ctx);
    PlayerContext *GetPlayerContext(void) { return m_ctx; }
    MythPlayer *GetPlayer(void) { return (m_ctx) ? m_ctx->m_player : nullptr; }

  private:
    ProgramInfo         *m_proginfo            { nullptr };
    RecordingProfile    *m_recProfile          { nullptr };
    int                  m_keyframeDist        { 30 };
#if CONFIG_LIBMP3LAME
    NuppelVideoRecorder *m_nvr                 { nullptr };
#endif
    PlayerContext       *m_ctx                 { nullptr };
    MythMediaBuffer     *m_outBuffer           { nullptr };
    MythFIFOWriter      *m_fifow               { nullptr };
    KFATable            *m_kfaTable            { nullptr };
    bool                 m_showProgress        { false };
    QString              m_recorderOptions;
    bool                 m_avfMode             { false };
    bool                 m_hlsMode             { false };
    int                  m_hlsStreamID         { -1 };
    bool                 m_hlsDisableAudioOnly { false };
    int                  m_hlsMaxSegments      { 0 };
    QString              m_cmdContainer        { "mpegts" };
    QString              m_cmdAudioCodec       { "aac" };
    QString              m_cmdVideoCodec       { "libx264" };
    int                  m_cmdWidth            { 480 };
    int                  m_cmdHeight           { 0 };
    int                  m_cmdBitrate          { 600000 };
    int                  m_cmdAudioBitrate     { 64000 };
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
