/* BaseActions.h

   Copyright (C)  David C. J. Matthews 2004, 2008  dm at prolingua.co.uk

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

#if !defined(BASEACTIONS_H)
#define BASEACTIONS_H

#include "BaseClasses.h"

class MHParseNode;
class MHRoot;

// Abstract base class for MHEG elementary actions.
// The first argument of most (all?) actions is an object reference which is
// the target of the action so we can handle some of the parsing and printing
// within this class rather than the derived classes.
class MHElemAction
{
  public:
    explicit MHElemAction(const char *name): m_ActionName(name) {}
    virtual ~MHElemAction() = default;
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Perform(MHEngine *engine) = 0; // Perform the action.
  protected:
    virtual void PrintArgs(FILE */*file*/, int /*nTabs*/) const {}
    MHRoot *Target(MHEngine *engine); // Look up the target
    const char        *m_ActionName {nullptr};
    MHGenericObjectRef m_Target;
};


// Base class for actions with a single integer argument.
class MHActionInt: public MHElemAction
{
  public:
    explicit MHActionInt(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_Argument.PrintMe(fd, 0); }
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) = 0;
  protected:
    MHGenericInteger m_Argument;
};

// Base class for actions with a pair of integer arguments.
class MHActionIntInt: public MHElemAction
{
  public:
    explicit MHActionIntInt(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int/* nTabs*/) const override // MHElemAction
        { m_Argument1.PrintMe(fd, 0); m_Argument2.PrintMe(fd, 0); }
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) = 0;
  protected:
    MHGenericInteger m_Argument1, m_Argument2;
};

// Base class for actions with three integers.  Used for SetSliderParameters
class MHActionInt3: public MHElemAction
{
  public:
    explicit MHActionInt3(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3) = 0;
  protected:
    MHGenericInteger m_Argument1, m_Argument2, m_Argument3;
};

// Base class for actions with four integers.  Used in the DynamicLineArt class
class MHActionInt4: public MHElemAction
{
  public:
    explicit MHActionInt4(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3, int nArg4) = 0;
  protected:
    MHGenericInteger m_Argument1, m_Argument2, m_Argument3, m_Argument4;
};

// Base class for actions with six integers.  Used in the DynamicLineArt class
class MHActionInt6: public MHElemAction
{
  public:
    explicit MHActionInt6(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3, int nArg4, int nArg5, int nArg6) = 0;
  protected:
    MHGenericInteger m_Argument1, m_Argument2, m_Argument3, m_Argument4, m_Argument5, m_Argument6;
};


// An action with an object reference as an argument.
class MHActionObjectRef: public MHElemAction
{
  public:
    explicit MHActionObjectRef(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) = 0;
  private:
    void PrintArgs(FILE *fd, int/* nTabs*/) const override // MHElemAction
        { m_resultVar.PrintMe(fd, 0); }
    MHObjectRef m_resultVar;
};

// An action with two object references as an argument.
class MHActionObjectRef2: public MHElemAction
{
  public:
    explicit MHActionObjectRef2(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) = 0;
  private:
    void PrintArgs(FILE *fd, int/* nTabs*/) const override // MHElemAction
        { m_resultVar1.PrintMe(fd, 0); m_resultVar2.PrintMe(fd, 0);}
    MHObjectRef m_resultVar1, m_resultVar2;
};

class MHActionGenericObjectRef: public MHElemAction
{
  public:
    explicit MHActionGenericObjectRef(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pObj) = 0;
  protected:
    void PrintArgs(FILE *fd, int/* nTabs*/) const override // MHElemAction
        { m_RefObject.PrintMe(fd, 0); }
    MHGenericObjectRef m_RefObject;
};


// Base class for actions with a single boolean argument.
class MHActionBool: public MHElemAction
{
  public:
    explicit MHActionBool(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_Argument.PrintMe(fd, 0); }
    void Perform(MHEngine *engine) override; // MHElemAction
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, bool fArg) = 0;
  protected:
    MHGenericBoolean m_Argument;
};

#endif
