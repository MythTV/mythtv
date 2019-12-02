/* ParseNode.cpp

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

#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Logging.h"

// Add an argument to the argument sequence.
void MHPTagged::AddArg(MHParseNode *pArg)
{
    m_Args.Append(pArg);
}

// General utility function for display.
void PrintTabs(FILE *fd, int n)
{
    for (int i = 0; i < n; i++)
    {
        fprintf(fd, "    ");
    }
}

// Report a failure.  This can be called when we use the parse tree to set up object tree.
void MHParseNode::Failure(const char *p)
{
    MHERROR(p);
}


int MHParseNode::GetTagNo()
{
    if (m_nNodeType != PNTagged)
    {
        Failure("Expected tagged value");
    }

    return ((MHPTagged *)this)->m_TagNo;
}

// Return the number of items in the sequence.
int MHParseNode::GetArgCount()
{
    if (m_nNodeType == PNTagged)
    {
        auto *pTag = (MHPTagged *)this;
        return pTag->m_Args.Size();
    }
    if (m_nNodeType == PNSeq)
    {
        auto *pSeq = (MHParseSequence *)this;
        return pSeq->Size();
    }

    Failure("Expected tagged value");
    return 0; // To keep the compiler happy
}

// Get the Nth entry.
MHParseNode *MHParseNode::GetArgN(int n)
{
    if (m_nNodeType == PNTagged)
    {
        auto *pTag = (MHPTagged *)this;

        if (n < 0 || n >= pTag->m_Args.Size())
        {
            Failure("Argument not found");
        }

        return pTag->m_Args.GetAt(n);
    }
    if (m_nNodeType == PNSeq)
    {
        auto *pSeq = (MHParseSequence *)this;

        if (n < 0 || n >= pSeq->Size())
        {
            Failure("Argument not found");
        }

        return pSeq->GetAt(n);
    }

    Failure("Expected tagged value");
    return nullptr; // To keep the compiler happy
}

// Get an argument with a specific tag.  Returns nullptr if it doesn't exist.
// There is a defined order of tags for both the binary and textual representations.
// Unfortunately they're not the same.
MHParseNode *MHParseNode::GetNamedArg(int nTag)
{
    MHParseSequence *pArgs = nullptr;

    if (m_nNodeType == PNTagged)
    {
        pArgs = &((MHPTagged *)this)->m_Args;
    }
    else if (m_nNodeType == PNSeq)
    {
        pArgs = (MHParseSequence *)this;
    }
    else
    {
        Failure("Expected tagged value or sequence");
    }

    for (int i = 0; i < pArgs->Size(); i++)
    {
        MHParseNode *p = pArgs->GetAt(i);

        if (p && p->m_nNodeType == PNTagged && ((MHPTagged *)p)->m_TagNo == nTag)
        {
            return p;
        }
    }

    return nullptr;
}

// Sequence.
int MHParseNode::GetSeqCount()
{
    if (m_nNodeType != PNSeq)
    {
        Failure("Expected sequence");
    }

    auto *pSeq = (MHParseSequence *)this;
    return pSeq->Size();
}

MHParseNode *MHParseNode::GetSeqN(int n)
{
    if (m_nNodeType != PNSeq)
    {
        Failure("Expected sequence");
    }

    auto *pSeq = (MHParseSequence *)this;

    if (n < 0 || n >= pSeq->Size())
    {
        Failure("Argument not found");
    }

    return pSeq->GetAt(n);
}

// Int
int MHParseNode::GetIntValue()
{
    if (m_nNodeType != PNInt)
    {
        Failure("Expected integer");
    }

    return ((MHPInt *)this)->m_Value;
}

// Enum
int MHParseNode::GetEnumValue()
{
    if (m_nNodeType != PNEnum)
    {
        Failure("Expected enumerated type");
    }

    return ((MHPEnum *)this)->m_Value;
}

// Bool
bool MHParseNode::GetBoolValue()
{
    if (m_nNodeType != PNBool)
    {
        Failure("Expected boolean");
    }

    return ((MHPBool *)this)->m_Value;
}

// String
void MHParseNode::GetStringValue(MHOctetString &str)
{
    if (m_nNodeType != PNString)
    {
        Failure("Expected string");
    }

    str.Copy(((MHPString *)this)->m_Value);
}

void MHParseNode::PrintMe(FILE *f)
{
    for (int i = 0; i < GetArgCount(); ++i)
    {
        if (i) fprintf(f, ", ");
        MHParseNode* pn = GetArgN(i);
        switch (pn->m_nNodeType)
        {
        case MHParseNode::PNTagged:
            fprintf(f, "tagged { "); pn->PrintMe(f); fprintf(f, "}");
            break;
        case MHParseNode::PNBool:
            fprintf(f, "bool %s", pn->GetBoolValue() ? "true" : "false");
            break;
        case MHParseNode::PNInt:
            fprintf(f, "int %d", pn->GetIntValue());
            break;
        case MHParseNode::PNEnum:
            fprintf(f, "enum %d", pn->GetEnumValue());
            break;
        case MHParseNode::PNString: {
            MHOctetString str;
            pn->GetStringValue(str);
            fprintf(f, "string '%s'", str.Bytes());
            break;
        }
        case MHParseNode::PNNull:
            fprintf(f, "null");
            break;
        case MHParseNode::PNSeq:
            fprintf(f, "seq { "); pn->PrintMe(f); fprintf(f, "}");
            break;
        }
    }
    fprintf(f, "\n");
}
