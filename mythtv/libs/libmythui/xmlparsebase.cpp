#include "xmlparsebase.h"
#include "mythmainwindow.h"

#include "mythcontext.h"

/* ui type includes */
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"

QString XMLParseBase::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

bool XMLParseBase::parseBool(const QString &text)
{
    QString s = text.lower();
    return (s == "yes" || s.toInt());
}

bool XMLParseBase::parseBool(QDomElement &element)
{
    return parseBool(getFirstText(element));
}

QPoint XMLParseBase::parsePoint(const QString &text, bool normalize)
{
    int x, y;
    QPoint retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);

    if (normalize)
        retval = GetMythMainWindow()->NormPoint(retval);

    return retval;
}

QPoint XMLParseBase::parsePoint(QDomElement &element, bool normalize)
{
    return parsePoint(getFirstText(element), normalize);
}

QSize XMLParseBase::parseSize(const QString &text, bool normalize)
{
    int x, y;
    QSize retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QSize(x, y);

    if (normalize)
        retval = GetMythMainWindow()->NormSize(retval);

    return retval;
}

QSize XMLParseBase::parseSize(QDomElement &element, bool normalize)
{
    return parseSize(getFirstText(element), normalize);
}

QRect XMLParseBase::parseRect(const QString &text, bool normalize)
{
    int x, y, w, h;
    QRect retval;
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    if (normalize)
        retval = GetMythMainWindow()->NormRect(retval);

    return retval;
}

QRect XMLParseBase::parseRect(QDomElement &element, bool normalize)
{
    return parseRect(getFirstText(element), normalize);
}

static MythUIType *globalObjectStore = NULL;

MythUIType *XMLParseBase::GetGlobalObjectStore(void)
{
    if (!globalObjectStore)
        globalObjectStore = new MythUIType(NULL, "global store");
    return globalObjectStore;
}

void XMLParseBase::ClearGlobalObjectStore(void)
{
    delete globalObjectStore;
    GetGlobalObjectStore();
}


MythUIType *XMLParseBase::parseUIType(QDomElement &element, const QString &type,
                                      MythUIType *parent, 
                                      MythScreenType *screen)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "element has no name");
        return NULL;
    }

    // check for name in 'screen' && parent

    MythUIType *uitype;
    MythUIType *base;

    QString inherits = element.attribute("from", "");
    if (!inherits.isEmpty())
    {
        MythUIType *base = NULL;
        if (parent)
            base = parent->GetChild(inherits);

        if (screen && !base)
            base = screen->GetChild(inherits);

        if (!base)
            base = GetGlobalObjectStore()->GetChild(inherits);

        if (!base)
        {
            VERBOSE(VB_IMPORTANT, QString("Couldn't find object '%1' to "
                                          "inherit '%2' from")
                                         .arg(inherits).arg(name));
            return NULL;
        }
    }

    if (type == "MythUIImage")
        uitype = new MythUIImage(parent, name);
    else if (type == "MythUIText")
        uitype = new MythUIText(parent, name);

    if (base)
    {
        if (typeid(base) != typeid(uitype))
        {
            VERBOSE(VB_IMPORTANT, QString("Type of new object '%1' doesn't "
                                          "match old '%2'")
                                         .arg(name).arg(inherits));
            delete uitype;
        }
        uitype->CopyFrom(base);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (uitype->ParseElement(info))
            {
            }
            else
            {
            // check other types, fonts
            }
        }
    }

    uitype->Finalize();

    // add to 'screen'
    // add to global if asked
    return uitype;
}

