/* BaseClasses.cpp

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

#include "BaseClasses.h"

#include <algorithm>

#include "ParseNode.h"
#include "Engine.h"
#include "ASN1Codes.h"
#include "Logging.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

// Construct from a string
MHOctetString::MHOctetString(const char *str, int nLen)
{
    if (nLen < 0)
    {
        nLen = strlen(str);
    }

    m_nLength = nLen;

    if (nLen == 0)
    {
        m_pChars = nullptr;
    }
    else
    {
        m_pChars = (unsigned char *)malloc(nLen + 1);

        if (! m_pChars)
        {
            throw "Out of memory";
        }

        memcpy(m_pChars, str, nLen);
    }
}

MHOctetString::MHOctetString(const unsigned char *str, int nLen)
  : m_nLength(nLen)
{
    if (nLen == 0)
    {
        m_pChars = nullptr;
    }
    else
    {
        m_pChars = (unsigned char *)malloc(nLen + 1);

        if (! m_pChars)
        {
            throw "Out of memory";
        }

        memcpy(m_pChars, str, nLen);
    }
}

// Construct a substring.
MHOctetString::MHOctetString(const MHOctetString &str, int nOffset, int nLen)
{
    if (nLen < 0)
    {
        nLen = str.Size() - nOffset;    // The rest of the string.
    }

    nLen = std::clamp(nLen, 0, str.Size());

    m_nLength = nLen;

    if (nLen == 0)
    {
        m_pChars = nullptr;
    }
    else
    {
        m_pChars = (unsigned char *)malloc(nLen + 1);

        if (! m_pChars)
        {
            throw "Out of memory";
        }

        memcpy(m_pChars, str.m_pChars + nOffset, nLen);
    }
}

MHOctetString::~MHOctetString()
{
    free(m_pChars);
}

// Copy a string
void MHOctetString::Copy(const MHOctetString &str)
{
    free(m_pChars);
    m_pChars = nullptr;
    m_nLength = str.m_nLength;

    if (str.m_pChars)
    {
        // Allocate a copy of the string.  For simplicity we always add a null.
        m_pChars = (unsigned char *)malloc(m_nLength + 1);

        if (m_pChars == nullptr)
        {
            throw "Out of memory";
        }

        memcpy(m_pChars, str.m_pChars, m_nLength);
        m_pChars[m_nLength] = 0;
    }
}

// Print the string in a suitable format.
// For the moment it uses quoted printable.
void MHOctetString::PrintMe(FILE *fd, int /*nTabs*/) const
{
    putc('\'', fd);

    for (int i = 0; i < m_nLength; i++)
    {
        unsigned char ch = m_pChars[i];

        // Escape a non-printable character or an equal sign or a quote.
        if (ch == '=' || ch == '\'' || ch < ' ' || ch >= 127)
        {
            fprintf(fd, "=%02X", ch);
        }
        else
        {
            putc(ch, fd);
        }
    }

    putc('\'', fd);
}

// Test strings.
int MHOctetString::Compare(const MHOctetString &str) const
{
    int nLength = m_nLength;

    nLength = std::min(nLength, str.m_nLength);

    // Test up to the length of the shorter string.
    int nTest = 0;

    if (nLength > 0)
    {
        nTest = memcmp(str.m_pChars, m_pChars, nLength);
    }

    // If they differ return the result
    if (nTest != 0)
    {
        return nTest;
    }

    // If they are the same then the longer string is greater.
    if (m_nLength == str.m_nLength)
    {
        return 0;
    }
    if (m_nLength < str.m_nLength)
    {
        return -1;
    }
    return 1;
}

// Add text to the end of the string.
void MHOctetString::Append(const MHOctetString &str)
{
    if (str.m_nLength == 0)
    {
        return;    // Nothing to do and it simplifies the code
    }

    int newLen = m_nLength + str.m_nLength;
    // Allocate a new string big enough to contain both and initialised to the first string.
    auto *p = (unsigned char *)realloc(m_pChars, newLen);

    if (p == nullptr)
    {
        throw "Out of memory";
    }

    m_pChars = p;
    // Copy the second string onto the end of the first.
    memcpy(m_pChars + m_nLength, str.m_pChars, str.m_nLength);
    m_nLength = newLen;
}

