#ifndef OSDDATATYPES_H_
#define OSDDATATYPES_H_

#include <qstring.h>
#include <qrect.h>
#include <qmap.h>
#include <qvaluelist.h>
#include <vector>
#include <qobject.h>
#include <qregexp.h>
#include <cmath>
#include <qcolor.h>
#include "cc708window.h"
#include "osdimagecache.h"

using namespace std;

class TTFFont;
class OSDType;
class OSDTypeText;
class OSDSurface;
class TV;

typedef QMap<QString,QString> InfoMap;
typedef QMap<int,uint>        HotKeyMap;

static inline QRect unbias(QRect rect, float wmult, float hmult)
{
    return QRect((int)round(rect.x()      / wmult),
                 (int)round(rect.y()      / hmult),
                 (int)ceil( rect.width()  / wmult),
                 (int)ceil( rect.height() / hmult));
}

static inline QRect bias(QRect rect, float wmult, float hmult)
{
    return QRect((int)round(rect.x()      * wmult),
                 (int)round(rect.y()      * hmult),
                 (int)ceil( rect.width()  * wmult),
                 (int)ceil( rect.height() * hmult));
}

class OSDSet : public QObject
{
    Q_OBJECT
  public:
    OSDSet(const QString &name, bool cache, int screenwidth, int screenheight, 
           float wmult, float hmult, int frint);
    OSDSet(const OSDSet &other);
   ~OSDSet();

    void Clear(void);
    void ClearAllText(void);
    void AddType(OSDType *type);
    void Draw(OSDSurface *surface, bool actuallydraw);
    void Display(bool onoff = true, int osdFunctionalType = 0);
    void DisplayFor(int time, int osdFunctionalType = 0);
    void Hide(void);
    bool HandleKey(const QKeyEvent *e, bool *focus_change = NULL,
                   QString *button_pressed = NULL);
    QString HandleHotKey(const QKeyEvent *e);

    void Reinit(int screenwidth,  int screenheight,
                int xoff,         int yoff, 
                int displaywidth, int displayheight,
                float wmult,      float hmult,
                int frint);

    bool NeedsUpdate(void) { return m_needsupdate; }

    // Gets 
    QString  GetName(void)          const { return m_name;         }
    bool     GetCache(void)         const { return m_cache;        }
    bool     GetAllowFade(void)     const { return m_allowfade;    }
    int      GetPriority(void)      const { return m_priority;     }
    bool     Displaying(void)       const { return m_displaying;   }
    bool     HasDisplayed(void)     const { return m_hasdisplayed; }
    int      GetFadeTime(void)      const { return m_fadetime;     }
    bool     IsFading(void)         const
        { return (m_fadetime > 0) && (m_timeleft <= 0); }
    int      GetTimeLeft(void)      const { return m_timeleft;     }
    int      GetFrameInterval(void) const { return m_frameint;     }
    void     GetText(InfoMap &infoMap) const;
    bool     CanShowWith(const QString &name) const;

    const OSDType     *GetType(const QString &name) const;
    const OSDTypeText *GetSelected(void) const;

    // non-const gets
    OSDType     *GetType(const QString &name);
    OSDTypeText *GetSelected(void);

    // Sets
    void SetFadeTime(int time)          { m_fadetime = m_maxfade = time; }
    void SetCache(bool cache)           { m_cache = cache; }
    void SetName(const QString &name)   { m_name = name; }
    void SetAllowFade(bool allow)       { m_allowfade = allow; }
    void SetPriority(int priority)      { m_priority = priority; }
    void SetFadeMovement(int x, int y)  { m_xmove = x; m_ymove = y; }
    void SetFrameInterval(int frint)    { m_frameint = frint; }
    void SetWantsUpdates(bool updates)  { m_wantsupdates = updates; }
    void SetDrawEveryFrame(bool draw)   { m_draweveryframe = draw; }
    void SetShowWith(const QString &re) { m_showwith = re; };
    bool SetSelected(int index);
    void SetText(const InfoMap &infoMap);
    
  signals:
    void OSDClosed(int);

  private:
    int m_screenwidth;
    int m_screenheight;
    int m_frameint;
    float m_wmult;
    float m_hmult;

    bool m_cache;
    QString m_name;

    bool m_notimeout;

