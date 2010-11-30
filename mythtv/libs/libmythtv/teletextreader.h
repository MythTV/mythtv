#ifndef TELETEXTREADER_H
#define TELETEXTREADER_H

#include <stdint.h>
#include <map>

#include <QString>
#include <QMutex>

using namespace std;

typedef enum
{
    kTTColorBlack       = 0,
    kTTColorRed         = 1,
    kTTColorGreen       = 2,
    kTTColorYellow      = 3,
    kTTColorBlue        = 4,
    kTTColorMagenta     = 5,
    kTTColorCyan        = 6,
    kTTColorWhite       = 7,
    kTTColorTransparent = 8,
} TTColor;

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

class TeletextMagazine
{
  public:
    mutable QMutex    lock;
    int               current_page;
    int               current_subpage;
    TeletextSubPage   loadingpage;
    int_to_page_t     pages;
};

class TeletextReader
{
  public:
    TeletextReader();
   ~TeletextReader();

    // OSD/Player methods
    void Reset(void);
    void KeyPress(uint key);
    QString GetPage(void);
    void SetPage(int page, int subpage);
    void SetSubPage(int subpage)        { m_cursubpage = subpage;      }
    bool PageChanged(void)              { return m_page_changed;       }
    void SetPageChanged(bool changed)   { m_page_changed = changed;    }
    void SetShowHeader(bool show)       { m_curpage_showheader = show; }
    void SetHeaderChanged(bool changed) { m_header_changed = changed;  }
    bool IsSubtitle(void)               { return m_curpage_issubtitle; }
    void SetIsSubtitle(bool sub)        { m_curpage_issubtitle = sub;  }
    bool IsTransparent(void)            { return m_transparent;        }
    bool RevealHidden(void)             { return m_revealHidden;       }
    int  GetPageInput(uint num)         { return m_pageinput[num];     }
    TeletextSubPage* FindSubPage(void)
        { return FindSubPage(m_curpage, m_cursubpage); }
    uint8_t* GetHeader(void)            { return m_header;             }

    // Decoder methods
    void AddPageHeader(int page, int subpage, const uint8_t *buf,
                       int vbimode, int lang, int flags);
    void AddTeletextData(int magazine, int row,
                         const uint8_t* buf, int vbimode);


  private:
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

    mutable int      m_curpage;
    mutable int      m_cursubpage;
    mutable bool     m_curpage_showheader;
    mutable bool     m_curpage_issubtitle;
    int              m_pageinput[3];
    bool             m_transparent;
    bool             m_revealHidden;
    uint8_t          m_header[40];
    mutable bool     m_header_changed;
    mutable bool     m_page_changed;
    TeletextMagazine m_magazines[8];
    unsigned char    m_bitswap[256];
    int              m_fetchpage;
    int              m_fetchsubpage;
};

#endif // TELETEXTREADER_H