// Colour
void MHColour::Initialise(MHParseNode *p, MHEngine * /*engine*/)
{
    if (p->m_nNodeType == MHParseNode::PNInt)
    {
        m_nColIndex = p->GetIntValue();
    }
    else
    {
        p->GetStringValue(m_colStr);
    }
}

void MHColour::PrintMe(FILE *fd, int nTabs) const
{
    if (m_nColIndex >= 0)
    {
        fprintf(fd, " %d ", m_nColIndex);
    }
    else
    {
        m_colStr.PrintMe(fd, nTabs);
    }
}

void MHColour::SetFromString(const char *str, int nLen)
{
    m_nColIndex = -1;
    m_colStr.Copy(MHOctetString(str, nLen));
}

void MHColour::Copy(const MHColour &col)
{
    m_nColIndex = col.m_nColIndex;
    m_colStr.Copy(col.m_colStr);
}

// An object reference is used to identify and refer to an object.
// Internal objects have the m_groupId field empty.

MHObjectRef MHObjectRef::Null; // This has zero for the object number and an empty group id.

// An object reference is either an integer or a pair of a group id and an integer.
void MHObjectRef::Initialise(MHParseNode *p, MHEngine *engine)
{
    if (p->m_nNodeType == MHParseNode::PNInt)
    {
        m_nObjectNo = p->GetIntValue();
        // Set the group id to the id of this group.
        m_groupId.Copy(engine->GetGroupId());
    }
    else if (p->m_nNodeType == MHParseNode::PNSeq)
    {
        MHParseNode *pFirst = p->GetSeqN(0);
        MHOctetString groupId;
        pFirst->GetStringValue(m_groupId);
        m_nObjectNo = p->GetSeqN(1)->GetIntValue();
    }
    else
    {
        MHParseNode::Failure("ObjectRef: Argument is not int or sequence");
    }
}

void MHObjectRef::PrintMe(FILE *fd, int nTabs) const
{
    if (m_groupId.Size() == 0)
    {
        fprintf(fd, " %d ", m_nObjectNo);
    }
    else
    {
        fprintf(fd, " ( ");
        m_groupId.PrintMe(fd, nTabs);
        fprintf(fd, " %d ) ", m_nObjectNo);
    }
}

QString MHObjectRef::Printable() const
{
    if (m_groupId.Size() == 0)
    {
        return QString(" %1 ").arg(m_nObjectNo);
    }
    return QString(" ( ") + m_groupId.Printable() + QString(" %1 ").arg(m_nObjectNo);
}

// Make a copy of an object reference.
void MHObjectRef::Copy(const MHObjectRef &objr)
{
    m_nObjectNo = objr.m_nObjectNo;
    m_groupId.Copy(objr.m_groupId);
}

// The object references may not be identical but may nevertheless refer to the same
// object.  The only safe way to do this is to compare the canonical paths.
bool MHObjectRef::Equal(const MHObjectRef &objr, MHEngine *engine) const
{
    return m_nObjectNo == objr.m_nObjectNo && engine->GetPathName(m_groupId) == engine->GetPathName(objr.m_groupId);
}

bool MHContentRef::Equal(const MHContentRef &cr, MHEngine *engine) const
{
    return engine->GetPathName(m_contentRef) == engine->GetPathName(cr.m_contentRef);
}

// "Generic" versions of int, bool etc can be either the value or the an indirect reference.

void MHGenericBoolean::Initialise(MHParseNode *pArg, MHEngine *engine)
{
    if (pArg->m_nNodeType == MHParseNode::PNTagged && pArg->GetTagNo() == C_INDIRECTREFERENCE)
    {
        // Indirect reference.
        m_fIsDirect = false;
        m_indirect.Initialise(pArg->GetArgN(0), engine);
    }
    else   // Simple integer value.
    {
        m_fIsDirect = true;
        m_fDirect = pArg->GetBoolValue();
    }
}

void MHGenericBoolean::PrintMe(FILE *fd, int nTabs) const
{
    if (m_fIsDirect)
    {
        fprintf(fd, "%s ", m_fDirect ? "true" : "false");
    }
    else
    {
        fprintf(fd, ":IndirectRef ");
        m_indirect.PrintMe(fd, nTabs + 1);
    }
}

// Return the value, looking up any indirect ref.
bool MHGenericBoolean::GetValue(MHEngine *engine) const
{
    if (m_fIsDirect)
    {
        return m_fDirect;
    }

    MHUnion result;
    MHRoot *pBase = engine->FindObject(m_indirect);
    pBase->GetVariableValue(result, engine);
    result.CheckType(MHUnion::U_Bool);
    return result.m_fBoolVal;
}

