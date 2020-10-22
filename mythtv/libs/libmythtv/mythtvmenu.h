#ifndef MYTHTVMENU_H
#define MYTHTVMENU_H

// Qt
#include <QDomDocument>

enum MenuCategory
{
    kMenuCategoryItem,
    kMenuCategoryItemlist,
    kMenuCategoryMenu
};

enum MenuShowContext
{
    kMenuShowActive,
    kMenuShowInactive,
    kMenuShowAlways
};

enum MenuCurrentContext
{
    kMenuCurrentDefault,
    kMenuCurrentActive,
    kMenuCurrentAlways
};

class MenuBase;

class MenuItemContext
{
  public:
    MenuItemContext(const MenuBase& Menu, const QDomNode& Node,
                    QString Name, MenuCurrentContext Current, bool Display);
    MenuItemContext(const MenuBase& Menu, const QDomNode& Node,
                    MenuShowContext Context, MenuCurrentContext Current,
                    QString Action, QString ActionText, bool Display);
    MenuItemContext(const MenuBase& Menu, const QDomNode& Node,
                    MenuShowContext Context, MenuCurrentContext Current,
                    QString Action, bool Display);

    const MenuBase&    m_menu;
    const QDomNode&    m_node;
    MenuCategory       m_category       { kMenuCategoryItem   };
    const QString      m_menuName;
    MenuShowContext    m_showContext    { kMenuShowActive     };
    MenuCurrentContext m_currentContext { kMenuCurrentDefault };
    const QString      m_action;
    const QString      m_actionText;
    bool               m_doDisplay      { false };
};

class MenuItemDisplayer
{
  public:
    virtual ~MenuItemDisplayer() = default;
    virtual bool MenuItemDisplay(const MenuItemContext& Context) = 0;
};

class MenuBase
{
  public:
    MenuBase() = default;
   ~MenuBase();
    bool        LoadFromFile(const QString& Filename, const QString& Menuname,
                             const char * TranslationContext, const QString& KeyBindingContext,
                             int IncludeLevel = 0);
    bool        LoadFromString(const QString& Text, const QString& Menuname,
                               const char * TranslationContext, const QString& KeyBindingContext,
                               int IncludeLevel = 0);
    bool        IsLoaded() const;
    QDomElement GetRoot() const;
    QString     Translate(const QString& Text) const;
    bool        Show(const QDomNode& Node, const QDomNode& Selected,
                     MenuItemDisplayer& Displayer, bool Display = true) const;
    QString     GetName() const;
    const char* GetTranslationContext() const;
    const QString& GetKeyBindingContext() const;

  private:
    void ProcessIncludes(QDomElement& Root, int IncludeLevel);

    QDomDocument* m_document           { nullptr };
    const char*   m_translationContext { "" };
    QString       m_menuName;
    QString       m_keyBindingContext;
};

#endif
