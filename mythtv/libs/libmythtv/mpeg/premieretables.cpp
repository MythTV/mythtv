#include "premieretables.h"

bool PremiereContentInformationTable::IsEIT(uint table_id)
{
    return table_id == TableID::PREMIERE_CIT;
}
