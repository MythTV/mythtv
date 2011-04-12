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
            {name:'ChanNum', editable: true, width:120, sorttype:"int", jsonmap: 'ChanNum'},
            {name:'CallSign', editable: true, width:90, sorttype:"text", jsonmap: 'CallSign'},
            {name:'ChannelName', editable: true, width:300, align:"right", sorttype:"text", jsonmap: 'ChannelName'},
            {name:'Visible', editable: true, width:40, align:"center", sorttype:"bool", jsonmap: 'Visible', formatter:'checkbox',edittype:"checkbox"},
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
        sortname: 'ChanNum',
        viewrecords: true,
        sortorder: "desc",
        loadonce: true,
        autoWidth: true,
        width: 820,
        height: 442
    });

    /* Resize the grid if the window width changes */

    $(window).bind('resize', function() {
        $("#channels").setGridWidth($(window).width() - 223);
    }).trigger('resize');

    /* Sort ascending by Channel Number on load */

    $("#channels").setGridParam({sortname:'ChanNum', sortorder:'asc'}).trigger('reloadGrid');
}

function reloadChannelGrid() {
    var sourceid = $("#sourceList").val();
    setStatusMessage("Loading Channel Source (" + $('#sourceList').val() + ")...");

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

    $("#channelDetailSettingFrequencyId").val(rowdata.FrequencyId);
    $("#channelDetailSettingMplexId").val(rowdata.MplexId);
    $("#channelDetailSettingServiceId").val(rowdata.ServiceId);
    $("#channelDetailSettingFormat").val(rowdata.Format);
    $("#channelDetailSettingDefaultAuthority").val(rowdata.DefaultAuth);
    $("#channelDetailSettingATSCMajorChannel").val(rowdata.ATSCMajorChan);
    $("#channelDetailSettingATSCMinorChannel").val(rowdata.ATSCMinorChan);

    /* Expert */

    $("#channelDetailSettingChanId").html("<b>" + rowdata.ChanId + "</b>");
    $("#channelDetailSettingSourceId").html("<b>" + rowdata.SourceId + "</b>");
    $("#channelDetailSettingModulation").html("<b>" + rowdata.Modulation + "</b>")
    $("#channelDetailSettingFrequency").html("<b>" + rowdata.Frequency + "</b>")
    $("#channelDetailSettingSIStandard").html("<b>" + rowdata.SIStandard + "</b>")

    $("#edit").dialog({
        modal: true,
        width: 800,
        height: 620,
        'title': 'Edit Channel',
        closeOnEscape: false,
        buttons: {
           'Save': function() {},
           'Cancel': function() { $(this).dialog('close'); }
    }});

    $("#channelsettings").accordion();

    $("#edit").dialog("open");
}

function promptToDeleteChannel() {
    var message = "Are you sure you want to delete these channels?  This cannot be undone.";
    var rowNum = $('#channels').getGridParam('selrow');
    if (rowNum != null) {
        if (rowNum.length == 1) {
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
            if ($("#channels").jqGrid('delRowData', rowArray[i])) {
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

initSourceList();
initChannelEditor();
