#include "acQuisor_WiFi_motor.h"
#include<ESP8266WiFi.h>
acQuisorWiFiMotor::acQuisorWiFiMotor(String WiFiSsid, String WiFiPass, String AP_ssid, String AP_pass, String host)
{
  customerWifiSsid = WiFiSsid;
  customerWifiPass = WiFiPass;
  device_SSID = AP_ssid;
  device_PASS = AP_pass;
  server = host;
}

void acQuisorWiFiMotor::Wconnect()
{
  deviceIP="";
  //server.handleClient();
  WiFi.begin(customerWifiSsid.c_str(), customerWifiPass.c_str());

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  
  Serial.print("\nIP address: ");
  deviceIP = String(WiFi.localIP()[0]) + String('.');
  deviceIP += String(WiFi.localIP()[1]) + String('.');
  deviceIP += String(WiFi.localIP()[2]) + String('.');
  deviceIP += String(WiFi.localIP()[3]);
}


void acQuisorWiFiMotor::generateURL(String customerName, String deviceName, String Cdate, float btryLvl, int stat)
{
  String u = "http://";
  u += server;
  u += "/water/embeddedGateway/dataFromMotorControl.php";
  u += "?status=";
  u += stat;
  u += "&customerName=";
  u += customerName;
  u += "&deviceName=";
  u += deviceName;
  u += "&cdate=";
  u += Cdate;
  u += "&IP=";
  u += deviceIP;
  u += "&customerWifiSsid=";
  u += customerWifiSsid;
  u += "&btry=";
  u += btryLvl;
  url = u;
}

