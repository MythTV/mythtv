
function recordProgram(chanID, startTime, type)
{
    hideMenu("optMenu");
    var url = "/tv/ajax_backends/dvr_util.qsp?action=simpleRecord&chanID=" + chanID + "&startTime=" + startTime + "&type=" + type;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recRuleChanged( response[0], response[1] );
                            });
}

// Override the one in common.js for now
function checkRecordingStatus(chanID, startTime)
{
    var url = "/tv/ajax_backends/dvr_util.qsp?action=checkRecStatus&chanID=" + chanID + "&startTime=" + startTime;
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

function reloadGuideContent()
{
    loadTVContent(currentContentURL, "guideGrid", "dissolve", {"GuideOnly": "1"});  // currentContentURL is defined in util.qjs
}

