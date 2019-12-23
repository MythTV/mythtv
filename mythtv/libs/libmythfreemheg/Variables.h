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
    MHVariable() = default;
    ~MHVariable() override = default;

    // Internal behaviours.
    void Activation(MHEngine *engine) override; // MHRoot
};

class MHBooleanVar : public MHVariable  
{
  public:
    MHBooleanVar() = default;
    const char *ClassName() override // MHRoot
        { return "BooleanVariable"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    virtual void Prepare() { m_fValue = m_fOriginalValue; }

    // Internal behaviours.
    void Preparation(MHEngine *engine) override; // MHIngredient

    // Actions implemented
    void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine) override; // MHRoot
    void GetVariableValue(MHUnion &value, MHEngine *engine) override; // MHRoot
    void SetVariableValue(const MHUnion &value) override; // MHRoot

  protected:
    bool m_fOriginalValue {false};
    bool m_fValue         {false};
};

class MHIntegerVar : public MHVariable  
{
  public:
    MHIntegerVar() = default;
    const char *ClassName() override // MHRoot
        { return "IntegerVariable"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    virtual void Prepare() { m_nValue = m_nOriginalValue; }

    // Internal behaviours.
    void Preparation(MHEngine *engine) override; // MHIngredient

    // Actions implemented
    void TestVariable(int nOp, const MHUnion &parmm, MHEngine *engine) override; // MHRoot
    void GetVariableValue(MHUnion &value, MHEngine *engine) override; // MHRoot
    void SetVariableValue(const MHUnion &value) override; // MHRoot
  protected:
    int m_nOriginalValue {0};
    int m_nValue         {0};
};

class MHOctetStrVar : public MHVariable  
{
  public:
    MHOctetStrVar() = default;
    const char *ClassName() override // MHRoot
        { return "OctetStringVariable"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    virtual void Prepare() { m_Value.Copy(m_OriginalValue); }

    // Internal behaviours.
    void Preparation(MHEngine *engine) override; // MHIngredient

    // Actions implemented
    void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine) override; // MHRoot
    void GetVariableValue(MHUnion &value, MHEngine *engine) override; // MHRoot
    void SetVariableValue(const MHUnion &value) override; // MHRoot
  protected:
    MHOctetString m_OriginalValue, m_Value;
};

class MHObjectRefVar : public MHVariable  
{
  public:
    MHObjectRefVar() = default;
    const char *ClassName() override // MHRoot
        { return "ObjectRefVariable"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    virtual void Prepare() { m_Value.Copy(m_OriginalValue); }

    // Internal behaviours.
    void Preparation(MHEngine *engine) override; // MHIngredient

    // Actions implemented
    void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine) override; // MHRoot
    void GetVariableValue(MHUnion &value, MHEngine *engine) override; // MHRoot
    void SetVariableValue(const MHUnion &value) override; // MHRoot
  protected:
    MHObjectRef m_OriginalValue, m_Value;
};

class MHContentRefVar : public MHVariable  
{
  public:
    MHContentRefVar() = default;
    const char *ClassName() override // MHRoot
        { return "ContentRefVariable"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    virtual void Prepare() { m_Value.Copy(m_OriginalValue); }

    // Internal behaviours.
    void Preparation(MHEngine *engine) override; // MHIngredient

    // Actions implemented
    void TestVariable(int nOp, const MHUnion &parm, MHEngine *engine) override; // MHRoot
    void GetVariableValue(MHUnion &value, MHEngine *engine) override; // MHRoot
    void SetVariableValue(const MHUnion &value) override; // MHRoot
  protected:
    MHContentRef m_OriginalValue, m_Value;
};


// Set a variable to a value.
class MHSetVariable: public MHElemAction
{
  public:
    MHSetVariable():MHElemAction(":SetVariable") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_NewValue.PrintMe(fd, 0); }
    MHParameter m_NewValue; // New value to store.
};

// Compare a variable with a value and generate a TestEvent event on the result.
class MHTestVariable: public MHElemAction
{
  public:
    MHTestVariable(): MHElemAction(":TestVariable") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    int m_nOperator {0};
    MHParameter m_Comparison; // Value to compare with.
};

// Integer actions.

// Integer operations.
class MHIntegerAction: public MHElemAction
{
  public:
    explicit MHIntegerAction(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_Operand.PrintMe(fd, 0); }
    virtual int DoOp(int arg1, int arg2) = 0; // Do the operation.
    MHGenericInteger m_Operand; // Value to add, subtract etc.
};

class MHAdd: public MHIntegerAction {
  public:
    MHAdd(): MHIntegerAction(":Add") {}
  protected:
    int DoOp(int arg1, int arg2) override // MHIntegerAction
        { return arg1+arg2; }
};

class MHSubtract: public MHIntegerAction {
  public:
    MHSubtract(): MHIntegerAction(":Subtract") {}
  protected:
    int DoOp(int arg1, int arg2) override // MHIntegerAction
        { return arg1-arg2; }
};

class MHMultiply: public MHIntegerAction {
  public:
    MHMultiply(): MHIntegerAction(":Multiply") {}
  protected:
    int DoOp(int arg1, int arg2) override // MHIntegerAction
        { return arg1*arg2; }
};

class MHDivide: public MHIntegerAction {
  public:
    MHDivide(): MHIntegerAction(":Divide") {}
  protected:
    int DoOp(int arg1, int arg2) override { // MHIntegerAction
        if (arg2 == 0) throw "Divide by 0";
        return arg1/arg2;
    }
};

class MHModulo: public MHIntegerAction {
  public:
    MHModulo(): MHIntegerAction(":Modulo") {}
  protected:
    int DoOp(int arg1, int arg2) override // MHIntegerAction
        { return arg2 ? arg1%arg2 : 0; }
};

// Append - 
class MHAppend: public MHElemAction
{
  public:
    MHAppend(): MHElemAction(":Append") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int /*nTabs*/) const  override // MHElemAction
        { m_Operand.PrintMe(fd, 0); }
    MHGenericOctetString m_Operand; // Value to append.
};

#endif
