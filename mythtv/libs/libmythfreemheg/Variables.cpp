/* Variables.cpp

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
#include "compat.h"

#include "Variables.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Logging.h"

enum TestCodes { TC_Equal = 1, TC_NotEqual, TC_Less, TC_LessOrEqual, TC_Greater, TC_GreaterOrEqual };

static const char *TestToText(int tc)
{
    switch (tc)
    {
        case TC_Equal:
            return "Equal";
        case TC_NotEqual:
            return "NotEqual";
        case TC_Less:
            return "Less";
        case TC_LessOrEqual:
            return "LessOrEqual";
        case TC_Greater:
            return "Greater";
        case TC_GreaterOrEqual:
            return "GreaterOrEqual";
    }

    return NULL; // To keep the compiler happy
}

// Normal activation behaviour.
void MHVariable::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHIngredient::Activation(engine);
    m_fRunning = true;
    engine->EventTriggered(this, EventIsRunning);
}

void MHBooleanVar::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVariable::Initialise(p, engine);
    // Original value should be a bool.
    MHParseNode *pInitial = p->GetNamedArg(C_ORIGINAL_VALUE);

    if (pInitial)
    {
        m_fOriginalValue = pInitial->GetArgN(0)->GetBoolValue();
    }
}

void MHBooleanVar::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:BooleanVar");
    MHVariable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":OrigValue %s\n", m_fOriginalValue ? "true" : "false");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHBooleanVar::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    m_fValue = m_fOriginalValue;
    MHVariable::Preparation(engine);
}

// Implement the TestVariable action.  Triggers a TestEvent event on the result.
void MHBooleanVar::TestVariable(int nOp, const MHUnion &parm, MHEngine *engine)
{
    parm.CheckType(MHUnion::U_Bool);
    bool fRes = false;

    switch (nOp)
    {
        case TC_Equal:
            fRes = m_fValue == parm.m_fBoolVal;
            break;
        case TC_NotEqual:
            fRes = m_fValue != parm.m_fBoolVal;
            break;
        default:
            MHERROR("Invalid comparison for bool");
    }

    MHLOG(MHLogDetail, QString("Comparison %1 between %2 and %3 => %4").arg(TestToText(nOp))
          .arg(m_fValue ? "true" : "false").arg(parm.m_fBoolVal ? "true" : "false").arg(fRes ? "true" : "false"));
    engine->EventTriggered(this, EventTestEvent, fRes);
}

// Get the variable value.  Used in IndirectRef.
void MHBooleanVar::GetVariableValue(MHUnion &value, MHEngine *)
{
    value.m_Type = MHUnion::U_Bool;
    value.m_fBoolVal = m_fValue;
}

// Implement the SetVariable action.
void MHBooleanVar::SetVariableValue(const MHUnion &value)
{
    value.CheckType(MHUnion::U_Bool);
    m_fValue = value.m_fBoolVal;
    MHLOG(MHLogDetail, QString("Update %1 := %2").arg(m_ObjectReference.Printable()).arg(m_fValue ? "true" : "false"));
}


void MHIntegerVar::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVariable::Initialise(p, engine);
    // Original value should be an int.
    MHParseNode *pInitial = p->GetNamedArg(C_ORIGINAL_VALUE);

    if (pInitial)
    {
        m_nOriginalValue = pInitial->GetArgN(0)->GetIntValue();
    }
}

void MHIntegerVar::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:IntegerVar");
    MHVariable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":OrigValue %d\n", m_nOriginalValue);
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHIntegerVar::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    m_nValue = m_nOriginalValue;
    MHVariable::Preparation(engine);
}

// Implement the TestVariable action.  Triggers a TestEvent event on the result.
void MHIntegerVar::TestVariable(int nOp, const MHUnion &parm, MHEngine *engine)
{
    parm.CheckType(MHUnion::U_Int);
    bool fRes = false;

    switch (nOp)
    {
        case TC_Equal:
            fRes = m_nValue == parm.m_nIntVal;
            break;
        case TC_NotEqual:
            fRes = m_nValue != parm.m_nIntVal;
            break;
        case TC_Less:
            fRes = m_nValue < parm.m_nIntVal;
            break;
        case TC_LessOrEqual:
            fRes = m_nValue <= parm.m_nIntVal;
            break;
        case TC_Greater:
            fRes = m_nValue > parm.m_nIntVal;
            break;
        case TC_GreaterOrEqual:
            fRes = m_nValue >= parm.m_nIntVal;
            break;
        default:
            MHERROR("Invalid comparison for int"); // Shouldn't ever happen
    }

    MHLOG(MHLogDetail, QString("Comparison %1 between %2 and %3 => %4").arg(TestToText(nOp))
          .arg(m_nValue).arg(parm.m_nIntVal).arg(fRes ? "true" : "false"));
    engine->EventTriggered(this, EventTestEvent, fRes);
}

// Get the variable value.  Used in IndirectRef.
void MHIntegerVar::GetVariableValue(MHUnion &value, MHEngine *)
{
    value.m_Type = MHUnion::U_Int;
    value.m_nIntVal = m_nValue;
}

// Implement the SetVariable action.  Also used as part of Add, Subtract etc
void MHIntegerVar::SetVariableValue(const MHUnion &value)
{
    if (value.m_Type == MHUnion::U_String)
    {
        // Implicit conversion of string to integer.
        int v = 0;
        int p = 0;
        bool fNegative = false;

        if (value.m_StrVal.Size() > 0 && value.m_StrVal.GetAt(0) == '-')
        {
            p++;
            fNegative = true;
        }

        for (; p < value.m_StrVal.Size(); p++)
        {
            unsigned char ch =  value.m_StrVal.GetAt(p);

            if (ch < '0' || ch > '9')
            {
                break;
            }

            v = v * 10 + ch - '0';
        }

        if (fNegative)
        {
            m_nValue = -v;
        }
        else
        {
            m_nValue = v;
        }
    }
    else
    {
        value.CheckType(MHUnion::U_Int);
        m_nValue = value.m_nIntVal;
    }

    MHLOG(MHLogDetail, QString("Update %1 := %2").arg(m_ObjectReference.Printable()).arg(m_nValue));
}



void MHOctetStrVar::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVariable::Initialise(p, engine);
    // Original value should be a string.
    MHParseNode *pInitial = p->GetNamedArg(C_ORIGINAL_VALUE);

    if (pInitial)
    {
        pInitial->GetArgN(0)->GetStringValue(m_OriginalValue);
    }
}

void MHOctetStrVar::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:OStringVar");
    MHVariable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":OrigValue ");
    m_OriginalValue.PrintMe(fd, nTabs + 1);
    fprintf(fd, "\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHOctetStrVar::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    m_Value.Copy(m_OriginalValue);
    MHVariable::Preparation(engine);
}

// Implement the TestVariable action.  Triggers a TestEvent event on the result.
void MHOctetStrVar::TestVariable(int nOp, const MHUnion &parm, MHEngine *engine)
{
    parm.CheckType(MHUnion::U_String);
    int nRes = m_Value.Compare(parm.m_StrVal);
    bool fRes = false;

    switch (nOp)
    {
        case TC_Equal:
            fRes = nRes == 0;
            break;
        case TC_NotEqual:
            fRes = nRes != 0;
            break;
            /*  case TC_Less: fRes = nRes < 0; break;
                case TC_LessOrEqual: fRes = nRes <= 0; break;
                case TC_Greater: fRes = nRes > 0; break;
                case TC_GreaterOrEqual: fRes = nRes >= 0; break;*/
        default:
            MHERROR("Invalid comparison for string"); // Shouldn't ever happen
    }

    MHOctetString sample1(m_Value, 0, 10);
    MHOctetString sample2(parm.m_StrVal, 0, 10);
    MHLOG(MHLogDetail, QString("Comparison %1 %2 and %3 => %4").arg(TestToText(nOp))
          .arg(sample1.Printable()).arg(sample2.Printable()).arg(fRes ? "true" : "false"));
    engine->EventTriggered(this, EventTestEvent, fRes);
}

