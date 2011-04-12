function initGuideDataSources() {
    $("#sources").jqGrid({
        url:'/Channel/GetVideoSourceList',
        datatype: "json",
        colNames:['Source ID', 'Source Name', 'Grabber Name', 'User Name', 'Frequency Table',
                  'Lineup ID', 'Password', 'Use EIT', 'Configure Path', 'NIT ID'],
        colModel:[
            {name:'Id', width:130, sorttype:"int", hidden:false, jsonmap:"Id"},
            {name:'SourceName', width:280, sorttype:"text", jsonmap:"SourceName"},
            {name:'Grabber', width:200, sorttype:"text", jsonmap:"Grabber"},
            {name:'UserId', width:200, align:"right", sorttype:"text", jsonmap:"UserId"},
            {name:'FreqTable', width:200, align:"right", sorttype:"text", hidden:true, jsonmap:"FreqTable"},
            {name:'LineupId', width:200, align:"right", sorttype:"text", hidden:true, jsonmap:"LineupId"},
            {name:'Password', width:40, align:"center", sortable:true, hidden:true, jsonmap:"Password"},
            {name:'UseEIT', width:80, align:"right", sorttype:"bool", jsonmap:"UseEIT"},
            {name:'ConfigPath', width:40, align:"center", sortable:true, hidden:true, jsonmap:"ConfigPath"},
            {name:'NITId', width:200, align:"right", sorttype:"text", hidden:true, jsonmap:"NITId"}
        ],
        jsonReader: { 
           root:"VideoSourceList.VideoSources", 
           cell:"VideoSources",
           repeatitems:false,
           id : "Id"
        },
        rowNum:20,
        pager: '#pager',
        sortname: 'id',
        viewrecords: true,
        sortorder: "desc",
        loadonce: true,
        autoWidth: true,
        width: 820,
        height: 442
    });

    /* Resize the grid if the window width changes */

    $(window).bind('resize', function() {
        $("#sources").setGridWidth($(window).width() - 223);
    }).trigger('resize');

    /* Sort ascending by Source ID on load */
        
    $("#sources").setGridParam({sortname:'Id', sortorder:'asc'}).trigger('reloadGrid');

    /* Initialize the Popup Menu */

    $("#sources").contextMenu('sourcemenu', {
            bindings: {
                'editopt': function(t) {
                    editSelectedSource();
                },
                'del': function(t) {
                    promptToDeleteSource();
                }
            },
            onContextMenu : function(event, menu) {
                var rowId = $(event.target).parent("tr").attr("id")
                var grid = $("#sources");
                if (rowId != $('#sources').getGridParam('selrow'))
                    grid.setSelection(rowId);
                return true;
            }
    });

}

function editSelectedSource() {
    loadEditWindow("/setup/guidedatasources-sourcedetail.html");
    var row = $('#sources').getGridParam('selrow');
    var rowdata = $("#sources").jqGrid('getRowData', row);

    /* Basic */

    $("#sourceDetailSettingSourceName").val(rowdata.SourceName);
    $("#sourceDetailSettingSDUserName").val(rowdata.UserId);
    $("#sourceDetailSettingSDPassword").val(rowdata.Password);

    if (rowdata.UseEIT == "Yes")
        $("#sourceDetailSettingUseEIT").attr('checked', true);

    /* Advanced */

    $("#sourceDetailSettingFrequencyTable").val(rowdata.FreqTable);
    $("#sourceDetailSettingNITId").val(rowdata.NITId);
    $("#sourceDetailSettingConfigPath").val(rowdata.ConfigPath);

    /* Expert */

    $("#sourceDetailSettingSourceId").html(rowdata.Id);

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

    $("#sourcesettings").accordion();

    $("#edit").dialog("open");
}
            
function promptToDeleteSource() {
    var rowNum = $('#sources').getGridParam('selrow');
    if (rowNum != null) {
        showConfirm("Are you sure you want to delete this guide source?  This cannot be undone.", deleteSelectedSource);
    }
}

function deleteSelectedSource() {
    var rowNum = $('#sources').getGridParam('selrow');
    if (rowNum != null) {
        if ($("#sources").jqGrid('delRowData', rowNum)) {
            $('#sources').trigger('reloadGrid');
            setStatusMessage("Deleting source " + rowNum + "...");
        }
    }
}

initGuideDataSources();
