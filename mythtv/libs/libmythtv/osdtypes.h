#ifndef OSDDATATYPES_H_
#define OSDDATATYPES_H_

#include <qstring.h>
#include <qrect.h>
#include <qmap.h>
#include <vector>
using namespace std;

class TTFFont;
class OSDType;

class OSDSet
{
  public:
    OSDSet(const QString &name, bool cache, int screenwidth, int screenheight, 
           float wmult, float hmult);
    OSDSet(const OSDSet &other);
   ~OSDSet();

    void SetCache(bool cache) { m_cache = cache; }
    bool GetCache() { return m_cache; }

    QString GetName() { return m_name; }
    void SetName(const QString &name) { m_name = name; }

    void SetAllowFade(bool allow) { m_allowfade = allow; }
    bool GetAllowFade() { return m_allowfade; }

    void AddType(OSDType *type);
    void Draw(unsigned char *yuvptr);

    void SetPriority(int priority) { m_priority = priority; }
    int GetPriority() const { return m_priority; }

    void SetFadeMovement(int x, int y) { m_xmove = x; m_ymove = y; }

    int GetFramesLeft() { return m_framesleft; }

    bool Displaying() { return m_displaying; }
    bool HasDisplayed() { return m_hasdisplayed; }

    void Display(bool onoff = true);
    void DisplayFor(int frames);
    void FadeFor(int frames);
    void Hide(void);

    OSDType *GetType(const QString &name);

    void SetFrameRate(int framerate) { m_framerate = framerate; }
    int GetFrameRate() { return m_framerate; }

    void ClearAllText(void);
    void SetTextByRegexp(QMap<QString, QString> &regexpMap);

    void Reinit(int screenwidth, int screenheight, int xoff, int yoff, 
                int displaywidth, int displayheight, float wmult, float hmult);

  private:
    int m_screenwidth;
    int m_screenheight;
    int m_framerate;
    float m_wmult;
    float m_hmult;

    bool m_cache;
    QString m_name;

    bool m_notimeout;

    bool m_hasdisplayed;
    int m_framesleft;
    bool m_displaying;
    int m_fadeframes;
    int m_maxfade;

    int m_priority;

    int m_xmove;
    int m_ymove;

    int m_xoff;
    int m_yoff;

    bool m_allowfade;

    QMap<QString, OSDType *> typeList;
    vector<OSDType *> *allTypes;
};

class OSDType
{
  public:
    OSDType(const QString &name);
    virtual ~OSDType();

    void SetParent(OSDSet *parent) { m_parent = parent; }

    QString Name() { return m_name; }

    virtual void Draw(unsigned char *screenptr, int vid_width, 
                      int vid_height, int fade, int maxfade, int xoff,
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

    QRect DisplayArea() { return m_displaysize; }

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    void DrawString(unsigned char *screenptr, int vid_width, int vid_height,
                    QRect rect, const QString &text,
                    int fade, int maxfade, int xoff, int yoff);

    QRect m_displaysize;
    QString m_message;
    QString m_default_msg;

    TTFFont *m_font;
    TTFFont *m_altfont;

    bool m_centered;
    bool m_right;

    bool m_multiline;
    bool m_usingalt;
};
    
class OSDTypeImage : public OSDType
{
  public:
    OSDTypeImage(const QString &name, const QString &filename, 
                 QPoint displaypos, float wmult, float hmult, int scalew = -1, 
                 int scaleh = -1);
    OSDTypeImage(const OSDTypeImage &other);
    OSDTypeImage(const QString &name);    
    virtual ~OSDTypeImage();

    void Reinit(float wmult, float hmult);

    void LoadImage(const QString &filename, float wmult, float hmult, 
                   int scalew = -1, int scaleh = -1);

    void SetStaticSize(int scalew, int scaleh) { m_scalew = scalew;
                                                 m_scaleh = scaleh; }

    QPoint DisplayPos() { return m_displaypos; }
    void SetPosition(QPoint pos) { m_displaypos = pos; }

    QRect ImageSize() { return m_imagesize; }

    virtual void Draw(unsigned char *screenptr, int vid_width, int vid_height, 
                      int fade, int maxfade, int xoff, int yoff);

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
    float m_wmult, m_hmult;
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

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    QRect m_displayrect;
    int m_maxval;
    int m_curval;
    int m_drawwidth;
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

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    QRect m_displayrect;
    int m_maxval;
    int m_curval;
    int m_drawwidth;

    int *m_drawMap;

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

    void Draw(unsigned char *screenptr, int vid_width, int vid_height, 
              int fade, int maxfade, int xoff, int yoff);

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

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

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

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

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

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    TTFFont *m_font;
    vector<ccText *> *m_textlist;
    OSDTypeBox *m_box;

    int xoffset, yoffset, displaywidth, displayheight;
};

#endif
