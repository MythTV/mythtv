#ifndef COMMANDLINEPARSER_H_
#define COMMANDLINEPARSER_H_

#include <QString>

#include "libmythbase/mythcommandlineparser.h"

class MythUtilCommandLineParser : public MythCommandLineParser
{
  public:
    MythUtilCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

#endif

