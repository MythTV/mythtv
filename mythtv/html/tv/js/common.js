
function jq(id) // F*%$ jQuery
{
    return "#" + id.replace( /(:|\.|\[|\])/g, "\\$1" );
}

function toggleVisibility(layer, show)
{
    if (!isValidObject(layer))
    {
        setErrorMessage("toggleVisibility() called with invalid element");
        return;
    }

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
    if (!isValidObject(layer))
    {
        setErrorMessage("toggleVisibility() called with invalid element");
        return;
    }

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
    //alert("Page Bottom: " + pageBottom + " Page Scroll Offset: " +  window.pageYOffset + " Customer Bottom: " + customerBottom + " Layer Height: " + layerHeight + " Parent Offset: " + parentYOffset);

    if ((customerLeft + layerWidth + 10) < (pageWidth + window.pageXOffset))
    {
        layer.style.left = (customerLeft + 10 - parentXOffset) + "px";
    }
    else
        layer.style.left = (pageWidth - layerWidth - 5 - parentXOffset) + "px";

    if ((customerBottom + layerHeight) < pageBottom)
    {
        layer.style.top = (customerBottom - parentYOffset) + "px";
    }
    else
        layer.style.top = (customerTop - layerHeight - parentYOffset) + "px";
}

function toggleClass(element, className)
{
    if (!isValidObject(element))
    {
        setErrorMessage("toggleClass() called with invalid element");
        return;
    }

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
    if (!isValidObject(element))
    {
        setErrorMessage("isClassSet() called with invalid element");
        return;
    }

    class_array = element.className.split(" ");

    var index = class_array.lastIndexOf(className);
    if (index >= 0)
        return true;

    return false;
}

var selectedElement;
var gChanID = 0; // Need to move away from these to the data- attributes
var gStartTime = ""; // Need to move away from these to the data- attributes
function showMenu(parent, typeStr)
{
    hideDetail(parent);
    hideMenu("optMenu");

    if (selectedElement === parent)
    {
        selectedElement = null;
        return;
    }

    var parentID = parent.id;
    var values = parentID.split("_", 2);
    // Old Way
    gChanID = values[0];
    gStartTime = values[1];
    // New Way
//     $('#optMenu').data('chanID', values[0]);
//     $('#optMenu').data('startTime', values[1]);

    var menu = document.getElementById("optMenu");
    var types = typeStr.split(' ');
    for (var i = 0; i < types.length; i++)
    {
        var children = menu.getElementsByClassName(types[i]);
        for (var j = 0; j < children.length; j++)
        {
            children[j].style.display = "block";
        }
    }


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
    if (!isValidObject(menu))
    {
        setErrorMessage("hideMenu() called with invalid menu ID");
        return;
    }

    var children = menu.getElementsByClassName("button");
    for (var i = 0; i < children.length; i++)
    {
        children[i].style.display = "none";
    }
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
    if (!isValidObject(parent))
    {
        setErrorMessage("startDetailTimer() called with invalid parent");
        return;
    }

    var parentID = parent.id;
    timer = setTimeout(function(){showDetail(parentID, type)}, 1200);
}

function showDetail(parentID, type)
{
    clearTimeout(timer);

    // Check whether a context menu is being displayed and if so, then don't
    // display the detail popup
    var parent = document.getElementById(parentID);
    if (!isValidObject(parent))
    {
        //setErrorMessage("showDetail() called with invalid parent ID: (" + parentID + ")");
        return;
    }

    if (isClassSet(parent, "itemSelected"))
        return;

    var values = parentID.split("_", 2);
    var chanId = values[0];
    var startTime = values[1];
//     $('#programDetails').data('chanID', values[0]);
//     $('#programDetails').data('startTime', values[1]);

    // FIXME: We should be able to get a Program object back from
    // from the Services API that contains all the information available
    // whether it's a recording or not
    var method = "getProgramDetailHTML";
    if (type == "recording")
        method = "getRecordingDetailHTML";

    var url = "/tv/ajax_backends/program_util.qsp?_action=" + method + "&chanID=" + chanId + "&startTime=" + startTime;
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

    if (!isValidObject(parent))
    {
        setErrorMessage("hideDetail() called with invalid parent ID: (" + parentID + ")");
        return;
    }

    var parentID = parent.id;
    var layer = document.getElementById('programDetails');

    if (!isValidObject(layer))
        return;

    toggleVisibility(layer, false);
}

