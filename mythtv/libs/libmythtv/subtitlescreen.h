#ifndef SUBTITLESCREEN_H
#define SUBTITLESCREEN_H

#include <QFont>

#include "mythscreentype.h"
#include "subtitlereader.h"
#include "mythplayer.h"

class SubtitleScreen : public MythScreenType
{
    static bool InitialiseFont(int fontStretch = QFont::Unstretched);
    static bool Initialise708Fonts(int fontStretch = QFont::Unstretched);

  public:
    SubtitleScreen(MythPlayer *player, const char * name, int fontStretch);
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
    void DisplayRawTextSubtitles(void);
    void OptimiseTextSubs(QStringList &list);
    void DrawTextSubtitles(QStringList &wrappedsubs, uint64_t start,
                           uint64_t duration);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void Clear708Cache(int num);
    void Display708Strings(const CC708Window &win, int num,
                           float aspect, vector<CC708String*> &list);
    MythFontProperties* Get708Font(CC708CharacterAttribute attr);

    MythPlayer *m_player;
    SubtitleReader    *m_subreader;
    CC608Reader       *m_608reader;
    CC708Reader       *m_708reader;
    QRect              m_safeArea;
    bool               m_useBackground;
    QRegExp            m_removeHTML;
    int                m_subtitleType;
    QHash<MythUIType*, long long> m_expireTimes;
    int                m_708fontSizes[3];
    int                m_textFontZoom;
    bool               m_refreshArea;
    QHash<int,QList<MythUIType*> > m_708imageCache;
    int                m_fontStretch;
};

#endif // SUBTITLESCREEN_H
