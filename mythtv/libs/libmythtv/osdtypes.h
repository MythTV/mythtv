#ifndef OSDDATATYPES_H_
#define OSDDATATYPES_H_

#include <qstring.h>
#include <qrect.h>
#include <qmap.h>
#include <qvaluelist.h>
#include <vector>
#include <qobject.h>
#include <qregexp.h>

using namespace std;

class TTFFont;
class OSDType;
class OSDSurface;
class TV;

class OSDSet : public QObject
{
    Q_OBJECT
  public:
    OSDSet(const QString &name, bool cache, int screenwidth, int screenheight, 
           float wmult, float hmult, int frint);
    OSDSet(const OSDSet &other);
   ~OSDSet();

    void Clear();

    void SetCache(bool cache) { m_cache = cache; }
    bool GetCache() { return m_cache; }

    QString GetName() { return m_name; }
    void SetName(const QString &name) { m_name = name; }

    void SetAllowFade(bool allow) { m_allowfade = allow; }
    bool GetAllowFade() { return m_allowfade; }

    void AddType(OSDType *type);
    void Draw(OSDSurface *surface, bool actuallydraw);

    void SetPriority(int priority) { m_priority = priority; }
    int GetPriority() const { return m_priority; }

    void SetFadeMovement(int x, int y) { m_xmove = x; m_ymove = y; }

    int GetTimeLeft() { return m_timeleft; }

    bool Displaying() { return m_displaying; }
    bool HasDisplayed() { return m_hasdisplayed; }
    bool Fading() { return m_fadetime > 0; }

    void Display(bool onoff = true, int osdFunctionalType = 0);
    void DisplayFor(int time, int osdFunctionalType = 0);
    void FadeFor(int time);
    void Hide(void);

    OSDType *GetType(const QString &name);

    void SetFrameInterval(int frint) { m_frameint = frint; }
    int GetFrameInterval() { return m_frameint; }

    void ClearAllText(void);
    void SetText(QMap<QString, QString> &infoMap);

    void Reinit(int screenwidth, int screenheight, int xoff, int yoff, 
                int displaywidth, int displayheight, float wmult, float hmult,
                int frint);

    void SetWantsUpdates(bool updates) { m_wantsupdates = updates; }
    bool NeedsUpdate(void) { return m_needsupdate; }

    void SetDrawEveryFrame(bool draw) { m_draweveryframe = draw; }

    void SetShowWith(const QString &re) { m_showwith = re; };
    bool CanShowWith(const QString &name) const { 
        return m_showwith.exactMatch(name); };
    
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

    QString Name() { return m_name; }

    virtual void Draw(OSDSurface *surface, int fade, int maxfade, int xoff,
                      int yoff) = 0;

  protected:
    QString m_name;
    OSDSet *m_parent;
};

class OSDTypeText : public OSDType
{
  public:
    OSDTypeText(const QString &name, TTFFont *font, const QString &text,
                QRect displayrect);
    OSDTypeText(const OSDTypeText &text);
   ~OSDTypeText();

    void Reinit(float wchange, float hchange);

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

    QRect DisplayArea() { return m_displaysize; }

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    void DrawString(OSDSurface *surface, QRect rect, const QString &text,
                    int fade, int maxfade, int xoff, int yoff);

    QRect m_displaysize;
    QRect m_screensize;
    QString m_message;
    QString m_default_msg;

    TTFFont *m_font;
    TTFFont *m_altfont;

    bool m_centered;
    bool m_right;

    bool m_multiline;
    bool m_usingalt;

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
    void Reinit(float wchange, float hchange, float wmult, float hmult);

    void LoadImage(const QString &filename, float wmult, float hmult, 
                   int scalew = -1, int scaleh = -1);
    void LoadFromQImage(const QImage &img);

    void SetStaticSize(int scalew, int scaleh) { m_scalew = scalew;
                                                 m_scaleh = scaleh; }

    QPoint DisplayPos() { return m_displaypos; }
    void SetPosition(QPoint pos) { m_displaypos = pos; }

    QRect ImageSize() { return m_imagesize; }

    int width() { return m_imagesize.width(); }
    int height() { return m_imagesize.height(); }

    virtual void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                      int yoff);

  protected:
    QRect m_imagesize;
    QPoint m_displaypos;

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
};

class OSDTypePosSlider : public OSDTypeImage
{
  public:
    OSDTypePosSlider(const QString &name, const QString &filename,
                      QRect displayrect, float wmult, float hmult,
                      int scalew = -1, int scaleh = -1);
   ~OSDTypePosSlider();

    void Reinit(float wchange, float hchange, float wmult, float hmult);

    void SetRectangle(QRect rect) { m_displayrect = rect; }
    QRect ImageSize() { return m_imagesize; }

    void SetPosition(int pos);
    int GetPosition() { return m_curval; }

  private:
    QRect m_displayrect;
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

    void Reinit(float wchange, float hchange, float wmult, float hmult);

    void SetRectangle(QRect rect) { m_displayrect = rect; }
    QRect ImageSize() { return m_imagesize; }

    void SetPosition(int pos);
    int GetPosition() { return m_curval; }

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    QRect m_displayrect;
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

    void Reinit(float wchange, float hchange, float wmult, float hmult);

    void SetRectangle(QRect rect) { m_displayrect = rect; }
    QRect ImageSize() { return m_imagesize; }

    void ClearAll(void);
    void SetRange(int start, int end);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    QRect m_displayrect;
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
    OSDTypeBox(const QString &name, QRect displayrect); 
    OSDTypeBox(const OSDTypeBox &other);
   ~OSDTypeBox();

    void Reinit(float wchange, float hchange);
    void SetRect(QRect newrect) { size = newrect; }

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    QRect size;
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

    void AddPosition(QRect rect);

    void Reinit(float wchange, float hchange);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    vector<QRect> positions; 
};

class OSDTypePositionImage : public virtual OSDTypeImage, 
                             public OSDTypePositionIndicator
{
  public:
    OSDTypePositionImage(const QString &name);
    OSDTypePositionImage(const OSDTypePositionImage &other);
   ~OSDTypePositionImage();

    void Reinit(float wchange, float hchange, float wmult, float hmult);

    void AddPosition(QPoint pos);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

  private:
    vector<QPoint> positions;
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
              int dispw, int disph);
   ~OSDTypeCC();

    void Reinit(int xoff, int yoff, int dispw, int disph);

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

    int xoffset, yoffset, displaywidth, displayheight;
};

#endif