// Get the variable value.  Used in IndirectRef.
void MHOctetStrVar::GetVariableValue(MHUnion &value, MHEngine *)
{
    value.m_Type = MHUnion::U_String;
    value.m_StrVal.Copy(m_Value);
}

// Implement the SetVariable action.
void MHOctetStrVar::SetVariableValue(const MHUnion &value)
{
    if (value.m_Type == MHUnion::U_Int)
    {
        // Implicit conversion of int to string.
        char buff[30]; // 30 chars is more than enough.
        snprintf(buff, sizeof(buff), "%0d", value.m_nIntVal);
        m_Value.Copy(buff);
    }
    else
    {
        value.CheckType(MHUnion::U_String);
        m_Value.Copy(value.m_StrVal);
    }

    // Debug
    MHOctetString sample(m_Value, 0, 60);
    MHLOG(MHLogDetail, QString("Update %1 := %2").arg(m_ObjectReference.Printable())
          .arg(sample.Printable()));
}


void MHObjectRefVar::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVariable::Initialise(p, engine);
    // Original value should be an object reference.
    MHParseNode *pInitial = p->GetNamedArg(C_ORIGINAL_VALUE);

    // and this should be a ObjRef node.
    if (pInitial)
    {
        MHParseNode *pArg = pInitial->GetNamedArg(C_OBJECT_REFERENCE);

        if (pArg)
        {
            m_OriginalValue.Initialise(pArg->GetArgN(0), engine);
        }
    }
}

