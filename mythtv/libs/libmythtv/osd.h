#ifndef OSD_H
#define OSD_H

#include <qstring.h>
#include <qstringlist.h>
#include <qrect.h>
#include <qpoint.h>
#include <qvaluevector.h>
#include <time.h>
#include <qmap.h>
#include <qdom.h>
#include <qmutex.h>

#include <vector>
using namespace std;

class QImage;
class TTFFont;
class OSDSet;
class OSDTypeImage;
class OSDTypePositionIndicator;
class OSDSurface;

class OSD
{
 public:
    OSD(int width, int height, int framerate, const QString &font, 
        const QString &ccfont, const QString &prefix, const QString &osdtheme,
        int dispx, int dispy, int dispw, int disph);
   ~OSD(void);

    OSDSurface *Display(void);

    void ClearAllText(const QString &name);
    void SetTextByRegexp(const QString &name, QMap<QString, QString> &regexpMap,
                         int length);
    void SetInfoText(QMap<QString, QString> regexpMap, int length);
    void SetInfoText(const QString &text, const QString &subtitle, 
                     const QString &desc, const QString &category,
                     const QString &start, const QString &end, 
                     const QString &callsign, const QString &iconpath,
                     int length);
    void SetChannumText(const QString &text, int length);
    void AddCCText(const QString &text, int x, int y, int color, 
                   bool teletextmode = false);
    void ClearAllCCText();
    void SetSettingsText(const QString &text, int length);

    void NewDialogBox(const QString &name, const QString &message, 
                      QStringList &options, int length);
    void DialogUp(const QString &name);
    void DialogDown(const QString &name);
    bool DialogShowing(const QString &name);
    void TurnDialogOff(const QString &name);
    void DialogAbort(const QString &name);
    int GetDialogResponse(const QString &name);

    // position is 0 - 1000 
    void StartPause(int position, bool fill, QString msgtext,
                    QString slidertext, int displaytime);
    void UpdatePause(int position, QString slidertext);
    void EndPause(void);

    bool Visible(void);

    void HideSet(const QString &name);

    void AddSet(OSDSet *set, QString name, bool withlock = true);

    void SetVisible(OSDSet *set, int length);
 
    OSDSet *GetSet(const QString &text);
    TTFFont *GetFont(const QString &text);

    void ShowEditArrow(long long number, long long totalframes, int type);
    void HideEditArrow(long long number, int type);
    void UpdateEditText(const QString &seek_amount, const QString &deletemarker,
                        const QString &edittime, const QString &framecnt);
    void DoEditSlider(QMap<long long, int> deleteMap, long long curFrame,
                      long long totalFrames);

    int getTimeType(void) { return timeType; }

    void Reinit(int width, int height, int frint, int dispx, int dispy, 
                int dispw, int disph);

    void SetFrameInterval(int frint);

 private:
    void SetDefaults();
    TTFFont *LoadFont(QString name, int size); 
    QString FindTheme(QString name);

    void HighlightDialogSelection(OSDSet *container, int num);  
 
    bool LoadTheme();
    void normalizeRect(QRect *rect);
    QPoint parsePoint(QString text);
    QRect parseRect(QString text);

    void RemoveSet(OSDSet *set);

    QString getFirstText(QDomElement &element);
    void parseFont(QDomElement &element);
    void parseContainer(QDomElement &element);
    void parseImage(OSDSet *container, QDomElement &element);
    void parseTextArea(OSDSet *container, QDomElement &element);
    void parseSlider(OSDSet *container, QDomElement &element);
    void parseBox(OSDSet *container, QDomElement &element);
    void parseEditArrow(OSDSet *container, QDomElement &element);
    void parsePositionRects(OSDSet *container, QDomElement &element);
    void parsePositionImage(OSDSet *container, QDomElement &element);

    QString fontname;
    QString ccfontname;

    int vid_width;
    int vid_height;
    int frameint;

    QString fontprefix;
    QString themepath;

    float hmult, wmult;
    int xoffset, yoffset, displaywidth, displayheight;

    QMutex osdlock;

    bool m_setsvisible;

    int totalfadetime;
    int timeType;

    QString timeFormat;

    QMap<QString, OSDSet *> setMap;
    vector<OSDSet *> *setList;

    QMap<QString, TTFFont *> fontMap;

    QMap<QString, int> dialogResponseList;

    OSDTypeImage *editarrowleft;
    OSDTypeImage *editarrowright;
    QRect editarrowRect;

    OSDSurface *drawSurface;
    bool changed;
};
    
#endif
