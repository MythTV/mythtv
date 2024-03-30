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
    MHTokenGroupItem() = default;
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
    MHSequence <int> m_movement;
};

// TokenGroup.  The standard defines a TokenManager class but that is incorporated in
// this class.
class MHTokenGroup : public MHPresentable  
{
  public:
    MHTokenGroup() = default;
    const char *ClassName() override // MHRoot
        { return "TokenGroup"; }
   void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot

    // Actions
    void CallActionSlot(int n, MHEngine *engine) override; // MHRoot
    void Move(int n, MHEngine *engine) override; // MHRoot
    void MoveTo(int n, MHEngine *engine) override // MHRoot
        { TransferToken(n, engine); }
    void GetTokenPosition(MHRoot *pResult, MHEngine */*engine*/) override // MHRoot
        { pResult->SetVariableValue(m_nTokenPosition); }

  protected:
    void PrintContents(FILE *fd, int nTabs) const;
    void TransferToken(int newPos, MHEngine *engine);

    MHOwnPtrSequence <MHMovement> m_movementTable;
    MHOwnPtrSequence <MHTokenGroupItem> m_tokenGrpItems;
    MHOwnPtrSequence <MHActionSequence> m_noTokenActionSlots;

    // Internal attributes
    int m_nTokenPosition {1};  // Initial value
};

// Items in the list group consist of a Visible and a "selected" flag.
// For simplicity we don't check that we have a Visible and instead use virtual functions
// which only work on visibles.
class MHListItem {
  public:
    explicit MHListItem(MHRoot *pVis): m_pVisible(pVis) {}
    MHRoot *m_pVisible { nullptr };
    bool   m_fSelected { false   };
};

class MHListGroup : public MHTokenGroup  
{
  public:
    MHListGroup() = default;
    ~MHListGroup() override;
    const char *ClassName() override // MHTokenGroup
        { return "ListGroup"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHTokenGroup
    void PrintMe(FILE *fd, int nTabs) const override; // MHTokenGroup
    void Preparation(MHEngine *engine) override; // MHIngredient
    void Destruction(MHEngine *engine) override; // MHIngredient
    void Activation(MHEngine *engine) override; // MHTokenGroup
    void Deactivation(MHEngine *engine) override; // MHTokenGroup

    // Actions
    void AddItem(int nIndex, MHRoot *pItem, MHEngine *engine) override; // MHRoot
    void DelItem(MHRoot *pItem, MHEngine *engine) override; // MHRoot
    void GetCellItem(int nCell, const MHObjectRef &itemDest, MHEngine *engine) override; // MHRoot
    void GetListItem(int nCell, const MHObjectRef &itemDest, MHEngine *engine) override; // MHRoot
    void GetItemStatus(int nCell, const MHObjectRef &itemDest, MHEngine *engine) override; // MHRoot
    void SelectItem(int nCell, MHEngine *engine) override; // MHRoot
    void DeselectItem(int nCell, MHEngine *engine) override; // MHRoot
    void ToggleItem(int nCell, MHEngine *engine) override; // MHRoot
    void ScrollItems(int nCell, MHEngine *engine) override; // MHRoot
    void SetFirstItem(int nCell, MHEngine *engine) override; // MHRoot
    void GetFirstItem(MHRoot *pResult, MHEngine */*engine*/) override // MHRoot
        { pResult->SetVariableValue(m_nFirstItem); }
    void GetListSize(MHRoot *pResult, MHEngine */*engine*/) override // MHRoot
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        { pResult->SetVariableValue(m_itemList.size()); }
#else
        { pResult->SetVariableValue(static_cast<int>(m_itemList.size())); }
#endif

  protected:
    // MHEG Internal attributes.
    void Update(MHEngine *engine);
    void Select(int nIndex, MHEngine *engine);
    void Deselect(int nIndex, MHEngine *engine);
    int AdjustIndex(int nIndex); // Added in the MHEG corrigendum

    // Exchanged attributes
    MHSequence <QPoint> m_positions;
    bool    m_fWrapAround        {false};
    bool    m_fMultipleSelection {false};
    //Internal attributes
    QList<MHListItem*> m_itemList; // Items found by looking up the object refs
    int  m_nFirstItem {1}; // First item displayed - N.B. MHEG indexes from 1.
    bool m_fFirstItemDisplayed   {false};
    bool m_fLastItemDisplayed    {false};
    int  m_nLastCount            {0};
    int  m_nLastFirstItem        {m_nFirstItem};
};

// Call action slot.
class MHCallActionSlot: public MHActionInt
{
  public:
    MHCallActionSlot(): MHActionInt(":CallActionSlot")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->CallActionSlot(nArg, engine); }
};

