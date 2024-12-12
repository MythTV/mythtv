// Qt
#include <QFile>
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythuihelper.h"
#include "osd.h"
#include "mythtvmenu.h"

#define LOC QString("TVMenu: ")

bool MythTVMenuItemContext::AddButton(MythOSDDialogData *Menu, bool Active, const QString& Action,
                                      const QString& DefaultTextActive,
                                      const QString& DefaultTextInactive,
                                      bool IsMenu, const QString& TextArg) const
{
    bool result = false;
    if (m_category == kMenuCategoryItemlist || Action == m_action)
    {
        if ((m_showContext != kMenuShowInactive && Active) || (m_showContext != kMenuShowActive && !Active))
        {
            result = true;
            if (m_visible)
            {
                QString text = m_actionText;
                if (text.isEmpty())
                    text = (Active || DefaultTextInactive.isEmpty()) ? DefaultTextActive : DefaultTextInactive;

                if (!TextArg.isEmpty())
                    text = text.arg(TextArg);

                bool current = false;
                if (m_currentContext == kMenuCurrentActive)
                    current = Active;
                else if (m_currentContext == kMenuCurrentAlways)
                    current = true;

                Menu->m_buttons.push_back( { text, Action, IsMenu, current });
            }
        }
    }
    return result;
}

// Constructor for a menu element.
MythTVMenuItemContext::MythTVMenuItemContext(const MythTVMenu& Menu, const QDomNode& Node,
                                             QString Name, MenuCurrentContext Current, bool Visible)
  : m_menu(Menu),
    m_node(Node),
    m_category(kMenuCategoryMenu),
    m_menuName(std::move(Name)),
    m_showContext(kMenuShowAlways),
    m_currentContext(Current),
    m_visible(Visible)
{
}

// Constructor for an item element.
MythTVMenuItemContext::MythTVMenuItemContext(const MythTVMenu& Menu, const QDomNode& Node,
                                             MenuShowContext Context, MenuCurrentContext Current,
                                             QString Action, QString ActionText, bool Visible)
  : m_menu(Menu),
    m_node(Node),
    m_showContext(Context),
    m_currentContext(Current),
    m_action(std::move(Action)),
    m_actionText(std::move(ActionText)),
    m_visible(Visible)
{
}

// Constructor for an itemlist element.
MythTVMenuItemContext::MythTVMenuItemContext(const MythTVMenu& Menu, const QDomNode& Node,
                                             MenuShowContext Context, MenuCurrentContext Current,
                                             QString Action, bool Visible)
  : m_menu(Menu),
    m_node(Node),
    m_category(kMenuCategoryItemlist),
    m_showContext(Context),
    m_currentContext(Current),
    m_action(std::move(Action)),
    m_visible(Visible)
{
}

MythTVMenu::~MythTVMenu()
{
    delete m_document;
}

QString MythTVMenu::GetName() const
{
    return m_menuName;
}

bool MythTVMenu::IsLoaded() const
{
    return m_document != nullptr;
}

QDomElement MythTVMenu::GetRoot() const
{
    return m_document->documentElement();
}

const char * MythTVMenu::GetTranslationContext() const
{
    return m_translationContext;
}

const QString& MythTVMenu::GetKeyBindingContext() const
{
       return m_keyBindingContext;
}

QString MythTVMenu::Translate(const QString& Text) const
{
    return QCoreApplication::translate(m_translationContext, Text.toUtf8(), nullptr);
}

bool MythTVMenu::MatchesGroup(const QString &Name, const QString &Prefix,
                              MenuCategory Category, QString &OutPrefix)
{
    OutPrefix = Name;
    return ((Category == kMenuCategoryItem && Name.startsWith(Prefix)) ||
            (Category == kMenuCategoryItemlist && Name == Prefix));
}

QString MythTVMenu::GetPathFromNode(QDomNode Node)
{
    QStringList path;

    while (Node.isElement())
    {
        QDomElement el = Node.toElement();
        if (el.tagName() != "menu")
        {
            path.prepend("NotMenu");
            break;
        }
        path.prepend(el.attribute("text"));
        Node = Node.parentNode();
    }
    return path.join('/');
}

QDomNode MythTVMenu::GetNodeFromPath(const QString& path) const
{
    QStringList pathList = path.split('/');
    if (pathList.isEmpty())
        return {};

    // Root node is special
    QDomElement result = GetRoot();
    QString name = pathList.takeFirst();
    if ((result.tagName() != "menu") || (result.attribute("text") != name))
        return {};

    // Start walking children
    while (!pathList.isEmpty())
    {
        bool found = false;
        name = pathList.takeFirst();
        auto children = result.childNodes();
        for (int i = 0 ; i < children.count(); i++)
        {
            auto child = children.at(i).toElement();
            if (child.isNull() ||
                (name == child.attribute("text")) ||
                (name == child.attribute("XXXtext")))
            {
                result = child;
                found = true;
                break;
            }
        }

        if (!found)
        {
            // Oops. Have name but no matching child.
            return {};
        }
    }
    return result;
}

