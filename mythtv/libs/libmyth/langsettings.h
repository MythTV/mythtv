#ifndef LANGSETTINGS_H_
#define LANGSETTINGS_H_

// QT headers
#include <QObject>
#include <QTranslator>

// MythDB headers
#include "mythexp.h"

// MythUI headers
#include "mythscreentype.h"
#include "mythscreenstack.h"

class QEventLoop;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIButton;
class MythUIText;

class MPUBLIC LanguageSelection : public MythScreenType
{
    Q_OBJECT

  public:
    LanguageSelection(MythScreenStack *parent, bool exitOnFinish = false);
   ~LanguageSelection(void);

    /// Ask the user for the language to use. If a
    /// language was already specified at the last
    /// load(), it will not ask unless force is set.
    static bool prompt(bool force = false);

    bool Create(void);
    void Load(void);

  private slots:
    //void LanguageClicked(MythUIButtonListItem *item);
    //void CountryClicked(MythUIButtonListItem *item);
    void Close(void);
    void Save(void);

  private:
    void LanguageChanged(void);

    MythUIButtonList *m_languageList;
    MythUIButtonList *m_countryList;
    MythUIButton *m_saveButton;
    MythUIButton *m_cancelButton;

    bool m_exitOnFinish;
    bool m_loaded;
    static bool m_languageChanged;
    QString m_language;
    QString m_country;
    QEventLoop *m_loop;
};

#endif
