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
    bool Create(void) override; // MythScreenType
    void Pulse(void) override; // MythUIType

    // TeletextViewer interface methods
    bool KeyPress(const QString &key);
    void SetPage(int page, int subpage);
    void SetDisplaying(bool display);
    void Reset(void) override; // MythUIType
    void ClearScreen(void);

  private:
    void OptimiseDisplayedArea(void);
    QImage* GetRowImage(int row, QRect &rect);
    static void SetForegroundColor(int color);
    void SetBackgroundColor(int color);
    void DrawBackground(int x, int y);
    void DrawRect(int row, const QRect);
    void DrawCharacter(int x, int y, QChar ch, bool doubleheight = false);
    void DrawMosaic(int x, int y, int code, bool doubleheight);
    void DrawLine(const uint8_t *page, uint row, int lang);
    void DrawHeader(const uint8_t *page, int lang);
    void DrawStatus(void);
    void DrawPage(void);

    MythPlayer     *m_player         {nullptr};
    TeletextReader *m_teletextReader {nullptr};
    QRect           m_safeArea;
    int             m_colWidth       {10};
    int             m_rowHeight      {10};
    QColor          m_bgColor        {kColorBlack};
    bool            m_displaying     {false};
    QHash<int, QImage*> m_rowImages;
    int             m_fontStretch;
    int             m_fontHeight     {10};

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
