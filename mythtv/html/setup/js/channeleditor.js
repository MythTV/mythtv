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
   	    {name:'ChanId', width:120, sorttype:"int", hidden:true, jsonmap: 'ChannelInfos.ChanId'},
   	    {name:'ChanNum', width:120, sorttype:"int", jsonmap: 'ChanNum'},
   	    {name:'CallSign', width:90, sorttype:"text", jsonmap: 'CallSign'},
            {name:'ChannelName', width:300, align:"right", sorttype:"text", jsonmap: 'ChannelName'},
            {name:'Visible', width:40, align:"center", sorttype:"bool", jsonmap: 'Visible'},
            {name:'XMLTVID', width:0, align:"right", sortable:false, hidden:true},
   	    {name:'IconURL', width:0, align:"right", sortable:false, hidden:true},
            {name:'MplexId', width:0, align:"right", sortable:false, hidden:true},
            {name:'TransportId', width:0, align:"right", sortable:false, hidden:true},
            {name:'ServiceId', width:0, align:"right", sortable:false, hidden:true},
            {name:'NetworkId', width:0, align:"right", sortable:false, hidden:true},
            {name:'ATSCMajorChan', width:0, align:"right", sortable:false, hidden:true},
            {name:'ATSCMinorChan', width:0, align:"right", sortable:false, hidden:true},
            {name:'Format', width:0, align:"right", sortable:false, hidden:true},
            {name:'Modulation', width:0, align:"right", sortable:false, hidden:true},
            {name:'Frequency', width:0, align:"right", sortable:false, hidden:true},
            {name:'FrequencyId', width:0, align:"right", sortable:false, hidden:true},
            {name:'FrequencyTable', width:0, align:"right", sortable:false, hidden:true},
            {name:'FineTune', width:0, align:"right", sortable:false, hidden:true},
            {name:'SIStandard', width:0, align:"right", sortable:false, hidden:true},
            {name:'ChanFilters', width:0, align:"right", sortable:false, hidden:true},
            {name:'SourceId', width:0, align:"right", sortable:false, hidden:true},
            {name:'InputId', width:0, align:"right", sortable:false, hidden:true},
            {name:'CommFree', width:0, align:"right", sortable:false, hidden:true},
            {name:'UseEIT', width:0, align:"right", sortable:false, hidden:true},
            {name:'DefaultAuth', width:0, align:"right", sortable:false, hidden:true}
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
        sortname: 'channum',
        viewrecords: true,
        sortorder: "desc",
        loadonce: true,
        autoWidth: true,
        width: 820,
        height: 442
    });

    $(window).bind('resize', function() {
        $("#channels").setGridWidth($(window).width() - 230);
    }).trigger('resize');
}

var sourceid = 3;

initChannelEditor(sourceid);
