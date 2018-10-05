// -*- Mode: c++ -*-

#ifndef _MYTH_CCEXTRACTOR_COMMAND_LINE_PARSER_H_
#define _MYTH_CCEXTRACTOR_COMMAND_LINE_PARSER_H_

#include "mythcommandlineparser.h"

class MythCCExtractorCommandLineParser : public MythCommandLineParser
{
  public:
    MythCCExtractorCommandLineParser();
    void LoadArguments(void) override; // MythCommandLineParser
  protected:
    QString GetHelpHeader(void) const override; // MythCommandLineParser
};

#endif // _MYTH_CCEXTRACTOR_COMMAND_LINE_PARSER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
