
#include <QString>

#include "mythcommandlineparser.h"

class MythScreenWizardCommandLineParser : public MythCommandLineParser
{
  public:
    MythScreenWizardCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

