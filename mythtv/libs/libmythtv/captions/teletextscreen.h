#ifndef TELETEXTSCREEN_H
#define TELETEXTSCREEN_H

#include <QFont>

#include "libmythui/mythscreentype.h"

#include "captions/teletextreader.h"
#include "mythplayer.h"

class TeletextScreen: public MythScreenType
{
    Q_OBJECT

    static bool  InitialiseFont();

  public:
    TeletextScreen(MythPlayer* Player, MythPainter* Painter, const QString& Name, int FontStretch);
    ~TeletextScreen() override;

    bool Create() override;
    void Pulse() override;

    // TeletextViewer interface methods
    bool KeyPress(const QString& Key, bool& Exit);
    void SetPage(int page, int subpage);
    void SetDisplaying(bool display);
    void Reset() override;
    void ClearScreen();

  private:
    void OptimiseDisplayedArea();
    QImage* GetRowImage(int row, QRect &rect);
    static void SetForegroundColor(int color);
    void SetBackgroundColor(int color);
    void DrawBackground(int x, int y);
    void DrawRect(int row, QRect rect);
    void DrawCharacter(int x, int y, QChar ch, bool doubleheight = false);
    void DrawMosaic(int x, int y, int code, bool doubleheight);
    void DrawLine(const tt_line_array& page, uint row, int lang);
    void DrawHeader(const tt_line_array &page, int lang);
    void DrawStatus();
    void DrawPage();

    MythPlayer*     m_player         {nullptr};
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
