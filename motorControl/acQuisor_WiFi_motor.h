#ifndef acQuisor_WiFi_motor
#define acQuisor_WiFi_motor

#if (ARDUINO >= 100)
  #include "Arduino.h"
#else
  #include "WProgram.h"

#endif

class acQuisorWiFiMotor
{
  public:
  acQuisorWiFiMotor(String WiFiSsid, String WiFiPass, String AP_ssid, String AP_pass, String host);
  
  String deviceIP, customerWifiSsid, customerWifiPass, device_SSID, device_PASS, url, server;
  void Wconnect();

  void generateURL(String customerName, String deviceName, String Cdate, float btryLvl, int stat);
};

#endif