function loadScheduler(chanID, startTime)
{
    hideMenu("optMenu");
    var layer = document.getElementById(chanID + "_" + startTime);
    if (!isValidObject(layer))
    {
        setErrorMessage("loadScheduler() called with invalid program ID: (" + chanID + "_" + startTime + ")");
        return;
    }
    var recRuleID = layer.getAttribute("data-recordid");
    if (isValidVar(layer.getAttribute("data-chanid")))
        chanID = layer.getAttribute("data-chanid");
    if (isValidVar(layer.getAttribute("data-starttime")))
        startTime = layer.getAttribute("data-starttime");
    loadContent('/tv/schedule.qsp?chanId=' + chanID + '&amp;startTime=' + startTime + '&amp;recRuleId=' + recRuleID);
}

function checkRecordingStatus(chanID, startTime)
{
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=checkRecStatus&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url ).done(function()
                            {
                                var response = ajaxRequest.responseText.trim().split("#");
                                var id = response[0] + "_" + response[1];
                                var layer = document.getElementById(id);
                                toggleClass(layer, "programScheduling");
                                // toggleClass(layer, response[2]);
                                var popup = document.getElementById(id + "_schedpopup");
                                toggleVisibility(popup);
                                // HACK: Easiest way to ensure that everything
                                //       on the page is correctly updated for now
                                //       is to reload
                                reloadTVContent();
                            });
}

function recRuleChanged(chanID, startTime)
{
    var layer = document.getElementById(chanID + "_" + startTime);
    if (!isValidObject(layer))
    {
        setErrorMessage("recRuleChanged called with invalid program ID: (" + chanID + "_" + startTime + ")");
        return;
    }
    toggleClass(layer, "programScheduling");
    var popup = document.getElementById(chanID + "_" + startTime + "_schedpopup");
    if (!isValidObject(popup))
    {
        setErrorMessage("recRuleChanged() called with invalid popup ID: (" + chanID + "_" + startTime + "_schedpopup)");
        return;
    }
    toggleVisibility(popup);

    setTimeout(function(){checkRecordingStatus(chanID, startTime)}, 2500);
}

