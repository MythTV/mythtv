// -*- Mode: c++ -*-

#ifndef TELETEXTREADER_H
#define TELETEXTREADER_H

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include <QString>
#include <QMutex>

enum TTColor : std::uint8_t
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
};

static constexpr uint8_t TP_SUPPRESS_HEADER  { 0x01 };
static constexpr uint8_t TP_UPDATE_INDICATOR { 0x02 };
static constexpr uint8_t TP_INTERRUPTED_SEQ  { 0x04 };
static constexpr uint8_t TP_INHIBIT_DISPLAY  { 0x08 };
static constexpr uint8_t TP_MAGAZINE_SERIAL  { 0x10 };
static constexpr uint8_t TP_ERASE_PAGE       { 0x20 };
static constexpr uint8_t TP_NEWSFLASH        { 0x40 };
static constexpr uint8_t TP_SUBTITLE         { 0x80 };

using tt_line_array = std::array<uint8_t,40>;

class TeletextSubPage
{
  public:
    int pagenum;              ///< the wanted page
    int subpagenum;           ///< the wanted subpage
    int lang;                 ///< language code
    int flags;                ///< misc flags
    std::array<tt_line_array,25> data;   ///< page data
    int flof;                 ///< page has FastText links
    std::array<int,6> floflink; ///< FastText links (FLOF)
    bool subtitle;            ///< page is subtitle page
    bool active;              ///< data has arrived since page last cleared
};

using int_to_subpage_t = std::map<int, TeletextSubPage>;

class TeletextPage
{
  public:
    int               pagenum         {0};
    int               current_subpage {0};
    int_to_subpage_t  subpages;
};
using int_to_page_t = std::map<int, TeletextPage>;

class TeletextMagazine
{
  public:
    TeletextMagazine() = default;
   ~TeletextMagazine() { delete lock; }
    QMutex*           lock            { new QMutex };
    int               current_page    {0};
    int               current_subpage {0};
    TeletextSubPage   loadingpage     {};
    int_to_page_t     pages;
};

class TeletextReader
{
  public:
    TeletextReader();
    virtual ~TeletextReader() = default;

    // OSD/Player methods
    void Reset(void);
    bool KeyPress(const QString& Key, bool& Exit);
    QString GetPage(void);
    void SetPage(int page, int subpage);
    void SetSubPage(int subpage)        { m_cursubpage = subpage;      }
    bool PageChanged(void) const        { return m_pageChanged;        }
    void SetPageChanged(bool changed)   { m_pageChanged = changed;     }
    void SetShowHeader(bool show)       { m_curpageShowHeader = show;  }
    void SetHeaderChanged(bool changed) { m_headerChanged = changed;   }
    bool IsSubtitle(void) const         { return m_curpageIsSubtitle;  }
    void SetIsSubtitle(bool sub)        { m_curpageIsSubtitle = sub;   }
    bool IsTransparent(void) const      { return m_transparent;        }
    bool RevealHidden(void) const       { return m_revealHidden;       }
    int  GetPageInput(uint num) const   { return m_pageinput[num];     }
    TeletextSubPage* FindSubPage(void)
        { return FindSubPage(m_curpage, m_cursubpage); }
    tt_line_array GetHeader(void)       { return m_header;             }

    // Decoder methods
    void AddPageHeader(int page, int subpage, const uint8_t *buf,
                       int vbimode, int lang, int flags);
    void AddTeletextData(int magazine, int row,
                         const uint8_t* buf, int vbimode);

  protected:
    virtual void PageUpdated(int page, int subpage);
    virtual void HeaderUpdated(
        int page, int subpage, tt_line_array& page_ptr, int lang);

    const TeletextSubPage *FindSubPage(int page, int subpage, int dir=0) const
        { return FindSubPageInternal(page, subpage, dir); }

    TeletextSubPage *FindSubPage(int page, int subpage, int dir = 0)
    {
        return const_cast<TeletextSubPage*>
            (FindSubPageInternal(page, subpage, dir));
    }

    const TeletextPage *FindPage(int page, int dir = 0) const
        { return FindPageInternal(page, dir); }

    TeletextPage *FindPage(int page, int dir = 0)
        { return const_cast<TeletextPage*>(FindPageInternal(page, dir)); }

    const TeletextSubPage *FindSubPageInternal(int page, int subpage, int direction) const;
    const TeletextPage    *FindPageInternal(int page, int direction) const;

    int              m_curpage            {0x100};
    int              m_cursubpage         {-1};
    bool             m_curpageShowHeader  {true};
    bool             m_curpageIsSubtitle  {false};
    std::array<int,3> m_pageinput         {0};
    bool             m_transparent        {false};
    bool             m_revealHidden       {false};
    tt_line_array    m_header             {0};
    bool             m_headerChanged      {false};
    bool             m_pageChanged        {false};
    std::array<TeletextMagazine,8> m_magazines {};
    std::array<uint8_t,256>        m_bitswap   {};
    int              m_fetchpage          {0};
    int              m_fetchsubpage       {0};
};

#endif // TELETEXTREADER_H
