
function getChild(element, name)
{
    if (!isValidObject(element))
        return;

    var children = element.children;
    for (var i = 0; i < children.length; i++)
    {
        if (children[i].name == name)
            return children[i];
    }
}

function getChildValue(element, name)
{
    var child = getChild(element, name);
    if (isValidObject(child))
        return child.value;

    return "";
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

    var index = class_array.lastIndexOf(className);
    if (index >= 0)
    {
        class_array.splice(index,1);
    }
    else
    {
        class_array.push(className);
    }

    element.className = class_array.join(" ");
}

function isClassSet(element, className)
{
    class_array = element.className.split(" ");

    var index = class_array.lastIndexOf(className);
    if (index >= 0)
        return true;

    return false;
}

var selectedElement;
var chanID = "";
var startTime = "";
function showMenu(parent, menuName)
{
    hideDetail(parent);
    hideMenu(menuName);

    if (selectedElement === parent)
    {
        selectedElement = null;
        return;
    }

    var parentID = parent.id;
    var values = parentID.split("_", 2);
    chanID = values[0];
    startTime = values[1];

    var menu = document.getElementById(menuName);

    // Toggle the "itemSelected" class on the program div, this gives the
    // user visual feedback on which one the menu relates to
    selectedElement = parent;
    toggleClass(selectedElement, "itemSelected");

    // Display the menu
    toggleVisibility(menu);

    // Move the menu position to the vicinity of the click event
    // (may change this to immediately be above/below the selected program div
    //  instead)
    moveToPosition(menu, parent);
}

function hideMenu(menuName)
{
    var menu = document.getElementById(menuName);
    // Reset visibility
    if (checkVisibility(menu))
    {
        toggleVisibility(menu);

        // Reset any currently selected program
        if (isValidObject(selectedElement))
            toggleClass(selectedElement, "itemSelected");
    }
}

var timer;
function startDetailTimer(parent, type)
{
    var parentID = parent.id;
    timer = setTimeout(function(){showDetail(parentID, type)}, 1200);
}

function showDetail(parentID, type)
{
    clearTimeout(timer);

    // Check whether a context menu is being displayed and if so, then don't
    // display the detail popup
    var parent = document.getElementById(parentID);
    if (isClassSet(parent, "itemSelected"))
        return;

    var values = parentID.split("_", 2);
    chanID = values[0];
    startTime = values[1];

    // FIXME: We should be able to get a Program object back from
    // from the Services API that contains all the information available
    // whether it's a recording or not
    var method = "getProgramDetailHTML";
    if (type == "recording")
        method = "getRecordingDetailHTML";

    var url = "/tv/qjs/program_util.qjs?action=" + method + "&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                        {
                            var parent = document.getElementById(parentID);
                            var layer = document.getElementById('programDetails');

                            layer.innerHTML = ajaxRequest.responseText;
                            toggleVisibility(layer, true);
                            moveToPosition(layer, parent);

//                             $("#debug").html( ajaxRequest.responseText );
//                             $("#debug").show();
                        });
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

function loadScheduler(chanID, startTime, from)
{
    hideMenu(from);
    loadContent('/tv/schedule.qsp?chanId=' + chanID + '&amp;startTime=' + startTime);
}

function deleteRecRule(chandID, startTime)
{
    var layer = document.getElementById(chanID + "_" + startTime);
    var recRuleID = getChildValue(layer, "recordid");
    hideMenu("editRecMenu");
    var url = "/tv/qjs/dvr_util.qjs?action=deleteRecRule&recRuleID=" + recRuleID + "&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

function leftSlideTransition(oldDivID, newDivID)
{
    // Transition works much better with a fixed width, so temporarily set
    // the width based on the parent,
    $("#" + newDivID).css("width", $("#" + oldDivID).width());
    $("#" + newDivID).css("left", "100%");
    $("#" + oldDivID).css("z-index", "-20");
    $("#" + newDivID).css("z-index", "-10");
    var oldLeft = $("#" + oldDivID).position().left;
    $("#" + oldDivID).animate({opacity: "0.3"}, 800, function() {
                   $("#" + oldDivID).remove(); });
    $("#" + newDivID).animate({left: oldLeft}, 800, function() {
                   $("#" + newDivID).css("width", '');
                   $("#" + newDivID).css("z-index", "0"); });
}

function rightSlideTransition(oldDivID, newDivID)
{
    $("#" + newDivID).css("width", $("#" + oldDivID).width());
    $("#" + newDivID).css("left", "-100%");
    $("#" + oldDivID).css("z-index", "-20");
    $("#" + newDivID).css("z-index", "-10");
    var oldLeft = $("#" + oldDivID).position().left;
    $("#" + oldDivID).animate({opacity: "0.3"}, 800, function() {
                   $("#" + oldDivID).remove(); });
    $("#" + newDivID).animate({left: oldLeft}, 800, function() {
                   $("#" + newDivID).css("width", '');
                   $("#" + newDivID).css("z-index", "0"); });
}

function dissolveTransition(oldDivID, newDivID)
{
    $("#" + newDivID).css("opacity", "0.0");
    var oldLeft = $("#" + oldDivID).position().left;
    $("#" + newDivID).css("left", oldLeft);
    $("#" + oldDivID).animate({opacity: "0.0"}, 800, function() {
                   $("#" + oldDivID).remove(); });
    $("#" + newDivID).animate({opacity: "1.0"}, 800);
}
