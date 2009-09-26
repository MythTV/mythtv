#ifndef MYTHUIUTILS_H_
#define MYTHUIUTILS_H_

#include <cstdlib>

#include "mythexp.h"

class QString;

struct MPUBLIC ETPrintWarning
{
    static bool Child(const QString &container_name, const QString &child_name);
    static bool Container(const QString &child_name);
};

struct MPUBLIC ETPrintError
{
    static bool Child(const QString &container_name, const QString &child_name);
    static bool Container(const QString &child_name);
};

template <typename ErrorDispatch = ETPrintWarning>
struct UIUtilDisp
{
    template <typename ContainerType, typename UIType>
    static bool Assign(ContainerType *container, UIType *&item,
                       const QString &name, bool *err = NULL)
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
            *err |= ErrorDispatch::Child(container->objectName(), name);
        else
            ErrorDispatch::Child(container->objectName(), name);
        return true;
    }
};

typedef struct UIUtilDisp<ETPrintWarning> UIUtilW;
typedef struct UIUtilDisp<ETPrintError> UIUtilE;

#endif
