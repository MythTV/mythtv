// Qt
#include <QFile>
#include <QCoreApplication>

// MythTV
#include "mythlogging.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythtvmenu.h"

#define LOC QString("TVMenu: ")

// Constructor for a menu element.
MenuItemContext::MenuItemContext(const MenuBase& Menu, const QDomNode& Node,
                                 QString Name, MenuCurrentContext Current, bool Display)
  : m_menu(Menu),
    m_node(Node),
    m_category(kMenuCategoryMenu),
    m_menuName(std::move(Name)),
    m_showContext(kMenuShowAlways),
    m_currentContext(Current),
    m_doDisplay(Display)
{
}

// Constructor for an item element.
MenuItemContext::MenuItemContext(const MenuBase& Menu, const QDomNode& Node,
                                 MenuShowContext Context, MenuCurrentContext Current,
                                 QString Action, QString ActionText, bool Display)
  : m_menu(Menu),
    m_node(Node),
    m_category(kMenuCategoryItem),
    m_showContext(Context),
    m_currentContext(Current),
    m_action(std::move(Action)),
    m_actionText(std::move(ActionText)),
    m_doDisplay(Display)
{
}

// Constructor for an itemlist element.
MenuItemContext::MenuItemContext(const MenuBase& Menu, const QDomNode& Node,
                                 MenuShowContext Context, MenuCurrentContext Current,
                                 QString Action, bool Display)
  : m_menu(Menu),
    m_node(Node),
    m_category(kMenuCategoryItemlist),
    m_showContext(Context),
    m_currentContext(Current),
    m_action(std::move(Action)),
    m_doDisplay(Display)
{
}

MenuBase::~MenuBase()
{
    delete m_document;
}

QString MenuBase::GetName() const
{
    return m_menuName;
}

bool MenuBase::IsLoaded() const
{
    return m_document != nullptr;
}

QDomElement MenuBase::GetRoot() const
{
    return m_document->documentElement();
}

const char * MenuBase::GetTranslationContext() const
{
    return m_translationContext;
}

const QString& MenuBase::GetKeyBindingContext() const
{
       return m_keyBindingContext;
}

QString MenuBase::Translate(const QString& Text) const
{
    return QCoreApplication::translate(m_translationContext, Text.toUtf8(), nullptr);
}

bool MenuBase::LoadFromFile(const QString& Filename, const QString& Menuname,
                            const char * TranslationContext, const QString& KeyBindingContext,
                            int IncludeLevel)
{
    bool result = false;

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

bool MenuBase::LoadFromString(const QString& Text, const QString& Menuname,
                              const char * TranslationContext, const QString& KeyBindingContext,
                              int IncludeLevel)
{
    bool result = false;

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

void MenuBase::ProcessIncludes(QDomElement& Root, int IncludeLevel)
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

                MenuBase menu;
                if (menu.LoadFromFile(include, include, m_translationContext,
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

bool MenuBase::Show(const QDomNode& Node, const QDomNode& Selected,
                    MenuItemDisplayer& Displayer, bool Display) const
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
            MenuShowContext showContext =
                (show == "active" ? kMenuShowActive :
                 show == "inactive" ? kMenuShowInactive : kMenuShowAlways);
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
                MenuItemContext context(*this, node, text, currentContext, Display);
                displayed |= Displayer.MenuItemDisplay(context);
            }
            else if (element.tagName() == "item")
            {
                QString action = element.attribute("action", "");
                MenuItemContext context(*this, node, showContext, currentContext, action, text, Display);
                displayed |= Displayer.MenuItemDisplay(context);
            }
            else if (element.tagName() == "itemlist")
            {
                QString actiongroup = element.attribute("actiongroup", "");
                MenuItemContext context(*this, node, showContext, currentContext, actiongroup, Display);
                displayed |= Displayer.MenuItemDisplay(context);
            }
        }

         // early exit optimization
        if (!Display && displayed)
            break;
    }
    return displayed;
}