    bool m_hasdisplayed;
    int m_timeleft;
    bool m_displaying;
    int m_fadetime;
    int m_maxfade;

    int m_priority;

    int m_xmove;
    int m_ymove;

    int m_xoff;
    int m_yoff;

    int m_xoffsetbase;
    int m_yoffsetbase;

    bool m_allowfade;

    QMap<QString, OSDType *> typeList;
    vector<OSDType *> *allTypes;

    bool m_wantsupdates;
    bool m_needsupdate;
    int m_lastupdate;

    int currentOSDFunctionalType;

    bool m_draweveryframe;

    QRegExp m_showwith;
};

class OSDType : public QObject
{
    Q_OBJECT
  public:
    OSDType(const QString &name);
    virtual ~OSDType();

    void SetParent(OSDSet *parent) { m_parent = parent; }
    void Hide(bool hidden = true) { m_hidden = hidden; }
    bool isHidden(void) { return m_hidden; }

    QString Name() { return m_name; }

    virtual void Reinit(float wmult, float hmult) = 0;

    virtual void Draw(OSDSurface *surface, int fade, int maxfade, int xoff,
                      int yoff) = 0;

  protected:
    bool m_hidden;
    QString m_name;
    OSDSet *m_parent;
};

class DrawInfo
{
  public:
    DrawInfo(const QString &_msg, uint _width, bool _hilite)
        : msg(_msg), width(_width), hilite(_hilite) {}

  public:
    QString msg;
    uint    width;
    bool    hilite;
};

class OSDTypeText : public OSDType
{
  public:
    OSDTypeText(const QString &name, TTFFont *font, const QString &text,
                QRect displayrect, float wmult, float hmult);
    OSDTypeText(const OSDTypeText &text);
   ~OSDTypeText();

    void Reinit(float wmult, float hmult);

    void SetAltFont(TTFFont *font);
    void SetUseAlt(bool usealt) { m_usingalt = usealt; }

    void SetText(const QString &text);
    QString GetText() { return m_message; }

    void SetDefaultText(const QString &text);
    QString GetDefaultText() { return m_default_msg; }

    void SetMultiLine(bool multi) { m_multiline = multi; }
    bool GetMultiLine() { return m_multiline; }

    void SetCentered(bool docenter) { m_centered = docenter; }
    bool GetCentered() { return m_centered; }

    void SetRightJustified(bool right) { m_right = right; }
    bool GetRightJustified() { return m_right; }

    void SetScrolling(int x, int y) { m_scroller = true; m_scrollx = x;
                                      m_scrolly = y; }

    void SetLineSpacing(float linespacing) { m_linespacing = linespacing; }
    float GetLineSpacing() { return m_linespacing; }

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);
    bool MoveCursor(int dir);
    bool Delete(int dir);
    void InsertCharacter(QChar);

    bool IsSelected(void)         const { return m_selected;        }
    bool IsButton(void)           const { return m_button;          }
    int  GetEntryNum(void)        const { return m_entrynum;        }
    bool IsEntry(void)            const { return m_entrynum >= 0;   }
    QRect DisplayArea(void)       const { return m_displaysize;     }

    void SetSelected(bool is_selected)  { m_selected = is_selected; }
    void SetButton(bool is_button)      { m_button = is_button;     }
    void SetEntryNum(int entrynum)      { m_entrynum = entrynum;    }

  private:
    void DrawString(OSDSurface *surface, QRect rect, const QString &text,
                    int fade, int maxfade, int xoff, int yoff,
                    bool double_size=false);
    void DrawHiLiteString(OSDSurface *surface, QRect xrect, 
                          const QString &text, int fade, int maxfade, 
                          int xoff, int yoff, bool double_size = false);

    QRect m_displaysize;
    QRect m_screensize;
    QRect m_unbiasedsize;
    QString m_message;
    QString m_default_msg;

    TTFFont *m_font;
    TTFFont *m_altfont;

    bool m_centered;
    bool m_right;

    bool m_multiline;
    bool m_usingalt;

    bool m_selected;
    bool m_button;
    int  m_entrynum;
    int  m_cursorpos;

    bool m_scroller;
    int m_scrollx;
    int m_scrolly;

    int m_scrollstartx;
    int m_scrollendx;
    int m_scrollposx;

    int m_scrollstarty;
    int m_scrollendy;
    int m_scrollposy;

    bool m_scrollinit;

    float m_linespacing;

    mutable QString m_draw_info_str;
    mutable uint    m_draw_info_len;
    mutable vector<DrawInfo> m_draw_info;
};
    
