#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "acQuisor_WiFi.h"
#include<ArduinoJson.h>
#include "FS.h"

float tankHeight_cm = 100;
float btryLvl=50.0;
char* host = "172.22.25.3";
const int httpPort = 80;

//String Cmd[2], tmp;
String serverResponse, Cdate;

ESP8266WebServer server(80);   //Web server object. Will be listening in port 80 (default for HTTP)

String customerName="",tankName="";
String wifiSsid = "q1", wifiPass = "12345678", apSsid = "water_acQuisor", apPass = "acquisor123";
//String customerName = "priyen@watertank", tankName = "Boys_hostel", customerWifiSsid = "q1", customerWifiPass = "12345678", APpass = "spatertech";

acQuisorWiFi acqWifi(wifiSsid, wifiPass, apSsid, apPass);

int relayOutput = D2;
int calibrationSwitch = D3;
int calibrationFlag = 0;
int echo = D5;
int trig = D6;
float soundSpeed=345.0, waterLevel;

float getDistance();
void devStat();
void virtualSwitch();
void calibrate();
void handleEdit();
void handleSuccess();
void handleRoot();
bool loadConfig();
bool saveConfig();

void wait();
void setup() {
  pinMode(D4,OUTPUT);//Server connection light. On when device connected to server
  digitalWrite(D4,1);

  Serial.begin(9600);

  Serial.println("Mounting FS...");

  if (!SPIFFS.begin()) 
  {
    Serial.println("Failed to mount file system");
    return;
  }
  
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(calibrationSwitch, INPUT);   //pull down calibration switch 
  pinMode(relayOutput, OUTPUT);  //Solenoid output
  
  //attachInterrupt(digitalPinToInterrupt(calibrationSwitch), calibrate, RISING);
  
  digitalWrite(relayOutput,0);
  
  Serial.print("Configuring access point...");
  WiFi.softAP(acqWifi.device_SSID.c_str(), acqWifi.device_PASS.c_str());
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  server.on("/switch",virtualSwitch);
  server.on("/deviceStatus",devStat);
  server.on("/edit", handleEdit); //Associate the handler function to the path
  server.on("/success", handleSuccess);
  server.on("/", handleRoot);
  server.begin();                                       //Start the server
  
  Serial.println("Server listening");
  
  if(!(loadConfig()))
    Serial.println("\n Flash memory didnt load");
  else
    Serial.println("\n Flash data imported successfully");
  
  Serial.println("\n Recovered data: ");
  Serial.println(acqWifi.customerWifiSsid);
  Serial.println(acqWifi.customerWifiPass);
  Serial.println(customerName);
  Serial.println(tankName);
  Serial.println(tankHeight_cm);

  if(acqWifi.customerWifiSsid!="default"&&acqWifi.customerWifiSsid!="")
    acqWifi.Wconnect();

}

