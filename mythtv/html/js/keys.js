// Function to create a cookie to check if virtual keyboard is enabled
function getCookie(cname) {
    var name = cname + "=";
    var ca = document.cookie.split(';');
    for(var i = 0; i < ca.length; i++) {
        var c = ca[i];
        while (c.charAt(0) == ' ') {
            c = c.substring(1);
        }
        if (c.indexOf(name) == 0) {
            return c.substring(name.length, c.length);
        }
    }
    return "";
}

// Set a cookie
function setCookie(cvalue) {
    document.cookie = "kbenabled=" + cvalue + ";";
    console.log("wrote value: " + cvalue);
}

// Get the cookie where keyboard is enabled or not
var kbenabled = getCookie("kbenabled");

// Let the keyboard to control the frontend
$(document).on('keydown', function(e) {
	console.log(window.kbenabled);
  // If keyboard is enabled
	if (kbenabled) {
    // Get the keypress into a variable
		var key = e.keyCode;
		switch (key) {
			// Arrow keys
			case 37:
				{
					key = "LEFT";
					break;
				}
			case 38:
				{
					key = "UP";
					break;
				}
			case 39:
				{
					key = "RIGHT";
					break;
				}
			case 40:
				{
					key = "DOWN";
					break;
				}
				// Play/Pause
			case 80:
				{
					key = "P";
					break;
				}
				// Info button
			case 73:
				{
					key = "I"
					break;
				}
				// Menu button
			case 77:
				{
					key = "M"
					break;
				}
				// Guide data button
			case 83:
				{
					key = "S"
					break;
				}
			case 27:
				{
					key = "ESCAPE"
					break;
				}
			case 13:
				{
					key = "ENTER"
					break;
				}
			case 32:
				{
					key = "SPACE"
					break;
				}
				// Volume down
			case 219:
				{
					key = "["
					break;
				}
			case 121:
				{
					key = "F10"
					break;
				}
				// Volume up
			case 122:
				{
					key = "F11"
					break;
				}
				// Mute the volume
			case 120:
				{
					key = "F9"
					break;
				}
		}
    // Print which key was pressed and its keycode
		console.log(key + " - " + e.keyCode)
    // Submit the ajax request
		$.ajax({
			url: "http://" + window.location.hostname + ":6547/Frontend/SendKey",
			type: "POST",
			data: {
				Key: key
			},
		})
	}
});

0
