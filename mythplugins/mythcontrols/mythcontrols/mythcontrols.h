/* -*- myth -*- */
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

#include <mythtv/mythdialogs.h>
#include <mythtv/uilistbtntype.h>

#include "keybindings.h"

typedef struct
{
    QString key;
    QString context;
    QString contextFrom;
    QString action;
    int bindlevel;
} binding_t;

typedef QPtrList<binding_t> BindingList;

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
     * @param ui_ok Will be set according to weather the UI was
     * correctly loaded.
     */
    MythControls(MythMainWindow *parent, bool& ui_ok);

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

    /**
     * @brief Get the currently selected key string
     * @return The currently selected key string
     *
     * If no context is selected, an empty string is returned.
     */
    QString getCurrentKey(void) const;

    
    
protected:

    /**
     * @brief Load up UI.
     * @return true if all UI elements were loaded successfully,
     * otherwise false it returned.
     *
     * This method grabs all of the UI "thingies" that are needed by
     * mythcontrols.  If this method returns false, the plugin should
     * exit since it will probably just core dump.
     */
    bool loadUI();

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
     * @brief Load the appropriate actions into the action list.
     * @param context The context, from which actions will be
     * displayed.
     */
    void refreshRightList(void);

    /**
     * @brief Redisplay both the left and right lists and fix focus
     */
    void updateLists(void);

    /**
     * @brief Load the settings for a particular host.
     * @param hostname The host to load settings for.
     */
    void loadHost(const QString & hostname);

    /**
     * @brief Focus a button in a particular direction.
     * @param direction Positave values focus to the right, negatives
     * to the left.  A value of zero focuses the first button.
     */
    void focusButton(int direction);

    /**
     * @brief Determine which button is focused.
     * @return Action::MAX_KEYS is no button is focused, otherwise, the
     * index of the button is returned.
     *
     * @sa Action::MAX_KEYS
     */
    size_t focusedButton(void) const;

    /**
     * @brief Resolve a potential conflict.
     * @return true if the conflict should be bound, otherwise, false
     * is returned.
     */
    bool resolveConflict(ActionID *conflict, int level);

    /**
     * @brief Focus a new list and take focus away from the old.
     * @param focus The list to gain focus
     * @param unfocus The list to loose focus
     */
    void switchListFocus(UIListBtnType *focus, UIListBtnType *unfocus);

    /**
     * @brief Add bindings to QDict<binding_t> for specified context
     * @param bindings the QDict to which to add the bindings
     * @param context the context to grab keybindings from
     * @param contextParent the context whose keybindings are being calculated
     * @param bindlevel the bind level associated with this context
     */
    void addBindings(QDict<binding_t> &bindings, const QString &context, const QString &contextParent, int bindlevel);

    /**
     * @brief Create a BindingList for the specified context
     * @param context the context for which a BindingList should be created
     */
    BindingList *getKeyBindings(const QString &context);

private slots:

/**
 * @brief Add a key to the currently selected action.
 */
void addKeyToAction(void);

    /**
     * @brief Delete the currently active key.
     */
    void deleteKey();

    /**
     * @brief Save the settings.
     */
    inline void save(void) { key_bindings->commitChanges(); }

    /**
     * @brief Recieves a signal when an item in the left list is
     * selected.
     * @param item The selected item.
     */
    void leftSelected(UIListBtnTypeItem *item);

    /**
     * @brief Recieves a signal when an item in the right list is
     * selected.
     * @param item The selected item.
     */
    void rightSelected(UIListBtnTypeItem *item);

    /**
     * @brief Sort a list of keys, removing duplicates
     * @param keys the list of keys to sort
     */
    void sortKeyList(QStringList &keys);

    /**
     * @brief Refresh binding information
     */
    void refreshKeyBindings();

    /**
     * @brief Jump to a particular key binding
     * @param e key event to use as jump
     */
    bool JumpTo(QKeyEvent *e);

private:

    UIType *focused;
    UIListBtnType *LeftList;
    UIListBtnType *RightList;
    UITextType *description, *LeftDesc, *RightDesc;
    UITextButtonType * ActionButtons[Action::MAX_KEYS];
    KeyBindings *key_bindings;
    LayerSet *container;

    QStringList contexts; ///< sorted list of contexts
    QStringList keys; ///< sorted list of keys

    QDict<QStringList> m_contexts; ///< actions for a given context
    QDict<BindingList> contextKeys; ///< key bindings for a given context
    QDict<BindingList> keyActions; ///< actions in each context for a given key

    typedef enum { kContextList, kKeyList, kActionList } ListType;
    ListType leftType, rightType;

    QString getTypeDesc(ListType type);

};


#endif /* MYTHCONTROLS_H */
