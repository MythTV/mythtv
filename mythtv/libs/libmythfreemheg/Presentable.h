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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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
    MHPresentable() = default;
    MHPresentable(const MHPresentable&) = default;
    // No new components.

    // Actions.
    void Run(MHEngine *engine) override; // MHRoot
    void Stop(MHEngine *engine) override; // MHRoot

    // Additional actions for stream components.
    virtual void BeginPlaying(MHEngine */*engine*/) {}
    virtual void StopPlaying(MHEngine */*engine*/) {}
};

// Run and stop actions.
class MHRun: public MHElemAction
{
  public:
    MHRun(): MHElemAction(":Run") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->Run(engine); }
};

class MHStop: public MHElemAction
{
  public:
    MHStop(): MHElemAction(":Stop") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->Stop(engine); }
};

#endif
