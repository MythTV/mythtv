
/*!
 * \namespace MythCommon
 * \brief Namespace for common functions used in 'TV' pages
 */
var MythCommon = new function() {

    /*!
    * \fn Init
    * \public
    *
    * Initialise, called by the window load event once the page has finished
    * loading
    */
    this.Init = function()
    {
    };

    /*!
    * \fn Destructor
    * \public
    */
    this.Destructor = function()
    {
    };

    /*!
    * \fn RecordProgram
    * \public
    * \param int Channel ID
    * \param string Programme start time
    * \param string Recording rule type
    * \brief Quick schedule
    *
    * Create an instant schedule, with the given type for the programme
    * indentified by the chanID and startTime combo.
    *
    * The schedule editor isn't displayed
    */
    this.RecordProgram = function (chanID, startTime, type)
    {
        hideMenu("optMenu");
        var url = "/tv/ajax_backends/dvr_util.qsp?_action=simpleRecord&ChanId=" + chanID + "&StartTime=" + startTime + "&Type=" + type;
        var ajaxRequest = $.ajax( url )
                                .done(function()
                                {
                                    var response = ajaxRequest.responseText.split("#");
                                    recRuleChanged( response[0], response[1] );
                                });
    }

};

window.addEventListener("load", MythCommon.Init);
window.addEventListener("unload", MythCommon.Destructor);

// TODO: Move what's below into the MythCommon namespace, or elsewhere as
//       appropriate
//


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
// Need to move away from these global vars to the data- attributes or
// something better
var gChanID = 0;
var gStartTime = "";
var gRecordedId = 0;
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
    // Old Way
    // TODO: This is really ugly, it only gets a little better when we switch to
    // recording ids. We need to rethink how this is done without increasing
    // the LOC or HTML
    // Old Way
    gChanID = parent.getAttribute("data-chanid");
    if (isValidVar(parent.getAttribute("data-recordedid")))
        gRecordedId = parent.getAttribute("data-recordedid");

    if (isValidVar(parent.getAttribute("data-recstarttime")))
        gStartTime = parent.getAttribute("data-recstarttime");
    else
        gStartTime = parent.getAttribute("data-starttime");

    // DEBUGGING
    //console.log("RecordedId " + gRecordedId + " ChanId " + gChanID + " StartTime " + gStartTime);

    // New Way
//     $('#optMenu').data('chanID', parent.getAttribute("data-chanid"));
//     $('#optMenu').data('startTime', parentID.getAttribute("data-starttime"));

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

    // TODO: This is really ugly, it only gets a little better when we switch to
    // recording ids. We need to rethink how this is done without increasing
    // the LOC or HTML
    // Old Way
    var recordedId;
    var chanId;
    var startTime;
    if (isValidVar(parent.getAttribute("data-recordedId")))
    {
        recordedId = parent.getAttribute("data-recordedId");
        //console.log("Recorded ID " + recordedId);
    }
    else
    {
        chanId = parent.getAttribute("data-chanid");
        startTime = parent.getAttribute("data-starttime");
        //console.log("Chan ID " + chanId + " StartTime " + startTime);
    }

    // New Way
//     $('#optMenu').data('chanID', parent.getAttribute("data-chanid"));
//     $('#optMenu').data('startTime', parentID.getAttribute("data-starttime"));

    // FIXME: We should be able to get a Program object back from
    // from the Services API that contains all the information available
    // whether it's a recording or not
    var url = "/tv/ajax_backends/program_util.qsp?_action=getProgramDetailHTML&ChanId=" + chanId + "&StartTime=" + startTime;
    if (type == "recording")
        url = "/tv/ajax_backends/program_util.qsp?_action=getRecordingDetailHTML&RecordedId=" + recordedId;

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

