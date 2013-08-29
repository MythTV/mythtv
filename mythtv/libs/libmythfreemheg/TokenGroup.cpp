/* TokenGroup.cpp

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


#include "TokenGroup.h"
#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"

void MHTokenGroupItem::Initialise(MHParseNode *p, MHEngine *engine)
{
    // A pair consisting of an object reference and an optional action slot sequence.
    m_Object.Initialise(p->GetSeqN(0), engine);

    if (p->GetSeqCount() > 1)
    {
        MHParseNode *pSlots = p->GetSeqN(1);

        for (int i = 0; i < pSlots->GetSeqCount(); i++)
        {
            MHParseNode *pAct = pSlots->GetSeqN(i);
            MHActionSequence *pActions = new MHActionSequence;
            m_ActionSlots.Append(pActions);

            // The action slot entry may be NULL.
            if (pAct->m_nNodeType != MHParseNode::PNNull)
            {
                pActions->Initialise(pAct, engine);
            }
        }
    }
}

void MHTokenGroupItem::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "( ");
    m_Object.PrintMe(fd, nTabs + 1);
    fprintf(fd, "\n");

    if (m_ActionSlots.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":ActionSlots (\n");

        for (int i = 0; i < m_ActionSlots.Size(); i++)
        {
            PrintTabs(fd, nTabs + 2);
            fprintf(fd, "( // slot %d\n", i);
            MHActionSequence *pActions = m_ActionSlots.GetAt(i);

            if (pActions->Size() == 0)
            {
                PrintTabs(fd, nTabs + 2);
                fprintf(fd, "NULL\n");
            }
            else
            {
                pActions->PrintMe(fd, nTabs + 2);
            }

            PrintTabs(fd, nTabs + 2);
            fprintf(fd, ")\n");
        }

        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ")\n");
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, ")\n");
}

void MHMovement::Initialise(MHParseNode *p, MHEngine * /*engine*/)
{
    for (int i = 0; i < p->GetSeqCount(); i++)
    {
        m_Movement.Append(p->GetSeqN(i)->GetIntValue());
    }
}

void MHMovement::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "( ");

    for (int i = 0; i < m_Movement.Size(); i++)
    {
        fprintf(fd, "%d ", m_Movement.GetAt(i));
    }

    fprintf(fd, ")\n");
}

MHTokenGroup::MHTokenGroup()
{
    m_nTokenPosition = 1; // Initial value
}

void MHTokenGroup::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPresentable::Initialise(p, engine);
    MHParseNode *pMovements = p->GetNamedArg(C_MOVEMENT_TABLE);

    if (pMovements)
    {
        for (int i = 0; i < pMovements->GetArgCount(); i++)
        {
            MHMovement *pMove = new MHMovement;
            m_MovementTable.Append(pMove);
            pMove->Initialise(pMovements->GetArgN(i), engine);
        }
    }

    MHParseNode *pTokenGrp = p->GetNamedArg(C_TOKEN_GROUP_ITEMS);

    if (pTokenGrp)
    {
        for (int i = 0; i < pTokenGrp->GetArgCount(); i++)
        {
            MHTokenGroupItem *pToken = new MHTokenGroupItem;
            m_TokenGrpItems.Append(pToken);
            pToken->Initialise(pTokenGrp->GetArgN(i), engine);
        }
    }

    MHParseNode *pNoToken = p->GetNamedArg(C_NO_TOKEN_ACTION_SLOTS);

    if (pNoToken)
    {
        for (int i = 0; i < pNoToken->GetArgCount(); i++)
        {
            MHParseNode *pAct = pNoToken->GetArgN(i);
            MHActionSequence *pActions = new MHActionSequence;
            m_NoTokenActionSlots.Append(pActions);

            // The action slot entry may be NULL.
            if (pAct->m_nNodeType != MHParseNode::PNNull)
            {
                pActions->Initialise(pAct, engine);
            }
        }
    }
}

