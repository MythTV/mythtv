#ifndef OSD_TYPE_TELETEXT_H_
#define OSD_TYPE_TELETEXT_H_

#include <vector>
#include <map>
using namespace std;

#include <qstring.h>
#include <qrect.h>
#include <qmap.h>
#include <qvaluelist.h>
#include <qobject.h>
#include <qcolor.h>
#include <qmutex.h>

#include "osdtypes.h"
#include "teletextdecoder.h"

class TTFFont;
class OSDType;
class OSDSurface;
class TV;

class TTColor
{
  public:
    static const uint BLACK       = 0;
    static const uint RED         = 1;
    static const uint GREEN       = 2;
    static const uint YELLOW      = 3;
    static const uint BLUE        = 4;
    static const uint MAGENTA     = 5;
    static const uint CYAN        = 6;
    static const uint WHITE       = 7;
    static const uint TRANSPARENT = 8;
};

class TTKey
{
  public:
    static const uint k0            = 0;
    static const uint k1            = 1;
    static const uint k2            = 2;
    static const uint k3            = 3;
    static const uint k4            = 4;
    static const uint k5            = 5;
    static const uint k6            = 6;
    static const uint k7            = 7;
    static const uint k8            = 8;
    static const uint k9            = 9;
    static const uint kNextPage     = 10;
    static const uint kPrevPage     = 11;
    static const uint kNextSubPage  = 12;
    static const uint kPrevSubPage  = 13;
    static const uint kHold         = 14;
    static const uint kTransparent  = 15;
    static const uint kFlofRed      = 16;
    static const uint kFlofGreen    = 17;
    static const uint kFlofYellow   = 18;
    static const uint kFlofBlue     = 19;
    static const uint kFlofWhite    = 20;
    static const uint kRevealHidden = 21;
};

#define TP_SUPPRESS_HEADER  0x01
#define TP_UPDATE_INDICATOR 0x02
#define TP_INTERRUPTED_SEQ  0x04
#define TP_INHIBIT_DISPLAY  0x08
#define TP_MAGAZINE_SERIAL  0x10
#define TP_ERASE_PAGE       0x20
#define TP_NEWSFLASH        0x40
#define TP_SUBTITLE         0x80

class TeletextSubPage
{
  public:
    int pagenum, subpagenum;  ///< the wanted page number
    int lang;                 ///< language code
    int flags;                ///< misc flags
    uint8_t data[25][40];     ///< page data
    int flof;                 ///< page has FastText links
    int floflink[6];          ///< FastText links (FLOF)
    bool subtitle;            ///< page is subtitle page
};

class TeletextPage
{
  public:
    int pagenum;
    int current_subpage;
    std::map<int, TeletextSubPage> subpages;
};

#define MAGAZINE(page) page / 256

class TeletextMagazine
{
  public:
    QMutex lock;
    int current_page;
    int current_subpage;
    std::map<int, TeletextPage> pages;
};

class OSDTypeTeletext : public OSDType, public TeletextViewer
{
    Q_OBJECT
  public:
    OSDTypeTeletext(const QString &name, TTFFont *font,
                    QRect displayrect, float wmult, float hmult);
    OSDTypeTeletext(const OSDTypeTeletext &other)
        : OSDType(other.m_name), TeletextViewer() {}
    virtual ~OSDTypeTeletext() {}


    void Reinit(float wmult, float hmult);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

    void SetForegroundColor(int color);
    void SetBackgroundColor(int color);

    void DrawBackground(int x, int y);
    void DrawRect(int x, int y, int dx, int dy);
    void DrawCharacter(int x, int y, QChar ch, int doubleheight = 0);
    void DrawMosaic(int x, int y, int code, int doubleheight);

    void DrawLine(const uint8_t* page, uint row, int lang);
    void DrawHeader(const uint8_t* page, int lang);
    void DrawPage(void);

    void NewsFlash(void) {};

    void PageUpdated(int page, int subpage);
    void HeaderUpdated(uint8_t* page, int lang);
    void StatusUpdated(void);

    // Teletext Viewer methods
    void KeyPress(uint key);
    void SetDisplaying(bool display) { m_displaying = display; };

    void Reset(void);
    void AddPageHeader(int page, int subpage,
                       const uint8_t *buf, int vbimode, int lang, int flags);
    void AddTeletextData(int magazine, int row,
                         const uint8_t* buf, int vbimode);

  private:
    char CharConversion(char ch, int lang);
    TeletextSubPage *FindSubPage(int page, int subpage, int direction = 0);
    TeletextPage    *FindPage(int page, int direction = 0);

  private:
    QMutex       m_lock;
    QRect        m_displayrect;
    QRect        m_unbiasedrect;

    TTFFont     *m_font;
    OSDSurface  *m_surface;
    OSDTypeBox  *m_box;

    int          m_tt_colspace;
    int          m_tt_rowspace;

    uint8_t      m_bgcolor_y;
    uint8_t      m_bgcolor_u;
    uint8_t      m_bgcolor_v;
    uint8_t      m_bgcolor_a;

    // last fetched page
    int          m_fetchpage;
    int          m_fetchsubpage;

    // currently displayed page:
    int          m_curpage;
    int          m_cursubpage;
    int          m_pageinput[3];
    bool         m_curpage_showheader;
    bool         m_curpage_issubtitle;

    bool         m_transparent;
    bool         m_revealHidden;

    bool         m_displaying;

    TeletextMagazine m_magazines[8];
    unsigned char    bitswap[256];

    static const QColor kColorBlack;
    static const QColor kColorRed;
    static const QColor kColorGreen;
    static const QColor kColorYellow;
    static const QColor kColorBlue;
    static const QColor kColorMagenta;
    static const QColor kColorCyan;
    static const QColor kColorWhite;
    static const int    kTeletextColumns;
    static const int    kTeletextRows;
};

#endif