function loadScheduler(type, chanID, startTime)
{
    hideMenu("optMenu");
    var layer = document.getElementById(chanID + "_" + startTime);
    if (!isValidObject(layer))
    {
        setErrorMessage("loadScheduler() called with invalid program ID: (" + chanID + "_" + startTime + ")");
        return;
    }

    // The order of preference for GetRecordSchedule is as follows:
    //     "recordid" will load the rule associated with this id, metadata
    //     associated with the first program against which the rule was created
    //
    //     "recordedid" will load metadata associated with the specific
    //     selected recording which we want
    //
    //     "chanid & starttime" will load metadata associated with the relevant
    //     program and the rule which currently matches this program in the scheduler
    //     if there are multiple which apply, otherwise a new rule will be created?

    // Ensure we use the program starttime, even if recording starttime
    // was passed in
    if (isValidVar(layer.getAttribute("data-starttime")))
        startTime = layer.getAttribute("data-starttime");

    if (type == "recording" &&
        isValidVar(layer.getAttribute("data-recordedid")) &&
        layer.getAttribute("data-recordedid") > 0)
    {
        var recordedID = layer.getAttribute("data-recordedid");
        loadFrontendContent('/tv/schedule.qsp?RecordedId=' + recordedID + '&amp;ChanId=' + chanID + '&amp;StartTime=' + startTime);
    }
    else if (type == "upcoming")
    {
        var recRuleID = layer.getAttribute("data-recordid");
        loadFrontendContent('/tv/schedule.qsp?ChanId=' + chanID + '&amp;StartTime=' + startTime);
    }
    else // type == "rule"
    {
        var recRuleID = layer.getAttribute("data-recordid");
        loadFrontendContent('/tv/schedule.qsp?RecRuleId=' + recRuleID + '&amp;ChanId=' + chanID + '&amp;StartTime=' + startTime);
    }
}

function checkRecordingStatus(chanID, startTime)
{
//     var url = "/tv/ajax_backends/dvr_util.qsp?_action=checkRecStatus&ChanId=" + chanID + "&StartTime=" + startTime;
//     var ajaxRequest = $.ajax( url ).done(function()
//                             {
//                                 var response = ajaxRequest.responseText.trim().split("#");
//                                 var id = response[0] + "_" + response[1];
//                                 var layer = document.getElementById(id);
//                                 toggleClass(layer, "programScheduling");
//                                 // toggleClass(layer, response[2]);
//                                 var popup = document.getElementById(id + "_schedpopup");
//                                 toggleVisibility(popup);
//                                 // HACK: Easiest way to ensure that everything
//                                 //       on the page is correctly updated for now
//                                 //       is to reload
//                                 reloadTVContent();
//                             });
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
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=deleteRecRule&RecRuleId=" + recRuleID + "&ChanId=" + chanID + "&StartTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.trim().split("#");
                                recRuleChanged( response[0], response[1] );
                                reloadTVContent();
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
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=dontRecord&ChanId=" + chanID + "&StartTime=" + startTime;
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
    var url = "/tv/ajax_backends/dvr_util.qsp?_action=neverRecord&ChanId=" + chanID + "&StartTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.trim().split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

function loadTVContent(url, targetDivID, transition, args)
{
    var historyUrl = url;
    for (var key in args)
    {
        // Check that this arg hasn't already been appended
        // we don't currently support altering of existing args
        if (url.indexOf(key) === -1)
        {
            // Check if any args presently exist
            if (url.indexOf("?") !== -1)
                url += "&";
            else
                url += "?";

            url += key + "=" + args[key];

            // If this page is reloaded from the history we want the whole
            // page, not just the list part so we want to strip the 'ListOnly'
            // argument
            if (key !== "ListOnly")
            {
                if (historyUrl.indexOf("?") !== -1)
                    historyUrl += "&";
                else
                    historyUrl += "?";

                historyUrl += key + "=" + args[key];
            }
        }
    }

    console.log("Storing history: " + historyUrl);
    // pushState might be more ideal except that it doesn't actually appear to
    // work properly. Hitting 'back' with pushed states _for the same page_ doesn't
    // actually load the old page state, it just reloads the present state.
    //
    // We can 'fix' this in future by altering the page 'src' based on the state
    // object
    history.pushState(args, '', historyUrl); // Update the page url in the browser

    if (!targetDivID)
        targetDivID = "content";
    if ((getSetting('', 'WebFrontend_enableAnimations', 1) == 0) ||
        !transition)
        transition = "none";

    if (targetDivID === "content" && transition === "none")
    {
        loadFrontendContent(url);
        return;
    }

    parent.document.getElementById("busyPopup").style.display = "block";

    var html = $.ajax({ url: url,
                        dataType: "html",
                        success:function(data, textStatus, jqXHR)
                            {
                                if (data)
                                    setTVContent(targetDivID, data, transition);
                            }
                        });
}

function setTVContent(divID, html, transition)
{
    var targetDiv = document.getElementById(divID);
    if (!isValidObject(targetDiv))
    {
        setErrorMessage("loadTVContent() target DIV doesn't exist: (" + divID + ")");
        parent.document.getElementById("busyPopup").style.display = "none";
        return;
    }

    var newDiv = document.createElement('div');
    newDiv.className = targetDiv.className;
    newDiv.innerHTML = html;
    targetDiv.parentNode.insertBefore(newDiv, null);

    // Need to assign the id to the new div
    targetDiv.id = "old" + divID;
    newDiv.id = divID;
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
        case 'none':
            postLoad(targetDiv.id, newDiv.id);
            break;

    }
    newDiv.id = divID;

    parent.document.getElementById("busyPopup").style.display = "none";
}