// Return the indirect reference or fail if it's direct
MHObjectRef *MHGenericBase::GetReference()
{
    if (m_fIsDirect)
    {
        MHERROR("Expected indirect reference");
    }

    return &m_indirect;
}

void MHGenericInteger::Initialise(MHParseNode *pArg, MHEngine *engine)
{
    if (pArg->m_nNodeType == MHParseNode::PNTagged && pArg->GetTagNo() == C_INDIRECTREFERENCE)
    {
        // Indirect reference.
        m_fIsDirect = false;
        m_indirect.Initialise(pArg->GetArgN(0), engine);
    }
    else   // Simple integer value.
    {
        m_fIsDirect = true;
        m_nDirect = pArg->GetIntValue();
    }
}

void MHGenericInteger::PrintMe(FILE *fd, int nTabs) const
{
    if (m_fIsDirect)
    {
        fprintf(fd, "%d ", m_nDirect);
    }
    else
    {
        fprintf(fd, ":IndirectRef ");
        m_indirect.PrintMe(fd, nTabs + 1);
    }
}

// Return the value, looking up any indirect ref.
int MHGenericInteger::GetValue(MHEngine *engine) const
{
    if (m_fIsDirect)
    {
        return m_nDirect;
    }

    MHUnion result;
    MHRoot *pBase = engine->FindObject(m_indirect);
    pBase->GetVariableValue(result, engine);

    // From my reading of the MHEG documents implicit conversion is only
    // performed when assigning variables.  Nevertheless the Channel 4
    // Teletext assumes that implicit conversion takes place here as well.
    if (result.m_Type == MHUnion::U_String)
    {
        // Implicit conversion of string to integer.
        int v = 0;
        int p = 0;
        bool fNegative = false;

        if (result.m_strVal.Size() > 0 && result.m_strVal.GetAt(0) == '-')
        {
            p++;
            fNegative = true;
        }

        for (; p < result.m_strVal.Size(); p++)
        {
            unsigned char ch =  result.m_strVal.GetAt(p);

            if (ch < '0' || ch > '9')
            {
                break;
            }

            v = (v * 10) + ch - '0';
        }

        if (fNegative)
        {
            return -v;
        }
        return v;
    }

    result.CheckType(MHUnion::U_Int);
    return result.m_nIntVal;
}

void MHGenericOctetString::Initialise(MHParseNode *pArg, MHEngine *engine)
{
    if (pArg->m_nNodeType == MHParseNode::PNTagged && pArg->GetTagNo() == C_INDIRECTREFERENCE)
    {
        // Indirect reference.
        m_fIsDirect = false;
        m_indirect.Initialise(pArg->GetArgN(0), engine);
    }
    else   // Simple string value.
    {
        m_fIsDirect = true;
        pArg->GetStringValue(m_direct);
    }
}

void MHGenericOctetString::PrintMe(FILE *fd, int /*nTabs*/) const
{
    if (m_fIsDirect)
    {
        m_direct.PrintMe(fd, 0);
    }
    else
    {
        fprintf(fd, ":IndirectRef ");
        m_indirect.PrintMe(fd, 0);
    }
}

// Return the value, looking up any indirect ref.
void MHGenericOctetString::GetValue(MHOctetString &str, MHEngine *engine) const
{
    if (m_fIsDirect)
    {
        str.Copy(m_direct);
    }
    else
    {
        MHUnion result;
        MHRoot *pBase = engine->FindObject(m_indirect);
        pBase->GetVariableValue(result, engine);

        // From my reading of the MHEG documents implicit conversion is only
        // performed when assigning variables.  Nevertheless the Channel 4
        // Teletext assumes that implicit conversion takes place here as well.
        if (result.m_Type == MHUnion::U_Int)
        {
            // Implicit conversion of int to string.
            str.Copy(std::to_string(result.m_nIntVal).data());
        }
        else
        {
            result.CheckType(MHUnion::U_String);
            str.Copy(result.m_strVal);
        }
    }
}

void MHGenericObjectRef::Initialise(MHParseNode *pArg, MHEngine *engine)
{
    if (pArg->m_nNodeType == MHParseNode::PNTagged && pArg->GetTagNo() == C_INDIRECTREFERENCE)
    {
        // Indirect reference.
        m_fIsDirect = false;
        m_indirect.Initialise(pArg->GetArgN(0), engine);
    }
    else   // Direct reference.
    {
        m_fIsDirect = true;
        m_objRef.Initialise(pArg, engine);
    }
}

