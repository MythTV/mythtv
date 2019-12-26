/* Programs.h

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


#if !defined(PROGRAMS_H)
#define PROGRAMS_H

#include "Ingredients.h"
#include "BaseActions.h"
// Dependencies
#include "Root.h"
#include "BaseClasses.h"

class MHEngine;

// Abstract base class for programs.
class MHProgram : public MHIngredient  
{
  public:
    MHProgram() = default;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    bool InitiallyAvailable() override // MHIngredient
        { return m_fInitiallyAvailable; }
    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot

    // Action - Stop can be used to stop the code.
    void Stop(MHEngine *engine) override // MHRoot
        { Deactivation(engine); }
  protected:
    MHOctetString m_Name; // Name of the program
    bool          m_fInitiallyAvailable {true};
};

// Resident program
class MHResidentProgram : public MHProgram  
{
  public:
    MHResidentProgram() = default;
    const char *ClassName() override // MHRoot
        { return "ResidentProgram"; }
    void PrintMe(FILE *fd, int nTabs) const override; // MHProgram
    void CallProgram(bool fIsFork, const MHObjectRef &success,
        const MHSequence<MHParameter *> &args, MHEngine *engine) override; // MHRoot
};

// Remote program - not needed for UK MHEG
class MHRemoteProgram : public MHProgram  
{
  public:
    MHRemoteProgram() = default;
    const char *ClassName() override // MHRoot
        { return "RemoteProgram"; }
    ~MHRemoteProgram() override = default;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHProgram
    void PrintMe(FILE *fd, int nTabs) const override; // MHProgram
};

// Interchange program - not needed for UK MHEG
class MHInterChgProgram : public MHProgram  
{
  public:
    MHInterChgProgram() = default;
    const char *ClassName() override // MHRoot
        { return "InterChgProgram"; }
    ~MHInterChgProgram() override = default;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHProgram
    void PrintMe(FILE *fd, int nTabs) const override; // MHProgram
};

// Call and Fork - call a "program".
class MHCall: public MHElemAction
{
  public:
    MHCall(const char *name, bool fIsFork): MHElemAction(name), m_fIsFork(fIsFork) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    bool m_fIsFork;
    MHObjectRef m_Succeeded; // Boolean variable set to call result
    MHOwnPtrSequence<MHParameter> m_Parameters; // Arguments.
};

#endif
