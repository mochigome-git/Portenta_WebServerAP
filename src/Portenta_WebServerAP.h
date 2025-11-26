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

private:
    // -------------------------
    // Constants
    // -------------------------
    const char *AP_SSID = "Portenta-Setup";
    const char *AP_PASS = "12345678";
    IPAddress AP_IP = IPAddress(192, 168, 4, 1);

    int HTTP_PORT;
    int DNS_PORT;
    const char *CRED_FILE = "/qspi/wifi.json";

    // -------------------------
    // State
    // -------------------------
    WiFiServer server;
    WiFiUDP udp;
    bool apModeActive;

    QSPIFlashBlockDevice blockDevice;
    FATFileSystem fs;

    // -------------------------
    // Helpers
    // -------------------------
    String urlDecode(const String &src);
    bool parseCredsFromJson(const String &json, String &ssid, String &pass);

    bool loadCredentials(WifiCredentials &creds);
    void saveCredentials(const WifiCredentials &creds);

    void handleDNS();
    bool connectSavedWiFi();
    void startAPMode();
    void stopAPMode();
    void updateLED();
};
