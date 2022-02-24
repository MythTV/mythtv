#ifndef ROMEDITDLG_H_
#define ROMEDITDLG_H_

// MythTV
#include <libmythui/mythscreentype.h>

class RomInfo;

class EditRomInfoDialog : public MythScreenType
{
    Q_OBJECT

  public:
     EditRomInfoDialog(MythScreenStack *parent,
                       const QString& name,
                       RomInfo *romInfo);

    ~EditRomInfoDialog() override;

    bool Create() override; // MythScreenType
    void customEvent(QEvent *levent) override; // MythUIType
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
    void SetScreenshot(const QString& file);
    void SetFanart(const QString& file);
    void SetBoxart(const QString& file);

  private:
    RomInfo             *m_workingRomInfo   {nullptr};
    QString              m_id;
    QObject             *m_retObject        {nullptr};

    MythUITextEdit      *m_gamenameEdit     {nullptr};
    MythUITextEdit      *m_genreEdit        {nullptr};
    MythUITextEdit      *m_yearEdit         {nullptr};
    MythUITextEdit      *m_countryEdit      {nullptr};
    MythUITextEdit      *m_plotEdit         {nullptr};
    MythUITextEdit      *m_publisherEdit    {nullptr};
    MythUICheckBox      *m_favoriteCheck    {nullptr};
    MythUIButton        *m_screenshotButton {nullptr};
    MythUIText          *m_screenshotText   {nullptr};
    MythUIButton        *m_fanartButton     {nullptr};
    MythUIText          *m_fanartText       {nullptr};
    MythUIButton        *m_boxartButton     {nullptr};
    MythUIText          *m_boxartText       {nullptr};
    MythUIButton        *m_doneButton       {nullptr};
};

#endif // ROMEDITDLG_H_
