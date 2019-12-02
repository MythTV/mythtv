#ifndef MYTHUIUTILS_H_
#define MYTHUIUTILS_H_

#include <cstdlib>

#include "mythuiexp.h"

class QString;
class MythUIType;

struct MUI_PUBLIC ETPrintWarning
{
    static bool Child(const MythUIType *container, const QString &child_name);
    static bool Container(const QString &child_name);
};

struct MUI_PUBLIC ETPrintError
{
    static bool Child(const MythUIType *container, const QString &child_name);
    static bool Container(const QString &child_name);
};

template <typename ErrorDispatch = ETPrintWarning>
struct UIUtilDisp
{
    template <typename ContainerType, typename UIType>
    static bool Assign(ContainerType *container, UIType *&item,
                       const QString &name, bool *err = nullptr)
    {
        if (!container)
        {
            if (err)
                *err |= ErrorDispatch::Container(name);
            else
                ErrorDispatch::Container(name);
            return true;
        }

        item = dynamic_cast<UIType *>(container->GetChild(name));

        if (item)
            return false;

        if (err)
            *err |= ErrorDispatch::Child(container, name);
        else
            ErrorDispatch::Child(container, name);
        return true;
    }
};

using UIUtilW = struct UIUtilDisp<ETPrintWarning>;
using UIUtilE = struct UIUtilDisp<ETPrintError>;

#endif
