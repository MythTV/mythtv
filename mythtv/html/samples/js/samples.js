
function GetSettingValue()
{
	var sKey = $("#Settings").val();

    jQuery.getJSON(
        "/Myth/GetSetting",
        { "Key": sKey },
        function( data ) 
        {
        	if (data.hasOwnProperty( 'd' ))
        	  data = data.d;
        	  
        	
           	$("#debug").html( inspect( data , 10 ) );


        	$("#SettingValue").val( data.SettingList.Settings[0].Value );
        });
}

$.getScript("/js/inspect.js");
$("#Settings").change( GetSettingValue );