// This is used to print the contents of the token group.
void MHTokenGroup::PrintContents(FILE *fd, int nTabs) const
{
    MHPresentable::PrintMe(fd, nTabs + 1);

    if (m_MovementTable.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":MovementTable (\n");

        for (int i = 0; i < m_MovementTable.Size(); i++)
        {
            m_MovementTable.GetAt(i)->PrintMe(fd, nTabs + 2);
        }

        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ")\n");
    }

    if (m_TokenGrpItems.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TokenGroupItems (\n");

        for (int i = 0; i < m_TokenGrpItems.Size(); i++)
        {
            m_TokenGrpItems.GetAt(i)->PrintMe(fd, nTabs + 2);
        }

        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ")\n");
    }

    if (m_NoTokenActionSlots.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":NoTokenActionSlots (\n");

        for (int i = 0; i < m_NoTokenActionSlots.Size(); i++)
        {
            MHActionSequence *pActions = m_NoTokenActionSlots.GetAt(i);

            if (pActions->Size() == 0)
            {
                PrintTabs(fd, nTabs + 2);
                fprintf(fd, "NULL ");
            }
            else
            {
                pActions->PrintMe(fd, nTabs + 2);
            }
        }

        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ")\n");
    }

}

void MHTokenGroup::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:TokenGroup ");
    PrintContents(fd, nTabs);
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// Activate the token group following the standard.
void MHTokenGroup::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHPresentable::Activation(engine);

    // We're supposed to apply Activation to each of the "items" but it isn't clear
    // exactly what that means.  Assume it means each of the visibles.
    for (int i = 0; i < m_TokenGrpItems.Size(); i++)
    {
        MHObjectRef *pObject = &m_TokenGrpItems.GetAt(i)->m_Object;

        // The object reference may be the null reference.
        // Worse: it seems that sometimes in BBC's MHEG the reference simply doesn't exist.
        if (pObject->IsSet())
        {
            try
            {
                engine->FindObject(m_TokenGrpItems.GetAt(i)->m_Object)->Activation(engine);
            }
            catch (char const *) {}
        }
    }

    engine->EventTriggered(this, EventTokenMovedTo, m_nTokenPosition);
    m_fRunning = true;
    engine->EventTriggered(this, EventIsRunning);
}

void MHTokenGroup::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    engine->EventTriggered(this, EventTokenMovedFrom, m_nTokenPosition);
    MHPresentable::Deactivation(engine);
}

// Internal function to generate the appropriate events.
void MHTokenGroup::TransferToken(int newPos, MHEngine *engine)
{
    if (newPos != m_nTokenPosition)
    {
        engine->EventTriggered(this, EventTokenMovedFrom, m_nTokenPosition);
        m_nTokenPosition = newPos;
        engine->EventTriggered(this, EventTokenMovedTo, m_nTokenPosition);
    }
}

// Perform the actions depending on where the token is.
void MHTokenGroup::CallActionSlot(int n, MHEngine *engine)
{
    if (m_nTokenPosition == 0)   // No slot has the token.
    {
        if (n > 0 && n <= m_NoTokenActionSlots.Size())
        {
            engine->AddActions(*(m_NoTokenActionSlots.GetAt(n - 1)));
        }
    }
    else
    {
        if (m_nTokenPosition > 0 && m_nTokenPosition <= m_TokenGrpItems.Size())
        {
            MHTokenGroupItem *pGroup = m_TokenGrpItems.GetAt(m_nTokenPosition - 1);

            if (n > 0 && n <= pGroup->m_ActionSlots.Size())
            {
                engine->AddActions(*(pGroup->m_ActionSlots.GetAt(n - 1)));
            }
        }
    }
}

void MHTokenGroup::Move(int n, MHEngine *engine)
{
    if (m_nTokenPosition == 0 || n < 1 || n > m_MovementTable.Size())
    {
        TransferToken(0, engine);    // Not in the standard
    }
    else
    {
        TransferToken(m_MovementTable.GetAt(n - 1)->m_Movement.GetAt(m_nTokenPosition - 1), engine);
    }
}

// ListGroup.  This is a complex class and the description was extensively revised in the MHEG corrigendum.
// It doesn't seem to be used a great deal in practice and quite a few of the actions haven't been tested.

