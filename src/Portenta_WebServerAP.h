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

// -------------------------
// Portenta WebServer AP class
// -------------------------
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
    // AP + credentials info
    const char *AP_SSID = "Portenta-Setup";
    const char *AP_PASS = "12345678";
    IPAddress AP_IP = IPAddress(192, 168, 4, 1);
    const char *CRED_FILE = "/qspi/wifi.json";

    // Server & UDP for DNS redirect
    WiFiServer server;
    WiFiUDP udp;
    bool apModeActive;

    // Flash filesystem
    static constexpr const char *FS_NAME = "qspi";
    QSPIFlashBlockDevice blockDevice;
    FATFileSystem fs;

    // Helpers
    void updateLED();
    void handleDNS();
    bool loadCredentials(WifiCredentials &creds);
    void saveCredentials(const WifiCredentials &creds);
    String urlDecode(const String &src);
    bool parseCredsFromJson(const String &json, String &ssid, String &pass);
};
