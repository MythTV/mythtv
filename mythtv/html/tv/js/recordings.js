
function recordingDeleted(chanID, startTime)
{
    $("#wastebin").show();
    var id = chanID + "_" + startTime + "_row";
    var layer = document.getElementById(id)
    layer.parentNode.removeChild(layer);
}

function recordingUnDeleted(chanID, startTime)
{
    var id = chanID + "_" + startTime + "_row";
    var layer = document.getElementById(id)
    layer.parentNode.removeChild(layer);
}

function deleteRecording(chanID, startTime, allowRerecord, forceDelete)
{
    hideMenu("optMenu");
    var reRecord = allowRerecord ? 1 : 0;
    var force = forceDelete ? 1 : 0;
    var url = "/tv/qjs/dvr_util.qjs?action=deleteRecording&chanID=" + chanID + "&startTime=" + startTime +
              "&allowRerecord=" + reRecord + "&forceDelete=" + force;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recordingDeleted( response[0], response[1] );
                            });
}

function unDeleteRecording(chanID, startTime)
{
    hideMenu("optMenu");
    var url = "/tv/qjs/dvr_util.qjs?action=unDeleteRecording&chanID=" + chanID + "&startTime=" + startTime;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recordingUnDeleted( response[0], response[1] );
                            });
}