
function recordProgram(chanID, startTime, type)
{
    hideMenu("recMenu");
    var url = "/tv/qjs/dvr_util.qjs?action=simpleRecord&chanID=" + chanID + "&startTime=" + startTime + "&type=" + type;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

function checkRecordingStatus(chanID, startTime)
{
    var url = "/tv/qjs/dvr_util.qjs?action=checkRecStatus&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url ).done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                var id = response[0] + "_" + response[1];
                                var layer = document.getElementById(id);
                                toggleClass(layer, "programScheduling");
                                // toggleClass(layer, response[2]);
                                var popup = document.getElementById(id + "_schedpopup");
                                toggleVisibility(popup);
                                // HACK: Easiest way to ensure that everything
                                //       on the page is correctly updated for now
                                //       is to reload
                                reloadGuideContent();
                            });
}

function recRuleChanged(chandID, startTime)
{
    var layer = document.getElementById(chanID + "_" + startTime);
    toggleClass(layer, "programScheduling");
    var popup = document.getElementById(chanID + "_" + startTime + "_schedpopup");
    toggleVisibility(popup);

    setTimeout(function(){checkRecordingStatus(chanID, startTime)}, 2500);
}

function loadGuideContent(url, transition)
{
    currentContentURL = url;   // currentContentURL is defined in util.qjs
    if (!transition)
        transition = "dissolve";

    $("#busyPopup").show();

    var guideDivID = "guideGrid";
    var guideDiv = document.getElementById(guideDivID);
    var newDiv = document.createElement('div');
    newDiv.style = "left: 100%";
    document.getElementById("content").insertBefore(newDiv, null);

    var html = $.ajax({
      url: url,
        async: false
     }).responseText;

    newDiv.innerHTML = html;
    newDiv.className = "guideGrid";

    // Need to assign the id to the new div
    newDiv.id = guideDivID;
    guideDiv.id = "old" + guideDivID;
    switch (transition)
    {
        case 'left':
            leftSlideTransition(guideDiv.id, newDiv.id);
            break;
        case 'right':
            rightSlideTransition(guideDiv.id, newDiv.id);
            break;
        case 'dissolve':
            dissolveTransition(guideDiv.id, newDiv.id);
            break;
    }
    newDiv.id = "guideGrid";

    $("#busyPopup").hide();
}

function reloadGuideContent()
{
    var url = currentContentURL;
    // HACK: This is a hacky approach and will be changed soon
    if (currentContentURL.indexOf("GuideOnly") === -1)
    {
        if (currentContentURL.indexOf("?") !== -1)
            url += "&amp;GuideOnly=1";
        else
            url += "?GuideOnly=1";
    }
    loadGuideContent(url, "dissolve");  // currentContentURL is defined in util.qjs
}

