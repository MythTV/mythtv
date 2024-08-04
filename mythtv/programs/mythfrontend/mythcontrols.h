// -*- Mode: c++ -*-
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#ifndef MYTHCONTROLS_H
#define MYTHCONTROLS_H

// QT
#include <QList>
#include <QHash>

// MythUI
#include "libmythui/mythscreentype.h"

#include "keybindings.h"

class MythUIText;
class MythUIButtonList;
class MythUIButton;
class MythUIImage;
class MythDialogBox;

enum ViewType : std::uint8_t { kActionsByContext, kKeysByContext, kContextsByKey };

/**
 *  \class MythControls
 *
 *  \brief Screen for managing and configuring keyboard input bindings
 */
class MythControls : public MythScreenType
{
    Q_OBJECT

  public:

    /**
     *  \brief Creates a new MythControls wizard
     *  \param parent Pointer to the screen stack
     *  \param name The name of the window
     *  \param Modifiers If true only show key modifications (e.g. LongPress) otherwise filter them out
     */
    MythControls(MythScreenStack *parent, const char *name, KeyBindings::Filter Filters = KeyBindings::AllBindings)
        : MythScreenType (parent, name),
          m_filters(Filters)
    {}
    ~MythControls() override;

    bool Create(void) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

    enum ListType : std::uint8_t
    {
        kContextList,
        kKeyList,
        kActionList
    };

    // Gets
    QString GetCurrentContext(void);
    QString GetCurrentAction(void);
    QString GetCurrentKey(void);

  protected:
    void Teardown(void);

    // Commands
    bool    LoadUI(void);
    void    LoadData(const QString &hostname);
    void    ChangeButtonFocus(int direction);
    void    ChangeView(void);
    static void    SetListContents(MythUIButtonList *uilist,
                            const QStringList & contents,
                            bool arrows = false);
    void    UpdateRightList(void);

    void GrabKey(void) const;
    void DeleteKey(void);
    void Save(void) { m_bindings->CommitChanges(); }

    // Gets
    uint    GetCurrentButton(void);

    // Functions
    void ResolveConflict(ActionID *conflict, int error_level,
                         const QString &key);
    QString GetTypeDesc(ListType type) const;

  private slots:
    void LeftSelected(MythUIButtonListItem *item);
    void RightSelected(MythUIButtonListItem *item);
    void LeftPressed(MythUIButtonListItem *item);
    void RightPressed(MythUIButtonListItem *item);
    void ActionButtonPressed();
    void RefreshKeyInformation(void);
    void AddKeyToAction(const QString& key, bool ignoreconflict);
    void AddKeyToAction(const QString& key);

  private:
    void ShowMenu(void) override; // MythScreenType
    void Close(void) override; // MythScreenType

    ViewType          m_currentView       {kActionsByContext};
    MythUIButtonList  *m_leftList         {nullptr};
    MythUIButtonList  *m_rightList        {nullptr};
    MythUIText        *m_description      {nullptr};
    MythUIText        *m_leftDescription  {nullptr};
    MythUIText        *m_rightDescription {nullptr};
    QList<MythUIButton*> m_actionButtons;
    MythDialogBox     *m_menuPopup        {nullptr};

    KeyBindings       *m_bindings         {nullptr};
    QStringList        m_sortedContexts; ///< sorted list of contexts
    /// actions for a given context
    QHash<QString, QStringList> m_contexts;
    ListType           m_leftListType     {kContextList};
    ListType           m_rightListType    {kActionList};
    KeyBindings::Filter m_filters         {KeyBindings::AllBindings};
};


#endif /* MYTHCONTROLS_H */
