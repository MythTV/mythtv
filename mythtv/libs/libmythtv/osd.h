#ifndef OSD_H
#define OSD_H

#include <qstring.h>
#include <qrect.h>
#include <qpoint.h>
#include <time.h>
#include <pthread.h>
#include <qmap.h>

#include <vector>
using namespace std;

class QImage;
class TTFFont;
class OSDSet;
class OSDTypeImage;
 
class OSD
{
 public:
    OSD(int width, int height, int framerate, const QString &filename, 
        const QString &prefix, const QString &osdtheme);
   ~OSD(void);

    void Display(unsigned char *yuvptr);
    
    void SetInfoText(const QString &text, const QString &subtitle, 
                     const QString &desc, const QString &category,
                     const QString &start, const QString &end, 
                     const QString &callsign, const QString &iconpath,
                     int length);
    void SetChannumText(const QString &text, int length);

    void NewDialogBox(const QString &name, const QString &message, 
                      const QString &optionone, const QString &optiontwo, 
                      const QString &optionthree, int length);
    void DialogUp(const QString &name);
    void DialogDown(const QString &name);
    bool DialogShowing(const QString &name);
    void TurnDialogOff(const QString &name);
    int GetDialogResponse(const QString &name);

    // position is 0 - 1000 
    void StartPause(int position, bool fill, QString msgtext,
                    QString slidertext, int displaytime);
    void UpdatePause(int position, QString slidertext);
    void EndPause(void);

    bool Visible(void);

    OSDSet *ShowText(const QString &name, const QString &message, int xpos,
                     int ypos, int width, int height, int secs);

    void ShowEditArrow(long long number, int type);
    void HideEditArrow(long long number);

    void AddSet(OSDSet *set, QString name, bool withlock = true);

    void SetVisible(OSDSet *set, int length);
 
    OSDSet *GetSet(const QString &text);
    TTFFont *GetFont(const QString &text);

    void DoEditSlider(QMap<long long, int> deleteMap, long long curFrame,
                      long long totalFrames);
 
 private:
    void SetNoThemeDefaults();
    TTFFont *LoadFont(QString name, int size); 
    QString FindTheme(QString name);
   
    bool LoadTheme();
    void normalizeRect(QRect *rect);
    QPoint parsePoint(QString text);
    QRect parseRect(QString text);

    void RemoveSet(OSDSet *set);

    QString fontname;

    int vid_width;
    int vid_height;
    int fps;

    QString fontprefix;
    
    bool usingtheme;
    QString themepath;

    float hmult, wmult;

    pthread_mutex_t osdlock;

    bool m_setsvisible;

    int totalfadeframes;

    QString timeFormat;

    QMap<QString, OSDSet *> setMap;
    vector<OSDSet *> *setList;

    QMap<QString, TTFFont *> fontMap;

    QMap<QString, int> dialogResponseList;

    OSDTypeImage *editarrowleft;
    OSDTypeImage *editarrowright;
    QRect editarrowRect;
};
    
#endif
