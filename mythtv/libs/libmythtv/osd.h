#ifndef OSD_H
#define OSD_H

#include <qstring.h>
#include <qstringlist.h>
#include <qrect.h>
#include <qpoint.h>
#include <qvaluevector.h>
#include <ctime>
#include <qmap.h>
#include <qdom.h>
#include <qmutex.h>
#include <qobject.h>

#include <vector>
using namespace std;

enum OSDFunctionalType
{
    kOSDFunctionalType_Default = 0,
    kOSDFunctionalType_PictureAdjust,
    kOSDFunctionalType_RecPictureAdjust,
    kOSDFunctionalType_SmartForward,
    kOSDFunctionalType_TimeStretchAdjust
};

class QImage;
class TTFFont;
class OSDSet;
class OSDTypeImage;
class OSDTypePositionIndicator;
class OSDSurface;
class TV;
class UDPNotifyOSDSet;
class OSDListTreeType;
class QKeyEvent;
class OSDGenericTree;
class ccText;

class OSD : public QObject
{
    Q_OBJECT
 public:
    OSD(int width, int height, int framerate,
        int dispx, int dispy, int dispw, int disph);
   ~OSD(void);

    OSDSurface *Display(void);

    void ClearAllText(const QString &name);
    void SetText(const QString &name, QMap<QString, QString> &infoMap,
                         int length);
    void SetInfoText(QMap<QString, QString> infoMap, int length);
    void SetInfoText(const QString &text, const QString &subtitle, 
                     const QString &desc, const QString &category,
                     const QString &start, const QString &end, 
                     const QString &callsign, const QString &iconpath,
                     int length);
    void SetChannumText(const QString &text, int length);
    void AddCCText(const QString &text, int x, int y, int color, 
                   bool teletextmode = false);
    void ClearAllCCText();
    void UpdateCCText(vector<ccText*> *ccbuf,
                      int replace = 0, int scroll = 0, bool scroll_prsv = false,
                      int scroll_yoff = 0, int scroll_ymax = 15);
    void SetSettingsText(const QString &text, int length);

    void NewDialogBox(const QString &name, const QString &message, 
                      QStringList &options, int length);
    void DialogUp(const QString &name);
    void DialogDown(const QString &name);
    bool DialogShowing(const QString &name);
    void TurnDialogOff(const QString &name);
    void DialogAbort(const QString &name);
    int GetDialogResponse(const QString &name);

    void SetUpOSDClosedHandler(TV *tv);

    // position is 0 - 1000 
    void StartPause(int position, bool fill, QString msgtext,
                    QString slidertext, int displaytime,
                    int osdFunctionalType = kOSDFunctionalType_Default);
    void UpdatePause(int position, QString slidertext);
    void EndPause(void);

    bool Visible(void);

    void HideAll(void);
    bool HideSet(const QString &name);
    bool HideSets(QStringList &name);

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

    void StartNotify(UDPNotifyOSDSet *notifySet, int displaytime = 5);
    void ClearNotify(UDPNotifyOSDSet *notifySet);

    bool IsRunningTreeMenu(void);
    bool TreeMenuHandleKeypress(QKeyEvent *e);
    OSDListTreeType *ShowTreeMenu(const QString &name, 
                                  OSDGenericTree *treeToShow);

    void DisableFade(void);

 private:
    void SetDefaults();
    TTFFont *LoadFont(QString name, int size); 
    QString FindTheme(QString name);

    void HighlightDialogSelection(OSDSet *container, int num);  
 
    bool LoadTheme();
    void normalizeRect(QRect &rect);
    QPoint parsePoint(QString text);
    QColor parseColor(QString text);
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
    void parseListTree(OSDSet *container, QDomElement &element);

    int vid_width;
    int vid_height;
    int frameint;

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

    OSDListTreeType *runningTreeMenu;
    QString treeMenuContainer;
};
    
#endif
