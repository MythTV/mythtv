#ifndef _CLASSIC_COMMDETECTOR_H_
#define _CLASSIC_COMMDETECTOR_H_

// C++ headers
#include <cstdint>

// Qt headers
#include <QObject>
#include <QMap>
#include <QDateTime>

// MythTV headers
#include "programinfo.h"
#include "mythframe.h"

// Commercial Flagging headers
#include "CommDetectorBase.h"

class MythPlayer;
class LogoDetectorBase;
class SceneChangeDetectorBase;

enum frameMaskValues {
    COMM_FRAME_SKIPPED       = 0x0001,
    COMM_FRAME_BLANK         = 0x0002,
    COMM_FRAME_SCENE_CHANGE  = 0x0004,
    COMM_FRAME_LOGO_PRESENT  = 0x0008,
    COMM_FRAME_ASPECT_CHANGE = 0x0010,
    COMM_FRAME_RATING_SYMBOL = 0x0020
};

class FrameInfoEntry
{
  public:
    int minBrightness;
    int maxBrightness;
    int avgBrightness;
    int sceneChangePercent;
    int aspect;
    int format;
    int flagMask;
    static QString GetHeader(void);
    QString toString(uint64_t frame, bool verbose) const;
};

class ClassicCommDetector : public CommDetectorBase
{
    Q_OBJECT

    public:
        ClassicCommDetector(SkipType commDetectMethod, bool showProgress,
                            bool fullSpeed, MythPlayer* player,
                            QDateTime startedAt_in,
                            QDateTime stopsAt_in,
                            QDateTime recordingStartedAt_in,
                            QDateTime recordingStopsAt_in);
        virtual void deleteLater(void);

        bool go() override; // CommDetectorBase
        void GetCommercialBreakList(frm_dir_map_t &marks) override; // CommDetectorBase
        void recordingFinished(long long totalFileSize) override; // CommDetectorBase
        void requestCommBreakMapUpdate(void) override; // CommDetectorBase

        void PrintFullMap(
            ostream &out, const frm_dir_map_t *comm_breaks,
            bool verbose) const override; // CommDetectorBase

        void logoDetectorBreathe();

        friend class ClassicLogoDetector;

    protected:
        ~ClassicCommDetector() override = default;

    private:
        struct FrameBlock
        {
            long start;
            long end;
            long frames;
            double length;
            int bfCount;
            int logoCount;
            int ratingCount;
            int scCount;
            double scRate;
            int formatMatch;
            int aspectMatch;
            int score;
        };

        void ClearAllMaps(void);
        void GetBlankCommMap(frm_dir_map_t &comms);
        void GetBlankCommBreakMap(frm_dir_map_t &comms);
        void GetSceneChangeMap(frm_dir_map_t &scenes,
                               int64_t start_frame);
        frm_dir_map_t Combine2Maps(
            const frm_dir_map_t &a, const frm_dir_map_t &b) const;
        static void UpdateFrameBlock(FrameBlock *fbp, FrameInfoEntry finfo,
                              int format, int aspect);
        void BuildAllMethodsCommList(void);
        void BuildBlankFrameCommList(void);
        void BuildSceneChangeCommList(void);
        void BuildLogoCommList();
        void MergeBlankCommList(void);
        bool FrameIsInBreakMap(uint64_t f, const frm_dir_map_t &breakMap) const;
        void DumpMap(frm_dir_map_t &map);
        static void CondenseMarkMap(show_map_t &map, int spacing, int length);
        static void ConvertShowMapToCommMap(
            frm_dir_map_t &out, const show_map_t &in);
        void CleanupFrameInfo(void);
        void GetLogoCommBreakMap(show_map_t &map);

        SkipType m_commDetectMethod;
        frm_dir_map_t m_lastSentCommBreakMap;
        bool m_commBreakMapUpdateRequested {false};
        bool m_sendCommBreakMapUpdates     {false};

        int m_commDetectBorder             {0};
        int m_commDetectBlankFrameMaxDiff  {25};
        int m_commDetectDarkBrightness     {80};
        int m_commDetectDimBrightness      {120};
        int m_commDetectBoxBrightness      {30};
        int m_commDetectDimAverage         {35};
        int m_commDetectMaxCommBreakLength {395};
        int m_commDetectMinCommBreakLength {60};
        int m_commDetectMinShowLength      {65};
        int m_commDetectMaxCommLength      {125};
        bool m_commDetectBlankCanHaveLogo  {true};

        bool m_verboseDebugging            {false};

        long long m_lastFrameNumber        {0};
        long long m_curFrameNumber         {0};

        int m_width                        {0};
        int m_height                       {0};
        int m_horizSpacing                 {0};
        int m_vertSpacing                  {0};
        bool m_blankFramesOnly             {false};
        int m_blankFrameCount              {0};
        int m_currentAspect                {0};


        int m_totalMinBrightness           {0};

        bool m_logoInfoAvailable           {false};
        LogoDetectorBase* m_logoDetector   {nullptr};

        frm_dir_map_t m_blankFrameMap;
        frm_dir_map_t m_blankCommMap;
        frm_dir_map_t m_blankCommBreakMap;
        frm_dir_map_t m_sceneMap;
        frm_dir_map_t m_sceneCommBreakMap;
        frm_dir_map_t m_commBreakMap;
        frm_dir_map_t m_logoCommBreakMap;

        bool m_frameIsBlank                {false};
        bool m_stationLogoPresent          {false};

        bool m_decoderFoundAspectChanges   {false};

        SceneChangeDetectorBase* m_sceneChangeDetector {nullptr};

protected:
        MythPlayer *m_player               {nullptr};
        QDateTime m_startedAt;
        QDateTime m_stopsAt;
        QDateTime m_recordingStartedAt;
        QDateTime m_recordingStopsAt;
        bool m_aggressiveDetection         {false};
        bool m_stillRecording              {false};
        bool m_fullSpeed                   {false};
        bool m_showProgress                {false};
        double m_fps                       {0.0};
        uint64_t m_framesProcessed         {0};
        long long m_preRoll                {0};
        long long m_postRoll               {0};


        void Init();
        void SetVideoParams(float aspect);
        void ProcessFrame(VideoFrame *frame, long long frame_number);
        QMap<long long, FrameInfoEntry> m_frameInfo;

public slots:
        void sceneChangeDetectorHasNewInformation(unsigned int framenum, bool isSceneChange,float debugValue);
};

#endif


/* vim: set expandtab tabstop=4 shiftwidth=4: */
