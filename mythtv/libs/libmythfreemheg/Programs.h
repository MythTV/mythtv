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
    MHProgram();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual bool InitiallyAvailable() { return m_fInitiallyAvailable; }
    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);

    // Action - Stop can be used to stop the code.
    virtual void Stop(MHEngine *engine) { Deactivation(engine); }
  protected:
    MHOctetString m_Name; // Name of the program
    bool    m_fInitiallyAvailable;
};

// Resident program
class MHResidentProgram : public MHProgram  
{
  public:
    MHResidentProgram() {}
    virtual const char *ClassName() { return "ResidentProgram"; }
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void CallProgram(bool fIsFork, const MHObjectRef &success,
        const MHSequence<MHParameter *> &args, MHEngine *engine);
};

// Remote program - not needed for UK MHEG
class MHRemoteProgram : public MHProgram  
{
  public:
    MHRemoteProgram();
    virtual const char *ClassName() { return "RemoteProgram"; }
    virtual ~MHRemoteProgram();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
};

// Interchange program - not needed for UK MHEG
class MHInterChgProgram : public MHProgram  
{
  public:
    MHInterChgProgram();
    virtual const char *ClassName() { return "InterChgProgram"; }
    virtual ~MHInterChgProgram();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
};

// Call and Fork - call a "program".
class MHCall: public MHElemAction
{
  public:
    MHCall(const char *name, bool fIsFork): MHElemAction(name), m_fIsFork(fIsFork) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int nTabs) const;
    bool m_fIsFork;
    MHObjectRef m_Succeeded; // Boolean variable set to call result
    MHOwnPtrSequence<MHParameter> m_Parameters; // Arguments.
};

#endif
