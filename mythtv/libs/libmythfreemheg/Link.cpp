/* Link.cpp

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
#include "Link.h"

#include <array>
#include <cstdio>

#include <QString>

#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Actions.h"
#include "Engine.h"
#include "Logging.h"

MHLink::MHLink()
{
    m_eventData.m_Type = MHUnion::U_None;
}

void MHLink::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHIngredient::Initialise(p, engine);
    // The link condition is encoded differently in the binary and text representations.
    MHParseNode *pLinkCond = p->GetNamedArg(C_LINK_CONDITION);

    if (pLinkCond)   // Only in binary.
    {
        m_eventSource.Initialise(pLinkCond->GetArgN(0), engine); // Event source
        m_nEventType = (enum EventType)pLinkCond->GetArgN(1)->GetEnumValue(); // Event type
        // The event data is optional and type-dependent.
        if (pLinkCond->GetArgCount() >= 3)
        {
            MHParseNode *pEventData = pLinkCond->GetArgN(2);

            switch (pEventData->m_nNodeType)
            {
                case MHParseNode::PNBool:
                    m_eventData.m_fBoolVal = pEventData->GetBoolValue();
                    m_eventData.m_Type = MHUnion::U_Bool;
                    break;
                case MHParseNode::PNInt:
                    m_eventData.m_nIntVal = pEventData->GetIntValue();
                    m_eventData.m_Type = MHUnion::U_Int;
                    break;
                case MHParseNode::PNString:
                    pEventData->GetStringValue(m_eventData.m_strVal);
                    m_eventData.m_Type = MHUnion::U_String;
                    break;
                default:
                    MHParseNode::Failure("Unknown type of event data");
            }
        }
    }
    else   // Only in text.
    {
        MHParseNode *pEventSource = p->GetNamedArg(P_EVENT_SOURCE); // Event source

        if (! pEventSource)
        {
            MHParseNode::Failure("Missing :EventSource");
        }
        else
        {
            m_eventSource.Initialise(pEventSource->GetArgN(0), engine);
        }

        MHParseNode *pEventType = p->GetNamedArg(P_EVENT_TYPE); // Event type

        if (! pEventType)
        {
            MHParseNode::Failure("Missing :EventType");
        }
        else
        {
            m_nEventType = (enum EventType)pEventType->GetArgN(0)->GetEnumValue();
        }

        MHParseNode *pEventData = p->GetNamedArg(P_EVENT_DATA); // Event data - optional

        if (pEventData)
        {
            MHParseNode *pEventDataArg = pEventData->GetArgN(0);

            switch (pEventDataArg->m_nNodeType)
            {
                case MHParseNode::PNBool:
                    m_eventData.m_fBoolVal = pEventDataArg->GetBoolValue();
                    m_eventData.m_Type = MHUnion::U_Bool;
                    break;
                case MHParseNode::PNInt:
                    m_eventData.m_nIntVal = pEventDataArg->GetIntValue();
                    m_eventData.m_Type = MHUnion::U_Int;
                    break;
                case MHParseNode::PNString:
                    pEventDataArg->GetStringValue(m_eventData.m_strVal);
                    m_eventData.m_Type = MHUnion::U_String;
                    break;
                default:
                    MHParseNode::Failure("Unknown type of event data");
            }
        }
    }

    MHParseNode *pLinkEffect = p->GetNamedArg(C_LINK_EFFECT);

    if (pLinkEffect)
    {
        m_linkEffect.Initialise(pLinkEffect, engine);
    }
}

static const std::array<const QString,33> rchEventType
{
    "IsAvailable",
    "ContentAvailable",
    "IsDeleted",
    "IsRunning",
    "IsStopped",
    "UserInput",
    "AnchorFired",
    "TimerFired",
    "AsyncStopped",
    "InteractionCompleted",
    "TokenMovedFrom",
    "TokenMovedTo",
    "StreamEvent",
    "StreamPlaying",
    "StreamStopped",
    "CounterTrigger",
    "HighlightOn",
    "HighlightOff",
    "CursorEnter",
    "CursorLeave",
    "IsSelected",
    "IsDeselected",
    "TestEvent",
    "FirstItemPresented",
    "LastItemPresented",
    "HeadItems",
    "TailItems",
    "ItemSelected",
    "ItemDeselected",
    "EntryFieldFull",
    "EngineEvent",
    "FocusMoved",
    "SliderValueChanged"
};

// Look up the event type. Returns zero if it doesn't match.
int MHLink::GetEventType(const QString& str)
{
    for (size_t i = 0; i < rchEventType.size(); i++)
    {
        if (str.compare(rchEventType[i], Qt::CaseInsensitive) == 0)
        {
            return (i + 1);    // Numbered from 1
        }
    }

    return 0;
}

QString MHLink::EventTypeToString(enum EventType ev)
{
    if (ev > 0 && ev <= rchEventType.size())
    {
        return rchEventType[ev-1];
    }
    return QString("Unknown event %1").arg(ev);
}

void MHLink::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Link");
    MHIngredient::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":EventSource ");
    m_eventSource.PrintMe(fd, nTabs + 1);
    fprintf(fd, "\n");
    MHASSERT(m_nEventType > 0 && m_nEventType <= rchEventType.size());
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":EventType %s\n", qPrintable(rchEventType[m_nEventType-1]));

    // The event data is optional and its format depends on the event type.
    switch (m_eventData.m_Type)
    {
        case MHUnion::U_Bool:
            PrintTabs(fd, nTabs + 1);
            fprintf(fd, ":EventData %s\n", m_eventData.m_fBoolVal ? "true" : "false");
            break;
        case MHUnion::U_Int:
            PrintTabs(fd, nTabs + 1);
            fprintf(fd, ":EventData %d\n", m_eventData.m_nIntVal);
            break;
        case MHUnion::U_String:
            PrintTabs(fd, nTabs + 1);
            fprintf(fd, ":EventData");
            m_eventData.m_strVal.PrintMe(fd, nTabs);
            fprintf(fd, "\n");
            break;
        default:
            break; // None and others
    }

    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":LinkEffect (\n");
    m_linkEffect.PrintMe(fd, nTabs + 2);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ")\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// Activation.
void MHLink::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHIngredient::Activation(engine);
    m_fRunning = true;
    engine->AddLink(this);
    engine->EventTriggered(this, EventIsRunning);
}

void MHLink::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    engine->RemoveLink(this);
    MHIngredient::Deactivation(engine);
}

// Activate or deactivate the link.
void MHLink::Activate(bool fActivate, MHEngine *engine)
{
    if (fActivate)
    {
        if (! m_fRunning)
        {
            Activation(engine);
        }
    }
    else
    {
        if (m_fRunning)
        {
            Deactivation(engine);
        }
    }
}

// Check this link to see if the event matches the requirements.  If the link does not specify
// any event data the link fires whatever the value of the data.
void MHLink::MatchEvent(const MHObjectRef &sourceRefRef, enum EventType ev, const MHUnion &evData, MHEngine *engine)
{
    if (m_fRunning && m_nEventType == ev && sourceRefRef.Equal(m_eventSource, engine))   // Source and event type match.
    {
        bool fMatch = false;

        switch (m_eventData.m_Type)
        {
            case MHUnion::U_None:
                fMatch = true;
                break; // No data specified - always matches.
            case MHUnion::U_Bool:
                fMatch = evData.m_Type == MHUnion::U_Bool && evData.m_fBoolVal == m_eventData.m_fBoolVal;
                break;
            case MHUnion::U_Int:
                fMatch = evData.m_Type == MHUnion::U_Int && evData.m_nIntVal == m_eventData.m_nIntVal;
                break;
            case MHUnion::U_String:
                fMatch = evData.m_Type == MHUnion::U_String && evData.m_strVal.Equal(m_eventData.m_strVal);
                break;
            default:
                fMatch = false;
                break;
        }

        // Fire the link
        if (fMatch)
        {
            MHLOG(MHLogLinks, QString("Link fired - %1").arg(m_ObjectReference.Printable()));
            engine->AddActions(m_linkEffect);
        }
    }
}
