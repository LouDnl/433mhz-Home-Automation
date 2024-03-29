/*
 * Lightswitch and dimmer controller with website
 * Version 1.0
 * Created by LouD - august 2019
 * 
 * Board Manager package: ESP8266 Modules 2.5.2
 * Board: LOLIN(WEMOS) D1 R2 & mini
 * Board version: 2.2.0
 * SDK version: 2.2.1
 * Core version: 2.5.2
 *    
 * Libraries used:
 * ESP8266WiFi (https://github.com/esp8266/Arduino/releases)
 * ESP8266mDNS (https://github.com/esp8266/Arduino/releases)
 * WiFiUdp (https://github.com/esp8266/Arduino/releases)
 * ArduinoOTA (https://github.com/esp8266/Arduino/releases)
 * RCSwitch (https://github.com/sui77/rc-switch)
 * NewRemoteSwitch (https://bitbucket.org/fuzzillogic/433mhzforarduino)
 * 
 * Based on code from: 
 * - ESP8266 library examples
 * - the RCSwitch and NewRemoteTransmitter library examples
 * - https://randomnerdtutorials.com/esp8266-web-server-with-arduino-ide/
 * - https://www.instructables.com/id/Programming-a-HTTP-Server-on-ESP-8266-12E/
 * 
 * Used ports:
 * D1: Receiver
 * D2: Transmitter
 * D3: Button 1
 * D4: Builtin led
 * D5: Button 2
 * D6: Button 3
 * D7: Button 4
 * 
*/

// #### INCLUDES ####
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <RCSwitch.h>
#include <NewRemoteTransmitter.h>


// #### DEBUG VARIABLE ####
#ifdef DEBUG_ESP_PORT
  #define DEBUG_MSG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#else
  #define DEBUG_MSG(...)
#endif


// #### SETUP WIFI####
#ifndef STASSID
  #define STASSID "YOUR SSID"
  #define STAPSK  "YOUR WIFI PASSWORD"
#endif

const char *ssid = STASSID;
const char *password = STAPSK;

// #### SETUP NETWORK ####
IPAddress staticIP(192,168,0,45);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);

// #### SETUP WEBSERVER ON PORT 80 ####
WiFiServer server(80);

// #### SETUP TRANSMITTERS / TRANSMITTER PINS ####
// ## klikaanklikuit / clickonclickoff ##
NewRemoteTransmitter transmitter(123, D2, 260, 3); // dimmer group on address 123, pin D2, period duration 260ms, repeat on 2^3=8 times
//NewRemoteTransmitter transmitter2(125, D2, 260, 3); // outlet receiver group, same as above but on address 125 <- not in use for now
// ## any other 433mhz receiver/transmitter ##
RCSwitch mySwitch = RCSwitch(); // activate RCSwitch library

// #### SETUP PINS ####
const int ledPin = D4; // builtin led
// setup button pins
const byte interruptPin1 = D3; // all lights
const byte interruptPin2 = D5; // living room
const byte interruptPin3 = D6; // dining table
const byte interruptPin4 = D7; // desk

// #### SETUP TIMER VARIABLES ####
unsigned long currentTime = millis(); // Set current time variable once at restart
unsigned long previousTime = 0; // Previous time variable
const long timeoutTime = 2000; // Define timeout time in milliseconds (example: 2000ms = 2s)
long previousMillis = 0; // startup timer variable
long interval = 5; // startup timer interval in ms

// #### SETUP VARIABLES ####
// RCSwitch group and device variables
const char* group1 = "10000";
const char* group2 = "01000";
const char* device1 = "10000";
const char* device2 = "01000";
const char* device3 = "00100";
const char* device4 = "00010";

// Auxiliar variables to store the current output state
String output1State = "off"; // all lights variable
String output2State = "off"; // livingroom lights variable
String output3State = "off"; // diningtable light variable
String output4State = "off"; // desk light variable
//String output5State = "off";
//String output6State = "off";
//String output7State = "off";
//String output8State = "off";

// startup variable
bool startup = false; // run once variable

