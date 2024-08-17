/* Ingredients.cpp

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

#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Actions.h"
#include "Engine.h"
#include "Logging.h"


// Copy constructor for cloning.
MHIngredient::MHIngredient(const MHIngredient &ref)
  : MHRoot(ref),
    m_fInitiallyActive(ref.m_fInitiallyActive),
    m_nContentHook(ref.m_nContentHook),
    m_fShared(ref.m_fShared),
    m_contentType(ref.m_contentType),
    m_nOrigContentSize(ref.m_nOrigContentSize),
    m_nOrigCCPrio(ref.m_nOrigCCPrio),
    m_nContentSize(ref.m_nContentSize),
    m_nCCPrio(ref.m_nCCPrio)
{
    // Don't copy the object reference since that's set separately.
    m_origIncludedContent.Copy(ref.m_origIncludedContent);
    m_origContentRef.Copy(ref.m_origContentRef);
}


void MHIngredient::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHRoot::Initialise(p, engine);
    MHParseNode *pIA = p->GetNamedArg(C_INITIALLY_ACTIVE);

    if (pIA)
    {
        m_fInitiallyActive = pIA->GetArgN(0)->GetBoolValue();
    }

    MHParseNode *pCHook = p->GetNamedArg(C_CONTENT_HOOK);

    if (pCHook)
    {
        m_nContentHook = pCHook->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pOrigContent = p->GetNamedArg(C_ORIGINAL_CONTENT);

    if (pOrigContent)
    {
        MHParseNode *pArg = pOrigContent->GetArgN(0);

        // Either a string - included content.
        if (pArg->m_nNodeType == MHParseNode::PNString)
        {
            m_contentType = IN_IncludedContent;
            pArg->GetStringValue(m_origIncludedContent);
        }
        else   // or a sequence - referenced content.
        {
            // In the text version this is tagged with :ContentRef
            m_contentType = IN_ReferencedContent;
            m_origContentRef.Initialise(pArg->GetArgN(0), engine);
            MHParseNode *pContentSize = pArg->GetNamedArg(C_CONTENT_SIZE);

            if (pContentSize)
            {
                m_nOrigContentSize = pContentSize->GetArgN(0)->GetIntValue();
            }

            MHParseNode *pCCPrio = pArg->GetNamedArg(C_CONTENT_CACHE_PRIORITY);

            if (pCCPrio)
            {
                m_nOrigCCPrio = pCCPrio->GetArgN(0)->GetIntValue();
            }
        }
    }

    MHParseNode *pShared = p->GetNamedArg(C_SHARED);

    if (pShared)
    {
        m_fShared = pShared->GetArgN(0)->GetBoolValue();
    }
}

void MHIngredient::PrintMe(FILE *fd, int nTabs) const
{
    MHRoot::PrintMe(fd, nTabs);

    if (! m_fInitiallyActive)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":InitiallyActive false\n");
    }

    if (m_nContentHook != 0)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":CHook %d\n", m_nContentHook);
    }

    // Original content
    if (m_contentType == IN_IncludedContent)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigContent ");
        m_origIncludedContent.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }
    else if (m_contentType == IN_ReferencedContent)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigContent (");
        m_origContentRef.PrintMe(fd, nTabs + 1);

        if (m_nOrigContentSize)
        {
            fprintf(fd, " :ContentSize %d", m_nOrigContentSize);
        }

        if (m_nOrigCCPrio != 127)
        {
            fprintf(fd, " :CCPriority %d", m_nOrigCCPrio);
        }

        fprintf(fd, " )\n");
    }

    if (m_fShared)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":Shared true\n");
    }
}


// Prepare the object as for the parent.
void MHIngredient::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    // Initialise the content information if any.
    m_includedContent.Copy(m_origIncludedContent);
    m_contentRef.Copy(m_origContentRef);
    m_nContentSize = m_nOrigContentSize;
    m_nCCPrio = m_nOrigCCPrio;
    // Prepare the base class.
    MHRoot::Preparation(engine);
}

void MHIngredient::Destruction(MHEngine *engine)
{
    engine->CancelExternalContentRequest(this);
    MHRoot::Destruction(engine);
}

void MHIngredient::ContentPreparation(MHEngine *engine)
{
    if (m_contentType == IN_IncludedContent)
    {
        // Included content is there - generate ContentAvailable.
        engine->EventTriggered(this, EventContentAvailable);
    }
    else if (m_contentType == IN_ReferencedContent)
    {
        // We are requesting external content
        engine->CancelExternalContentRequest(this);
        engine->RequestExternalContent(this);
    }
}

// Provide updated content.
void MHIngredient::SetData(const MHOctetString &included, MHEngine *engine)
{
    // If the content is currently Included then the data should be Included
    // and similarly for Referenced content.  I've seen cases where SetData
    // with included content has been used erroneously with the intention that
    // this should be the file name for referenced content.
    if (m_contentType == IN_ReferencedContent)
    {
        m_contentRef.m_contentRef.Copy(included);
    }
    else if (m_contentType == IN_IncludedContent)
    {
        m_includedContent.Copy(included);

    }
    else
    {
        MHLOG(MHLogWarning, "SetData with no content");    // MHEG Error
    }

    ContentPreparation(engine);
}

void MHIngredient::SetData(const MHContentRef &referenced, bool /*fSizeGiven*/, int size, bool fCCGiven, int /*cc*/, MHEngine *engine)
{
    if (m_contentType != IN_ReferencedContent)
    {
        MHERROR("SetData with referenced content applied to an ingredient without referenced content");
    }

    m_contentRef.Copy(referenced);
    m_nContentSize = size;

    if (fCCGiven)
    {
        m_nCCPrio = m_nOrigCCPrio;
    }

    ContentPreparation(engine);
}