void MHGenericObjectRef::PrintMe(FILE *fd, int nTabs) const
{
    if (m_fIsDirect)
    {
        m_objRef.PrintMe(fd, nTabs + 1);
    }
    else
    {
        fprintf(fd, ":IndirectRef ");
        m_indirect.PrintMe(fd, nTabs + 1);
    }
}

// Return the value, looking up any indirect ref.
void MHGenericObjectRef::GetValue(MHObjectRef &ref, MHEngine *engine) const
{
    if (m_fIsDirect)
    {
        ref.Copy(m_objRef);
    }
    else
    {
        // LVR - Hmm I don't think this is right. Should be: ref.Copy(m_indirect);
        // But it's used in several places so workaround in Stream::MHActionGenericObjectRefFix
        MHUnion result;
        MHRoot *pBase = engine->FindObject(m_indirect);
        pBase->GetVariableValue(result, engine);
        result.CheckType(MHUnion::U_ObjRef);
        ref.Copy(result.m_objRefVal);
    }
}

void MHGenericContentRef::Initialise(MHParseNode *pArg, MHEngine *engine)
{
    if (pArg->GetTagNo() == C_INDIRECTREFERENCE)
    {
        // Indirect reference.
        m_fIsDirect = false;
        m_indirect.Initialise(pArg->GetArgN(0), engine);
    }
    else if (pArg->GetTagNo() == C_CONTENT_REFERENCE)  // Direct reference.
    {
        m_fIsDirect = true;
        m_direct.Initialise(pArg->GetArgN(0), engine);
    }
    else
    {
        MHERROR("Expected direct or indirect content reference");
    }
}

void MHGenericContentRef::PrintMe(FILE *fd, int /*nTabs*/) const
{
    if (m_fIsDirect)
    {
        m_direct.PrintMe(fd, 0);
    }
    else
    {
        fprintf(fd, ":IndirectRef ");
        m_indirect.PrintMe(fd, 0);
    }
}

// Return the value, looking up any indirect ref.
void MHGenericContentRef::GetValue(MHContentRef &ref, MHEngine *engine) const
{
    if (m_fIsDirect)
    {
        ref.Copy(m_direct);
    }
    else
    {
        MHUnion result;
        MHRoot *pBase = engine->FindObject(m_indirect);
        pBase->GetVariableValue(result, engine);
        result.CheckType(MHUnion::U_ContentRef);
        ref.Copy(result.m_contentRefVal);
    }
}

// Copies the argument, getting the value of an indirect args.
void MHUnion::GetValueFrom(const MHParameter &value, MHEngine *engine)
{
    switch (value.m_Type)
    {
        case MHParameter::P_Int:
            m_Type = U_Int;
            m_nIntVal = value.m_intVal.GetValue(engine);
            break;
        case MHParameter::P_Bool:
            m_Type = U_Bool;
            m_fBoolVal = value.m_boolVal.GetValue(engine);
            break;
        case MHParameter::P_String:
            m_Type = U_String;
            value.m_strVal.GetValue(m_strVal, engine);
            break;
        case MHParameter::P_ObjRef:
            m_Type = U_ObjRef;
            value.m_objRefVal.GetValue(m_objRefVal, engine);
            break;
        case MHParameter::P_ContentRef:
            m_Type = U_ContentRef;
            value.m_contentRefVal.GetValue(m_contentRefVal, engine);
            break;
        case MHParameter::P_Null:
            m_Type = U_None;
            break;
    }
}

const char *MHUnion::GetAsString(enum UnionTypes t)
{
    switch (t)
    {
        case U_Int:
            return "int";
        case U_Bool:
            return "bool";
        case U_String:
            return "string";
        case U_ObjRef:
            return "objref";
        case U_ContentRef:
            return "contentref";
        case U_None:
            return "none";
    }

    return ""; // Not reached.
}

void MHUnion::CheckType(enum UnionTypes t) const
{
    if (m_Type != t)
    {
        MHERROR(QString("Type mismatch - expected %1 found %2")
                .arg(GetAsString(m_Type), GetAsString(t)));
    }
}