// Decode dimmer HTTP GET value
String valueString1 = String(15); // dimmer variable
int dim1pos1 = 0; // dimmer address position1
int dim1pos2 = 0; // dimmer address position2
String valueString2 = String(15); // dimmer variable
int dim2pos1 = 0; // dimmer address position1
int dim2pos2 = 0; // dimmer address position2

// #### FUNCTIONS TO IRAM ####
// load button ISR (Interrupt Service Routine) to IRAM, is mandatory on ESP platform
void ICACHE_RAM_ATTR button1();
void ICACHE_RAM_ATTR button2();
void ICACHE_RAM_ATTR button3();
void ICACHE_RAM_ATTR button4();

// load variables to IRAM
void ICACHE_RAM_ATTR remote();
void ICACHE_RAM_ATTR alloff();
void ICACHE_RAM_ATTR allon();
void ICACHE_RAM_ATTR g1d1on();
void ICACHE_RAM_ATTR g1d1off();
void ICACHE_RAM_ATTR g1d2on();
void ICACHE_RAM_ATTR g1d2off();
void ICACHE_RAM_ATTR kaku1on();
void ICACHE_RAM_ATTR kaku1off();
void ICACHE_RAM_ATTR kaku2on();
void ICACHE_RAM_ATTR kaku2off();

