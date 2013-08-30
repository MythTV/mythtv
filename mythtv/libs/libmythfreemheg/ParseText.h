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

class MHGroup;

#include "ParseNode.h"

class MHParseText: public MHParseBase
{
  public:
    MHParseText(QByteArray &program);
    virtual ~MHParseText();

    // Parse the text and return a pointer to the parse tree
    virtual MHParseNode *Parse();

  private:
    void GetNextChar();
    void NextSym();
    MHParseNode *DoParse();
    void Error(const char *str);

    int m_lineCount;

    enum { PTTag, PTInt, PTString, PTEnum, PTStartSection, PTEndSection, PTStartSeq, PTEndSeq, PTNull, PTEOF, PTBool } m_nType;

    int m_ch;
    int m_nTag;
    int m_nInt;
    bool m_fBool;
    unsigned char *m_String;
    int m_nStringLength;

    unsigned int m_p; // Count of bytes read
    QByteArray m_data;
};

#endif
