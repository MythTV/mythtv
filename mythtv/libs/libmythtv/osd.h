#ifndef OSD_H
#define OSD_H

// ANSI C
#include <ctime>
#include <stdint.h> // for [u]int[32,64]_t

// C++
#include <deque>
#include <vector>
using namespace std;

// Qt
#include <QKeyEvent>
#include <QObject>
#include <QRegExp>
#include <QString>
#include <QMutex>
#include <QPoint>
#include <QRect>
#include <QHash>
#include <QMap>

// MythTV Headers
#include "themeinfo.h"
#include "programtypes.h" // for frm_dir_map_t

enum OSDFunctionalType
{
    kOSDFunctionalType_Default = 0,
    kOSDFunctionalType_PictureAdjust,
    kOSDFunctionalType_RecPictureAdjust,
    kOSDFunctionalType_SmartForward,
    kOSDFunctionalType_TimeStretchAdjust,
    kOSDFunctionalType_AudioSyncAdjust
};

struct StatusPosInfo
{
    QString desc;
    QString extdesc;
    int position;
    bool progBefore;
    bool progAfter;
};

extern const char *kOSDDialogActive;
extern const char *kOSDDialogAllowRecording;
extern const char *kOSDDialogAlreadyEditing;
extern const char *kOSDDialogExitOptions;
extern const char *kOSDDialogAskDelete;
extern const char *kOSDDialogCanNotDelete;
extern const char *kOSDDialogIdleTimeout;
extern const char *kOSDDialogSleepTimeout;
extern const char *kOSDDialogChannelTimeout;
extern const char *kOSDDialogInfo;
extern const char *kOSDDialogEditChannel;

class QImage;
class TTFFont;
class OSDSet;
class OSDTypeImage;
class OSDTypePositionIndicator;
class OSDSurface;
class OSDTypeText;
class TV;
class UDPNotifyOSDSet;
class OSDListTreeType;
class QKeyEvent;
class OSDGenericTree;
class ccText;
class CC708Service;
class TeletextViewer;
class QStringList;
class QDomElement;

class OSD : public QObject
{
    Q_OBJECT
 public:
    OSD();
   ~OSD(void);

    void Init(const QRect &totalBounds,   int   frameRate,
        const QRect &visibleBounds, float visibleAspect, float fontScaling);

    OSDSurface *Display(void);

    OSDSurface *GetDisplaySurface(void);

    void SetListener(QObject *listener) { m_listener = listener; }

    void ClearAll(const QString &name);
    void ClearAllText(const QString &name);
    void SetText(const QString &name, QHash<QString, QString> &infoMap,
                         int length);
    void SetInfoText(QHash<QString, QString> infoMap, int length);
    void SetInfoText(const QString &text, const QString &subtitle, 
                     const QString &desc, const QString &category,
                     const QString &start, const QString &end, 
                     const QString &callsign, const QString &iconpath,
                     int length);
    void SetChannumText(const QString &text, int length);

    // CC-608 and DVB text captions (not DVB/DVD subtitles).
    void AddCCText(const QString &text, int x, int y, int color, 
                   bool teletextmode = false);
    void ClearAllCCText();
    void UpdateCCText(vector<ccText*> *ccbuf,
                      int replace = 0, int scroll = 0,
                      bool scroll_prsv = false,
                      int scroll_yoff = 0, int scroll_ymax = 15);
    // CC-708 text captions (for ATSC)
    void SetCC708Service(const CC708Service *service);
    void CC708Updated(void);

    // Teletext menus (for PAL)
    TeletextViewer *GetTeletextViewer(void);

    void SetSettingsText(const QString &text, int length);

    void    NewDialogBox(const QString &name,
                         const QString &message,
                         QStringList &options,
                         int length, int sel = 0);
    void    DialogUp(const QString &name);
    void    DialogDown(const QString &name);
    bool    DialogShowing(const QString &name);
    void    TurnDialogOff(const QString &name);
    void    DialogAbort(const QString &name);
    int     GetDialogResponse(const QString &name);
    void    DialogAbortAndHideAll(void);
    void    PushDialog(const QString &dialogname);
    QString GetDialogActive(void) const;
    bool    IsDialogActive(const QString &dialogname) const;
    bool    IsDialogExisting(const QString &dialogname) const;