MHListGroup::MHListGroup()
{
    m_fWrapAround = false;
    m_fMultipleSelection = false;
    m_nFirstItem = 1;
    m_nLastFirstItem = m_nFirstItem;
    m_nLastCount = 0;
    m_fFirstItemDisplayed = false;
    m_fLastItemDisplayed = false;
}
MHListGroup::~MHListGroup()
{
    while (!m_ItemList.isEmpty())
    {
        delete m_ItemList.takeFirst();
    }
}

void MHListGroup::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHTokenGroup::Initialise(p, engine);
    MHParseNode *pPositions = p->GetNamedArg(C_POSITIONS);

    if (pPositions)
    {
        for (int i = 0; i < pPositions->GetArgCount(); i++)
        {
            MHParseNode *pPos = pPositions->GetArgN(i);
            QPoint pos(pPos->GetSeqN(0)->GetIntValue(), pPos->GetSeqN(1)->GetIntValue());
            m_Positions.Append(pos);
        }
    }

    MHParseNode *pWrap = p->GetNamedArg(C_WRAP_AROUND);

    if (pWrap)
    {
        m_fWrapAround = pWrap->GetArgN(0)->GetBoolValue();
    }

    MHParseNode *pMultiple = p->GetNamedArg(C_WRAP_AROUND);

    if (pMultiple)
    {
        m_fMultipleSelection = pMultiple->GetArgN(0)->GetBoolValue();
    }
}

void MHListGroup::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:ListGroup ");
    MHTokenGroup::PrintContents(fd, nTabs);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":Positions (");

    for (int i = 0; i < m_Positions.Size(); i++)
    {
        fprintf(fd, " ( %d %d )", m_Positions.GetAt(i).x(), m_Positions.GetAt(i).y());
    }

    fprintf(fd, ")\n");

    if (m_fWrapAround)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":WrapAround true\n");
    }

    if (m_fMultipleSelection)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":MultipleSelection true\n");
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHListGroup::Preparation(MHEngine *engine)
{
    MHTokenGroup::Preparation(engine);

    for (int i = 0; i < m_TokenGrpItems.Size(); i++)
    {
        // Find the item and add it to the list if it isn't already there.
        try
        {
            MHRoot *pItem = (MHRoot *)engine->FindObject(m_TokenGrpItems.GetAt(i)->m_Object);
            MHListItem *p = 0;
            QList<MHListItem *>::iterator it = m_ItemList.begin();

            for (; it != m_ItemList.end(); ++it)
            {
                p = *it;

                if (p->m_pVisible == pItem)
                {
                    break;
                }
            }

            if (!p)
            {
                m_ItemList.append(new MHListItem(pItem));
            }
        }
        catch (...)  // Ignore invalid or null objects.
        {
        }
    }
}

void MHListGroup::Destruction(MHEngine *engine)
{
    // Reset the positions of the visibles.
    for (int j = 0; j < m_ItemList.size(); j++)
    {
        m_ItemList.at(j)->m_pVisible->ResetPosition();
    }

    MHTokenGroup::Destruction(engine);
}

void MHListGroup::Activation(MHEngine *engine)
{
    m_fFirstItemDisplayed = m_fLastItemDisplayed = false;
    MHTokenGroup::Activation(engine);
    Update(engine);
}


void MHListGroup::Deactivation(MHEngine *engine)
{
    // Deactivate the visibles.
    for (int j = 0; j < m_ItemList.size(); j++)
    {
        m_ItemList.at(j)->m_pVisible->Deactivation(engine);
    }

    MHTokenGroup::Deactivation(engine);
}

