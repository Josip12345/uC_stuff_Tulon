// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getValues(){
    websocket.send("getValues");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getValues();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function updateSliderPWM(element) {
    var sliderNumber = element.id.charAt(element.id.length-1);
    var sliderValue = document.getElementById(element.id).value;
    document.getElementById("sliderValue"+sliderNumber).innerHTML = sliderValue;
    console.log(sliderValue);
    websocket.send(sliderNumber+"s"+sliderValue.toString());
}

// Function to toggle the FSRP checkbox
function toggleCheckboxFSRP(checkbox) {
  const isChecked = checkbox.checked;
  const message = isChecked ? '1' : '0';
  websocket.send("FSRP"+message);
}

// Function to toggle the FSRP checkbox
function toggleCheckboxFSRN(checkbox) {
  const isChecked = checkbox.checked;
  const message = isChecked ? '1' : '0';
  websocket.send("FSRN"+message);
}

// Function to toggle the relock checkbox
function toggleCheckboxRelockL1(checkbox) {
  const isChecked = checkbox.checked;
  const message = isChecked ? '1' : '0';
  websocket.send("1REL"+message);
}


// Function to toggle the ramp checkbox
function toggleCheckboxRampL1(checkbox) {
  const isChecked = checkbox.checked;
  const message = isChecked ? '1' : '0';
  websocket.send("1RAM"+ message);
}



function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        if (key == 'lock_fail_counter') { // Check if the key is 'counter' and set the counter
        document.getElementById('lock_fail_counter').innerText = myObj[key]; // Update the counter display
        } else if (key == "FSRP") {
            if (myObj[key] == "STOP"){
           document.getElementById("FSRP").checked = false;
            }
        } else if (key == "FSRN") {
             if (myObj[key] == "STOP"){
           document.getElementById("FSRN").checked = false;
            }
        } else {
        document.getElementById(key).innerHTML = myObj[key];
        document.getElementById("slider"+ (i+1).toString()).value = myObj[key];
 
        
    }
 
    }
}