    void SetUpOSDClosedHandler(TV *tv);

    // position is 0 - 1000 
    void ShowStatus(int pos, bool fill, QString msgtext, QString desc,
                    int displaytime,
                    int osdFunctionalType = kOSDFunctionalType_Default);
    void ShowStatus(struct StatusPosInfo posInfo,
                      bool fill, QString msgtext, int displaytime,
                      int osdFunctionalType = kOSDFunctionalType_Default);
    void UpdateStatus(struct StatusPosInfo posInfo);
    void EndStatus(void);

    bool Visible(void);

    bool HideAll(void) { return HideAllExcept(QString::null); };
    bool HideAllExcept(const QString &name);
    bool HideSet(const QString &name, bool wait = false);
    bool HideSets(QStringList &name);

    void AddSet(OSDSet *set, QString name, bool withlock = true);

    void SetVisible(OSDSet *set, int length);
 
    OSDSet *GetSet(const QString &text);
    TTFFont *GetFont(const QString &text);

    void ShowEditArrow(long long number, long long totalframes, int type);
    void HideEditArrow(long long number, int type);
    void UpdateEditText(const QString &seek_amount, const QString &deletemarker,
                        const QString &edittime, const QString &framecnt);
    void DoEditSlider(const frm_dir_map_t &deleteMap,
                      uint64_t curFrame, uint64_t totalFrames);

    int getTimeType(void) { return timeType; }

    void Reinit(const QRect &totalBounds,   int   frameRate,
                const QRect &visibleBounds,
                float visibleAspect, float fontScaling);

    void SetFrameInterval(int frint);

    void StartNotify(const UDPNotifyOSDSet *notifySet);
    void ClearNotify(const QString &udpnotify_name);

    bool IsRunningTreeMenu(void);
    bool TreeMenuHandleKeypress(QKeyEvent *e);
    OSDListTreeType *ShowTreeMenu(const QString &name, 
                                  OSDGenericTree *treeToShow);

    void HideTreeMenu(bool hideMenu = false);
    void DisableFade(void);
    bool IsSetDisplaying(const QString &name);
    bool HasSet(const QString &name);
    QRect GetSubtitleBounds();

    void SetTextSubtitles(const QStringList&);
    void ClearTextSubtitles(void);

    void UpdateTeletext(void);

    float GetThemeAspect(void) { return m_themeaspect; }

    bool HasChanged(void) const { return changed; }

 private:
    bool InitDefaults(void);
    bool InitCC608(void);
    bool InitCC708(void);
    bool InitTeletext(void);
    bool InitSubtitles(void);
    bool InitMenu(void);
    bool InitInteractiveTV(void);

    TTFFont *LoadFont(const QString &name, int size);
    void ReinitFonts(void);
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

    QObject *m_listener;

    QRect osdBounds;
    int   frameint;
    bool  needPillarBox;

    QString themepath;

    float wscale, fscale;

    ThemeInfo *m_themeinfo;
    float m_themeaspect;

    float hmult, wmult;
    int xoffset, yoffset, displaywidth, displayheight;

    mutable QMutex osdlock;

    bool m_setsvisible;

    int totalfadetime;
    int timeType;

    QString timeFormat;

    QMap<QString, OSDSet *> setMap;
    vector<OSDSet *> *setList;

    QMap<QString, TTFFont *> fontMap;

    QMutex loadFontLock;
    QHash<QString, TTFFont*> loadFontHash;
    QHash<TTFFont*, QString> reinitFontHash;

    QMap<QString, int> dialogResponseList;
    deque<QString> dialogs;

    OSDTypeImage *editarrowleft;
    OSDTypeImage *editarrowright;
    QRect editarrowRect;

    OSDSurface *drawSurface;
    bool changed;

    OSDListTreeType *runningTreeMenu;
    QString treeMenuContainer;

    // EIA-708 captions
    QString fontname;
    QString ccfontname;
    QString cc708fontnames[16];
    QString fontSizeType;

    // Text file subtitles
    QRegExp removeHTML;
};
    
#endif