void setup(void) {
  Serial.begin(115200);
  DEBUG_MSG("## start bootup ##\n");
  
  // start wifi
  Serial.println("");
  Serial.print("## Connecting to ");
  Serial.print(ssid);
  Serial.println(" ##");
  WiFi.config(staticIP, gateway, subnet);
  WiFi.forceSleepWake();
  WiFi.persistent(false);
  WiFi.hostname("licht");
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for wifi connection
  /*
    while (WiFi.status() != WL_CONNECTED) { // just wifi
    delay(500);
    Serial.print(".");
    }
  */
  while (WiFi.waitForConnectResult() != WL_CONNECTED) { // wifi and OTA
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // print wifi connected info
  Serial.println("");
  Serial.println("## WiFi connected ##");
  Serial.print("## OTA hostname: ");
  Serial.print(WiFi.hostname());
  Serial.println(" ##");
  Serial.print("## OTA IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println(" ##");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266); // default

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("licht");

  // No authentication by default
  ArduinoOTA.setPassword("update");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  // Start mDNS
  if (!MDNS.begin("licht"))     { Serial.println("Error setting up MDNS responder!"); }
  else                          { Serial.println("## mDNS responder started ##"); }

  // start webserver
  server.begin();
  Serial.println("## HTTP server started ##");

  // start OTA
  ArduinoOTA.onStart([]() {
    String type;
    Serial.println("## START OTA ##");
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("## OTA READY ##");
  Serial.print("## IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println(" ##");

  // set up buttons
  pinMode(interruptPin1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin1), button1, FALLING);
  pinMode(interruptPin2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin2), button2, FALLING);
  pinMode(interruptPin3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin3), button3, FALLING);
  pinMode(interruptPin4, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin4), button4, FALLING);

  // enable RCSwitch library
  mySwitch.enableReceive(D1);  // receiver on interrupt pin D1
  mySwitch.enableTransmit(D2); // transmitter on pin D2
  mySwitch.setPulseLength(320); // set the pulselength in ms
  mySwitch.setRepeatTransmit(15); // repeat the tranmission 15 times

  // set ledPin to output (HIGH is off / LOW is on)
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  DEBUG_MSG("## bootup done in %d ms ##\n", currentTime);
}

void loop() {
  // when starting up / rebooting the esp switch everything off first
  bootup();
  
  // buttons
  // nothing to do here as they are ISR's and loaded to IRAM 

  // handle incoming signals from remote
  remote();
  
  // listen for incoming web clients
  runwebsite();

  // handle OTA update requests
  ArduinoOTA.handle();
}

void bootup () {
  if ( startup == false )
  {
    unsigned long currentMillis = millis();
    if ( currentMillis - previousMillis >= interval ) 
    {
      alloff();
      previousMillis = currentMillis;
      startup = true;
    }
  }
}

void outlet1on () {
  Serial.println("group1, device1 on");
  mySwitch.switchOn(group1, device1);
  output2State = "on"; // livingroom lights variable
}

void outlet1off () {
  Serial.println("group1, device1 off");
  mySwitch.switchOff(group1, device1);
  output2State = "off"; // livingroom lights variable
}

void outlet2on () {
  Serial.println("group1, device2 on");
  mySwitch.switchOn(group1, device2);
  output2State = "on"; // livingroom lights variable
}

void outlet2off () {
  Serial.println("group1, device2 off");
  mySwitch.switchOff(group1, device2);
  output2State = "off"; // livingroom lights variable
}

void dimmer1on () {
  delay(3);// needed between these type of receivers if the next command in loop is for the same receiver or esp crashes
  Serial.println("KAKU dimmer 1 on");
  //transmitter.sendUnit(1, true);
  transmitter.sendDim(1, 15);
  output3State = "on"; // diningtable light variable
  Serial.print("Set dimmer1 value to: ");
  Serial.println(valueString1);
}

void dimmer1off () {
  delay(3);// needed between these type of receivers if the next command in loop is for the same receiver or esp crashes
  Serial.println("KAKU dimmer 1 off");
  transmitter.sendUnit(1, false);
  output3State = "off"; // diningtable light variable
}

void dimmer2on () {
  delay(3);// needed between these type of receivers if the next command in loop is for the same receiver or esp crashes
  Serial.println("KAKU dimmer 2 on");
  //transmitter.sendUnit(2, true);
  transmitter.sendDim(2, 15);
  output4State = "on"; // desk light variable
  Serial.print("Set dimmer2 value to: ");
  Serial.println(valueString2);
}

void dimmer2off () {
  delay(3);// needed between these type of receivers if the next command in loop is for the same receiver or esp crashes
  Serial.println("KAKU dimmer 2 off");
  transmitter.sendUnit(2, false);
  output4State = "off"; // desk light variable
}

void allon () {
											
  Serial.println("start all on");
  outlet1on();
  outlet2on();
  dimmer1on();
  dimmer2on();
  output1State = "on"; // all lights variable
  
									   
		   
									   
		   
													

									 
			
													
																														
  
									 
			
											 
																														

  Serial.println("end all on");
										   
}

void alloff () {
											 
  Serial.println("start all off");
  outlet1off();
  outlet2off();
  dimmer1off();
  dimmer2off();
  output1State = "off"; // all lights variable  
			
													 

									  
			 
													 
																														
  
									  
			 
											  
																														 
  
  Serial.println("end all off");
											
}

void button1 () {
  if (output1State == "off") {
    Serial.println("button 1: switch all lights on");
    allon();
											   
  }
  else if (output1State == "on") {
    Serial.println("button 1: switch all lights off");
    alloff();
												
  }
}

void button2 () {
  if (output2State == "off") {
    Serial.println("button 2: livingroom lights on");
    outlet1on();
    outlet2on();
													  
  }
  else if (output2State == "on") {
    Serial.println("button 2: livingroom lights off");
    outlet1off();
    outlet2off();
													   
  }
}

void button3 () {
  if (output3State == "off") {
    Serial.println("button 3: diningtable light on");
    dimmer1on();
													  
  }
  else if (output3State == "on") {
    Serial.println("button 3: diningtable light off");
    dimmer1off();
														
  }
}

void button4 () {
  if (output4State == "off") {
    Serial.println("button 4: desk light on");
    dimmer2on();
											   
  }
  else if (output4State == "on") {
    Serial.println("button 4: desk light on");
    dimmer2off();
												
  }
}

void remote () {
  if (mySwitch.available()) { // simple receive version
    int value = mySwitch.getReceivedValue();

    switch(value)
    { 
      case 0:
      // nothing to do here
      break;

      case 5571921:
      if (output1State == "off") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button A ON received");
        allon();
												   
      }
      break;

      case 5571924:
      if (output1State == "on") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button A OFF received");
        alloff();
													
      }
      break;
      
      case 5574993:
      if (output2State == "off") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button B ON received");
        outlet1on();
        outlet2on();
														  
      }
      break;
      
      case 5574996:
      if (output2State == "on") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button B OFF received");
        outlet1off();
        outlet2off();
														   
      }
      break;
      
      case 5575761: 
      if (output3State == "off") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button C ON received");
        dimmer1on();
														  
      }
      break;
      
      case 5575764:
      if (output3State == "on") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button C OFF received");
        dimmer1off();
														   
      }
      break;
      
      case 5575953:
      if (output4State == "off") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button D ON received");
        dimmer2on();
												   
      }
      break;
      
      case 5575956:
      if (output4State == "on") {
        Serial.print( mySwitch.getReceivedValue() );
        Serial.println(" Remote button D OFF received");
        dimmer2off();
													
      }
      break;
      
      default:
      // do nothing
      break;
    }
  mySwitch.resetAvailable(); 
  }
}