void loop() 
{
 // static unsigned long cycleCount;
  server.handleClient();
  if(calibrationSwitch==HIGH)
    calibrate();
  while(acqWifi.customerWifiSsid=="default"||acqWifi.customerWifiSsid=="")
  {
    wait();
    server.handleClient();
  }

  if(calibrationFlag==1)
  {
    acqWifi.generateURL("height", tankHeight_cm, customerName, tankName, Cdate, btryLvl, digitalRead(relayOutput));
    calibrationFlag = 0;
  }
  else
  {
    waterLevel = getDistance();
    acqWifi.generateURL("level", waterLevel, customerName, tankName, Cdate, btryLvl, digitalRead(relayOutput));
  }

  if (WiFi.status() == WL_CONNECTED)
  {   
    Serial.print("\nThis device's IP: ");
    Serial.println(acqWifi.deviceIP);
    Serial.print("\nConnecting to host @ ");
    Serial.print(host);
    Serial.println();

    WiFiClient client;
    if (!client.connect(host, httpPort))
    {
      Serial.print("\nHost connection failed");
      digitalWrite(D4,1); //No connection indicator #D4 is active low pin#
    }
    else
    {
      
      digitalWrite(D4,0); //Turn off no connection indicator #D4 is active low pin#
      
      Serial.print("\nPresent water surface distance: ");
      Serial.println(waterLevel);
      Serial.println();
      Serial.println(acqWifi.url);
      client.print(String("GET ") + acqWifi.url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  
      unsigned long timeout = millis();
      while (client.available() == 0) 
      {
        if (millis() - timeout > 5000) 
        {
          Serial.println(">>> Client Timeout: ");
          client.stop();
          return;
        }
      }
     // cycleCount++;
      serverResponse="x";
      while (client.available()) {
        serverResponse = client.readStringUntil('\r');
      }
       Serial.println(serverResponse);
       
       int index = serverResponse.indexOf('_');
       String cmd = serverResponse.substring(0,index);
       Cdate = serverResponse.substring(index+1);
       Serial.print("\nAction to be performed: ");
       Serial.print(cmd);
       Serial.print("\nToday is: ");
       Serial.println(Cdate);
  
      if(cmd.length()==6)
      {
        digitalWrite(relayOutput, 1);
        Serial.println("\nValve opened, Water is filling up!");
      }
  
      else if(cmd.length()==5)
      {
        digitalWrite(relayOutput, 0);
        Serial.println("\nValve closed. Water tank refilled");
      }
  
      else
      {
        Serial.println("Motor / valve State unchanged");
      }
    }
   //----------------------------------------------------------//
    //Serial.print("\nNumber of successful cycles: ");
    //Serial.println(cycleCount);
    Serial.println("\n\nEnd of Cycle.");
  }

  else
    Serial.print(".");
  delay(200);
}

void wait()
{
  digitalWrite(D4,1);
  delay(200);
  digitalWrite(D4,0);
  delay(200);
}

void calibrate()
{
 // String x;
  float tempHeight;
  calibrationFlag = 1;  // set calibration flag
  
  Serial.println("Calibrating Tank");
  digitalWrite(D2, 0);
  for (int i = 0; i < 5; i++)
  {
 //   x = "Reading " + String(i) + String(": ");
    tempHeight = getDistance();
    delay(100);
  }
  if(tempHeight>=0.00)    // make > 0 after connecting sensor
  {
    tankHeight_cm = tempHeight;
  }
}

float getDistance()
{
  digitalWrite(trig,LOW);
  delayMicroseconds(2);

  digitalWrite(trig,HIGH);
  delayMicroseconds(10);
  
  digitalWrite(trig,LOW);
  
  float dur = pulseIn(echo,HIGH);
  float dis = 345.0 * dur / 20000;
  //delay(1000);
  return dis;
}
void handleEdit()
{
  String htmlCode = "<center><h1>Setup your Sensor module here</h1><br><br><form method='get' action='success'><table><tr><td>Default customer name:</td><td><input type='text' name='customerName' placeholder='username_in_website' value='priyen'/></td></tr><tr><td>Tank Name:</td><td><input type='text' name='tankName' placeholder='Ex:department_name'/></td></tr><tr><td> WIFI_SSID: </td><td><input type='text' name='customerWifiSsid' placeholder='Name of hotspot to connect' value='ap_comp_engg'/></td></tr><tr><td>Password: </td><td><input type='password' name='customerWifiPass' placeholder='Password of hotspot' value='computer12345'/></td></tr></table><br><input type='submit' value='update details'/></form></center>";
  server.send(200, "text/html", htmlCode);
}
void devStat()
{
  String req = String(server.arg("stat"));
  if(req=="on")
    digitalWrite(relayOutput,1);
   else if(req=="off")
    digitalWrite(relayOutput,0);
    
  String htmlCode = "<META HTTP-EQUIV='refresh' content='5' /><center><h2>Device / Sensor Status and Parameters</h2><table><tr><th>IP address: </th><td>";  
  htmlCode += acqWifi.deviceIP;
  htmlCode += "</td></tr><tr><th>Valve Status: </th><td>";
  if(digitalRead(relayOutput)==1)
    htmlCode += "ON" ;
  else
    htmlCode += "OFF";
    
  htmlCode += "</td></tr><tr><th>Water level: </th><td>";
  htmlCode += getDistance();
  htmlCode += "</td></tr><tr><th>Customer name: </th><td>";
  htmlCode += customerName;
  htmlCode += "</td></tr><tr><th>Tank Name</th><td>";
  htmlCode += tankName;
  htmlCode += "</td></tr><tr><th>Connected to: </th><td>";
  htmlCode += acqWifi.customerWifiSsid;
  htmlCode += "</td></tr><tr><th>Battery Level: </th><td>";
  htmlCode += btryLvl;
  htmlCode += "</td></tr></table><br><br><form method='get' action='#'><input type='radio' name='stat' value='on'> Turn ON valve <br><br><input type='radio' name='stat' value='off' checked> Turn OFF valve <br><br><input type='submit' name='status' value='Take Action'></button></form><br><br><b>Contact us at: 1505051@ritindia.edu</b></center>";
  server.send(200, "text/html", htmlCode);
}
void virtualSwitch()
{
  String req = String(server.arg("stat"));
  if(req=="on")
    digitalWrite(relayOutput,1);
   else if(req=="off")
    digitalWrite(relayOutput,0);
}
void handleSuccess()
{
  
  tankName = String(server.arg("tankName"));
  acqWifi.customerWifiSsid = String(server.arg("customerWifiSsid"));
  acqWifi.customerWifiPass = String(server.arg("customerWifiPass"));
  customerName = String(server.arg("customerName"));
  
  if(!(saveConfig()))
    Serial.println("\n Data not saved to Flash");
  else
    Serial.println("\n Data saved on Flash");

  acqWifi.Wconnect();

  String message = "<br><br><center><h1>The Tank details have been successfully updated.</h1><br><br> The Water acQuisor Sensor is connected to: ";
  message += acqWifi.customerWifiSsid;
  message += "<br><br>The IP address of tank is: ";
  message += acqWifi.deviceIP;
  message += "<br><br>The tank is named as: ";
  message += tankName;
  message += "<br><br><li><a href='http://172.22.25.3/water/'>Visit website for management and control</a></li><br><li><b>Contact us at: 1505051@ritindia.edu</b></li>";
  message += "</center>";
  delay(500);
  server.send(200, "text/html", message);
}

void handleRoot()
{
  String message2 = "<center><h1> Welcome to Water acQuisor </h1><br><br><li><a href = '/edit'> Setup the Water acQuisor </a></li><br><li><a href = '/deviceStatus'> View device status and parameters</a></li><br><br> Thank you for buying Water acQuisor.<br><br><b>Contact us at 1505051@ritindia.edu </b></center>";

  server.send(200, "text/html", message2);
}
bool loadConfig()
{
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }
  
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  String customerWifiSsidr = json["Wssid"];
  String customerWifiPassr = json["Wpass"];
  String customerNamer = json["cname"];
  String tankNamer = json["tname"];
  float tankHeight_cmr = json["tankHt"];

  acqWifi.customerWifiSsid = customerWifiSsidr;
  acqWifi.customerWifiPass = customerWifiPassr;
  customerName = customerNamer;
  tankName = tankNamer;
  tankHeight_cm = tankHeight_cmr;
  
  return true;
}
  
bool saveConfig()
{
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["Wssid"] = acqWifi.customerWifiSsid;
  json["Wpass"] = acqWifi.customerWifiPass;
  json["cname"] = customerName;
  json["tname"] = tankName;
  json["tankHt"] = tankHeight_cm;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}
