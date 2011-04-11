function initChannelEditor(sourceid) {
    $("#channels").jqGrid({
        url:'/Channel/GetChannelInfoList?SourceID=' + sourceid,
        datatype: "xml",
/*        postData:{ SourceID: sourceid , StartIndex: ($('select.foo option:selected').val() * $('#channels').jqGrid('getGridParam','page')) , Count: $('#channels').jqGrid('getGridParam','records') },*/
        colNames:['Channel ID', 'Channel Number', 'Callsign', 'Channel Name', 'Visible', 'XMLTVID', 'Icon Path',
                  'Multiplex ID', 'Transport ID', 'Service ID', 'Network ID', 'ATSC Major Channel',
                  'ATSC Minor Channel', 'Format', 'Modulation', 'Frequency', 'Frequency ID', 'Frequency Table',
                  'Fine Tuning', 'SI Standard', 'Channel Filters', 'Source ID', 'Input ID', 'Commercial Free',
                  'Use On Air Guide', 'Default Authority'],
        colModel:[
   	    {name:'chanid', index:'chanid', width:120, sorttype:"int", hidden:true, xmlmap:"ChanId"},
   	    {name:'channum', index:'channum', width:120, sorttype:"int", xmlmap:"ChanNum"},
   	    {name:'callsign', index:'callsign', width:90, sorttype:"text", xmlmap:"CallSign"},
            {name:'channame', index:'channame', width:300, align:"right", sorttype:"text", xmlmap:"ChannelName"},
            {name:'icon', index:'icon', width:40, align:"center", sortable:true, hidden:false, xmlmap:"Visible"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"XMLTVID"},
   	    {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"IconURL"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"MplexId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"TransportId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"ServiceId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"NetworkId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"ATSCMajorChan"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"ATSCMinorChan"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"Format"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"Modulation"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"Frequency"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"FrequencyId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"FrequencyTable"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"FineTune"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"SIStandard"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"ChanFilters"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"SourceId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"InputId"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"CommFree"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"UseEIT"},
            {name:'icon', index:'icon', width:0, align:"right", sortable:false, hidden:true, xmlmap:"DefaultAuth"}
        ],
        xmlReader: { 
           root:"ChannelInfos", 
           row:"ChannelInfo",
           records:"ChannelInfoList>TotalAvailable",
           page:"ChannelInfoList>CurrentPage",
           total:"ChannelInfoList>TotalPages",
           repeatitems:false,
           id : "ChanId"
        },
        rowNum:20,
        rowList:[10,20,30,50],
        pager: '#pager',
        sortname: 'id',
        viewrecords: true,
        sortorder: "desc",
        loadonce: true,
        autoWidth: true,
        width: 820,
        height: 442
    });
}

var sourceid = 3;

initChannelEditor(sourceid);
