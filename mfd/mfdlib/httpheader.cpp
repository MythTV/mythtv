/*
	httpheader.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an http header object

*/

#include "httpheader.h"

HttpHeader::HttpHeader(const QString &input_line)
{
    //
    //  Take a line of text and parse into a matching set of HTTP headers
    //  field: value
    //
    
    field = input_line.section(':', 0, 0);
    value = input_line.section(':', 1);

    field = field.simplifyWhiteSpace();
    value = value.simplifyWhiteSpace();

}

const QString& HttpHeader::getField()
{
    return field;
}

const QString& HttpHeader::getValue()
{
    return value;
}

HttpHeader::~HttpHeader()
{
}

