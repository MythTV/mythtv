#ifndef COMMANDLINEPARSER_H_
#define COMMANDLINEPARSER_H_

#include <QString>

#include "mythcommandlineparser.h"

class MythUtilCommandLineParser : public MythCommandLineParser
{
  public:
    MythUtilCommandLineParser();
    void LoadArguments(void);
  protected:
    QString GetHelpHeader(void) const;
};

#endif

