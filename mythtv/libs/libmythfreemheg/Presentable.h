/* Presentable.h

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/


#if !defined(PRESENTABLE_H)
#define PRESENTABLE_H

#include "Ingredients.h"
// Dependencies
#include "Root.h"
#include "BaseClasses.h"
#include "BaseActions.h"
#include "Actions.h"


class MHPresentable : public MHIngredient  
{
  public:
    MHPresentable() {}
    MHPresentable(const MHPresentable &ref): MHIngredient(ref) {}
    // No new components.

    // Actions.
    virtual void Run(MHEngine *engine);
    virtual void Stop(MHEngine *engine);

    // Additional actions for stream components.
    virtual void BeginPlaying(MHEngine *) {}
    virtual void StopPlaying(MHEngine *) {}
};

// Run and stop actions.
class MHRun: public MHElemAction
{
  public:
    MHRun(): MHElemAction(":Run") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->Run(engine); }
};

class MHStop: public MHElemAction
{
  public:
    MHStop(): MHElemAction(":Stop") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->Stop(engine); }
};

#endif
