/* Link.h

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

#if !defined(LINK_H)
#define LINK_H

#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "Actions.h"
#include "BaseActions.h"

#include <QString>

class MHParseNode;

// Link - basically a guarded command.
class MHLink : public MHIngredient  
{
  public:
    MHLink();
    const char *ClassName() override // MHRoot
        { return "Link"; }
    // Set this up from the parse tree.
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient
    // Look up the event type.  Returns zero if it doesn't match.
    static int GetEventType(const QString& str);
    // Print an event type.
    static QString EventTypeToString(enum EventType ev);

    // Internal behaviours.
    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot

    // Handle activation and deactivation
    void Activate(bool f, MHEngine *engine) override; // MHRoot
    // Called when an event has been triggered and fires this link if it matches. 
    virtual void MatchEvent(const MHObjectRef &sourceRef, enum EventType ev, const MHUnion &evData, MHEngine *engine);

  protected:
    MHObjectRef m_eventSource;
    enum EventType m_nEventType {EventIsAvailable};
    MHUnion m_eventData;
    MHActionSequence m_linkEffect;
};

// Actions.
// Activate and deactivate actions.
class MHActivate: public MHElemAction
{
  public:
    MHActivate(const char *name, bool fActivate): MHElemAction(name), m_fActivate(fActivate) {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->Activate(m_fActivate, engine); }
  protected:
    bool m_fActivate;
};

#endif
