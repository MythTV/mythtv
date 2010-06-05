#ifndef SUBTITLESCREEN_H
#define SUBTITLESCREEN_H

#include "mythscreentype.h"
#include "subtitlereader.h"
#include "NuppelVideoPlayer.h"

class SubtitleScreen : public MythScreenType
{
    static bool InitialiseFont(void);
    static bool Initialise708Fonts(void);

  public:
    SubtitleScreen(NuppelVideoPlayer *player, const char * name);
    virtual ~SubtitleScreen();

    void EnableSubtitles(int type);

    void ClearAllSubtitles(void);
    void ClearNonDisplayedSubtitles(void);
    void ClearDisplayedSubtitles(void);
    void ExpireSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos);

    // MythScreenType methods
    virtual bool Create(void);
    virtual void Pulse(void);

  private:
    void OptimiseDisplayedArea(void);
    void DisplayAVSubtitles(void);
    void DisplayTextSubtitles(void);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void Clear708Cache(int num);
    void Display708Strings(const CC708Window &win, int num,
                           float aspect, vector<CC708String*> &list);
    MythFontProperties* Get708Font(CC708CharacterAttribute attr);

    NuppelVideoPlayer *m_player;
    SubtitleReader    *m_subreader;
    CC608Reader       *m_608reader;
    CC708Reader       *m_708reader;
    QRect              m_safeArea;
    bool               m_useBackground;
    QRegExp            m_removeHTML;
    int                m_subtitleType;
    QHash<MythUIType*, QDateTime> m_expireTimes;
    int                m_708fontSizes[3];
    int                m_708fontZoom;
    bool               m_refreshArea;
    QHash<int,QList<MythUIType*> > m_708imageCache;
};

#endif // SUBTITLESCREEN_H