void MHObjectRefVar::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:ObjectRefVar");
    MHVariable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":OrigValue ");
    m_OriginalValue.PrintMe(fd, nTabs + 1);
    fprintf(fd, "\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHObjectRefVar::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    m_Value.Copy(m_OriginalValue);
    MHVariable::Preparation(engine);
}

// Implement the TestVariable action.  Triggers a TestEvent event on the result.
void MHObjectRefVar::TestVariable(int nOp, const MHUnion &parm, MHEngine *engine)
{
    parm.CheckType(MHUnion::U_ObjRef);
    bool fRes = false;

    switch (nOp)
    {
        case TC_Equal:
            fRes = m_Value.Equal(parm.m_ObjRefVal, engine);
            break;
        case TC_NotEqual:
            fRes = ! m_Value.Equal(parm.m_ObjRefVal, engine);
            break;
        default:
            MHERROR("Invalid comparison for object ref");
    }

    engine->EventTriggered(this, EventTestEvent, fRes);
}

// Get the variable value.  Used in IndirectRef.
void MHObjectRefVar::GetVariableValue(MHUnion &value, MHEngine *)
{
    value.m_Type = MHUnion::U_ObjRef;
    value.m_ObjRefVal.Copy(m_Value);
}

// Implement the SetVariable action.
void MHObjectRefVar::SetVariableValue(const MHUnion &value)
{
    value.CheckType(MHUnion::U_ObjRef);
    m_Value.Copy(value.m_ObjRefVal);
    MHLOG(MHLogDetail, QString("Update %1 := %2").arg(m_ObjectReference.Printable()).arg(m_Value.Printable()));
}


void MHContentRefVar::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVariable::Initialise(p, engine);
    // Original value should be a content reference.
    MHParseNode *pInitial = p->GetNamedArg(C_ORIGINAL_VALUE);

    // and this should be a ContentRef node.
    if (pInitial)
    {
        MHParseNode *pArg = pInitial->GetNamedArg(C_CONTENT_REFERENCE);

        if (pArg)
        {
            m_OriginalValue.Initialise(pArg->GetArgN(0), engine);
        }
    }
}

void MHContentRefVar::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:ContentRefVar");
    MHVariable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":OrigValue ");
    m_OriginalValue.PrintMe(fd, nTabs + 1);
    fprintf(fd, "\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHContentRefVar::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    m_Value.Copy(m_OriginalValue);
    MHVariable::Preparation(engine);
}

