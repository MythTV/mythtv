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
    virtual ~TeletextReader();

    // OSD/Player methods
    void Reset(void);
    bool KeyPress(const QString &key);
    QString GetPage(void);
    void SetPage(int page, int subpage);
    void SetSubPage(int subpage)        { m_cursubpage = subpage;      }
    bool PageChanged(void) const        { return m_page_changed;       }
    void SetPageChanged(bool changed)   { m_page_changed = changed;    }
    void SetShowHeader(bool show)       { m_curpage_showheader = show; }
    void SetHeaderChanged(bool changed) { m_header_changed = changed;  }
    bool IsSubtitle(void) const         { return m_curpage_issubtitle; }
    void SetIsSubtitle(bool sub)        { m_curpage_issubtitle = sub;  }
    bool IsTransparent(void) const      { return m_transparent;        }
    bool RevealHidden(void) const       { return m_revealHidden;       }
    int  GetPageInput(uint num) const   { return m_pageinput[num];     }
    TeletextSubPage* FindSubPage(void)
        { return FindSubPage(m_curpage, m_cursubpage); }
    uint8_t* GetHeader(void)            { return m_header;             }

    // Decoder methods
    void AddPageHeader(int page, int subpage, const uint8_t *buf,
                       int vbimode, int lang, int flags);
    void AddTeletextData(int magazine, int row,
                         const uint8_t* buf, int vbimode);

  protected:
    void NewsFlash(void) {};
    virtual void PageUpdated(int page, int subpage);
    virtual void HeaderUpdated(
        int page, int subpage, uint8_t *page_ptr, int lang);

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
