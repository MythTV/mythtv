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


typedef enum { kActionsByContext, kKeysByContext, kContextsByKey, } ViewType;

/** \class ViewMenu
 *  \brief Prompts the user to change the view.
 */
class ViewMenu : public MythPopupBox
{
    Q_OBJECT

  public:
    ViewMenu(MythMainWindow *window);

    /// \brief Execute the option popup.
    int GetOption(void) { return ExecPopup(this, SLOT(Cancel())); }

    /// \brief The available views
    enum actions { kContextAction, kContextKey, kKeyContext, kCancel, };

  public slots:
    void ActionsByContext(void) { done(ViewMenu::kContextAction); }
    void KeysByContext(void)    { done(ViewMenu::kContextKey);    }
    void ContextsByKey(void)    { done(ViewMenu::kKeyContext);    }
    void Cancel(void)           { done(ViewMenu::kCancel);        }
};

/** \class MythControls
 *  \brief The myth controls configuration class.
 */
class MythControls : public MythThemedDialog
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
    void    LoadData(const QString &hostname);
    void    ChangeButtonFocus(int direction);
    void    ChangeListFocus(UIListBtnType *focus, UIListBtnType *unfocus);
    void    ChangeView(void);
    void    SetListContents(UIListBtnType *uilist,
                            const QStringList & contents,
                            bool arrows = false);
    void    UpdateRightList(void);

    // Gets
    uint         GetCurrentButton(void) const;

    // Functions
    static bool ResolveConflict(ActionID *conflict, int error_level);
    QString GetTypeDesc(ListType type) const;

  private slots:
    void AddKeyToAction(void);
    void DeleteKey(void);
    void LeftSelected(UIListBtnTypeItem *item);
    void RightSelected(UIListBtnTypeItem *item);
    bool JumpTo(QKeyEvent *e);
    /// \brief Save the bindings to the Database.
    void Save(void) { m_bindings->CommitChanges(); }

  private:
    ViewType           m_currentView;
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
    QDict<QStringList> m_contexts;       ///< actions for a given context
    ListType           m_leftListType;
    ListType           m_rightListType;
};


#endif /* MYTHCONTROLS_H */
