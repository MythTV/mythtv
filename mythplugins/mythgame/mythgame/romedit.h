#ifndef ROMEDITDLG_H_
#define ROMEDITDLG_H_

#include <mythscreentype.h>

class RomInfo;

class EditRomInfoDialog : public MythScreenType
{
    Q_OBJECT

  public:
     EditRomInfoDialog(MythScreenStack *parent,
                       QString name,
                       RomInfo *romInfo);

    ~EditRomInfoDialog();

    bool Create();
    void customEvent(QEvent *levent);
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  public slots:
    void SetGamename();
    void SetGenre();
    void SetYear();
    void SetCountry();
    void SetPlot();
    void SetPublisher();
    void ToggleFavorite();
    void FindScreenshot();
    void FindFanart();
    void FindBoxart();
    void SaveAndExit();

  private:
    void fillWidgets();
    void SetScreenshot(QString file);
    void SetFanart(QString file);
    void SetBoxart(QString file);

  private:
    RomInfo             *m_workingRomInfo;
    QString              m_id;
    QObject             *m_retObject;

    MythUITextEdit      *m_gamenameEdit;
    MythUITextEdit      *m_genreEdit;
    MythUITextEdit      *m_yearEdit;
    MythUITextEdit      *m_countryEdit;
    MythUITextEdit      *m_plotEdit;
    MythUITextEdit      *m_publisherEdit;
    MythUICheckBox      *m_favoriteCheck;
    MythUIButton        *m_screenshotButton;
    MythUIText          *m_screenshotText;
    MythUIButton        *m_fanartButton;
    MythUIText          *m_fanartText;
    MythUIButton        *m_boxartButton;
    MythUIText          *m_boxartText;
    MythUIButton        *m_doneButton;
};

#endif // ROMEDITDLG_H_