// Update action - set the position of the cells to be displayed and deactivate those
// which aren't.
void MHListGroup::Update(MHEngine *engine)
{
    if (m_ItemList.isEmpty())   // Special cases when the list becomes empty
    {
        if (m_fFirstItemDisplayed)
        {
            m_fFirstItemDisplayed = false;
            engine->EventTriggered(this, EventFirstItemPresented, false);
        }

        if (m_fLastItemDisplayed)
        {
            m_fLastItemDisplayed = false;
            engine->EventTriggered(this, EventLastItemPresented, false);
        }
    }
    else   // Usual case.
    {
        for (int i = 0; i < m_ItemList.size(); i++)
        {
            MHRoot *pVis = m_ItemList.at(i)->m_pVisible;
            int nCell = i + 1 - m_nFirstItem; // Which cell does this item map onto?

            if (nCell >= 0 && nCell < m_Positions.Size())
            {
                if (i == 0 && ! m_fFirstItemDisplayed)
                {
                    m_fFirstItemDisplayed = true;
                    engine->EventTriggered(this, EventFirstItemPresented, true);
                }

                if (i == m_ItemList.size() - 1 && ! m_fLastItemDisplayed)
                {
                    m_fLastItemDisplayed = true;
                    engine->EventTriggered(this, EventLastItemPresented, true);
                }

                try
                {
                    pVis->SetPosition(m_Positions.GetAt(i - m_nFirstItem + 1).x(), m_Positions.GetAt(i - m_nFirstItem + 1).y(), engine);
                }
                catch (...) {}

                if (! pVis->GetRunningStatus())
                {
                    pVis->Activation(engine);
                }
            }
            else
            {
                if (i == 0 && m_fFirstItemDisplayed)
                {
                    m_fFirstItemDisplayed = false;
                    engine->EventTriggered(this, EventFirstItemPresented, false);
                }

                if (i == m_ItemList.size() - 1 && m_fLastItemDisplayed)
                {
                    m_fLastItemDisplayed = false;
                    engine->EventTriggered(this, EventLastItemPresented, false);
                }

                if (pVis->GetRunningStatus())
                {
                    pVis->Deactivation(engine);
                    pVis->ResetPosition();
                }
            }
        }
    }

    // Generate the HeadItems and TailItems events.  Even in the MHEG corrigendum this is unclear.
    // I'm not at all sure this is right.
    if (m_nLastFirstItem != m_nFirstItem)
    {
        engine->EventTriggered(this, EventHeadItems, m_nFirstItem);
    }

    if (m_nLastCount - m_nLastFirstItem != m_ItemList.size() - m_nFirstItem)
    {
        engine->EventTriggered(this, EventTailItems, m_ItemList.size() - m_nFirstItem);
    }

    m_nLastCount = m_ItemList.size();
    m_nLastFirstItem = m_nFirstItem;
}

// Add an item to the list
void MHListGroup::AddItem(int nIndex, MHRoot *pItem, MHEngine *engine)
{
    // See if the item is already there and ignore this if it is.
    QList<MHListItem *>::iterator it = m_ItemList.begin();

    for (; it != m_ItemList.end(); ++it)
    {
        if ((*it)->m_pVisible == pItem)
        {
            return;
        }
    }

    // Ignore this if the index is out of range
    if (nIndex < 1 || nIndex > m_ItemList.size() + 1)
    {
        return;
    }

    // Insert it at the appropriate position (MHEG indexes count from 1).
    m_ItemList.insert(nIndex - 1, new MHListItem(pItem));

    if (nIndex <= m_nFirstItem && m_nFirstItem < m_ItemList.size())
    {
        m_nFirstItem++;
    }

    Update(engine); // Apply the update behaviour
}

// Remove an item from the list
void MHListGroup::DelItem(MHRoot *pItem, MHEngine *)
{
    // See if the item is already there and ignore this if it is.
    for (int i = 0; i < m_ItemList.size(); i++)
    {
        if (m_ItemList.at(i)->m_pVisible == pItem)   // Found it - remove it from the list and reset the posn.
        {
            delete m_ItemList.takeAt(i);
            pItem->ResetPosition();

            if (i + 1 < m_nFirstItem && m_nFirstItem > 1)
            {
                m_nFirstItem--;
            }

            return;
        }
    }
}

// Set the selection status of the item to true
void MHListGroup::Select(int nIndex, MHEngine *engine)
{
    MHListItem *pListItem = m_ItemList.at(nIndex - 1);

    if (pListItem == 0 || pListItem->m_fSelected)
    {
        return;    // Ignore if already selected.
    }

    if (! m_fMultipleSelection)
    {
        // Deselect any existing selections.
        for (int i = 0; i < m_ItemList.size(); i++)
            if (m_ItemList.at(i)->m_fSelected)
            {
                Deselect(i + 1, engine);
            }
    }

    pListItem->m_fSelected = true;
    engine->EventTriggered(this, EventItemSelected, nIndex);
}