void runwebsite () {
  WiFiClient client = server.available();

  if (!client) {
    return;
  }

  Serial.println("## Client connected ##");

  //Read what the browser has sent into a String class and print the request to the monitor
  String request = client.readStringUntil('\r');
  //Looking under the hood
  Serial.println(request);

  if (request.indexOf("GET /1/off") != -1) {
    Serial.println("button 1: switch all lights off");
    alloff();
						 
  } else if (request.indexOf("GET /1/on") != -1) {
    Serial.println("button 1: switch all lights on");
    allon();
						
  } else if (request.indexOf("GET /2/off") != -1) {
    Serial.println("button 2: livingroom lights off");
    outlet1off();
    outlet2off();
						 
  } else if (request.indexOf("GET /2/on") != -1) {
    Serial.println("button 2: livingroom lights on");
    outlet1on();
    outlet2on();
						
  } else if (request.indexOf("GET /3/off") != -1) {
    Serial.println("button 3: diningtable light off");
    dimmer1off();
						 
  } else if (request.indexOf("GET /3/on") != -1) {
    Serial.println("button 3: diningtable light on");
    dimmer1on();
						
  } else if (request.indexOf("GET /4/off") != -1) {
    Serial.println("button 4: desk light off");
    dimmer2off();
						 
  } else if (request.indexOf("GET /4/on") != -1) {
    Serial.println("button 4: desk light on");
    dimmer2on();
						
  } else if (request.indexOf("GET /?value1=") != -1) {
    dim1pos1 = request.indexOf('=');
    dim1pos2 = request.indexOf('&');
    valueString1 = request.substring(dim1pos1 + 1, dim1pos2);
    if (output3State == "on") {
      transmitter.sendDim(1, valueString1.toInt()); // no separate function for this yet, dimmer only accessible via webpage
      Serial.print("Set dimmer1 value to: ");
      Serial.println(valueString1);
    } else {
      Serial.println("Dimmer1 is not on");
    }
  } else if (request.indexOf("GET /?value2=") != -1) {
    dim2pos1 = request.indexOf('=');
    dim2pos2 = request.indexOf('&');
    valueString2 = request.substring(dim2pos1 + 1, dim2pos2);
    if (output4State == "on") {
      transmitter.sendDim(2, valueString2.toInt()); // no separate function for this yet, dimmer only accessible via webpage
      Serial.print("Set dimmer2 value to: ");
      Serial.println(valueString2);
    } else {
      Serial.println("Dimmer2 is not on");
    }
  }

  // HTML preparation
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-type:text/html\r\n";
  s += "Connection: keep-alive\r\n\r\n"; // add extra white line to send msg that html will be sent next
  s += "<!DOCTYPE html>\r\n";
  s += "<html>";
  s += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  s += "<meta http-equiv=\"refresh\" content=\"10;URL=/\">";
  s += "<link rel=\"icon\" href=\"data:,\">";
  s += "<title>LICHT SCHAKELAARS</title>";
  s += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  s += ".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;";
  s += "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  s += ".button2 {background-color: #77878A;}";
  s += ".slidecontainer { width: 100%; }";
  s += ".slider { -webkit-appearance: none; width: 100%; height: 25px; background: #d3d3d3; outline: none; opacity: 0.7; -webkit-transition: .2s; transition: opacity .2s; }";
  s += ".slider:hover { opacity: 1; }";
  s += ".slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 25px; height: 25px; background: #4CAF50; cursor: pointer; }";
  s += ".slider::-moz-range-thumb { width: 25px; height: 25px; background: #4CAF50; cursor: pointer; }";
  s += ".hr {display: block; height: 1px; border: 0; border-top: 1px solid #000; margin: 1em 0; padding: 0; }";
  s += "</style></head>";
  s += "<body><h1>LICHT SCHAKELAARS</h1>";
  
  //s += "<p>Alle lampen zijn " + output1State + "</p>";
  s += "<p>Alle lampen</p>";
  if (output1State == "on") {
    s += "<p><a href=\"/1/off\"><button class=\"button\">AAN</button></a></p>";
  } else {
    s += "<p><a href=\"/1/on\"><button class=\"button button2\">UIT</button></a></p>";
  } 

  //s += "<p>Woonkamer lampen zijn " + output2State + "</p>";
  s += "<p>Woonkamer</p>";
  if (output2State == "on") {
    s += "<p><a href=\"/2/off\"><button class=\"button\">AAN</button></a></p>";
  } else {
    s += "<p><a href=\"/2/on\"><button class=\"button button2\">UIT</button></a></p>";
  }

  //s += "<p>Eettafel verlichting is " + output3State + "</p>";
  s += "<p>Eettafel</p>";
  if (output3State == "on") {
    s += "<p><a href=\"/3/off\"><button class=\"button\">AAN</button></a></p>";
  } else {
    s += "<p><a href=\"/3/on\"><button class=\"button button2\">UIT</button></a></p>";
  }

  //s += "<p>Bureau verlichting is " + output4State + "</p>";
  s += "<p>Bureau</p>";
  if (output4State == "on") {
    s += "<p><a href=\"/4/off\"><button class=\"button\">AAN</button></a></p>";
  } else {
    s += "<p><a href=\"/4/on\"><button class=\"button button2\">UIT</button></a></p>";
  }

  if (output3State == "on") {
    s += "<div class=\"slidecontainer\">";
    s += "<div class=\"hr\"></div>";
    s += "<p>Eettafel dimmer</p>";
    s += "<input type=\"range\" min=\"1\" max=\"15\" value=\""+ valueString1 + "\" class=\"slider\" id=\"dimmer1\">";
    s += "<p>Value: <span id=\"dimvalue1\"></span></p>";
    s += "</div>";
  
    s += "<script>";
    s += "var slider1 = document.getElementById(\"dimmer1\");";
    s += "var output1 = document.getElementById(\"dimvalue1\");";
    s += "var request1 = new XMLHttpRequest();";
    //s += "slider.oninput = function() {"; // only works when selecting a position, sliding doesnt
    s += "slider1.onchange = function() {"; // testing
    s += "output1.innerHTML = this.value;";
    s += "request1.open(\"GET\", \"/?value1=\" + this.value + \"&\", false);";
    s += "request1.send();";
    s += "}";
    s += "</script>";
  } 

  if (output4State == "on") {
    s += "<div class=\"slidecontainer\">";
    s += "<div class=\"hr\"></div>";
    s += "<p>Bureau dimmer</p>";
    s += "<input type=\"range\" min=\"1\" max=\"15\" value=\""+ valueString2 + "\" class=\"slider\" id=\"dimmer2\">";
    s += "<p>Value: <span id=\"dimvalue2\"></span></p>";
    s += "</div>";
  
    s += "<script>";
    s += "var slider2 = document.getElementById(\"dimmer2\");";
    s += "var output2 = document.getElementById(\"dimvalue2\");";
    s += "var request2 = new XMLHttpRequest();";
    //s += "slider.oninput = function() {"; // only works when selecting a position, sliding doesnt
    s += "slider2.onchange = function() {"; // testing
    s += "output2.innerHTML = this.value;";
    s += "request2.open(\"GET\", \"/?value2=\" + this.value + \"&\", false);";
    s += "request2.send();";
    s += "}";
    s += "</script>";
  } 
  
  s += "</body></html>";

  client.flush();
  client.print(s);
  delay(1);
  Serial.println("Client disconnected.");
}
