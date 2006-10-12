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

class Binding
{
  public:
    Binding(const QString &_key,         const QString &_context,
            const QString &_contextFrom, const QString &_action,
            int            _bindlevel) :
        key(_key), context(_context),
        contextFrom(_contextFrom), action(_action), bindlevel(_bindlevel) {}

  public:
    QString key;
    QString context;
    QString contextFrom;
    QString action;
    int     bindlevel;
};
typedef QPtrList<Binding> BindingList;

/** \class MythControls
 *  \brief The myth controls configuration class.
 */
class MythControls : virtual public MythThemedDialog
{
    Q_OBJECT

    typedef enum
    {
        kContextList,
        kKeyList,
        kActionList
    } ListType;

  public:
    MythControls(MythMainWindow *parent, bool &ok);
    ~MythControls();

    // Gets
    QString GetCurrentContext(void) const;
    QString GetCurrentAction(void) const;
    QString GetCurrentKey(void) const;
    
  protected:
    // Event handlers
    void keyPressEvent(QKeyEvent *e);

    // Commands
    bool    LoadUI(void);
    void    RefreshKeyInformation(void);
    QString RefreshKeyInformationKeyList(void);
    void    RefreshRightList(void);
    void    UpdateLists(void);
    void    LoadData(const QString &hostname);
    void    ChangeButtonFocus(int direction);
    void    ChangeListFocus(UIListBtnType *focus, UIListBtnType *unfocus);
    void    AddBindings(QDict<Binding> &bindings, const QString &context,
                        const QString &contextParent, int bindlevel);

    // Gets
    BindingList *GetKeyBindings(const QString &context);
    uint         GetCurrentButton(void) const;

    // Functions
    static bool ResolveConflict(ActionID *conflict, int error_level);
    QString GetTypeDesc(ListType type) const;

  private slots:
    void AddKeyToAction(void);
    void DeleteKey(void);
    void LeftSelected(UIListBtnTypeItem *item);
    void RightSelected(UIListBtnTypeItem *item);
    void SortKeyList(QStringList &keys);
    void RefreshKeyBindings(void);
    bool JumpTo(QKeyEvent *e);
    /// \brief Save the bindings to the Database.
    void Save(void) { m_bindings->CommitChanges(); }

  private:
    UIType            *m_focusedUIElement;
    UIListBtnType     *m_leftList;
    UIListBtnType     *m_rightList;
    UITextType        *m_description;
    UITextType        *m_leftDescription;
    UITextType        *m_rightDescription;
    UITextButtonType  *m_actionButtons[Action::kMaximumNumberOfBindings];

    KeyBindings       *m_bindings;
    LayerSet          *m_container;
    QStringList        m_sortedContexts; ///< sorted list of contexts
    QStringList        m_sortedKeys;     ///< sorted list of keys
    QDict<QStringList> m_contexts;       ///< actions for a given context
    QDict<BindingList> m_contextToBindingsMap;
    QDict<BindingList> m_keyToBindingsMap;
    ListType           m_leftListType;
    ListType           m_rightListType;
};


#endif /* MYTHCONTROLS_H */
