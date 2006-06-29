#ifndef OSD_TYPE_TELETEXT_H_
#define OSD_TYPE_TELETEXT_H_

#include <vector>
#include <map>
using namespace std;

#include <qstring.h>
#include <qrect.h>
#include <qmap.h>
#include <qcolor.h>
#include <qmutex.h>

#include "osdtypes.h"
#include "teletextdecoder.h"

class TTFFont;
class OSD;
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
    int pagenum;              ///< the wanted page
    int subpagenum;           ///< the wanted subpage
    int lang;                 ///< language code
    int flags;                ///< misc flags
    uint8_t data[25][40];     ///< page data
    int flof;                 ///< page has FastText links
    int floflink[6];          ///< FastText links (FLOF)
    bool subtitle;            ///< page is subtitle page
    bool active;              ///< data has arrived since page last cleared
};
typedef map<int, TeletextSubPage> int_to_subpage_t;

class TeletextPage
{
  public:
    int               pagenum;
    int               current_subpage;
    int_to_subpage_t  subpages;
};
typedef map<int, TeletextPage> int_to_page_t;

#define MAGAZINE(page) (page / 256)

class TeletextMagazine
{
  public:
    mutable QMutex    lock;
    int               current_page;
    int               current_subpage;
    TeletextSubPage   loadingpage;
    int_to_page_t     pages;
};

class OSDTypeTeletext : public OSDType, public TeletextViewer
{
    Q_OBJECT
    friend QColor color_tt2qt(int ttcolor);
  public:
    OSDTypeTeletext(const QString &name, TTFFont *font,
                    QRect displayrect, float wmult, float hmult, OSD *osd);
    OSDTypeTeletext(const OSDTypeTeletext &other)
        : OSDType(other.m_name), TeletextViewer() {}
    virtual ~OSDTypeTeletext() {}

    void Reinit(float wmult, float hmult);

    // Teletext Drawing methods
    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

    // TeletextViewer interface methods
    void KeyPress(uint key);
    void SetPage(int page, int subpage);
    void SetDisplaying(bool display) { m_displaying = display; };

    void Reset(void);
    void AddPageHeader(int page, int subpage,
                       const uint8_t *buf, int vbimode, int lang, int flags);
    void AddTeletextData(int magazine, int row,
                         const uint8_t* buf, int vbimode);

  private:
    // Internal Teletext Sets
    void SetForegroundColor(int color) const;
    void SetBackgroundColor(int color) const;

    // Teletext Drawing methods
    void DrawBackground(OSDSurface *surface, int x, int y) const;
    void DrawRect(OSDSurface *surface, const QRect) const;
    void DrawCharacter(OSDSurface *surface, int x, int y,
                       QChar ch, int doubleheight = 0) const;
    void DrawMosaic(OSDSurface *surface, int x, int y,
                    int code, int doubleheight) const;
    void DrawLine(OSDSurface *surface, const uint8_t *page,
                  uint row, int lang) const;
    void DrawHeader(OSDSurface *surface, const uint8_t *page, int lang) const;
    void DrawStatus(OSDSurface *surface) const;
    void DrawPage(OSDSurface *surface) const;

    // Internal Teletext Commands
    void NewsFlash(void) {};
    void PageUpdated(int page, int subpage);
    void HeaderUpdated(uint8_t *page, int lang);

    const TeletextSubPage *FindSubPage(int page, int subpage, int dir=0) const
        { return FindSubPageInternal(page, subpage, dir); }
    TeletextSubPage       *FindSubPage(int page, int subpage, int dir = 0)
        { return (TeletextSubPage*) FindSubPageInternal(page, subpage, dir); }

    const TeletextPage    *FindPage(int page, int dir = 0) const
        { return (TeletextPage*) FindPageInternal(page, dir); }
    TeletextPage          *FindPage(int page, int dir = 0)
        { return (TeletextPage*) FindPageInternal(page, dir); }

    const TeletextSubPage *FindSubPageInternal(int,int,int) const;
    const TeletextPage    *FindPageInternal(int,int) const;

  private:
    QMutex       m_lock;
    QRect        m_displayrect;
    QRect        m_unbiasedrect;

    OSDTypeBox  *m_box;

    int          m_tt_colspace;
    int          m_tt_rowspace;

    // last fetched page
    int          m_fetchpage;
    int          m_fetchsubpage;

    // needs to mutable so we can change color character by character.
    mutable TTFFont *m_font;

    // current character background
    mutable uint8_t  m_bgcolor_y;
    mutable uint8_t  m_bgcolor_u;
    mutable uint8_t  m_bgcolor_v;
    mutable uint8_t  m_bgcolor_a;

    // currently displayed page:
    mutable int  m_curpage;
    mutable int  m_cursubpage;
    mutable bool m_curpage_showheader;
    mutable bool m_curpage_issubtitle;

    int          m_pageinput[3];

    bool         m_transparent;
    bool         m_revealHidden;

    bool         m_displaying;

    OSD         *m_osd;
    uint8_t      m_header[40];
    mutable bool m_header_changed;
    mutable bool m_page_changed;

    TeletextMagazine m_magazines[8];
    unsigned char    m_bitswap[256];

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

