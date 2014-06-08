function initChannelEditor() {
    var sourceid = $("#sourceList").val();

    $("#channels").jqGrid({
        url:'/Channel/GetChannelInfoList?Details=1&SourceID=' + sourceid,
        datatype: 'json',
        colNames:['Channel ID', 'Channel Number', 'Callsign', 'Channel Name', 'XMLTVID', 'Visible', 'Icon Path',
                  'Multiplex ID', 'Transport ID', 'Service ID', 'Network ID', 'ATSC Major Channel',
                  'ATSC Minor Channel', 'Format', 'Modulation', 'Frequency', 'Frequency ID', 'Frequency Table',
                  'Fine Tuning', 'SI Standard', 'Channel Filters', 'Source ID', 'Input ID', 'Commercial Free',
                  'Use On Air Guide', 'Default Authority'],
        colModel:[
            {name:'ChanId', editable: true, width:50, sorttype:"int", hidden:true, jsonmap: 'ChanId'},
            {name:'ChanNum', search: true, editable: true, width:70, sorttype:"int", jsonmap: 'ChanNum'},
            {name:'CallSign', search: true, editable: true, width:90, sorttype:"text", jsonmap: 'CallSign'},
            {name:'ChannelName', search: true, editable: true, width:90, sorttype:"text", jsonmap: 'ChannelName'},
            {name:'XMLTVID', search: true, editable: true, width:120,  sortable:false, jsonmap: 'XMLTVID'},
            {name:'Visible', search: false, editable: true, width:40, align:"center", sorttype:"bool", jsonmap: 'Visible', formatter:'checkbox',edittype:"checkbox"},
            {name:'IconURL', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'IconURL'},
            {name:'MplexId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'MplexId'},
            {name:'TransportId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'TransportId'},
            {name:'ServiceId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ServiceId'},
            {name:'NetworkId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'NetworkId'},
            {name:'ATSCMajorChan', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ATSCMajorChan'},
            {name:'ATSCMinorChan', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ATSCMinorChan'},
            {name:'Format', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Format'},
            {name:'Modulation', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Modulation'},
            {name:'Frequency', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Frequency'},
            {name:'FrequencyId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FrequencyId'},
            {name:'FrequencyTable', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FrequencyTable'},
            {name:'FineTune', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FineTune'},
            {name:'SIStandard', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'SIStandard'},
            {name:'ChanFilters', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ChanFilters'},
            {name:'SourceId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'SourceId'},
            {name:'InputId', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'InputId'},
            {name:'CommFree', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'CommFree'},
            {name:'UseEIT', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'UseEIT'},
            {name:'DefaultAuth', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'DefaultAuth'}
        ],
        jsonReader: { 
           root:"ChannelInfoList.ChannelInfos",
           page:"ChannelInfoList.CurrentPage",
           total:"ChannelInfoList.TotalPages", 
           records:"ChannelInfoList.TotalAvailable",
           cell:"ChannelInfos",
           id : "ChanId",
           repeatitems: false
        },
        rowNum:20,
        multiselect: true,
        rowList:[10,20,30,50],
        pager: '#pager',
        defaultSearch: 'cn',
        ignoreCase: true,
        sortname: 'ChanNum',
        viewrecords: true,
        sortorder: "desc",
        loadonce: true,
        autoWidth: true,
        width: 820,
        height: 442
    });

    /* Add a search toolbar */

    jQuery("#channels").jqGrid('filterToolbar',{ searchOnEnter: false, defaultSearch: "cn" });

    /* Resize the grid if the window width changes */

    $(window).bind('resize', function() {
        $("#channels").setGridWidth($(window).width() - 223);
    }).trigger('resize');

    /* Sort ascending by Channel Number on load */

    $("#channels").setGridParam({sortname:'ChanNum', sortorder:'asc'}).trigger('reloadGrid');

    /* Initialize the Popup Menu */

    $("#channels").contextMenu('channelmenu', {
            bindings: {
                'editopt': function(t) {
                    editChannel();
                },
                'del': function(t) {
                    promptToDeleteChannel();
                }
            },
            onContextMenu : function(event, menu) {
                var rowId = $(event.target).parent("tr").attr("id")
                var grid = $("#channels");
                if (rowId != $('#channels').getGridParam('selrow'))
                    grid.setSelection(rowId);
                return true;
            }
    });
}

function reloadChannelGrid() {
    var sourceid = $("#sourceList").val();

    $('#channels').setGridParam({
        url:'/Channel/GetChannelInfoList?Details=1&SourceID=' + sourceid,
        datatype:'json',
        page:1}); 
    $('#channels').trigger("reloadGrid");
}

function initSourceList() {
    $("#sourceSelect").html("Guide Data Source: <select id='sourceList' "
        + "onChange='javascript:reloadChannelGrid()'>"
        + getGuideSourceList() + "</select>");
}

function editChannel() {
    var rowNum = $('#channels').getGridParam('selrow');
    var rowArr = $('#channels').getGridParam('selarrrow');
    if (rowNum != null) {
        if (rowArr.length == 1) {
            editSelectedChannel();
        }
        else {
            editMultiChannel();
        }
    }
}

function editSelectedChannel() {
    loadEditWindow("/setup/channeleditor-channeldetail.html");
    var row = $('#channels').getGridParam('selrow');
    var rowdata = $("#channels").jqGrid('getRowData', row);

    /* Basic */

    $("#channelDetailSettingChanNum").val(rowdata.ChanNum);
    $("#channelDetailSettingChannelName").val(rowdata.ChannelName);
    $("#channelDetailSettingCallSign").val(rowdata.CallSign);
    initXMLTVIdList();
    $("#channelDetailSettingXMLTVID").val(rowdata.XMLTVID);
    $("#channelDetailSettingIconURL").val(rowdata.IconURL);
    var preview = $("#channelDetailSettingIconPreview");
    if (rowdata.IconURL.length > 0) {
        preview.attr('src', rowdata.IconURL);
    }
    else
        preview.attr('src', '/images/blank.gif');

    if (rowdata.Visible == "Yes") {
        $("#channelDetailSettingVisible").attr('checked', true);
    }
    if (rowdata.UseEIT == "Yes") 
        $("#channelDetailSettingUseEIT").attr('checked', true);

    /* Advanced */

    var sourceid = rowdata.SourceId;

    $("#channelDetailSettingFrequencyId").val(rowdata.FrequencyId);
    $("#channelDetailSettingMplexId").html(initVideoMultiplexSelect(sourceid));
    $("#mplexList").val(rowdata.MplexId);
    $("#channelDetailSettingServiceId").val(rowdata.ServiceId);
    $("#channelDetailSettingDefaultAuthority").val(rowdata.DefaultAuth);
    $("#channelDetailSettingATSCMajorChannel").val(rowdata.ATSCMajorChan);
    $("#channelDetailSettingATSCMinorChannel").val(rowdata.ATSCMinorChan);

    /* Expert */

    $("#channelDetailSettingChanId").html(rowdata.ChanId);
    $("#channelDetailSettingSourceId").html(sourceid);
    $("#channelDetailSettingModulation").html(rowdata.Modulation)
    $("#channelDetailSettingFrequency").html(rowdata.Frequency)
    $("#channelDetailSettingSIStandard").html(rowdata.SIStandard)
    $("#channelDetailSettingFormat").html(rowdata.Format);

    $("#edit").dialog({
        modal: true,
        width: 800,
        height: 620,
        'title': 'Edit Channel',
        closeOnEscape: false,
        buttons: {
           'Save': function() { saveChannelEdits(); },
           'Cancel': function() { $(this).dialog('close'); }
    }});

    $("#channelsettings").accordion();

    $("#edit").dialog("open");
}

function saveChannelEdits() {
    var mplexid = $("#mplexList").val();
    var sourceid = $("#channelDetailSettingSourceId").html();
    var chanid = $("#channelDetailSettingChanId").html()

    var callsign = $.trim($("#channelDetailSettingCallSign").val());
    var channelname = $.trim($("#channelDetailSettingChannelName").val());
    var channum =  $.trim($("#channelDetailSettingChanNum").val());
    var serviceid =  $("#channelDetailSettingServiceId").val();
    var atscmajorchannel =  $("#channelDetailSettingATSCMajorChannel").val();
    var atscminorchannel =  $("#channelDetailSettingATSCMinorChannel").val();

    var useeit = false;

    if ($("#channelDetailSettingUseEIT").attr("checked")) {
        useeit = true;
    }

    var visible = false;

    if ($("#channelDetailSettingVisible").attr("checked")) {
        visible = true;
    }

    var frequencyid = $("#channelDetailSettingFrequencyId").val();
    var icon = $("#channelDetailSettingIconURL").val();
    var format = $("#channelDetailSettingFormat").val();
    var xmltvid = $("#channelDetailSettingXMLTVID").val();
    var defaultauth = $("#channelDetailSettingDefaultAuthority").val();

    if ($("#channels").jqGrid('setRowData', chanid,
        { MplexId: mplexid, SourceId: sourceid, ChanId: chanid,
        CallSign: callsign, ChannelName: channelname, ChanNum: channum,
        ServiceId: serviceid, ATSCMajorChan: atscmajorchannel,
        ATSCMinorChan: atscminorchannel, UseEIT: useeit, Visible: visible,
        FrequencyId: frequencyid, IconURL: icon, Format: format, XMLTVID: xmltvid,
        DefaultAuth: defaultauth }))
        {
            $.post("/Channel/UpdateDBChannel",
                { MplexID: mplexid, SourceID: sourceid, ChannelID: chanid,
                  CallSign: callsign, ChannelName: channelname, ChannelNumber: channum,
                  ServiceID: serviceid, ATSCMajorChannel: atscmajorchannel,
                  ATSCMinorChannel: atscminorchannel, UseEIT: useeit, visible: visible,
                  FrequencyID: frequencyid, Icon: icon, Format: format, XMLTVID: xmltvid,
                  DefaultAuthority: defaultauth},
                  function(data) {
                      if (data.bool == "true") {
                          setStatusMessage("Channel updated successfully!");
                          $("#edit").dialog('close');
                      }
                      else
                          setErrorMessage("Channel update failed!");
                  }, "json");

            setStatusMessage("Updating channel...");
        }
}

function editMultiChannel() {
    loadEditWindow("/setup/channeleditor-channeldetail-multi.html");
    var rows = $('#channels').getGridParam('selarrrow');

    var rowinfo = getMultiRowDescription();

    $("#channeldetailtable").html(rowinfo);

    $("#edit").dialog({
        modal: true,
        width: 800,
        height: 620,
        'title': 'Edit Multiple Channels',
        closeOnEscape: false,
        buttons: {
           'Save': function() { showConfirm('Editing multiple channels at once should be done with great care.  Do you want to continue?  This cannot be undone.', saveMultiChannelEdits); },
           'Cancel': function() { $(this).dialog('close'); }
    }});

    $("#multichannelsettings").accordion();
}

function getMultiRowDescription() {
    var result = '';
    var rowArr = $('#channels').getGridParam('selarrrow');

    $.each(rowArr, function(i, value) {
        var rowdata = $("#channels").jqGrid('getRowData', value);
        result = result + "Affected ChanId:" + rowdata.ChanId + "(" + rowdata.ChannelName + ")" + "<br>";
    });

    return result;
}

function saveMultiChannelEdits() {
    var action = $("#multiChannelOptions").val();
    var rowArr = $('#channels').getGridParam('selarrrow');
    $.each(rowArr, function(i, value) {
        var rowdata = $("#channels").jqGrid('getRowData', value);
        var orig = '';
        var news = '';
        if (action == "ChanNum") {
            orig = rowdata.ChanNum;
            rowdata.ChanNum = multiChannelEditTextReplace(rowdata);
            news = rowdata.ChanNum;
        }
        else if (action == "ChanName") {
            orig = rowdata.ChannelName;
            rowdata.ChannelName = multiChannelEditTextReplace(rowdata);
            news = rowdata.ChannelName;
        }
        else if (action == "CallSign") {
            orig = rowdata.CallSign;
            rowdata.CallSign = multiChannelEditTextReplace(rowdata);
            news = rowdata.CallSign;
        }

        if (updateChannelRow(value, rowdata)) {
        }
    });
    $("#edit").dialog('close');
}

function multiChannelEditTextReplace(rowdata) {
    var template = $("#multiChannelRenameTemplate").val();

    template = template.replace("%ChanId%", rowdata.ChanId);
    template = template.replace("%ChanNum%", rowdata.ChanNum);
    template = template.replace("%ChanName%", rowdata.ChannelName);
    template = template.replace("%CallSign%", rowdata.CallSign);
    template = template.replace("%MplexId%", rowdata.MplexId);
    template = template.replace("%ServiceId%", rowdata.ServiceId);
    template = template.replace("%ATSCMajorChan%", rowdata.ATSCMajorChan);
    template = template.replace("%ATSCMinorChan%", rowdata.ATSCMinorChan);
    template = template.replace("%SourceId%", rowdata.SourceId);
    template = template.replace("%XMLTVID%", rowdata.XMLTVID);

    return template;
}

function updateChannelRow(rownum, rowdata) {
    if ($("#channels").jqGrid('setRowData', rownum, rowdata)) {
        if (updateDBChannelRow(rowdata))
            return true;
        else
            return false;
    }
    else {
        return false;
    }
}

function updateDBChannelRow(rowdata) {
    result = false;
    var visible = false;
    var useeit = false;

    if (rowdata.Visible == "Yes")
        visible = true;
    if (rowdata.UseEIT == "Yes")
        useeit = true;

    $.ajaxSetup({ async: false });
    $.post("/Channel/UpdateDBChannel",
        { MplexID: rowdata.MplexId, SourceID: rowdata.SourceId, ChannelID: rowdata.ChanId,
          CallSign: rowdata.CallSign, ChannelName: rowdata.ChannelName, ChannelNumber: rowdata.ChanNum,
          ServiceID: rowdata.ServiceId, ATSCMajorChannel: rowdata.ATSCMajorChan,
          ATSCMinorChannel: rowdata.ATSCMinorChan, UseEIT: useeit , visible: visible,
          FrequencyID: rowdata.FrequencyId, Icon: rowdata.IconURL, Format: rowdata.Format, XMLTVID: rowdata.XMLTVID,
          DefaultAuthority: rowdata.DefaultAuth},
          function(data) {
              if (data.bool == "true") {
                  $('#channels').trigger('reloadGrid');
                  result = true;
              }
              else
                  setErrorMessage("Channels update failed!");
         }, "json");
    $.ajaxSetup({ async: true });

    return result;
}

function promptToDeleteChannel() {
    var message = "Are you sure you want to delete these channels?  This cannot be undone.";
    var rowNum = $('#channels').getGridParam('selrow');
    var rowArr = $('#channels').getGridParam('selarrrow');
    if (rowNum != null) {
        if (rowArr.length == 1) {
            message = "Are you sure you want to delete this channel?  This cannot be undone.";
        }
        showConfirm(message, deleteSelectedChannel);
    }
}

function deleteSelectedChannel() {
    var rowArray = $('#channels').jqGrid('getGridParam','selarrrow');
    if (rowArray.length > 0) {
        var len = rowArray.length;
        for (var i = len - 1; i >= 0; i--) {
            var chanid = rowArray[i];
            if ($("#channels").jqGrid('delRowData', chanid)) {
                $.post("/Channel/RemoveDBChannel",
                { ChannelID: chanid },
                function(data) {
                    if (data.bool == "true") {
                        setStatusMessage("Channel deleted successfully!");
                      }
                      else
                          setErrorMessage("Channel delete failed!");
                  }, "json");
            }
        }
        $('#channels').trigger('reloadGrid');
    }
}

function getGuideSourceList() {
    var result = '';

    $.ajaxSetup({ async: false });
    $.getJSON("/Channel/GetVideoSourceList", function(data) {
        $.each(data.VideoSourceList.VideoSources, function(i, value) {
            var sourcename = ''
            if (value.SourceName == '')
                sourcename = "Source ID " + value.Id;
            else
                sourcename = value.SourceName;
            result += "<option value='" + value.Id + "'>" + sourcename + "</option>";
        });
    });
    $.ajaxSetup({ async: true });

    return result;
}

function getVideoMultiplexList(sourceid) {
    var result = '';

    $.ajaxSetup({ async: false });
    $.post("/Channel/GetVideoMultiplexList",
        { SourceID : sourceid },
        function(data) {
        $.each(data.VideoMultiplexList.VideoMultiplexes, function(i, value) {
            var label = '';
            if (value.TransportId > 0)
                label = value.MplexId + " " + "(Transport: " + value.TransportId + ")";
            else
                label = value.MplexId + " " + "(Frequency: " + value.Frequency + ")";
            result += "<option value='" + value.MplexId + "'>" + label + "</option>";
        });
    }, "json");
    $.ajaxSetup({ async: true });

    return result;
}

function initVideoMultiplexSelect(sourceid) {
    var result = "<select id='mplexList'> "
        + getVideoMultiplexList(sourceid) + "</select>";

    return result;
}


function initXMLTVIdList() {
    var sourceid = $("#sourceList").val();
    var ids = new Array();
    var x = 0;
    $.ajaxSetup({ async: false });
    $.post("/Channel/GetXMLTVIdList", { SourceID : sourceid }, function(data) {
       $.each(data.StringList, function(i, value) {
            ids[x] = value;
            x++;
        });
    }, "json");

    $.ajaxSetup({ async: true });

    $( "#channelDetailSettingXMLTVID" ).autocomplete({ source: ids });
}


function choseNewChanIcon(url) {
    $("#channelDetailSettingIconURL").val(url);
    $("#channelDetailSettingIconPreview").attr('src',
        '/Content/GetFile?StorageGroup=ChannelIcons&FileName=' +
        basename(url));
}

function browseForNewChanIcon() {
    openStorageGroupBrowser("Channel Icon", choseNewChanIcon,
                            "ChannelIcons");
}

function downloadChannelIcon(url) {
    $.post("/Content/DownloadFile",
        { URL: url, StorageGroup: "ChannelIcons" },
        function(data) {
            $("#channelDetailSettingIconPreview").attr('src',
                '/Content/GetFile?StorageGroup=ChannelIcons&FileName=' +
                basename(url));
            setStatusMessage("Channel Icon download succeeded!");
        }, "json").error(function() {
            setErrorMessage("Channel Icon download failed!");
        });
}

function selectChannelIcon(url) {
    $("#channelDetailSettingIconPreview").attr('src', '/images/blank.gif');
    downloadChannelIcon(url);
    $("#channelDetailSettingIconURL").val(basename(url));
    $("#iconBrowser").dialog("close");
}

var iconCount = 0;
var iconRow = "";
function channelIconLink(url) {
    return "<td class='iconGridCell'><a href='#' " +
        "onClick='javascript:selectChannelIcon(\"" + url + "\")'><img src='" +
        url + "' id='channelIcon_" + iconCount + "' class='iconGridIcon'" +
        " onLoad='resizeChannelIcon(\"channelIcon_" + iconCount + "\")'></a>" +
        "<br><div id='channelIcon_" + iconCount + "_size'></div></td>";
}

function addChannelIconToGrid(url) {
    var result = "";
    if (url.substring(0,5).toLowerCase() == "error")
        return;

    iconRow = "row_" + parseInt(iconCount / 3);

    if (iconCount == 0) {
        $("#iconBrowser").html(
            "<table id='iconGallery' border=0 cellpadding=0 cellspacing=1>" +
            "<tr><td class='xinvisible'>" +
                     "<img src='/images/blank.gif' width=200 height=0></td>" +
                "<td class='xinvisible'>" +
                     "<img src='/images/blank.gif' width=200 height=0></td>" +
                "<td class='xinvisible'>" +
                     "<img src='/images/blank.gif' width=200 height=0></td></tr>" +
            "<tr id='" + iconRow + "'>" + channelIconLink(url) +
            "</tr></table>");
    } else if (((iconCount % 3) == 0)) {
        $("#iconGallery").append("<tr id='" + iconRow + "'>" +
            channelIconLink(url) + "</tr>");
    } else {
        $("#" + iconRow).append(channelIconLink(url));
    }

    iconCount++;
}

function resizeChannelIcon(id, maxWidth, maxHeight) {
    var preview = $("#channelDetailSettingIconPreview");
    if (preview.attr('src') != '/images/blank.gif') {
        preview.css('display', 'inline');
    }
    else {
        preview.css('display', 'none');
    }
    var ratio = 0;
    var icon = $("#" + id);
    var width = icon.width();
    var height = icon.height();

    if (maxWidth == undefined)
        maxWidth = 200;
    if (maxHeight == undefined)
        maxHeight = 200;

    $("#" + id + "_size").html("Size: " + width + "x" + height);

    if (width > maxWidth) {
        ratio = maxWidth / width;
        icon.attr("width", maxWidth);
        icon.attr("height", height * ratio);
        height = height * ratio;
        width = width * ratio;
    }

    if (height > maxHeight) {
        ratio = maxHeight / height;
        icon.attr("height", maxHeight);
        icon.attr("width", width * ratio);
        width = width * ratio;
    }
}

function searchChannelIcon() {
    var iconsSeen = [];
    iconCount = 0;

    var iconBrowser = $("#iconBrowser");
    iconBrowser.dialog({
        modal: true,
        width: 680,
        height: 340,
        'title': 'Channel Icon Download',
        closeOnEscape: false,
        autoOpen: false,
        buttons: {
           'Cancel': function() { $(this).dialog('close'); }
    }});

    iconBrowser.html("");
    $.getJSON("http://services.mythtv.org/channel-icon/lookup?jsoncallback=?",
        { callsign: $("#channelDetailSettingCallSign").val() },
        function(data) {
            $.each(data.urls, function(i, url) {
                addChannelIconToGrid(url);
            });
        });

    $.getJSON("http://services.mythtv.org/channel-icon/lookup?jsoncallback=?",
        { xmltvid: $("#channelDetailSettingXMLTVID").val() },
        function(data) {
            $.each(data.urls, function(i, url) {
                addChannelIconToGrid(url);
            });
        });

    iconBrowser.dialog("open");
}

initSourceList();
initChannelEditor();