// Move - move the token in a token group according to the movement table.
class MHMove: public MHActionInt
{
  public:
    MHMove(): MHActionInt(":Move")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->Move(nArg, engine); }
};

// MoveTo - move the token to a particular slot.
class MHMoveTo: public MHActionInt
{
  public:
    MHMoveTo(): MHActionInt(":MoveTo")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->MoveTo(nArg, engine); }
};

class MHGetTokenPosition: public MHActionObjectRef {
  public:
    MHGetTokenPosition(): MHActionObjectRef(":GetTokenPosition") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) override // MHActionObjectRef
        { pTarget->GetTokenPosition(pArg, engine); }
};

class MHAddItem: public MHElemAction {
  public:
    MHAddItem(): MHElemAction(":AddItem") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int /*nTabs*/) const override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    MHGenericInteger m_index;
    MHGenericObjectRef m_item;

};

class MHDelItem: public MHActionGenericObjectRef {
  public:
    MHDelItem(): MHActionGenericObjectRef(":DelItem") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pObj) override // MHActionGenericObjectRef
        { pTarget->DelItem(pObj, engine); }
};

// Base class for a few actions.
class MHGetListActionData: public MHElemAction {
  public:
    explicit MHGetListActionData(const char *name): MHElemAction(name) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int /*nTabs*/) const override; // MHElemAction
  protected:
    MHGenericInteger m_index;
    MHObjectRef m_result;
};

class MHGetCellItem: public MHGetListActionData {
  public:
    MHGetCellItem(): MHGetListActionData(":GetCellItem") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->GetCellItem(m_index.GetValue(engine), m_result, engine); }
};

class MHGetListItem: public MHGetListActionData {
  public:
    MHGetListItem(): MHGetListActionData(":GetListItem") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->GetListItem(m_index.GetValue(engine), m_result, engine); }
};

class MHGetItemStatus: public MHGetListActionData {
  public:
    MHGetItemStatus(): MHGetListActionData(":GetItemStatus") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->GetItemStatus(m_index.GetValue(engine), m_result, engine); }
};

class MHSelectItem: public MHActionInt {
  public:
    MHSelectItem(): MHActionInt(":SelectItem") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) override // MHActionInt
        { Target(engine)->SelectItem(nArg, engine); }
};

class MHDeselectItem: public MHActionInt {
  public:
    MHDeselectItem(): MHActionInt(":DeselectItem") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) override // MHActionInt
        { Target(engine)->DeselectItem(nArg, engine); }
};

class MHToggleItem: public MHActionInt {
  public:
    MHToggleItem(): MHActionInt(":ToggleItem") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) override // MHActionInt
        { Target(engine)->ToggleItem(nArg, engine); }
};

class MHScrollItems: public MHActionInt {
  public:
    MHScrollItems(): MHActionInt(":ScrollItems") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) override // MHActionInt
        { Target(engine)->ScrollItems(nArg, engine); }
};

class MHSetFirstItem: public MHActionInt {
  public:
    MHSetFirstItem(): MHActionInt(":SetFirstItem") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, int nArg) override // MHActionInt
        { Target(engine)->SetFirstItem(nArg, engine); }
};

class MHGetFirstItem: public MHActionObjectRef {
  public:
    MHGetFirstItem(): MHActionObjectRef(":GetFirstItem") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, MHRoot *pArg) override // MHActionObjectRef
        { Target(engine)->GetFirstItem(pArg, engine); }
};

class MHGetListSize: public MHActionObjectRef {
  public:
    MHGetListSize(): MHActionObjectRef(":GetListSize") {}
    void CallAction(MHEngine *engine, MHRoot * /*pTarget*/, MHRoot *pArg) override // MHActionObjectRef
        { Target(engine)->GetListSize(pArg, engine); }
};

#endif
