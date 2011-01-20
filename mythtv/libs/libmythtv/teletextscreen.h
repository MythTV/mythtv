#ifndef TELETEXTSCREEN_H
#define TELETEXTSCREEN_H

#include <QFont>

#include "mythscreentype.h"
#include "teletextreader.h"
#include "mythplayer.h"

class TeletextScreen: public MythScreenType
{
    static bool  InitialiseFont(void);

  public:
    TeletextScreen(MythPlayer *player, const char * name, int fontStretch);
    virtual ~TeletextScreen();

    // MythScreenType methods
    virtual bool Create(void);
    virtual void Pulse(void);

    // TeletextViewer interface methods
    bool KeyPress(const QString &key);
    void SetPage(int page, int subpage);
    void SetDisplaying(bool display);
    void Reset(void);

  private:
    void CleanUp();
    void OptimiseDisplayedArea(void);
    QImage* GetRowImage(int row, QRect &rect);
    void SetForegroundColor(int color);
    void SetBackgroundColor(int color);
    void DrawBackground(int x, int y);
    void DrawRect(int row, const QRect);
    void DrawCharacter(int x, int y, QChar ch, int doubleheight = 0);
    void DrawMosaic(int x, int y, int code, int doubleheight);
    void DrawLine(const uint8_t *page, uint row, int lang);
    void DrawHeader(const uint8_t *page, int lang);
    void DrawStatus(void);
    void DrawPage(void);

    MythPlayer     *m_player;
    TeletextReader *m_teletextReader;
    QRect           m_safeArea;
    int             m_colWidth;
    int             m_rowHeight;
    QColor          m_bgColor;
    bool            m_displaying;
    QHash<int, QImage*> m_rowImages;
    int             m_fontStretch;
    int             m_fontHeight;

  public:
    static const QColor kColorBlack;
    static const QColor kColorRed;
    static const QColor kColorGreen;
    static const QColor kColorYellow;
    static const QColor kColorBlue;
    static const QColor kColorMagenta;
    static const QColor kColorCyan;
    static const QColor kColorWhite;
    static const QColor kColorTransp;
    static const int    kTeletextColumns;
    static const int    kTeletextRows;
};

#endif // TELETEXTSCREEN_H
