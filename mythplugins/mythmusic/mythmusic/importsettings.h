#ifndef IMPORTSETTINGS_H
#define IMPORTSETTINGS_H

#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

class ImportSettings : public MythScreenType
{
    Q_OBJECT
  public:
    explicit ImportSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~ImportSettings() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    MythUIButtonList   *m_paranoiaLevel     {nullptr};
    MythUITextEdit     *m_filenameTemplate  {nullptr};
    MythUICheckBox     *m_noWhitespace      {nullptr};
    MythUITextEdit     *m_postCDRipScript   {nullptr};
    MythUICheckBox     *m_ejectCD           {nullptr};
    MythUIButtonList   *m_encoderType       {nullptr};
    MythUIButtonList   *m_defaultRipQuality {nullptr};
    MythUICheckBox     *m_mp3UseVBR         {nullptr};
    MythUIButton       *m_saveButton        {nullptr};
    MythUIButton       *m_cancelButton      {nullptr};

  private slots:
    void slotSave(void);

};

#endif // IMPORTSETTINGS_H
