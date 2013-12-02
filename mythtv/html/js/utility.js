// This file is used SERVER SIDE, it MUST comply to ECMA Script 5.1 and not
// use jQuery or any other client side extensions.
//
// e.g. ECMA Script 5.1 cannot parse ISO DateTime strings
//
// It can however be used clientside as well so long as it complies with the
// above stipulation

// "use strict";

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

// For debugging only, this isn't JSON
function objectDump(arr,level)
{
    var objectString = "<br/>\n";
    if (!level)
        level = 0;

    var indentation = "";
    for (var j = 0; j < (level + 1); j++)
    {
        indentation += "    ";
    }

    var count = 1;
    if (typeof(arr) == 'object')
    {
        for (var item in arr)
        {
            var value = arr[item];

            objectString += count + ". ";
            if (typeof(value) == 'object')
            {
                objectString += indentation + "'" + item + "' ...<br/>\n";
                objectString += objectDump(value, level+1);
            }
            else
            {
                objectString += indentation + "'" + item + "' => \"" + value + "\"<br/>\n";
            }
            count++;
        }
    }
    else
    {
        objectString = "===>"+arr+"<===("+typeof(arr)+")";
    }
    return objectString;
}