function deleteRecRule(chanID, startTime)
{
    var layer = document.getElementById(chanID + "_" + startTime);
    if (!isValidObject(layer))
    {
        setErrorMessage("deleteRecRule() called with invalid program ID: (" + chanID + "_" + startTime + ")");
        return;
    }
    var recRuleID = Number(layer.getAttribute("data-recordid"));
    if (recRuleID <= 0)
    {
        setErrorMessage("deleteRecRule() called with invalid Recording Rule ID: (" + recRuleID + ")");
        return;
    }
    hideMenu("optMenu");
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=deleteRecRule&recRuleID=" + recRuleID + "&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.trim().split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

function dontRecord(chanID, startTime)
{
    var layer = document.getElementById(chanID + "_" + startTime);
    if (!isValidObject(layer))
    {
        setErrorMessage("dontRecord() called with invalid program ID: (" + chanID + "_" + startTime + ")");
        return;
    }
    var recRuleID = layer.getAttribute("data-recordid");
    hideMenu("optMenu");
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=dontRecord&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.trim().split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

function neverRecord(chanID, startTime)
{
    var layer = document.getElementById(chanID + "_" + startTime);
    if (!isValidObject(layer))
    {
        setErrorMessage("neverRecord() called with invalid program ID: (" + chanID + "_" + startTime + ")");
        return;
    }
    var recRuleID = layer.getAttribute("data-recordid");
    hideMenu("optMenu");
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=neverRecord&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.trim().split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

function loadTVContent(url, targetDivID, transition, args)
{
    currentContentURL = url;   // currentContentURL is defined in util.qjs

    if (!targetDivID)
        targetDivID = "content";
    if (!transition)
        transition = "none"; // dissolve

    if (transition === "none")
    {
        loadContent(url);
        return;
    }

    $("#busyPopup").show();

    var targetDiv = document.getElementById(targetDivID);
    if (!isValidObject(targetDiv))
    {
        setErrorMessage("loadTVContent() target DIV doesn't exist: (" + targetDivID + ")");
        return;
    }
    var newDiv = document.createElement('div');
    newDiv.style = "left: 100%";
    targetDiv.parentNode.insertBefore(newDiv, null);

    for (var key in args)
    {
        // Check that this arg hasn't already been appended
        // we don't currently support altering of existing args
        if (url.indexOf(key) === -1)
        {
            // Check if any args presently exist
            if (url.indexOf("?") !== -1)
                url += "&amp;";
            else
                url += "?";

            url += key + "=" + args[key];
        }
    }

    var html = $.ajax({
      url: url,
        async: false
     }).responseText;

    newDiv.innerHTML = html;
    newDiv.className = targetDiv.className;

    // Need to assign the id to the new div
    newDiv.id = targetDivID;
    targetDiv.id = "old" + targetDivID;
    switch (transition)
    {
        case 'left':
            leftSlideTransition(targetDiv.id, newDiv.id);
            break;
        case 'right':
            rightSlideTransition(targetDiv.id, newDiv.id);
            break;
        case 'dissolve':
            dissolveTransition(targetDiv.id, newDiv.id);
            break;
    }
    newDiv.id = targetDivID;

    $("#busyPopup").hide();
}

function reloadTVContent()
{
    loadTVContent(currentContentURL);  // currentContentURL is defined in util.qjs
}

function formOnLoad(form)
{
    // Prepopulate some user-attributes to save time later
    var elements = form.elements;
    for (var idx = 0; idx < elements.length; idx++)
    {
        if (elements[idx].tagName == "SELECT")
            elements[idx].setAttribute('data-oldIndex', elements[idx].selectedIndex);
    }
}

function submitForm(formElement, target, transition)
{
    if (!isValidObject(formElement))
    {
        setErrorMessage("submitForm() invalid form element");
        return;
    }
    var queryString = $(formElement).serialize();
    var url = formElement.action + "?" + queryString;

    loadTVContent(url, target, transition);
    return false;
}

function saveFormData(formElement, callback)
{
    if (!isValidObject(formElement))
    {
        setErrorMessage("submitForm() invalid form element");
        return;
    }

    var url = formElement.action;
    var postData = $(formElement).serializeArray();

    $.ajax(
    {
        url : url,
        type: "POST",
        data : postData,
        dataType: "text",
        success:function(data, textStatus, jqXHR)
        {
            callback(data.trim());
            $("#saveFormDataSuccess").show().delay(5000).fadeOut();
        },
        error: function(jqXHR, textStatus, errorThrown)
        {
            $("#saveFormDataError").show().delay(5000).fadeOut();
        }
    });
    return false;
}

function loadJScroll()
{
    // Always have at least one window heights worth loaded off-screen
    $('.jscroll').jscroll({
    padding: $(window).height(),
    nextSelector: 'a.jscroll-next:last',
    callback: function() { scrollCallback(); }
    });
}

function postLoad()
{
    //loadJScroll();
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
                   $("#" + oldDivID).remove(); postLoad(); });
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
                   $("#" + oldDivID).remove(); postLoad(); });
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
                   $("#" + oldDivID).remove(); postLoad(); });
    $("#" + newDivID).animate({opacity: "1.0"}, 800);
}

function handleKeyboardShortcutsTV(key) {
    if (key.keyCode == 27)
        hideMenu("optMenu");
}
