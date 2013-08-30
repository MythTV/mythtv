/* TokenGroup.h

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


#if !defined(TOKENGROUP_H)
#define TOKENGROUP_H

#include "Presentable.h"
// Dependencies
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "BaseActions.h"
#include "Actions.h"

#include <QPoint>
#include <QList>

class MHEngine;

class MHTokenGroupItem
{
  public:
    MHTokenGroupItem() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    MHObjectRef m_Object;
    MHOwnPtrSequence <MHActionSequence> m_ActionSlots;
};

class MHMovement
{
  public:
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    MHSequence <int> m_Movement;
};

// TokenGroup.  The standard defines a TokenManager class but that is incorporated in
// this class.
class MHTokenGroup : public MHPresentable  
{
  public:
    MHTokenGroup();
    virtual const char *ClassName() { return "TokenGroup"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);

    // Actions
    virtual void CallActionSlot(int n, MHEngine *engine);
    virtual void Move(int n, MHEngine *engine);
    virtual void MoveTo(int n, MHEngine *engine) { TransferToken(n, engine); }
    virtual void GetTokenPosition(MHRoot *pResult, MHEngine *) { pResult->SetVariableValue(m_nTokenPosition); }

  protected:
    void PrintContents(FILE *fd, int nTabs) const;
    void TransferToken(int newPos, MHEngine *engine);

    MHOwnPtrSequence <MHMovement> m_MovementTable;
    MHOwnPtrSequence <MHTokenGroupItem> m_TokenGrpItems;
    MHOwnPtrSequence <MHActionSequence> m_NoTokenActionSlots;

    // Internal attributes
    int m_nTokenPosition;
};

// Items in the list group consist of a Visible and a "selected" flag.
// For simplicity we don't check that we have a Visible and instead use virtual functions
// which only work on visibles.
class MHListItem {
  public:
    MHListItem(MHRoot *pVis): m_pVisible(pVis), m_fSelected(false) {}
    MHRoot *m_pVisible;
    bool   m_fSelected;
};

class MHListGroup : public MHTokenGroup  
{
  public:
    MHListGroup();
    ~MHListGroup();
    virtual const char *ClassName() { return "ListGroup"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Preparation(MHEngine *engine);
    virtual void Destruction(MHEngine *engine);
    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);

    // Actions
    virtual void AddItem(int nIndex, MHRoot *pItem, MHEngine *engine);
    virtual void DelItem(MHRoot *pItem, MHEngine *engine);
    virtual void GetCellItem(int nCell, const MHObjectRef &itemDest, MHEngine *engine);
    virtual void GetListItem(int nCell, const MHObjectRef &itemDest, MHEngine *engine);
    virtual void GetItemStatus(int nCell, const MHObjectRef &itemDest, MHEngine *engine);
    virtual void SelectItem(int nCell, MHEngine *engine);
    virtual void DeselectItem(int nCell, MHEngine *engine);
    virtual void ToggleItem(int nCell, MHEngine *engine);
    virtual void ScrollItems(int nCell, MHEngine *engine);
    virtual void SetFirstItem(int nCell, MHEngine *engine);
    virtual void GetFirstItem(MHRoot *pResult, MHEngine *) { pResult->SetVariableValue(m_nFirstItem); }
    virtual void GetListSize(MHRoot *pResult, MHEngine *) { pResult->SetVariableValue((int)(m_ItemList.size())); }

  protected:
    // MHEG Internal attributes.
    void Update(MHEngine *engine);
    void Select(int nIndex, MHEngine *engine);
    void Deselect(int nIndex, MHEngine *engine);
    int AdjustIndex(int nIndex); // Added in the MHEG corrigendum

    // Exchanged attributes
    MHSequence <QPoint> m_Positions;
    bool    m_fWrapAround, m_fMultipleSelection;
    //Internal attributes
    QList<MHListItem*> m_ItemList; // Items found by looking up the object refs
    int m_nFirstItem; // First item displayed - N.B. MHEG indexes from 1.
    bool m_fFirstItemDisplayed, m_fLastItemDisplayed;
    int m_nLastCount, m_nLastFirstItem;
};

// Call action slot.
class MHCallActionSlot: public MHActionInt
{
  public:
    MHCallActionSlot(): MHActionInt(":CallActionSlot")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->CallActionSlot(nArg, engine); }
};

// Move - move the token in a token group according to the movement table.
class MHMove: public MHActionInt
{
  public:
    MHMove(): MHActionInt(":Move")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->Move(nArg, engine); }
};

// MoveTo - move the token to a particular slot.
class MHMoveTo: public MHActionInt
{
  public:
    MHMoveTo(): MHActionInt(":MoveTo")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->MoveTo(nArg, engine); }
};

class MHGetTokenPosition: public MHActionObjectRef {
  public:
    MHGetTokenPosition(): MHActionObjectRef(":GetTokenPosition") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) { pTarget->GetTokenPosition(pArg, engine); }
};

class MHAddItem: public MHElemAction {
  public:
    MHAddItem(): MHElemAction(":AddItem") {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintArgs(FILE *fd, int) const;
    virtual void Perform(MHEngine *engine);
  protected:
    MHGenericInteger m_Index;
    MHGenericObjectRef m_Item;

};

class MHDelItem: public MHActionGenericObjectRef {
  public:
    MHDelItem(): MHActionGenericObjectRef(":DelItem") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pObj) { pTarget->DelItem(pObj, engine); }
};

// Base class for a few actions.
class MHGetListActionData: public MHElemAction {
  public:
    MHGetListActionData(const char *name): MHElemAction(name) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintArgs(FILE *fd, int) const;
  protected:
    MHGenericInteger m_Index;
    MHObjectRef m_Result;
};

class MHGetCellItem: public MHGetListActionData {
  public:
    MHGetCellItem(): MHGetListActionData(":GetCellItem") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->GetCellItem(m_Index.GetValue(engine), m_Result, engine); }
};

class MHGetListItem: public MHGetListActionData {
  public:
    MHGetListItem(): MHGetListActionData(":GetListItem") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->GetListItem(m_Index.GetValue(engine), m_Result, engine); }
};

class MHGetItemStatus: public MHGetListActionData {
  public:
    MHGetItemStatus(): MHGetListActionData(":GetItemStatus") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->GetItemStatus(m_Index.GetValue(engine), m_Result, engine); }
};

class MHSelectItem: public MHActionInt {
  public:
    MHSelectItem(): MHActionInt(":SelectItem") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) { Target(engine)->SelectItem(nArg, engine); }
};

class MHDeselectItem: public MHActionInt {
  public:
    MHDeselectItem(): MHActionInt(":DeselectItem") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) { Target(engine)->DeselectItem(nArg, engine); }
};

class MHToggleItem: public MHActionInt {
  public:
    MHToggleItem(): MHActionInt(":ToggleItem") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) { Target(engine)->ToggleItem(nArg, engine); }
};

class MHScrollItems: public MHActionInt {
  public:
    MHScrollItems(): MHActionInt(":ScrollItems") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) { Target(engine)->ScrollItems(nArg, engine); }
};

class MHSetFirstItem: public MHActionInt {
  public:
    MHSetFirstItem(): MHActionInt(":SetFirstItem") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) { Target(engine)->SetFirstItem(nArg, engine); }
};

class MHGetFirstItem: public MHActionObjectRef {
  public:
    MHGetFirstItem(): MHActionObjectRef(":GetFirstItem") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, MHRoot *pArg) { Target(engine)->GetFirstItem(pArg, engine); }
};

class MHGetListSize: public MHActionObjectRef {
  public:
    MHGetListSize(): MHActionObjectRef(":GetListSize") {}
    virtual void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, MHRoot *pArg) {Target(engine)->GetListSize(pArg, engine); }
};

#endif
