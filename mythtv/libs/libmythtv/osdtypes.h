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
   ~OSDSet();

    void SetCache(bool cache) { m_cache = cache; }
    bool GetCache() { return m_cache; }

    QString GetName() { return m_name; }

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
   ~OSDTypeText();

    void SetText(const QString &text);
    QString GetText() { return m_message; }

    void SetMultiLine(bool multi) { m_multiline = multi; }
    bool GetMultiLine() { return m_multiline; }

    void SetOutline(bool dooutline) { m_outline = dooutline; }
    bool GetOutline() { return m_outline; }

    void SetCentered(bool docenter) { m_centered = docenter; }
    bool GetCentered() { return m_centered; }

    QRect DisplayArea() { return m_displaysize; }

    void SetColor(int color) { m_color = color; }
    int GetColor() { return m_color; }

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    void DrawString(unsigned char *screenptr, int vid_width, int vid_height,
                    QRect rect, const QString &text,
                    int fade, int maxfade, int xoff, int yoff);

    QRect m_displaysize;
    QString m_message;

    TTFFont *m_font;

    bool m_outline;
    bool m_centered;
    bool m_multiline;

    int m_color;
};
    
class OSDTypeImage : public OSDType
{
  public:
    OSDTypeImage(const QString &name, const QString &filename, 
                 QPoint displaypos, float wmult, float hmult, int scalew = -1, 
                 int scaleh = -1);
    OSDTypeImage(const OSDTypeImage &other);
    OSDTypeImage(const QString &name);    
   ~OSDTypeImage();

    void LoadImage(const QString &filename, float wmult, float hmult, 
                   int scalew = -1, int scaleh = -1);

    QPoint DisplayPos() { return m_displaypos; }
    void SetPosition(QPoint pos) { m_displaypos = pos; }
    QRect ImageSize() { return m_imagesize; }

    void Draw(unsigned char *screenptr, int vid_width, int vid_height, 
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
};

class OSDTypePosSlider : public OSDTypeImage
{
  public:
    OSDTypePosSlider(const QString &name, const QString &filename,
                      QRect displayrect, float wmult, float hmult,
                      int scalew = -1, int scaleh = -1);
   ~OSDTypePosSlider();

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
};


class OSDTypeBox : public OSDType
{
  public:
    OSDTypeBox(const QString &name, QRect displayrect); 
   ~OSDTypeBox();

    void Draw(unsigned char *screenptr, int vid_width, int vid_height, 
              int fade, int maxfade, int xoff, int yoff);

  private:
    QRect size;
};

class OSDTypePositionRectangle : public OSDType
{
  public:
    OSDTypePositionRectangle(const QString &name);
   ~OSDTypePositionRectangle();

    void AddPosition(QRect rect);

    int GetPosition() { return m_curposition; }
    void SetPosition(int pos);

    void PositionUp();  
    void PositionDown();

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    vector<QRect> positions; 
    int m_numpositions;
    int m_curposition;
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
    OSDTypeCC(const QString &name, TTFFont *font);
   ~OSDTypeCC();

    void AddCCText(const QString &text, int x, int y, int color, 
                   bool teletextmode = false);
    void ClearAllCCText();

    void Draw(unsigned char *screenptr, int vid_width, int vid_height,
              int fade, int maxfade, int xoff, int yoff);

  private:
    TTFFont *m_font;
    vector<ccText *> *m_textlist;

};

#endif
