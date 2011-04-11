function initChannelEditor(sourceid) {
    $("#channels").jqGrid({
        url:'/Channel/GetChannelInfoList?SourceID=' + sourceid,
        datatype: 'json',
        colNames:['Channel ID', 'Channel Number', 'Callsign', 'Channel Name', 'Visible', 'XMLTVID', 'Icon Path',
                  'Multiplex ID', 'Transport ID', 'Service ID', 'Network ID', 'ATSC Major Channel',
                  'ATSC Minor Channel', 'Format', 'Modulation', 'Frequency', 'Frequency ID', 'Frequency Table',
                  'Fine Tuning', 'SI Standard', 'Channel Filters', 'Source ID', 'Input ID', 'Commercial Free',
                  'Use On Air Guide', 'Default Authority'],
        colModel:[
            {name:'ChanId', width:120, sorttype:"int", hidden:true, jsonmap: 'ChanId'},
            {name:'ChanNum', width:120, sorttype:"int", jsonmap: 'ChanNum'},
            {name:'CallSign', width:90, sorttype:"text", jsonmap: 'CallSign'},
            {name:'ChannelName', width:300, align:"right", sorttype:"text", jsonmap: 'ChannelName'},
            {name:'Visible', width:40, align:"center", sorttype:"bool", jsonmap: 'Visible'},
            {name:'XMLTVID', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'XMLTVID'},
            {name:'IconURL', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'IconURL'},
            {name:'MplexId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'MplexId'},
            {name:'TransportId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'TransportId'},
            {name:'ServiceId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ServiceID'},
            {name:'NetworkId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'NetworkId'},
            {name:'ATSCMajorChan', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ATSCMajorChan'},
            {name:'ATSCMinorChan', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ATSCMinorChan'},
            {name:'Format', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Format'},
            {name:'Modulation', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Modulation'},
            {name:'Frequency', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'Frequency'},
            {name:'FrequencyId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FrequencyId'},
            {name:'FrequencyTable', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FrequencyTable'},
            {name:'FineTune', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'FineTune'},
            {name:'SIStandard', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'SIStandard'},
            {name:'ChanFilters', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'ChanFilters'},
            {name:'SourceId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'SourceId'},
            {name:'InputId', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'InputId'},
            {name:'CommFree', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'CommFree'},
            {name:'UseEIT', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'UseEIT'},
            {name:'DefaultAuth', width:0, align:"right", sortable:false, hidden:true, jsonmap: 'DefaultAuth'}
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
        $("#channels").setGridWidth($(window).width() - 230);
    }).trigger('resize');

    $("#channels").setGridParam({sortname:'ChanNum', sortorder:'asc'}).trigger('reloadGrid');
}

var sourceid = 3;

initChannelEditor(sourceid);
