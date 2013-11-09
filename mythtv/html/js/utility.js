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
