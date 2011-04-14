function initChannelEditor() {
    var sourceid = $("#sourceList").val();

    $("#channels").jqGrid({
        url:'/Channel/GetChannelInfoList?SourceID=' + sourceid,
        datatype: 'json',
        colNames:['Channel ID', 'Channel Number', 'Callsign', 'Channel Name', 'Visible', 'XMLTVID', 'Icon Path',
                  'Multiplex ID', 'Transport ID', 'Service ID', 'Network ID', 'ATSC Major Channel',
                  'ATSC Minor Channel', 'Format', 'Modulation', 'Frequency', 'Frequency ID', 'Frequency Table',
                  'Fine Tuning', 'SI Standard', 'Channel Filters', 'Source ID', 'Input ID', 'Commercial Free',
                  'Use On Air Guide', 'Default Authority'],
        colModel:[
            {name:'ChanId', editable: true, width:120, sorttype:"int", hidden:true, jsonmap: 'ChanId'},
            {name:'ChanNum', search: true, editable: true, width:120, sorttype:"int", jsonmap: 'ChanNum'},
            {name:'CallSign', search: true, editable: true, width:90, sorttype:"text", jsonmap: 'CallSign'},
            {name:'ChannelName', search: true, editable: true, width:300, align:"right", sorttype:"text", jsonmap: 'ChannelName'},
            {name:'Visible', search: false, editable: true, width:40, align:"center", sorttype:"bool", jsonmap: 'Visible', formatter:'checkbox',edittype:"checkbox"},
            {name:'XMLTVID', editable: true, width:0, align:"right", sortable:false, hidden:true, jsonmap: 'XMLTVID'},
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
        url:'/Channel/GetChannelInfoList?SourceID=' + sourceid,
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
    $("#channelDetailSettingXMLTVID").val(rowdata.XMLTVID);
    $("#channelDetailSettingIconURL").val(rowdata.IconURL);
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

    var callsign = $("#channelDetailSettingCallSign").val();
    var channelname = $("#channelDetailSettingChannelName").val();
    var channum =  $("#channelDetailSettingChanNum").val();
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
                          $('#channels').trigger('reloadGrid');
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
           'Save': function() { saveMultiChannelEdits(); },
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
    $("#edit").dialog('close');
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
        for (var i=0; i < len; i++) {
            var chanid = rowArray[i];
            if ($("#channels").jqGrid('delRowData', chanid)) {
                $.post("/Channel/DeleteDBChannel",
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

initSourceList();
initChannelEditor();