// Set the selection status of the item to false
void MHListGroup::Deselect(int nIndex, MHEngine *engine)
{
    MHListItem *pListItem = m_ItemList.at(nIndex - 1);

    if (pListItem == 0 || ! pListItem->m_fSelected)
    {
        return;    // Ignore if not selected.
    }

    pListItem->m_fSelected = false;
    engine->EventTriggered(this, EventItemDeselected, nIndex);
}

// Return the reference to the visible at the particular position.
void MHListGroup::GetCellItem(int nCell, const MHObjectRef &itemDest, MHEngine *engine)
{
    if (nCell < 1)
    {
        nCell = 1;    // First cell
    }

    if (nCell > m_Positions.Size())
    {
        nCell = m_Positions.Size();    // Last cell.
    }

    int nVisIndex = nCell + m_nFirstItem - 2;

    if (nVisIndex >= 0 && nVisIndex < m_ItemList.size())
    {
        MHRoot *pVis = m_ItemList.at(nVisIndex)->m_pVisible;
        engine->FindObject(itemDest)->SetVariableValue(pVis->m_ObjectReference);
    }
    else
    {
        engine->FindObject(itemDest)->SetVariableValue(MHObjectRef::Null);
    }
}

int MHListGroup::AdjustIndex(int nIndex) // Added in the MHEG corrigendum
{
    int nItems = m_ItemList.size();

    if (nItems == 0)
    {
        return 1;
    }

    if (nIndex > nItems)
    {
        return ((nIndex - 1) % nItems) + 1;
    }
    else if (nIndex < 0)
    {
        return nItems - ((-nIndex) % nItems);
    }
    else
    {
        return nIndex;
    }
}

void MHListGroup::GetListItem(int nCell, const MHObjectRef &itemDest, MHEngine *engine)
{
    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;    // Ignore it if it's out of range and not wrapping
    }

    engine->FindObject(itemDest)->SetVariableValue(m_ItemList.at(nCell - 1)->m_pVisible->m_ObjectReference);
}

void MHListGroup::GetItemStatus(int nCell, const MHObjectRef &itemDest, MHEngine *engine)
{
    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;
    }

    engine->FindObject(itemDest)->SetVariableValue(m_ItemList.at(nCell - 1)->m_fSelected);
}

void MHListGroup::SelectItem(int nCell, MHEngine *engine)
{
    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;
    }

    Select(nCell, engine);
}

void MHListGroup::DeselectItem(int nCell, MHEngine *engine)
{
    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;
    }

    Deselect(nCell, engine);
}

void MHListGroup::ToggleItem(int nCell, MHEngine *engine)
{
    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;
    }

    if (m_ItemList.at(nCell - 1)->m_fSelected)
    {
        Deselect(nCell, engine);
    }
    else
    {
        Select(nCell, engine);
    }
}

void MHListGroup::ScrollItems(int nCell, MHEngine *engine)
{
    nCell += m_nFirstItem;

    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;
    }

    m_nFirstItem = nCell;
    Update(engine);
}

void MHListGroup::SetFirstItem(int nCell, MHEngine *engine)
{
    if (m_fWrapAround)
    {
        nCell = AdjustIndex(nCell);
    }

    if (nCell < 1 || nCell > m_ItemList.size())
    {
        return;
    }

    m_nFirstItem = nCell;
    Update(engine);
}



// Actions
void MHAddItem::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    m_Index.Initialise(p->GetArgN(1), engine);
    m_Item.Initialise(p->GetArgN(2), engine);
}

void MHAddItem::PrintArgs(FILE *fd, int) const
{
    m_Index.PrintMe(fd, 0);
    m_Item.PrintMe(fd, 0);
}

void MHAddItem::Perform(MHEngine *engine)
{
    MHObjectRef item;
    m_Item.GetValue(item, engine);
    Target(engine)->AddItem(m_Index.GetValue(engine), engine->FindObject(item), engine);
}

void MHGetListActionData::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    m_Index.Initialise(p->GetArgN(1), engine);
    m_Result.Initialise(p->GetArgN(2), engine);
}

void MHGetListActionData::PrintArgs(FILE *fd, int) const
{
    m_Index.PrintMe(fd, 0);
    m_Result.PrintMe(fd, 0);
}
