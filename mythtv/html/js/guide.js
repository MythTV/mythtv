
function isValidObject(variable)
{
    return ((typeof variable === 'object') && (variable != null));
}

function toggleVisibility(layer, show)
{
    // We can override the toggle behaviour with the show arg
    if (typeof show === 'boolean')
    {
        layer.style.display = show ? "block" : "none";
    }
    else
        layer.style.display = (layer.style.display != "block") ? "block" : "none";
}

function checkVisibility(layer)
{
    return (layer.style.display == "block");
}

function moveToPosition(layer, customer)
{
    // getBoundingClientRect() excludes the offset due to scrolling, the
    // values return are relative to the currently displayed area
    var customerLeft = customer.getBoundingClientRect().left;
    var customerTop = customer.getBoundingClientRect().top;
    var customerBottom = customer.getBoundingClientRect().bottom;
    var layerWidth  = layer.clientWidth;
    var layerHeight = layer.clientHeight;
    var pageTop = 0;

    // If our layer is inside an absolute DIV then we need to compensate for
    // that parent DIVs position
    var parentXOffset = layer.parentNode.getBoundingClientRect().left;
    var parentYOffset = layer.parentNode.getBoundingClientRect().top;

    var pageBottom = window.innerHeight; //document.body.clientHeight;
    var pageWidth = document.body.clientWidth; //window.innerHeight;

    //alert("Page Width: " + pageWidth + " Customer Left: " + customerLeft + " Layer Width: " + layerWidth + " Parent Offset: " + parentXOffset);
    //alert("Page Height: " + pageHeight + " Customer Bottom: " + customerBottom + " Layer Height: " + layerHeight + " Parent Offset: " + parentYOffset);

    if ((customerLeft + layerWidth + 10) < pageWidth + window.pageXOffset)
    {
        layer.style.left = (customerLeft + 10 - parentXOffset) + "px";
    }
    else
        layer.style.left = (pageWidth - layerWidth - 5 - parentXOffset) + "px";

    if ((customerBottom + layerHeight) < pageBottom + window.pageYOffset)
    {
        layer.style.top = (customerBottom - parentYOffset) + "px";
    }
    else
        layer.style.top = (customerTop - layerHeight - parentYOffset) + "px";
}

function toggleClass(element, className)
{
    class_array = element.className.split(" ");

    if (class_array.length > 0 &&
        class_array[class_array.length - 1] == className)
    {
        class_array.pop();
    }
    else
    {
        class_array.push(className);
    }

    element.className = class_array.join(" ");
}

var timer;
function startDetailTimer(parent)
{
    var parentID = parent.id;
    timer = setTimeout(function(){showDetail(parentID)}, 1500);
}

function showDetail(parentID)
{
    clearTimeout(timer);

    var menu = document.getElementById('recMenu');
    if (checkVisibility(menu))
        return;

    var parent = document.getElementById(parentID);
    var content = document.getElementById(parentID + '_detail');
    var layer = document.getElementById('programDetails');

    layer.innerHTML = content.innerHTML;
    toggleVisibility(layer, true);
    moveToPosition(layer, parent);
}

function hideDetail(parent)
{
    clearTimeout(timer);

    var parentID = parent.id;
    var layer = document.getElementById('programDetails');

    if (!isValidObject(layer))
        return;

    toggleVisibility(layer, false);
}

var selectedElement;
var chanID = "";
var startTime = "";
function showRecMenu(parent)
{
    hideDetail(parent);
    var menu = document.getElementById('recMenu');
    // Reset visibility
    if (checkVisibility(menu))
    {
        toggleVisibility(menu);

        // Reset any currently selected program
        if (isValidObject(selectedElement))
            toggleClass(selectedElement, "programSelected");
    }

    if (selectedElement === parent)
    {
        selectedElement = null;
        return;
    }

    var parentID = parent.id;
    var values = parentID.split("_", 2);
    chanID = values[0];
    startTime = values[1];

    // Toggle the "programSelected" class on the program div, this gives the
    // user visual feedback on which one the menu relates to
    selectedElement = parent;
    toggleClass(selectedElement, "programSelected");

    // Display the menu
    toggleVisibility(menu);

    // Move the menu position to the vicinity of the click event
    // (may change this to immediately be above/below the selected program div
    //  instead)
    moveToPosition(menu, parent);
}

function recordProgram(chanID, startTime, type)
{
    
}
