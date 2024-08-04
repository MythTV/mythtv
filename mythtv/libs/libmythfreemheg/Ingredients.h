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
    MHIngredient() = default;
    MHIngredient(const MHIngredient &ref);
    ~MHIngredient() override = default;
    // Set this up from the parse tree.
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHRoot
    void PrintMe(FILE *fd, int nTabs) const override; // MHRoot
    virtual bool InitiallyActive() { return m_fInitiallyActive; }
    virtual bool InitiallyAvailable() { return false; } // Used for programs only.
    bool IsShared() override // MHRoot
        { return m_fShared; }

    // Internal behaviours.
    void Preparation(MHEngine *engine) override; // MHRoot
    void Destruction(MHEngine *engine) override; // MHRoot
    void ContentPreparation(MHEngine *engine) override; // MHRoot

    // Actions.
    void SetData(const MHOctetString &included, MHEngine *engine) override; // MHRoot
    void SetData(const MHContentRef &referenced, bool fSizeGiven,
                 int size, bool fCCGiven, int cc, MHEngine *engine) override; // MHRoot
    void Preload(MHEngine *engine) override // MHRoot
        { Preparation(engine); }
    void Unload(MHEngine *engine) override // MHRoot
        { Destruction(engine); }

    // Called by the engine to deliver external content.
    virtual void ContentArrived(const unsigned char */*data*/, int/*length*/, MHEngine */*engine*/) { }

  protected:
    bool    m_fInitiallyActive         {true}; // Default is true
    int     m_nContentHook             {0};    // Need to choose a value that
                                               // isn't otherwise used
    bool    m_fShared                  {false};
    // Original content.  The original included content and the other fields are
    // mutually exclusive.
    enum : std::uint8_t { IN_NoContent, IN_IncludedContent, IN_ReferencedContent } m_contentType {IN_NoContent};
    MHOctetString   m_origIncludedContent;
    MHContentRef    m_origContentRef;
    int             m_nOrigContentSize {0};
    int             m_nOrigCCPrio      {127};  // Default.
    // Internal attributes
    MHOctetString   m_includedContent;
    MHContentRef    m_contentRef;
    int             m_nContentSize     {0};
    int             m_nCCPrio          {0};
    friend class MHEngine;
};

// Font - not needed for UK MHEG
class MHFont : public MHIngredient  
{
  public:
    MHFont() = default;
    ~MHFont() override = default;
    const char *ClassName() override // MHRoot
        { return "Font"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

  protected:
};

// CursorShape - not needed for UK MHEG
class MHCursorShape : public MHIngredient  
{
  public:
    MHCursorShape() = default;
    ~MHCursorShape() override = default;
    const char *ClassName() override // MHRoot
        { return "CursorShape"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

  protected:
};

// Paletter - not needed for UK MHEG
class MHPalette : public MHIngredient  
{
  public:
    MHPalette() = default;
    ~MHPalette() override = default;
    const char *ClassName() override // MHRoot
        { return "Palette"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

  protected:
};

// Actions.
// SetData - provide new content for an ingredient.
class MHSetData: public MHElemAction
{
  public:
  MHSetData(): MHElemAction(":SetData") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
  protected:
    // Either included content or referenced content.
    bool m_fIsIncluded        {false};
    bool m_fSizePresent       {false};
    bool m_fCCPriorityPresent {false};
    MHGenericOctetString m_included;
    MHGenericContentRef m_referenced;
    MHGenericInteger m_contentSize;
    MHGenericInteger m_ccPriority;
};

// Preload and unload
class MHPreload: public MHElemAction
{
  public:
    MHPreload(): MHElemAction(":Preload") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->Preload(engine); }
};

class MHUnload: public MHElemAction
{
  public:
    MHUnload(): MHElemAction(":Unload") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->Unload(engine); }
};

// Clone - make a copy of an existing object.
class MHClone: public MHActionGenericObjectRef
{
  public:
    MHClone(): MHActionGenericObjectRef(":Clone") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef) override; // MHActionGenericObjectRef
};

#endif
