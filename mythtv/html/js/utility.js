// This file is used SERVER SIDE, it MUST comply to ECMA Script 5.1 and not
// use jQuery or any other client side extensions.
//
// e.g. ECMA Script 5.1 cannot parse ISO DateTime strings
//
// It can however be used clientside as well so long as it complies with the
// above stipulation

// "use strict";

function getArg(name)
{
    name = name.toLowerCase();

    if (isValidObject(this.Parameters))
        return this.Parameters[name];
    else
        return "";
}

function getIntegerArg(name)
{
    var ret = Number(getArg(name));
    return ret.NaN ? Number(0) : ret;
}

function getBoolArg(name)
{
    // Should check for string types here such as 'true' and 'false'
    return Boolean(getIntegerArg(name));
}

function getArgCount()
{
    if (isValidObject(this.Parameters))
        return Number(Object.keys(this.Parameters).length);
    else
        return Number(0);
}

function toCapitalCase(str)
{
    return str.charAt(0).toUpperCase() + str.slice(1);
}

function escapeHTML(str)
{
    str = str.replace(/&/g, "&amp;")
                .replace(/'/g, "&apos;")
                .replace(/"/g, "&quot;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;");

    return str;
}

function isValidObject(variable)
{
    return ((typeof variable === "object")
            && typeof variable !== "undefined"
            && (variable != null));
}

function isValidVar(variable)
{
    return ((variable != null) && (variable != ""));
}

// For debugging only, this isn't JSON
function objectDump(arr)
{
    var objectString = "<dl>\n";

    var count = 1;
    if (typeof(arr) == 'object')
    {
        for (var item in arr)
        {
            var value = arr[item];

            objectString += "<dt>" + count + ". ";
            if (typeof(value) == 'object')
            {
                objectString += "'" + item + "' \n";
                objectString += "<dd>" + objectDump(value) + "</dd></dt>";
            }
            else
            {
                objectString += "'" + item + "' => \"" + value + "\"</dt>\n";
            }
            count++;
        }
    }
    else
    {
        objectString = "<dt>===>"+arr+"<===("+typeof(arr)+")</dt>";
    }
    objectString += "</dl>\n";
    return objectString;
}

function toQueryString(obj)
{
  var str = [];
  for(var key in obj)
  {
    if (obj.hasOwnProperty(key))
    {
      str.push(encodeURIComponent(key) + "=" + encodeURIComponent(obj[key]));
    }
  }
  return str.join("&amp;");
}
