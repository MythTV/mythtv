#ifndef LANGSETTINGS_H_
#define LANGSETTINGS_H_

// QT headers
#include <QObject>
#include <QTranslator>

// MythDB headers
#include "mythuiexp.h"

// MythUI headers
#include "libmythui/mythscreentype.h"

class QEventLoop;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIButton;
class MythUIText;
class MythScreenStack;

class MUI_PUBLIC LanguageSelection : public MythScreenType
{
    Q_OBJECT

  public:
    explicit LanguageSelection(MythScreenStack *parent, bool exitOnFinish = false);
   ~LanguageSelection(void) override;

    /// Ask the user for the language to use. If a
    /// language was already specified at the last
    /// load(), it will not ask unless force is set.
    static bool prompt(bool force = false);

    bool Create(void) override; // MythScreenType
    void Load(void) override; // MythScreenType

  private slots:
    //void LanguageClicked(MythUIButtonListItem *item);
    //void CountryClicked(MythUIButtonListItem *item);
    void Close(void) override; // MythScreenType
    void Save(void);

  private:
    void LanguageChanged(void);

    MythUIButtonList *m_languageList    {nullptr};
    MythUIButtonList *m_countryList     {nullptr};
    MythUIButton     *m_saveButton      {nullptr};
    MythUIButton     *m_cancelButton    {nullptr};

    bool              m_exitOnFinish;
    bool              m_loaded          {false};
    static bool       m_languageChanged;
    QString           m_language;
    QString           m_country;
    QEventLoop       *m_loop            {nullptr};
};

#endif
