#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>

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

    void ThemeWidget(QWidget *widget, int screenwidth, int screenheight,
                     float wmult, float hmult);

    QPixmap *LoadScalePixmap(QString filename, int screenwidth, 
                             int screenheight, float wmult, float hmult);

  private:
    // font sizes
    int qtfontbig, qtfontmed, qtfontsmall;

    Settings *m_settings;
    QString m_installprefix;
};

#endif
