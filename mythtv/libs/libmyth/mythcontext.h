#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qpalette.h>

class Settings;
class QSqlDatabase;

class MythContext
{
  public:
    MythContext();
   ~MythContext();

    QString GetInstallPrefix() { return m_installprefix; }
    QString GetFilePrefix();

    void LoadSettingsFiles(const QString &filename);
    void LoadQtConfig();

    void GetScreenSettings(int &width, float &wmult, int &height, float &hmult);
   
    QString RunProgramGuide(QString startchannel);
 
    QString FindThemeDir(QString themename);

    int OpenDatabase(QSqlDatabase *db);

    Settings *settings() { return m_settings; }

    QString GetSetting(const QString &key);
    int GetNumSetting(const QString &key);

    int GetBigFontSize() { return qtfontbig; }
    int GetMediumFontSize() { return qtfontmed; }
    int GetSmallFontSize() { return qtfontsmall; }

    void ThemeWidget(QWidget *widget);

    QPixmap *LoadScalePixmap(QString filename); 

  private:
    void SetPalette(QWidget *widget);

    // font sizes
    int qtfontbig, qtfontmed, qtfontsmall;

    Settings *m_settings;
    QString m_installprefix;

    bool m_themeloaded;
    QPixmap m_backgroundimage;
    QPalette m_palette;

    int m_height, m_width;
    float m_wmult, m_hmult;
};

#endif
