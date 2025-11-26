#include "Portenta_WebServerAP.h"

// -------------------------
// Constructor
// -------------------------
PortentaWebServerAP::PortentaWebServerAP(int httpPort, int dnsPort)
    : server(httpPort), udp(), apModeActive(false),
      blockDevice(PIN_QSPI_CLK, PIN_QSPI_SS, PIN_QSPI_D0, PIN_QSPI_D1, PIN_QSPI_D2, PIN_QSPI_D3),
      fs("qspi")
{
}

// -------------------------
// Helpers
// -------------------------
String PortentaWebServerAP::urlDecode(const String &src)
{
    String ret = "";
    for (unsigned int i = 0; i < src.length(); i++)
    {
        char c = src[i];
        if (c == '+')
            ret += ' ';
        else if (c == '%' && i + 2 < src.length())
        {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            char *endptr = nullptr;
            int val = (int)strtol(hex, &endptr, 16);
            if (endptr != nullptr)
            {
                ret += (char)val;
                i += 2;
            }
            else
                ret += c;
        }
        else
            ret += c;
    }
    return ret;
}

bool PortentaWebServerAP::parseCredsFromJson(const String &json, String &ssid, String &pass)
{
    int i1 = json.indexOf("\"ssid\"");
    if (i1 < 0)
        return false;
    int col = json.indexOf(':', i1);
    if (col < 0)
        return false;
    int q1 = json.indexOf('"', col);
    int q2 = json.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0)
        return false;
    ssid = json.substring(q1 + 1, q2);

    int i2 = json.indexOf("\"pass\"");
    if (i2 < 0)
    {
        pass = "";
        return true;
    }
    col = json.indexOf(':', i2);
    q1 = json.indexOf('"', col);
    q2 = json.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0)
        return false;
    pass = json.substring(q1 + 1, q2);
    return true;
}

// -------------------------
// Load / Save credentials
// -------------------------
bool PortentaWebServerAP::loadCredentials(WifiCredentials &creds)
{
    FILE *f = fopen(CRED_FILE, "r");
    if (!f)
        return false;
    char buf[128] = {0};
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    String ssidStr, passStr;
    if (!parseCredsFromJson(buf, ssidStr, passStr))
        return false;
    ssidStr.toCharArray(creds.ssid, sizeof(creds.ssid));
    passStr.toCharArray(creds.pass, sizeof(creds.pass));
    return true;
}

void PortentaWebServerAP::saveCredentials(const WifiCredentials &creds)
{
    FILE *f = fopen(CRED_FILE, "w");
    if (!f)
        return;
    String json = "{\"ssid\":\"" + String(creds.ssid) + "\",\"pass\":\"" + String(creds.pass) + "\"}";
    fwrite(json.c_str(), 1, json.length(), f);
    fclose(f);
}

// -------------------------
// LED status
// -------------------------
void PortentaWebServerAP::updateLED()
{
    if (apModeActive)
        LED_SetColor(RED);
    else if (WiFi.status() == WL_CONNECTED)
        LED_SetColor(CYAN);
    else
        LED_SetColor(OFF);
}

// -------------------------
// DNS handler
// -------------------------
void PortentaWebServerAP::handleDNS()
{
    if (!apModeActive)
        return;
    int packetSize = udp.parsePacket();
    if (packetSize)
    {
        byte buf[512];
        int len = udp.read(buf, sizeof(buf));
        if (len <= 0)
            return;
        buf[2] = 0x81;
        buf[3] = 0x80;
        buf[7] = 0x01;
        int qend = 12;
        while (qend < len && buf[qend] != 0)
            qend++;
        int rdataIndex = qend + 12;
        if (rdataIndex + 13 < sizeof(buf))
        {
            buf[rdataIndex + 10] = AP_IP[0];
            buf[rdataIndex + 11] = AP_IP[1];
            buf[rdataIndex + 12] = AP_IP[2];
            buf[rdataIndex + 13] = AP_IP[3];
        }
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(buf, packetSize);
        udp.endPacket();
    }
}

// -------------------------
// Connect saved WiFi
// -------------------------
bool PortentaWebServerAP::connectSavedWiFi()
{
    WifiCredentials creds;
    if (!loadCredentials(creds))
        return false;

    WiFi.end();
    delay(150);
    WiFi.begin(creds.ssid, creds.pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
        delay(300);

    return (WiFi.status() == WL_CONNECTED);
}

// -------------------------
// Start / Stop AP
// -------------------------
void PortentaWebServerAP::startAPMode()
{
    WiFi.end();
    delay(150);
    if (WiFi.beginAP(AP_SSID, AP_PASS) != WL_AP_LISTENING)
        return;
    udp.begin(53);
    server.begin();
    apModeActive = true;
}

void PortentaWebServerAP::stopAPMode()
{
    if (!apModeActive)
        return;
    WiFi.end();
    delay(150);
    apModeActive = false;
}

// -------------------------
// Begin library
// -------------------------
void PortentaWebServerAP::begin()
{
    LED_Init();
    LED_Test();

    // Mount filesystem, format only if failed
    if (!fs.mount(&blockDevice))
        fs.reformat(&blockDevice);

    // Try to connect saved WiFi
    if (connectSavedWiFi())
    {
        Serial.print("Connected to WiFi. IP: ");
        Serial.println(WiFi.localIP());
        apModeActive = false; // no AP
    }
    else
    {
        Serial.println("Failed to connect WiFi. Starting AP mode...");
        startAPMode();
    }

    server.begin(); // webserver always started
}

// -------------------------
// Main loop
// -------------------------
void PortentaWebServerAP::loop()
{
    updateLED();
    if (apModeActive)
        handleDNS();

    WiFiClient client = server.available();
    if (client)
    {
        String request = "";
        unsigned long timeout = millis() + 5000;
        while (client.connected() && millis() < timeout)
        {
            while (client.available())
                request += (char)client.read();
        }

        String body = "";
        int idx = request.indexOf("\r\n\r\n");
        if (idx >= 0)
            body = request.substring(idx + 4);

        // handle GET /L, /H, /test
        if (request.indexOf("GET /L") >= 0)
            LED_SetColor(BLUE);
        if (request.indexOf("GET /H") >= 0)
            LED_SetColor(OFF);
        if (request.indexOf("GET /test") >= 0)
            LED_Test();

        // handle POST /save
        if (request.startsWith("POST /save"))
        {
            int ssidPos = body.indexOf("ssid=");
            int passPos = body.indexOf("pass=");
            if (ssidPos >= 0 && passPos >= 0)
            {
                String s = urlDecode(body.substring(ssidPos + 5, passPos - 1));
                String p = urlDecode(body.substring(passPos + 5));
                WifiCredentials creds;
                s.toCharArray(creds.ssid, sizeof(creds.ssid));
                p.toCharArray(creds.pass, sizeof(creds.pass));
                saveCredentials(creds);
                client.println("HTTP/1.1 200 OK\r\nContent-type:text/html\r\n\r\n<h2>Saved. Rebooting...</h2>");
                client.stop();
                NVIC_SystemReset();
                return;
            }
        }

        // serve portal page
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println("<h2>Portenta WiFi Setup</h2>");
        client.stop();
    }
}