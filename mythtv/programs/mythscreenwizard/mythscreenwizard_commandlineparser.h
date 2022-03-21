
#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythScreenWizardCommandLineParser : public MythCommandLineParser
{
  public:
    MythScreenWizardCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

