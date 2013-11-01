
function recordingDeleted(chanID, startTime)
{
    var id = chanID + "_" + startTime + "_row";
    var layer = document.getElementById(id)
    layer.parentNode.removeChild(layer);
}

function deleteRecording(chanID, startTime, allowRerecord)
{
    hideMenu("recMenu");
    var reRecord = allowRerecord ? 1 : 0;
    var url = "/tv/qjs/dvr_util.qjs?action=deleteRecording&chanID=" + chanID + "&startTime=" + startTime + "&allowRerecord=" + reRecord;
    var ajaxRequest = $.ajax( url )
                            .done(function()
                            {
                                var response = ajaxRequest.responseText.split("#");
                                recordingDeleted( response[0], response[1] );
                            });
}
