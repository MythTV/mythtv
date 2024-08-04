/* ParseText.h

   Copyright (C)  David C. J. Matthews 2004  dm at prolingua.co.uk

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/


#if !defined(PARSETEXT_H)
#define PARSETEXT_H

#include <cstdlib> // malloc etc.

class MHGroup;

#include "ParseNode.h"

class MHParseText: public MHParseBase
{
  public:
    explicit MHParseText(QByteArray &program)
        : m_string((unsigned char *)malloc(100)),
          m_data(program) {}
    ~MHParseText() override;

    // Parse the text and return a pointer to the parse tree
    MHParseNode *Parse() override; // MHParseBase

  private:
    void GetNextChar();
    void NextSym();
    MHParseNode *DoParse();
    void Error(const char *str) const;

    int            m_lineCount     {1};

    enum ParseTextType : std::uint8_t
                       { PTTag, PTInt, PTString, PTEnum, PTStartSection,
                         PTEndSection, PTStartSeq, PTEndSeq, PTNull,
                         PTEOF, PTBool };
    ParseTextType  m_nType         {PTNull};

    char           m_ch            {0};
    int            m_nTag          {0};
    int            m_nInt          {0};
    bool           m_fBool         {false};
    unsigned char *m_string        {nullptr};
    int            m_nStringLength {0};

    unsigned int   m_p             {0}; // Count of bytes read
    QByteArray     m_data;
};

#endif
