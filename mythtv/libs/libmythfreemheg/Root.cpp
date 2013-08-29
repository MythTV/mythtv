/* Root.cpp

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

#include "Root.h"
#include "ParseNode.h"
#include "BaseClasses.h"
#include "Ingredients.h"
#include "Engine.h"
#include "Logging.h"

// Initialise the root class from the parse tree.
void MHRoot::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHParseNode *pArg = p->GetArgN(0); // The first argument should be present.
    // Extract the field.
    m_ObjectReference.Initialise(pArg, engine);
}

// Print the contents, in this case just the object reference.
void MHRoot::PrintMe(FILE *fd, int nTabs) const
{
    m_ObjectReference.PrintMe(fd, nTabs);
    fprintf(fd, "\n");
}

// An action was attempted on an object of a class which doesn't support this.
void MHRoot::InvalidAction(const char *actionName)
{
    MHLOG(MHLogWarning, QString("WARN Action \"%1\" is not understood by class \"%2\"").arg(actionName).arg(ClassName()));
    throw "Invalid Action";
}

// Preparation - sets up the run-time representation.  Sets m_fAvailable and generates IsAvailable event.
void MHRoot::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;    // Already prepared
    }

    // Retrieve object
    // Set the internal attributes.
    m_fAvailable = true;
    engine->EventTriggered(this, EventIsAvailable);
    // When the content becomes available generate EventContentAvailable.  This is not
    // generated if an object has no Content.
    ContentPreparation(engine);
}

// Activation - starts running the object.  Sets m_fRunning and generates IsRunning event.
void MHRoot::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;    // Already running.
    }

    if (! m_fAvailable)
    {
        Preparation(engine);    // Prepare it if that hasn't already been done.
    }

    // The subclasses are responsible for setting m_fRunning and generating IsRunning.
}

// Deactivation - stops running the object.  Clears m_fRunning
void MHRoot::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;    // Already stopped.
    }

    m_fRunning = false;
    engine->EventTriggered(this, EventIsStopped);
}

// Destruction - deletes the run-time representation.  Clears m_fAvailable.
void MHRoot::Destruction(MHEngine *engine)
{
    if (! m_fAvailable)
    {
        return;    // Already destroyed or never prepared.
    }

    if (m_fRunning)
    {
        Deactivation(engine);    // Deactivate it if it's still running.
    }

    // We're supposed to wait until it's stopped here.
    m_fAvailable = false;
    engine->EventTriggered(this, EventIsDeleted);
}

// Return this object if it matches.
MHRoot *MHRoot::FindByObjectNo(int n)
{
    if (n == m_ObjectReference.m_nObjectNo)
    {
        return this;
    }
    else
    {
        return NULL;
    }
}

void MHGetAvailabilityStatus::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    m_ResultVar.Initialise(p->GetArgN(1), engine);
}

void MHGetAvailabilityStatus::Perform(MHEngine *engine)
{
    // This is a special case.  If the object does not exist we set the result to false.
    MHObjectRef target;
    m_Target.GetValue(target, engine); // Get the target
    MHRoot *pObject = engine->FindObject(target, false);
    bool fResult = false; // Default result.

    if (pObject)
    {
        fResult = pObject->GetAvailabilityStatus();
    }

    engine->FindObject(m_ResultVar)->SetVariableValue(fResult);
}
