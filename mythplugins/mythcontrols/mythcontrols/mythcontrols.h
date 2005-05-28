/**
 * @file mythcontrols.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main header for mythcontrols
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */
#ifndef MYTHCONTROLS_H
#define MYTHCONTROLS_H

#include <stdexcept>

#include <mythtv/mythdialogs.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>

#include "keybindings.h"


/**
 * @class MythControls
 * @brief The myth controls configuration class.
 */
class MythControls : virtual public MythThemedDialog
{

    Q_OBJECT

 public:
    /**
     * @brief Create a new myth controls wizard.
     * @param parent The main myth window.
     * @param name The name of this window?  I dunno, none of the docs
     * say.
     */
    MythControls(MythMainWindow *parent, const char *name=0);

    /**
     * @brief Delete the myth controls object.
     */
    ~MythControls();

    /**
     * @brief Get the currently selected context string.
     * @return The currently selected context string.
     *
     * If no context is selected, an empty string is returned.
     */
    QString getCurrentContext(void) const;

    /**
     * @brief Get the currently selected action string.
     * @return The currently selected action string.
     *
     * If no action is selected, an empty string is returned.
     */
    QString getCurrentAction(void) const;

 protected:

    /**
     * @brief The key press handler.
     * @param e The key event.
     */
    void keyPressEvent(QKeyEvent *e);

    /**
     * @brief Redraw the key information
     *
     * Updates the list of keys that are shown and the description of
     * the action.
     */
    void refreshKeyInformation();

    /**
     * @brief Launches the action menu.
     */
    void actionMenu();

    /**
     * @brief Load the settings for a particular host.
     * @param hostname The host to load settings for.
     */
    void loadHost(const QString & hostname);

 private slots:

     /**
      * @brief Clear the keys for the selected action.
      */
     void clearActionKeys();

     /**
      * @brief Add a key to the currently selected action.
      */
     void addKeyToAction();

     /**
      * @brief Capture a key press using the key grabber.
      */
     void captureKey();

     /**
      * @brief Kill the currently visbile popup window.
      */
     void killPopup();

     /**
      * @brief Save the settings.
      */
     void save();

     /**
      * @brief Resolve the conflict.
      * @param winner Which conflicting action to keep (first or second).
      *
      * This is supposed to be called when a conflict resolver has
      * one of its buttons clicked.
      */
     void resolve(int winner);

     inline void keepFirst() { resolve(1); }
     inline void keepSecond() { resolve(2); }

 private:

     GenericTree * context_tree;
     UIManagedTreeListType * control_tree_list;
     UITextType * action_description;
     UITextType * action_keys;
     KeyBindings *key_bindings;
     MythPopupBox *popup;
};


#endif /* MYTHCONTROLS_H */
