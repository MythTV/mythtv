
$(document).mouseup(function (e)
{
    var container = $("#videoOverlay");
    var doneButton = $("#doneButton");

    //if we click the overlay(dark part) or the doneButton
    if ((container.is(e.target) && container.has(e.target).length === 0) || doneButton.is(e.target))
    {
        $("#videoDetailBox").html("");  //reset the detail box
        container.hide();
    }
});

function showVideoDetail(videoId)
{
    $("#videoOverlay").show();

    $.get("video/gallery_detail.qsp?Id="+videoId)
        .done(function(data) {
                 $("#videoDetailBox").html(data);
              });
}



