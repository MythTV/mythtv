#ifndef GENERALSETTINGS_H
#define GENERALSETTINGS_H

// MythTV
#include <mythscreentype.h>
#include <mythuispinbox.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>


class GeneralSettings : public MythScreenType
{
    Q_OBJECT
  public:
    explicit GeneralSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~GeneralSettings() override = default;

    bool Create(void) override; // MythScreenType

  private:
    MythUITextEdit     *m_musicAudioDevice     {nullptr};
    MythUICheckBox     *m_musicDefaultUpmix    {nullptr};
    MythUITextEdit     *m_musicCDDevice        {nullptr};
    MythUITextEdit     *m_nonID3FileNameFormat {nullptr};
    MythUICheckBox     *m_ignoreID3Tags        {nullptr};
    MythUICheckBox     *m_allowTagWriting      {nullptr};
    MythUIButton       *m_resetDBButton        {nullptr};
    MythUIButton       *m_saveButton           {nullptr};
    MythUIButton       *m_cancelButton         {nullptr};

  private slots:
    void slotSave(void);

    void slotResetDB(void) const;
    static void slotDoResetDB(bool ok);
};

#endif // GENERALSETTINGS_H
