/*
    httpgetvar.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    Methods for an object to hold GET variables

*/

#include "httpgetvar.h"

HttpGetVariable::HttpGetVariable(const QString &text_segment)
{
    //
    //  Take a line of text and parse into a matching set of HTTP get
    //  variable field names and values
    //
    
    field = text_segment.section('=', 0, 0);
    value = text_segment.section('=', 1);

    //
    //  Do %20 (etc.) changes ?
    //
}

HttpGetVariable::HttpGetVariable(const QString &label, const QString &a_value)
{
    field = label;
    value = a_value;
}

const QString& HttpGetVariable::getField()
{
    return field;
}

const QString& HttpGetVariable::getValue()
{
    return value;
}

HttpGetVariable::~HttpGetVariable()
{
}

