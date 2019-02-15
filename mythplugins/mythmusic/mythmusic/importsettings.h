#ifndef IMPORTSETTINGS_H
#define IMPORTSETTINGS_H

#include <mythscreentype.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuicheckbox.h>
#include <mythuitext.h>
#include <mythuitextedit.h>

class ImportSettings : public MythScreenType
{
    Q_OBJECT
  public:
    ImportSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~ImportSettings() = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

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
