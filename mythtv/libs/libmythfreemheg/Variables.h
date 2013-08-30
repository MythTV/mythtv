/* Variables.h

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

#if !defined(VARIABLES_H)
#define VARIABLES_H

#include "Ingredients.h"
// Dependencies
#include "Root.h"
#include "BaseClasses.h"
#include "BaseActions.h"

class MHVariable : public MHIngredient  
{
  public:
    MHVariable() {}
    virtual ~MHVariable() {}

    // Internal behaviours.
    virtual void Activation(MHEngine *engine);
};

class MHBooleanVar : public MHVariable  
{
  public:
    MHBooleanVar(): m_fOriginalValue(false), m_fValue(false) {}
    virtual const char *ClassName() { return "BooleanVariable"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Prepare() { m_fValue = m_fOriginalValue; }

    // Internal behaviours.
    virtual void Preparation(MHEngine *engine);

    // Actions implemented
    virtual void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine);
    virtual void GetVariableValue(MHUnion &value, MHEngine *);
    virtual void SetVariableValue(const MHUnion &value);

  protected:
    bool m_fOriginalValue, m_fValue;
};

class MHIntegerVar : public MHVariable  
{
  public:
    MHIntegerVar(): m_nOriginalValue(0), m_nValue(0) {}
    virtual const char *ClassName() { return "IntegerVariable"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Prepare() { m_nValue = m_nOriginalValue; }

    // Internal behaviours.
    virtual void Preparation(MHEngine *engine);

    // Actions implemented
    virtual void TestVariable(int nOp, const MHUnion &parmm, MHEngine *engine);
    virtual void GetVariableValue(MHUnion &value, MHEngine *);
    virtual void SetVariableValue(const MHUnion &value);
  protected:
    int m_nOriginalValue, m_nValue;
};

class MHOctetStrVar : public MHVariable  
{
  public:
    MHOctetStrVar() {}
    virtual const char *ClassName() { return "OctetStringVariable"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Prepare() { m_Value.Copy(m_OriginalValue); }

    // Internal behaviours.
    virtual void Preparation(MHEngine *engine);

    // Actions implemented
    virtual void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine);
    virtual void GetVariableValue(MHUnion &value, MHEngine *);
    virtual void SetVariableValue(const MHUnion &value);
  protected:
    MHOctetString m_OriginalValue, m_Value;
};

class MHObjectRefVar : public MHVariable  
{
  public:
    MHObjectRefVar() {}
    virtual const char *ClassName() { return "ObjectRefVariable"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Prepare() { m_Value.Copy(m_OriginalValue); }

    // Internal behaviours.
    virtual void Preparation(MHEngine *engine);

    // Actions implemented
    virtual void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine);
    virtual void GetVariableValue(MHUnion &value, MHEngine *);
    virtual void SetVariableValue(const MHUnion &value);
  protected:
    MHObjectRef m_OriginalValue, m_Value;
};

class MHContentRefVar : public MHVariable  
{
  public:
    MHContentRefVar() {}
    virtual const char *ClassName() { return "ContentRefVariable"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Prepare() { m_Value.Copy(m_OriginalValue); }

    // Internal behaviours.
    virtual void Preparation(MHEngine *engine);

    // Actions implemented
    virtual void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine);
    virtual void GetVariableValue(MHUnion &value, MHEngine *);
    virtual void SetVariableValue(const MHUnion &value);
  protected:
    MHContentRef m_OriginalValue, m_Value;
};


// Set a variable to a value.
class MHSetVariable: public MHElemAction
{
  public:
    MHSetVariable():MHElemAction(":SetVariable") {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int /*nTabs*/) const { m_NewValue.PrintMe(fd, 0); }
    MHParameter m_NewValue; // New value to store.
};

// Compare a variable with a value and generate a TestEvent event on the result.
class MHTestVariable: public MHElemAction
{
  public:
    MHTestVariable(): MHElemAction(":TestVariable"), m_nOperator(0) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int nTabs) const;
    int m_nOperator;
    MHParameter m_Comparison; // Value to compare with.
};

// Integer actions.

// Integer operations.
class MHIntegerAction: public MHElemAction
{
  public:
    MHIntegerAction(const char *name): MHElemAction(name) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int /*nTabs*/) const { m_Operand.PrintMe(fd, 0); }
    virtual int DoOp(int arg1, int arg2) = 0; // Do the operation.
    MHGenericInteger m_Operand; // Value to add, subtract etc.
};

class MHAdd: public MHIntegerAction {
  public:
    MHAdd(): MHIntegerAction(":Add") {}
  protected:
    virtual int DoOp(int arg1, int arg2) { return arg1+arg2; }
};

class MHSubtract: public MHIntegerAction {
  public:
    MHSubtract(): MHIntegerAction(":Subtract") {}
  protected:
    virtual int DoOp(int arg1, int arg2) { return arg1-arg2; }
};

class MHMultiply: public MHIntegerAction {
  public:
    MHMultiply(): MHIntegerAction(":Multiply") {}
  protected:
    virtual int DoOp(int arg1, int arg2) { return arg1*arg2; }
};

class MHDivide: public MHIntegerAction {
  public:
    MHDivide(): MHIntegerAction(":Divide") {}
  protected:
    virtual int DoOp(int arg1, int arg2) {
        if (arg2 == 0) throw "Divide by 0";
        return arg1/arg2;
    }
};

class MHModulo: public MHIntegerAction {
  public:
    MHModulo(): MHIntegerAction(":Modulo") {}
  protected:
    virtual int DoOp(int arg1, int arg2) { return arg2 ? arg1%arg2 : 0; }
};

// Append - 
class MHAppend: public MHElemAction
{
  public:
    MHAppend(): MHElemAction(":Append") {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int /*nTabs*/) const { m_Operand.PrintMe(fd, 0); }
    MHGenericOctetString m_Operand; // Value to append.
};

#endif
