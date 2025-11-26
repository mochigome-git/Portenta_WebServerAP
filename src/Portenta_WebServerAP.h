#pragma once
#include <WiFiC3.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include "LittleFileSystem.h"
#include <Portenta_LedControl.h>
#include "QSPIFlashBlockDevice.h"
#include "FATFileSystem.h"

// -------------------------
// WiFi credentials struct
// -------------------------
struct WifiCredentials
{
    char ssid[32];
    char pass[64];
};

class PortentaWebServerAP
{
public:
    PortentaWebServerAP(int httpPort = 80, int dnsPort = 53);
    void begin();
    void loop();
    bool connectSavedWiFi();
    void startAPMode();
    void stopAPMode();

private:
    const char *CRED_FILE = "/qspi/wifi.json";
    const char *AP_SSID = "Portenta-Setup";
    const char *AP_PASS = "12345678";
    IPAddress AP_IP = IPAddress(192, 168, 4, 1);

    WiFiServer server;
    WiFiUDP udp;
    bool apModeActive;

    QSPIFlashBlockDevice blockDevice;
    FATFileSystem fs;

    void updateLED();
    void handleDNS();
    bool loadCredentials(WifiCredentials &creds);
    void saveCredentials(const WifiCredentials &creds);
    String urlDecode(const String &src);
    bool parseCredsFromJson(const String &json, String &ssid, String &pass);
};