class OSDTypeImage : public OSDType
{
  public:
    OSDTypeImage(const QString &name, const QString &filename, 
                 QPoint displaypos, float wmult, float hmult, int scalew = -1, 
                 int scaleh = -1);
    OSDTypeImage(const OSDTypeImage &other);
    OSDTypeImage(const QString &name);    
    OSDTypeImage(void);
    virtual ~OSDTypeImage();

    void SetName(const QString &name);
    void Reinit(float wmult, float hmult);

    void LoadImage(const QString &filename, float wmult, float hmult, 
                   int scalew = -1, int scaleh = -1);
    void LoadFromQImage(const QImage &img);

    void SetStaticSize(int scalew, int scaleh) { m_scalew = scalew;
                                                 m_scaleh = scaleh; }
    void SetPosition(QPoint pos, float wmult, float hmult);

    QPoint DisplayPos() const { return m_displaypos;         }
    QRect  ImageSize()  const { return m_imagesize;          }
    int    width()      const { return m_imagesize.width();  }
    int    height()     const { return m_imagesize.height(); }

    virtual void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                      int yoff);

    void SetDontRoundPosition(bool round) { m_dontround = round; }

  protected:
    QRect m_imagesize;
    QPoint m_displaypos;
    QPoint m_unbiasedpos;

    QString m_filename;

    bool m_isvalid;

    unsigned char *m_yuv;
    unsigned char *m_ybuffer;
    unsigned char *m_ubuffer;
    unsigned char *m_vbuffer;

    unsigned char *m_alpha;

    int m_scalew, m_scaleh;

    int m_drawwidth;
    bool m_onlyusefirst;
    bool m_dontround;

    static OSDImageCache  c_cache;
    OSDImageCacheValue   *m_cacheitem;
};

class OSDTypePosSlider : public OSDTypeImage
{
  public:
    OSDTypePosSlider(const QString &name, const QString &filename,
                      QRect displayrect, float wmult, float hmult,
                      int scalew = -1, int scaleh = -1);
   ~OSDTypePosSlider();

    void Reinit(float wmult, float hmult);

    void SetRectangle(QRect rect) { m_displayrect = rect; }
    QRect ImageSize() { return m_imagesize; }

    void SetPosition(int pos);
    int GetPosition() { return m_curval; }

  private:
    QRect m_displayrect;
    QRect m_unbiasedrect;
    int m_maxval;
    int m_curval;
};

class OSDTypeFillSlider : public OSDTypeImage
{
  public:
    OSDTypeFillSlider(const QString &name, const QString &filename,
                      QRect displayrect, float wmult, float hmult, 
                      int scalew = -1, int scaleh = -1);
   ~OSDTypeFillSlider();

    void Reinit(float wmult, float hmult);

    void SetRectangle(QRect rect) { m_displayrect = rect; }
    QRect ImageSize() { return m_imagesize; }

    void SetPosition(int pos);
    int GetPosition() { return m_curval; }

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    QRect m_displayrect;
    QRect m_unbiasedrect;
    int m_maxval;
    int m_curval;
};

class OSDTypeEditSlider : public OSDTypeImage
{
  public:
    OSDTypeEditSlider(const QString &name, const QString &bluefilename,
                      const QString &redfilename, QRect displayrect, 
                      float wmult, float hmult,
                      int scalew = -1, int scaleh = -1);
   ~OSDTypeEditSlider();

    void Reinit(float wmult, float hmult);

    void SetRectangle(QRect rect) { m_displayrect = rect; }
    QRect ImageSize() { return m_imagesize; }

    void ClearAll(void);
    void SetRange(int start, int end);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    QRect m_displayrect;
    QRect m_unbiasedrect;
    int m_maxval;
    int m_curval;

    unsigned char *m_drawMap;

    bool m_risvalid;
    unsigned char *m_ryuv;
    unsigned char *m_rybuffer;
    unsigned char *m_rubuffer;
    unsigned char *m_rvbuffer;
    unsigned char *m_ralpha;
    QRect m_rimagesize;

