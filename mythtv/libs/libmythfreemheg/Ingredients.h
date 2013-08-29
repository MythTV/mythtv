/* Ingredients.h

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

#if !defined(INGREDIENTS_H)
#define INGREDIENTS_H

#include "Root.h"
#include "BaseClasses.h"
#include "Actions.h"
#include "BaseActions.h"

class MHParseNode;

// Abstract class for ingredients of a scene or application.
class MHIngredient : public MHRoot  
{
  public:
    MHIngredient();
    MHIngredient(const MHIngredient &ref);
    virtual ~MHIngredient() {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine); // Set this up from the parse tree.
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual bool InitiallyActive() { return m_fInitiallyActive; }
    virtual bool InitiallyAvailable() { return false; } // Used for programs only.
    virtual bool IsShared() { return m_fShared; }

    // Internal behaviours.
    virtual void Preparation(MHEngine *engine);
    virtual void Destruction(MHEngine *engine);
    virtual void ContentPreparation(MHEngine *engine);

    // Actions.
    virtual void SetData(const MHOctetString &included, MHEngine *engine);
    virtual void SetData(const MHContentRef &referenced, bool fSizeGiven, int size, bool fCCGiven, int cc, MHEngine *engine);
    virtual void Preload(MHEngine *engine) { Preparation(engine); } 
    virtual void Unload(MHEngine *engine) { Destruction(engine); }

    // Called by the engine to deliver external content.
    virtual void ContentArrived(const unsigned char *, int, MHEngine *) { }

  protected:
    bool    m_fInitiallyActive;
    int     m_nContentHook;
    bool    m_fShared;
    // Original content.  The original included content and the other fields are
    // mutually exclusive.
    enum { IN_NoContent, IN_IncludedContent, IN_ReferencedContent } m_ContentType;
    MHOctetString   m_OrigIncludedContent;
    MHContentRef    m_OrigContentRef;
    int             m_nOrigContentSize;
    int             m_nOrigCCPrio;
    // Internal attributes
    MHOctetString   m_IncludedContent;
    MHContentRef    m_ContentRef;
    int             m_nContentSize;
    int             m_nCCPrio;
    friend class MHEngine;
};

// Font - not needed for UK MHEG
class MHFont : public MHIngredient  
{
  public:
    MHFont();
    virtual ~MHFont();
    virtual const char *ClassName() { return "Font"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

  protected:
};

// CursorShape - not needed for UK MHEG
class MHCursorShape : public MHIngredient  
{
  public:
    MHCursorShape();
    virtual ~MHCursorShape();
    virtual const char *ClassName() { return "CursorShape"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

  protected:
};

// Paletter - not needed for UK MHEG
class MHPalette : public MHIngredient  
{
  public:
    MHPalette();
    virtual ~MHPalette();
    virtual const char *ClassName() { return "Palette"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

  protected:
};

// Actions.
// SetData - provide new content for an ingredient.
class MHSetData: public MHElemAction
{
  public:
  MHSetData(): MHElemAction(":SetData"), m_fIsIncluded(false),
        m_fSizePresent(false), m_fCCPriorityPresent(false) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
    virtual void PrintArgs(FILE *fd, int nTabs) const;
  protected:
    // Either included content or referenced content.
    bool m_fIsIncluded, m_fSizePresent, m_fCCPriorityPresent;
    MHGenericOctetString m_Included;
    MHGenericContentRef m_Referenced;
    MHGenericInteger m_ContentSize;
    MHGenericInteger m_CCPriority;
};

// Preload and unload
class MHPreload: public MHElemAction
{
  public:
    MHPreload(): MHElemAction(":Preload") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->Preload(engine); }
};

class MHUnload: public MHElemAction
{
  public:
    MHUnload(): MHElemAction(":Unload") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->Unload(engine); }
};

// Clone - make a copy of an existing object.
class MHClone: public MHActionGenericObjectRef
{
  public:
    MHClone(): MHActionGenericObjectRef(":Clone") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef);
};

#endif