function reloadTVContent()
{
    loadFrontendContent(window.location.href);
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

    var path = formElement.action;
    var args = formArray(formElement);

    loadTVContent(path, target, transition, args);
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
            if (data)
            {
                console.log("saveFormData response: " + data.trim());
                callback(data.trim());
            }
            $("#saveFormDataSuccess").show().delay(5000).fadeOut();
        },
        error: function(jqXHR, textStatus, errorThrown)
        {
            $("#saveFormDataError").show().delay(5000).fadeOut();
        }
    });
    return false;
}

function scrollCallback()
{
    $("#NumPages").val(Number($("#NumPages").val())+1);
}

function loadJScroll()
{
    if (!document.querySelector(".jscroll"))
        return;

    // Always have at least one window heights worth loaded off-screen
    $('.jscroll').jscroll({
    padding: $(window).height(),
    nextSelector: 'a.jscroll-next:last',
    callback: function() { scrollCallback(); }
    });
}

function postLoad(oldDivID, newDivID)
{
    $("#" + oldDivID).remove();
    loadJScroll();
}

// CSS (Native) transitions are faster than jQuery and preferred generally
function leftSlideTransition(oldDivID, newDivID)
{
    // Transition works much better with a fixed width, so temporarily set
    // the width based on the parent,
    $("#" + newDivID).css("width", $("#" + oldDivID).width());
    $("#" + newDivID).addClass("leftSlideInTransitionStart");
    $("#" + oldDivID).bind("transitionend", function() {
        postLoad(oldDivID, newDivID);

    });
    $("#" + newDivID).bind("transitionend", function() {
        $("#" + newDivID).removeClass("leftSlideInTransitionStart");
        $("#" + newDivID).removeClass("leftSlideInTransition");
        $("#" + newDivID).css("width", null);
    });
    $("#" + newDivID).addClass("leftSlideInTransition");
    $("#" + oldDivID).addClass("leftSlideOutTransition");
}

// CSS (Native) transitions are faster than jQuery and preferred generally
function rightSlideTransition(oldDivID, newDivID)
{
    // HACK Transition works much better with a fixed width, so temporarily set
    // the width based on the parent,
    $("#" + newDivID).css("width", $("#" + oldDivID).width());
    $("#" + newDivID).addClass("rightSlideInTransitionStart");
    $("#" + oldDivID).bind("transitionend", function() {
        postLoad(oldDivID, newDivID);

    });
    $("#" + newDivID).bind("transitionend", function() {
        $("#" + newDivID).removeClass("rightSlideInTransitionStart");
        $("#" + newDivID).removeClass("rightSlideInTransition");
        $("#" + newDivID).css("width", null);
    });
    $("#" + newDivID).addClass("rightSlideInTransition");
    $("#" + oldDivID).addClass("rightSlideOutTransition");
}

// CSS (Native) transitions are faster than jQuery and preferred generally
function dissolveTransition(oldDivID, newDivID)
{
    $("#" + newDivID).addClass("dissolveInTransitionStart");
    $("#" + oldDivID).bind("animationend", function() {
        $("#" + oldDivID).remove();
        postLoad();
    });
    $("#" + newDivID).bind("animationend", function() {
        $("#" + newDivID).removeClass("dissolveInTransitionStart");
        $("#" + newDivID).removeClass("dissolveInTransition");
    });

    // Browser prefixes suck
    $("#" + oldDivID).bind("webkitAnimationEnd", function() {
        postLoad(oldDivID, newDivID);
    });
    $("#" + newDivID).bind("webkitAnimationEnd", function() {
        $("#" + newDivID).removeClass("dissolveInTransitionStart");
        $("#" + newDivID).removeClass("dissolveInTransition");
    });
    // End prefixed code

    $("#" + newDivID).addClass("dissolveInTransition");
    $("#" + oldDivID).addClass("dissolveOutTransition");
}

function handleKeyboardShortcutsTV(key) {
    if (key.keyCode == 27)
        hideMenu("optMenu");
}

window.onpopstate = function(event)
{
    // Only force load if this is a 'fake' history for which we have saved
    // state. For 'normal' history navigation this isn't required.
    // FIXME: Disable this check for now, since we need to start saving state
    //        for 'default' pages before it works as intended
    if (event) // && event.state)
    {
        // TODO: Do the load ourselves to allow animated transitions?
        window.location = window.location; // Feels faster than location.reload(); ?
    }
}