    QString m_redname;
    QString m_bluename;
};


class OSDTypeBox : public OSDType
{
  public:
    OSDTypeBox(const QString &name, QRect displayrect,
               float wmult, float hmult);
    OSDTypeBox(const OSDTypeBox &other);
   ~OSDTypeBox();

    void Reinit(float wmult, float hmult);
    void SetRect(QRect newrect, float wmult, float hmult);

    void Draw(OSDSurface *surface, int fade, int maxfade,
              int xoff, int yoff, unsigned int alpha);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff)
        { Draw(surface, fade, maxfade, xoff, yoff, 192); }

    void SetColor(QColor c) { m_color = c; }
  private:
    QRect  size;
    QRect  m_unbiasedsize;
    QColor m_color;
};

class OSDTypePositionIndicator
{
  public:
    OSDTypePositionIndicator(void);
    OSDTypePositionIndicator(const OSDTypePositionIndicator &other);
   ~OSDTypePositionIndicator();

    void SetOffset(int offset) { m_offset = offset; }
    int GetOffset(void) { return m_offset; }

    int GetPosition() { return m_curposition - m_offset; }
    void SetPosition(int pos);

    void PositionUp();
    void PositionDown();

  protected:
    int m_numpositions;
    int m_curposition;
    int m_offset;
};

class OSDTypePositionRectangle : public OSDType,
                                 public OSDTypePositionIndicator
{
  public:
    OSDTypePositionRectangle(const QString &name);
    OSDTypePositionRectangle(const OSDTypePositionRectangle &other);
   ~OSDTypePositionRectangle();

    void AddPosition(QRect rect, float wmult, float hmult);

    void Reinit(float wmult, float hmult);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    vector<QRect> positions;
    vector<QRect> unbiasedpos;
};

class OSDTypePositionImage : public virtual OSDTypeImage, 
                             public OSDTypePositionIndicator
{
  public:
    OSDTypePositionImage(const QString &name);
    OSDTypePositionImage(const OSDTypePositionImage &other);
   ~OSDTypePositionImage();

    void Reinit(float wmult, float hmult);

    void AddPosition(QPoint pos, float wmult, float hmult);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    vector<QPoint> positions;
    vector<QPoint> unbiasedpos;
    float m_wmult;
    float m_hmult;
};

class ccText
{
  public:
    QString text;
    int x;
    int y;
    int color;
    bool teletextmode;
};

class OSDTypeCC : public OSDType
{
  public:
    OSDTypeCC(const QString &name, TTFFont *font, int xoff, int yoff,
              int dispw, int disph, float wmult, float hmult);
   ~OSDTypeCC();

    void Reinit(float wmult, float hmult);

    void Reinit(int xoff, int yoff,
                int dispw, int disph,
                float wmult, float hmult);

    void AddCCText(const QString &text, int x, int y, int color, 
                   bool teletextmode = false);
    void ClearAllCCText();
    int UpdateCCText(vector<ccText*> *ccbuf,
                     int replace = 0, int scroll = 0, bool scroll_prsv = false,
                     int scroll_yoff = 0, int scroll_ymax = 15);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    TTFFont *m_font;
    vector<ccText *> *m_textlist;
    OSDTypeBox *m_box;
    int m_ccbackground;
    float m_wmult, m_hmult;
    int xoffset, yoffset, displaywidth, displayheight;
};

class OSDType708CC : public OSDType
{
  public:
    OSDType708CC(const QString &name, TTFFont *fonts[48],
                 int xoff, int yoff, int dispw, int disph);
    virtual ~OSDType708CC() {}

    void Reinit(float, float) {}
    void Reinit(int xoff, int yoff, int dispw, int disph);

    void SetCCService(const CC708Service *service)
        { cc708data = service; }

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    QRect CalcBounds(const OSDSurface*, const CC708Window&,
                     const vector<CC708String*>&, uint &min_offset);
    void  Draw(OSDSurface*,        const QPoint&,
               const CC708Window&, const vector<CC708String*>&);

    const CC708Service *cc708data;

    TTFFont *m_fonts[48];

    int xoffset, yoffset, displaywidth, displayheight;
};

#endif
