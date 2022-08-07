#ifndef _HTML_
#define _HTML_
//***********************************************************************************************************************************************************************************************************************************
//WEB PAGES
//***********************************************************************************************************************************************************************************************************************************

const char index_html1[] PROGMEM =  R"=====(
<!DOCTYPE HTML><html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://kit.fontawesome.com/a47d5e547d.js" crossorigin="anonymous"></script>  
<style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
       .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #FFD65C;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; } 
    .button {display: inline-block;padding: 15px 25px;font-size: 24px;cursor: pointer;text-align: center;text-decoration: none;outline: none;color: #fff;background-color: #4CAF50;border: none;border-radius: 15px;box-shadow: 0 9px #999;}
    .button:hover {background-color: #3e8e41}
    .button:active {background-color: #3e8e41;box-shadow: 0 5px #666;transform: translateY(4px);}
  </style>
</head>
<body>
  <h2>REA Smart Lamp</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <i class="fas fa-chart-line" style="color:#D4583E;"></i>
    <span class="dht-labels">Stock Price $</span>
    <span id="price">%STOCK%</span>
  </p>
  <p>
  <i class="fa-solid fa-sun" style="color:#E3CD0B;"></i>
  <span class="dht-labels">Lamp Brighness:</span><span id="textSliderValue">%SLIDERVALUE%</span>
  </p>
  <p><input type="range" onchange="updateSliderLED(this)" id="ledSlider" min="0" max="255" value="%SLIDERVALUE%" step="1" class="slider"></p>
<p>
<button class="button" onclick="cycleColorMode();">Cycle Color Mode</button>
</p>
</body>
<script>
function cycleColorMode () {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "/cycleColor", true);
  xhttp.send();
}
function updateSliderLED(element) {
  var sliderValue = document.getElementById("ledSlider").value;
  document.getElementById("textSliderValue").innerHTML = sliderValue;
  console.log(sliderValue);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider?value="+sliderValue, true);
  xhr.send();
};
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 30000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 30000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("price").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/price", true);
  xhttp.send();
}, 60000 ) ;
</script>
</html>
)=====";
#endif