// Font
void MHFont::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHIngredient::Initialise(p, engine);
    //
}

void MHFont::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Font");
    MHIngredient::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}


// CursorShape

void MHCursorShape::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHIngredient::Initialise(p, engine);
    //
}

void MHCursorShape::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:CursorShape");
    MHIngredient::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// Palette

void MHPalette::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHIngredient::Initialise(p, engine);
    //
}

void MHPalette::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Palette");
    MHIngredient::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHSetData::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    MHParseNode *pContent = p->GetArgN(1);

    if (pContent->m_nNodeType == MHParseNode::PNSeq)
    {
        // Referenced content.
        m_fIsIncluded = false;
        m_fSizePresent = m_fCCPriorityPresent = false;
        m_referenced.Initialise(pContent->GetSeqN(0), engine);

        if (pContent->GetSeqCount() > 1)
        {
            MHParseNode *pArg = pContent->GetSeqN(1);

            if (pArg->m_nNodeType == MHParseNode::PNTagged && pArg->GetTagNo() == C_NEW_CONTENT_SIZE)
            {
                MHParseNode *pVal = pArg->GetArgN(0);

                // It may be NULL as a place-holder
                if (pVal->m_nNodeType == MHParseNode::PNInt)
                {
                    m_fSizePresent = true;
                    m_contentSize.Initialise(pVal, engine);
                }
            }
        }

        if (pContent->GetSeqCount() > 2)
        {
            MHParseNode *pArg = pContent->GetSeqN(2);

            if (pArg->m_nNodeType == MHParseNode::PNTagged && pArg->GetTagNo() == C_NEW_CONTENT_CACHE_PRIO)
            {
                MHParseNode *pVal = pArg->GetArgN(0);

                if (pVal->m_nNodeType == MHParseNode::PNInt)
                {
                    m_fCCPriorityPresent = true;
                    m_ccPriority.Initialise(pVal, engine);
                }
            }
        }
    }
    else
    {
        m_included.Initialise(pContent, engine);
        m_fIsIncluded = true;
    }
}

void MHSetData::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    if (m_fIsIncluded)
    {
        m_included.PrintMe(fd, 0);
    }
    else
    {
        m_referenced.PrintMe(fd, 0);

        if (m_fSizePresent)
        {
            fprintf(fd, " :NewContentSize ");
            m_contentSize.PrintMe(fd, 0);
        }

        if (m_fCCPriorityPresent)
        {
            fprintf(fd, " :NewCCPriority ");
            m_ccPriority.PrintMe(fd, 0);
        }
    }
}

void MHSetData::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_target.GetValue(target, engine); // Get the target

    if (m_fIsIncluded)   // Included content
    {
        MHOctetString included;
        m_included.GetValue(included, engine);
        engine->FindObject(target)->SetData(included, engine);
    }
    else
    {
        MHContentRef referenced;
        int size = 0;
        int cc = 0;
        m_referenced.GetValue(referenced, engine);

        if (m_fSizePresent)
        {
            size = m_contentSize.GetValue(engine);
        }

        if (m_fCCPriorityPresent)
        {
            cc = m_ccPriority.GetValue(engine);
        }

        engine->FindObject(target)->SetData(referenced, m_fSizePresent, size, m_fCCPriorityPresent, cc, engine);
    }
}

// Clone - make a copy of the target and set the object reference variable to the new object ref.
void MHClone::CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef)
{
    // We need to get the group (scene or application) that contains the target.
    MHObjectRef groupRef;
    groupRef.m_groupId.Copy(pTarget->m_ObjectReference.m_groupId);
    groupRef.m_nObjectNo = 0; // The group always has object ref zero.
    MHRoot *pGroup = engine->FindObject(groupRef);
    // Get the group to make the clone and add it to its ingredients.
    pGroup->MakeClone(pTarget, pRef, engine);
}
