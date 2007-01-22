var curElement = null;
var curMethodId;
var bMax = false;
var sRootPath = "/"

function Browser() 
{
	// 1 = IE
	// 2 = Netscape
	// 3 = other

	var ua = navigator.userAgent;

	if (ua.indexOf( "MSIE" ) >= 0) 
		return 1;

	if (ua.indexOf("Netscape") >= 0) 
		return 2;

	if (ua.indexOf("Mozilla") >= 0) 
		return 2;

	return 3;
}

function FindObject( szDivID ) 
{
	return( document.getElementById(szDivID) );
}

function GetEventSrc( e )
{
	switch( Browser() )
	{
		case 1:
	        return( e.srcElement.parentElement );
		default:
	        return( e.currentTarget );
	}
}

function ShowBox(szDivID, iState) // 1 visible, 0 hidden
{
	var obj = FindObject( szDivID );
	if (obj != null)
	{	
    	if(document.layers)	   //NN4+
			obj.display = iState ? "" : "none";
		else
			obj.style.display = iState ? "" : "none";
    }
}

function MinMax()
{
	var oResults = FindObject( "fraResults" );
	var oSpan    = FindObject( "MinMaxId" );

	if (oSpan.direction == "up")
	{
		oSpan.innerHTML = "<img src='arr_down.gif' width='17' height='17'>";
		oSpan.direction = "down";
		ShowBox( curMethodId, 0 );
		ShowBox( 'tbBlank'  , 0);
		bMax = true;

	}
	else
	{
		oSpan.innerHTML = "<img src='arr_up.gif' width='17' height='17'>";
		oSpan.direction = "up";
		ShowBox( curMethodId, 1 );
		ShowBox( 'tbBlank', 1);

		bMax = false;
	}

}

function UpdateIFrame( sURL )
{
	var obj = FindObject( "fraResults" );
	
	if (obj != null)
		obj.src = window.parent.frames[ "topFrame" ].GetMasterBackend( sRootPath, sURL );

	if (curElement != null)
		curElement.className = "menu";
	
	if ( window.event.srcElement != null )
	{	
		curElement = window.event.srcElement.parentElement;

		if (curElement != null)
			curElement.className = "selected";
	}
	else
		curElement = null;
}
	
function SetFormAddress( sMethod )
{
	var obj = FindObject( sMethod );
	
	if (obj != null)
		obj.action = window.parent.frames[ "topFrame" ].GetMasterBackend( sRootPath, sMethod );

	return true;
}

function MenuClick( e, methodId )
{
	if (curElement != null)
		curElement.className = "menu";

	var p = GetEventSrc( e );

	if ((curElement = p ) != null)
		curElement.className = "selected";

	if ( methodId != curMethodId)
	{
		ShowBox( curMethodId, 0 );
		ShowBox( methodId   , 1 );
		curMethodId = methodId;

		if (bMax)
			MinMax();

		var obj = FindObject( "fraResults" );
	
		if (obj != null)
			obj.src = "about:blank";

	}
}
