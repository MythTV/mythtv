function initGuideDataSources() {
    $("#sources").jqGrid({
        url:'/Channel/GetVideoSourceList',
        datatype: "json",
        colNames:['Source ID', 'Source Name', 'Grabber Name', 'Username', 'Password', 'Use EIT', 'Configure Path'],
        colModel:[
            {name:'Id', width:130, sorttype:"int", hidden:false, jsonmap:"Id"},
            {name:'SourceName', width:280, sorttype:"text", jsonmap:"SourceName"},
            {name:'Grabber', width:200, sorttype:"text", jsonmap:"Grabber"},
            {name:'UserId', width:200, align:"right", sorttype:"text", jsonmap:"UserId"},
            {name:'Password', width:40, align:"center", sortable:true, hidden:true, jsonmap:"Password"},
            {name:'UseEIT', width:80, align:"right", sorttype:"bool", jsonmap:"UseEIT"},
            {name:'ConfigPath', width:40, align:"center", sortable:true, hidden:true, jsonmap:"ConfigPath"}
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
}

function editSelectedSource() {
    var rowNum = $('#sources').getGridParam('selrow');
    if (rowNum != null) {
        $("#sources").jqGrid('editGridRow',rowNum,
                          { modal: true });
    }
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