// Implement the TestVariable action.  Triggers a TestEvent event on the result.
void MHContentRefVar::TestVariable(int nOp, const MHUnion &parm, MHEngine *engine)
{
    parm.CheckType(MHUnion::U_ContentRef);
    bool fRes = false;

    switch (nOp)
    {
        case TC_Equal:
            fRes = m_Value.Equal(parm.m_ContentRefVal, engine);
            break;
        case TC_NotEqual:
            fRes = !m_Value.Equal(parm.m_ContentRefVal, engine);
            break;
        default:
            MHERROR("Invalid comparison for content ref");
    }

    engine->EventTriggered(this, EventTestEvent, fRes);
}

// Get the variable value.  Used in IndirectRef.
void MHContentRefVar::GetVariableValue(MHUnion &value, MHEngine *)
{
    value.m_Type = MHUnion::U_ContentRef;
    value.m_ContentRefVal.Copy(m_Value);
}

// Implement the SetVariable action.
void MHContentRefVar::SetVariableValue(const MHUnion &value)
{
    value.CheckType(MHUnion::U_ContentRef);
    m_Value.Copy(value.m_ContentRefVal);
    MHLOG(MHLogDetail, QString("Update %1 := %2").arg(m_ObjectReference.Printable()).arg(m_Value.Printable()));
}

// Actions
void MHSetVariable::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_NewValue.Initialise(p->GetArgN(1), engine); // Value to store
}

void MHSetVariable::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_Target.GetValue(target, engine); // Get the target
    MHUnion newValue;
    newValue.GetValueFrom(m_NewValue, engine); // Get the actual value to set.
    engine->FindObject(target)->SetVariableValue(newValue); // Set the value.
}


void MHTestVariable::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_nOperator = p->GetArgN(1)->GetIntValue(); // Test to perform
    m_Comparison.Initialise(p->GetArgN(2), engine); // Value to compare against
}

void MHTestVariable::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    fprintf(fd, " %d ", m_nOperator);
    m_Comparison.PrintMe(fd, 0);
}

void MHTestVariable::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_Target.GetValue(target, engine); // Get the target
    MHUnion testValue;
    testValue.GetValueFrom(m_Comparison, engine); // Get the actual value to compare.
    engine->FindObject(target)->TestVariable(m_nOperator, testValue, engine); // Do the test.
}

void MHIntegerAction::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    MHParseNode *pOp = p->GetArgN(1);
    m_Operand.Initialise(pOp, engine); // Operand to add, subtract etc.
}

void MHIntegerAction::Perform(MHEngine *engine)
{
    MHUnion targetVal;
    // Find the target and get its current value.  The target can be an indirect reference.
    MHObjectRef parm;
    m_Target.GetValue(parm, engine);
    MHRoot *pTarget = engine->FindObject(parm);
    pTarget->GetVariableValue(targetVal, engine);
    targetVal.CheckType(MHUnion::U_Int);
    // Get the value of the operand.
    int nOperand = m_Operand.GetValue(engine);
    // Set the value of targetVal to the new value and store it.
    targetVal.m_nIntVal = DoOp(targetVal.m_nIntVal, nOperand);
    pTarget->SetVariableValue(targetVal);
}

void MHAppend::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_Operand.Initialise(p->GetArgN(1), engine); // Operand to append
}

void MHAppend::Perform(MHEngine *engine)
{
    MHUnion targetVal;
    // Find the target and get its current value.  The target can be an indirect reference.
    MHObjectRef parm;
    m_Target.GetValue(parm, engine);
    MHRoot *pTarget = engine->FindObject(parm);
    pTarget->GetVariableValue(targetVal, engine);
    targetVal.CheckType(MHUnion::U_String);
    // Get the string to append.
    MHOctetString toAppend;
    m_Operand.GetValue(toAppend, engine);
    targetVal.m_StrVal.Append(toAppend); // Add it on the end
    pTarget->SetVariableValue(targetVal); // Set the target to the result.
}