QString MHUnion::Printable() const
{
    switch (m_Type)
    {
    case U_Int: return QString::number(m_nIntVal);
    case U_Bool: return m_fBoolVal ? "true" : "false";
    case U_String: return m_strVal.Printable();
    case U_ObjRef: return m_objRefVal.Printable();
    case U_ContentRef: return m_contentRefVal.Printable();
    case U_None: break;
    }
    return "";
}

// A parameter is a generic whose argument is either the value itself or an indirect reference.
void MHParameter::Initialise(MHParseNode *p, MHEngine *engine)
{
    switch (p->GetTagNo())
    {
        case C_NEW_GENERIC_BOOLEAN:
            m_Type = P_Bool;
            m_boolVal.Initialise(p->GetArgN(0), engine);
            break;
        case C_NEW_GENERIC_INTEGER:
            m_Type = P_Int;
            m_intVal.Initialise(p->GetArgN(0), engine);
            break;
        case C_NEW_GENERIC_OCTETSTRING:
            m_Type = P_String;
            m_strVal.Initialise(p->GetArgN(0), engine);
            break;
        case C_NEW_GENERIC_OBJECT_REF:
            m_Type = P_ObjRef;
            m_objRefVal.Initialise(p->GetArgN(0), engine);
            break;
        case C_NEW_GENERIC_CONTENT_REF:
            m_Type = P_ContentRef;
            m_contentRefVal.Initialise(p->GetArgN(0), engine);
            break;
        default:
            MHParseNode::Failure("Expected generic");
    }
}

void MHParameter::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);

    switch (m_Type)
    {
            // Direct values.
        case P_Int:
            fprintf(fd, ":GInteger ");
            m_intVal.PrintMe(fd, 0);
            break;
        case P_Bool:
            fprintf(fd, ":GBoolean ");
            m_boolVal.PrintMe(fd, 0);
            break;
        case P_String:
            fprintf(fd, ":GOctetString ");
            m_strVal.PrintMe(fd, 0);
            break;
        case P_ObjRef:
            fprintf(fd, ":GObjectRef ");
            m_objRefVal.PrintMe(fd, 0);
            break;
        case P_ContentRef:
            fprintf(fd, ":GObjectRef ");
            m_contentRefVal.PrintMe(fd, 0);
            break;
        case P_Null:
            break;
    }
}

// Return the indirect reference so that it can be updated.  We don't need to check
// the type at this stage.  If we assign the wrong value that will be picked up in
// the assignment.
MHObjectRef *MHParameter::GetReference()
{
    switch (m_Type)
    {
        case P_Int:
            return m_intVal.GetReference();
        case P_Bool:
            return m_boolVal.GetReference();
        case P_String:
            return m_strVal.GetReference();
        case P_ObjRef:
            return m_objRefVal.GetReference();
        case P_ContentRef:
            return m_contentRefVal.GetReference();
        case P_Null:
            return nullptr;
    }

    return nullptr; // To keep VC++ happy
}

// A content reference is simply a string.
MHContentRef MHContentRef::Null; // This is the empty string.

void MHContentRef::Initialise(MHParseNode *p, MHEngine * /*engine*/)
{
    p->GetStringValue(m_contentRef);
}

void MHFontBody::Initialise(MHParseNode *p, MHEngine *engine)
{
    if (p->m_nNodeType == MHParseNode::PNString)
    {
        p->GetStringValue(m_dirFont);
    }
    else
    {
        m_indirFont.Initialise(p, engine);
    }
}

void MHFontBody::PrintMe(FILE *fd, int nTabs) const
{
    if (m_dirFont.Size() > 0)
    {
        m_dirFont.PrintMe(fd, nTabs);
    }
    else
    {
        m_indirFont.PrintMe(fd, nTabs);
    }
}

void MHFontBody::Copy(const MHFontBody &fb)
{
    m_dirFont.Copy(fb.m_dirFont);
    m_indirFont.Copy(fb.m_indirFont);
}

void MHPointArg::Initialise(MHParseNode *p, MHEngine *engine)
{
    m_x.Initialise(p->GetSeqN(0), engine);
    m_y.Initialise(p->GetSeqN(1), engine);
}

void MHPointArg::PrintMe(FILE *fd, int nTabs) const
{
    fprintf(fd, "( ");
    m_x.PrintMe(fd, nTabs);
    m_y.PrintMe(fd, nTabs);
    fprintf(fd, ") ");
}

