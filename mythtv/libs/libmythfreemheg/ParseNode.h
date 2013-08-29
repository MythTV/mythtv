/* ParseNode.h

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


#if !defined(PARSE_NODE_H)
#define PARSE_NODE_H

#include "BaseClasses.h"

class MHGroup;

// Abstract base class for the text and binary parsers.
class MHParseBase {
  public:
    virtual ~MHParseBase() {}
    virtual MHParseNode *Parse() = 0;
};

// Element of the parse tree, basically a piece of the program.
class MHParseNode  
{
  public:
    enum NodeType { PNTagged, PNBool, PNInt, PNEnum, PNString, PNNull, PNSeq };
  protected:
    MHParseNode(enum NodeType nt): m_nNodeType(nt) {}
  public:
    virtual ~MHParseNode() {}
    enum NodeType m_nNodeType;

    void Failure(const char *p); // Report a parse failure.

    // Destructors - extract data and raise an exception if the Node Type is wrong.

    // Tagged
    int GetTagNo();
    int GetArgCount();
    MHParseNode *GetArgN(int n);
    MHParseNode *GetNamedArg(int nTag); // Also for sequence.

    // Sequence.
    int GetSeqCount();
    MHParseNode *GetSeqN(int n);

    // Int
    int GetIntValue();
    // Enum
    int GetEnumValue();
    // Bool
    bool GetBoolValue();
    // String
    void GetStringValue(MHOctetString &str);
};

// Sequence of parse nodes.
class MHParseSequence: public MHParseNode, public MHOwnPtrSequence<MHParseNode>
{
  public:
    MHParseSequence(): MHParseNode(PNSeq) {}
    void PrintUnbracketed(int nTabs);
};

// At the moment all tagged items, i.e. everything except a primitive constant, belong
// to this class.  We will make derived classes in due course.
class MHPTagged: public MHParseNode
{
  public:
    MHPTagged(int nTag);
    void AddArg(MHParseNode *pNode);

    int m_TagNo;
    MHParseSequence m_Args;
};

// Primitive integer value.
class MHPInt: public MHParseNode
{
  public:
    MHPInt(int v): MHParseNode(PNInt), m_Value(v) {}

  public:
    int m_Value;
};

// Enumerated type - treat much as integer
class MHPEnum: public MHParseNode
{
  public:
    MHPEnum(int v): MHParseNode(PNEnum), m_Value(v) {}
  public:
    int m_Value;
};

// Primitive boolean value
class MHPBool: public MHParseNode
{
  public:
    MHPBool(bool v): MHParseNode(PNBool),  m_Value(v) {}

  public:
    bool m_Value;
};

// Primitive string value
class MHPString: public MHParseNode
{
  public:
    MHPString(MHOctetString &pSrc): MHParseNode(PNString) { m_Value.Copy(pSrc); }

  public:
    MHOctetString m_Value;
};

// ASN1 NULL value.
class MHPNull: public MHParseNode
{
  public:
    MHPNull(): MHParseNode(PNNull) {}
};

void PrintTabs(FILE *fd, int nTabs); // Support function for printing.

#endif
