function initViewLogs() {
    var hostname = $("#logHostNameList").val();
    var application = $("#logApplicationList").val();

    $("#logs").jqGrid({
        url:'/Myth/GetLogs?HostName='+hostname+'&Application=' + application,
        datatype: 'json',
        colNames:['Host Name', 'Application', 'PID', 'TID', 'Thread',
                  'Filename', 'Line', 'Function', 'Time', 'Level', 'Message'],
        colModel:[
            {name:'Host Name', hidden:true, search:false, editable:false, width:20, sortable:false, sorttype:'text', jsonmap:'HostName'},
            {name:'Application', hidden:true, search:false, editable:false, width:20, sortable:false, sorttype:'text', jsonmap:'Application'},
            {name:'PID', hidden:true, search:false, editable:false, width:10, sortable:false, sorttype:'int', jsonmap:'PID'},
            {name:'TID', hidden:true, search:false, editable:false, width:10, sortable:false, sorttype:'int', jsonmap:'TID'},
            {name:'Thread', hidden:true, search:false, editable:false, width:20, sortable:false, sorttype:'text', jsonmap:'Thread'},
            {name:'Filename', hidden:true, search:false, editable:false, width:20, sortable:false, sorttype:'text', jsonmap:'Filename'},
            {name:'Line', hidden:true, search:false, editable:false, width:10, sortable:false, sorttype:'int', jsonmap:'Line'},
            {name:'Function', hidden:true, search:false, editable:false, width:20, sortable:false, sorttype:'text', jsonmap:'Function'},
            {name:'Time', hidden:false, search:false, editable:false, width:30, sortable:false, sorttype:'date', datefmt:'Y-m-d\TH:i:s', formatter:'date', formatoptions:{srcformat:'Y-m-d\TH:i:s', newformat:'Y-m-d H:i:s'}, jsonmap:'Time'},
            {name:'Level', hidden:true, search:false, editable:false, width:15, sortable:false, sorttype:'text', jsonmap:'Level'},
            {name:'Message', hidden:false, search:true, editable:false, width:100, sortable:false, sorttype:'text', jsonmap:'Message'},
        ],
        jsonReader: {
           root:"LogMessageList.LogMessages",
           page:"ChannelInfoList.CurrentPage",
           total:"ChannelInfoList.TotalPages",
           records:"ChannelInfoList.TotalAvailable",
           cell:"ChannelInfos",
           id : "ChanId",
           repeatitems: false
        },
        rowNum:20,
        multiselect: true,
        rowList:[20,50,100,250,500],
        pager: '#pager',
        defaultSearch: 'cn',
        ignoreCase: true,
        viewrecords: true,
        loadonce: true,
        autoWidth: true,
        width: $(window).width() - 223,
        height: '100%'
    });

    /* Add a search toolbar */

    jQuery("#logs").jqGrid('filterToolbar',{ searchOnEnter: false, defaultSearch: 'cn' });

    /* Resize the grid if the window width changes */

    $(window).bind('resize', function() {
        $("#logs").setGridWidth($(window).width() - 223);
    }).trigger('resize');

    /* Initialize the Popup Menu */

    $("#logs").contextMenu('logsmenu', {
            bindings: {
                'selectCols': function(t) {
                    selectColumns();
                },
            },
            onContextMenu : function(event, menu) {
                var rowId = $(event.target).parent("tr").attr("id")
                var grid = $("#logs");
                if (rowId != $('#logs').getGridParam('selrow'))
                    grid.setSelection(rowId);
                return true;
            }
    });
}

function reloadLogsGrid() {
    var hostname = $("#logHostNameList").val();
    var application = $("#logApplicationList").val();

    $('#logs').setGridParam({
        url:'/Myth/GetLogs?HostName='+hostname+'&Application=' + application,
        datatype:'json',
        page:1});
    $('#logs').trigger("reloadGrid");
}

function initHostNameList() {
    $("#logHostNameSelect").html("Host Name: <select id='logHostNameList' "
        + "onChange='javascript:reloadLogsGrid()'>"
        + getHostNameList() + "</select>");
}

function initApplicationList() {
    $("#logApplicationSelect").html("Application: <select id='logApplicationList' "
        + "onChange='javascript:reloadLogsGrid()'>"
        + getApplicationList() + "</select>");
}

function getHostNameList() {
    var result = '';

    $.ajaxSetup({ async: false });
    $.getJSON("/Myth/GetLogs", function(data) {
        $.each(data.LogMessageList.HostNames, function(i, value) {
            var hostname = ''
            if (value.Label == '')
                hostname = value.Value;
            else
                hostname = value.Label;
            result += "<option value='" + value.Value + "'>" + hostname + "</option>";
        });
    });
    $.ajaxSetup({ async: true });

    return result;
}

function getApplicationList() {
    var result = '';

    $.ajaxSetup({ async: false });
    $.getJSON("/Myth/GetLogs", function(data) {
        $.each(data.LogMessageList.Applications, function(i, value) {
            var application = ''
            if (value.Label == '')
                application = value.Value;
            else
                application = value.Label;
            result += "<option value='" + value.Value + "'>" + application + "</option>";
        });
    });
    $.ajaxSetup({ async: true });

    return result;
}


function selectColumns() {
    jQuery("#logs").jqGrid('columnChooser',{ });
}

initHostNameList();
initApplicationList();
initViewLogs();