bool MythTVMenu::LoadFromFile(MenuTypeId id, const QString& Filename, const QString& Menuname,
                              const char * TranslationContext, const QString& KeyBindingContext,
                              int IncludeLevel)
{
    bool result = false;

    m_id = id;
    m_translationContext = TranslationContext;
    m_keyBindingContext  = KeyBindingContext;
    QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    searchpath.prepend(GetConfDir() + '/');
    for (auto it = searchpath.cbegin(); !result && it != searchpath.cend(); ++it)
    {
        QString themefile = *it + Filename;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Loading menu %1").arg(themefile));
        QFile file(themefile);
        if (file.open(QIODevice::ReadOnly))
        {
            m_document = new QDomDocument();
            if (m_document->setContent(&file))
            {
                result = true;
                QDomElement root = GetRoot();
                m_menuName = Translate(root.attribute("text", Menuname));
                ProcessIncludes(root, IncludeLevel);
            }
            else
            {
                delete m_document;
                m_document = nullptr;
            }
            file.close();
        }

        if (!result)
            LOG(VB_FILE, LOG_ERR, LOC + "No theme file " + themefile);
    }

    return result;
}

bool MythTVMenu::LoadFromString(MenuTypeId id, const QString& Text, const QString& Menuname,
                                const char * TranslationContext, const QString& KeyBindingContext,
                                int IncludeLevel)
{
    bool result = false;

    m_id = id;
    m_translationContext = TranslationContext;
    m_keyBindingContext = KeyBindingContext;
    m_document = new QDomDocument();
    if (m_document->setContent(Text))
    {
        result = true;
        QDomElement root = GetRoot();
        m_menuName = Translate(root.attribute("text", Menuname));
        ProcessIncludes(root, IncludeLevel);
    }
    else
    {
        delete m_document;
        m_document = nullptr;
    }
    return result;
}

void MythTVMenu::ProcessIncludes(QDomElement& Root, int IncludeLevel)
{
    const int maxInclude = 10;
    for (QDomNode node = Root.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        if (node.isElement())
        {
            QDomElement element = node.toElement();
            if (element.tagName() == "include")
            {
                QString include = element.attribute("file", "");
                if (include.isEmpty())
                    continue;

                if (IncludeLevel >= maxInclude)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Maximum include depth (%1) exceeded for %2")
                        .arg(maxInclude).arg(include));
                    return;
                }

                MythTVMenu menu;
                if (menu.LoadFromFile(m_id, include, include, m_translationContext,
                                      m_keyBindingContext, IncludeLevel + 1))
                {
                    QDomNode newChild = menu.GetRoot();
                    newChild = m_document->importNode(newChild, true);
                    Root.replaceChild(newChild, node);
                    node = newChild;
                }
            }
            else if (element.tagName() == "menu")
            {
                ProcessIncludes(element, IncludeLevel + 1);
            }
        }
    }
}

bool MythTVMenu::Show(const QDomNode& Node, const QDomNode& Selected,
                      MythTVMenuItemDisplayer& Displayer, MythOSDDialogData* Menu,
                      bool Visible) const
{
    bool hasSelected = false;
    bool displayed = false;
    for (QDomNode node = Node.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        if (node == Selected)
            hasSelected = true;
    }

    for (QDomNode node = Node.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        if (node.isElement())
        {
            QDomElement element = node.toElement();
            QString text  = Translate(element.attribute("text", ""));
            QString show = element.attribute("show", "");
            MenuShowContext showContext {kMenuShowAlways};
            if (show == "active")
                showContext = kMenuShowActive;
            else if (show == "inactive")
                showContext = kMenuShowInactive;
            QString current = element.attribute("current", "");
            MenuCurrentContext currentContext = kMenuCurrentDefault;
            if ((current == "active") && !hasSelected)
                currentContext = kMenuCurrentActive;
            else if (((current.startsWith("y") || current.startsWith("t") || current == "1")) && !hasSelected)
                currentContext = kMenuCurrentAlways;

            if (element.tagName() == "menu")
            {
                if (hasSelected && node == Selected)
                    currentContext = kMenuCurrentAlways;
                MythTVMenuItemContext context(*this, node, text, currentContext, Visible);
                displayed |= Displayer.MenuItemDisplay(context, Menu);
            }
            else if (element.tagName() == "item")
            {
                QString action = element.attribute("action", "");
                MythTVMenuItemContext context(*this, node, showContext, currentContext, action, text, Visible);
                displayed |= Displayer.MenuItemDisplay(context, Menu);
            }
            else if (element.tagName() == "itemlist")
            {
                QString actiongroup = element.attribute("actiongroup", "");
                MythTVMenuItemContext context(*this, node, showContext, currentContext, actiongroup, Visible);
                displayed |= Displayer.MenuItemDisplay(context, Menu);
            }
        }

         // early exit optimization
        if (!Visible && displayed)
            break;
    }
    return displayed;
}

MythTVMenuNodeTuple::MythTVMenuNodeTuple(MenuTypeId Id, QString Path)
  : m_id(Id),
    m_path(std::move(Path))
{
}
