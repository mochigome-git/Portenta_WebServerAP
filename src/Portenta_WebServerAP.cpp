#include "Portenta_WebServerAP.h"
#include "Arduino.h"
#include <stdlib.h>

// -------------------------
// Constructor
// -------------------------
PortentaWebServerAP::PortentaWebServerAP(int httpPort, int dnsPort)
    : HTTP_PORT(httpPort), DNS_PORT(dnsPort),
      server(httpPort), udp(),
      apModeActive(false),
      blockDevice(PIN_QSPI_CLK, PIN_QSPI_SS, PIN_QSPI_D0, PIN_QSPI_D1, PIN_QSPI_D2, PIN_QSPI_D3),
      fs("qspi")
{
}

// -------------------------
// URL decode for form fields
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

// -------------------------
// Parse JSON credentials
// -------------------------
bool PortentaWebServerAP::parseCredsFromJson(const String &json, String &ssid, String &pass)
{
    int i1 = json.indexOf("\"ssid\"");
    if (i1 < 0)
        return false;
    int col = json.indexOf(':', i1);
    if (col < 0)
        return false;
    int q1 = json.indexOf('"', col);
    if (q1 < 0)
        return false;
    int q2 = json.indexOf('"', q1 + 1);
    if (q2 < 0)
        return false;
    ssid = json.substring(q1 + 1, q2);

    int i2 = json.indexOf("\"pass\"");
    if (i2 < 0)
    {
        pass = "";
        return true;
    }
    col = json.indexOf(':', i2);
    if (col < 0)
        return false;
    q1 = json.indexOf('"', col);
    if (q1 < 0)
        return false;
    q2 = json.indexOf('"', q1 + 1);
    if (q2 < 0)
        return false;
    pass = json.substring(q1 + 1, q2);
    return true;
}

// -------------------------
// Load/Save Credentials
// -------------------------
bool PortentaWebServerAP::loadCredentials(WifiCredentials &creds)
{
    FILE *f = fopen(CRED_FILE, "r");
    if (!f)
    {
        Serial.println("loadCredentials: file not found");
        return false;
    }

    char buf[128] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0)
    {
        Serial.println("loadCredentials: empty file");
        return false;
    }

    String ssidStr, passStr;
    if (!parseCredsFromJson(String(buf), ssidStr, passStr))
    {
        Serial.println("Failed to parse credentials");
        return false;
    }

    ssidStr.toCharArray(creds.ssid, sizeof(creds.ssid));
    passStr.toCharArray(creds.pass, sizeof(creds.pass));

    Serial.print("Loaded creds: ");
    Serial.println(creds.ssid);
    return true;
}

void PortentaWebServerAP::saveCredentials(const WifiCredentials &creds)
{
    FILE *f = fopen(CRED_FILE, "w");
    if (!f)
    {
        Serial.println("saveCredentials: open failed");
        return;
    }

    String json = "{\"ssid\":\"" + String(creds.ssid) + "\",\"pass\":\"" + String(creds.pass) + "\"}";
    fwrite(json.c_str(), 1, json.length(), f);
    fclose(f);

    Serial.println("saveCredentials: written successfully");
}

// -------------------------
// DNS Captive Portal
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

        buf[2] = 0x81; // QR = response
        buf[3] = 0x80;
        buf[7] = 0x01;

        int qend = 12;
        while (qend < len && buf[qend] != 0)
            qend++;
        int rdataIndex = qend + 12;
        if (rdataIndex + 13 < (int)sizeof(buf))
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
// Connect Saved WiFi
// -------------------------
bool PortentaWebServerAP::connectSavedWiFi()
{
    WifiCredentials creds;
    if (!loadCredentials(creds))
    {
        Serial.println("No saved credentials found");
        return false;
    }

    Serial.print("Connecting to saved WiFi: ");
    Serial.println(creds.ssid);
    WiFi.end();
    delay(150);
    WiFi.begin(creds.ssid, creds.pass);

    unsigned long start = millis();
    const unsigned long timeout = 20000UL;
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeout)
    {
        delay(300);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nConnected! IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("\nFailed to connect.");
    return false;
}

// -------------------------
// AP Mode
// -------------------------
void PortentaWebServerAP::startAPMode()
{
    Serial.println("Starting AP...");
    WiFi.end();
    delay(150);
    int st = WiFi.beginAP(AP_SSID, AP_PASS);
    if (st != WL_AP_LISTENING)
    {
        Serial.println("Creating AP failed");
        return;
    }

    Serial.print("AP IP: ");
    Serial.println(WiFi.localIP());
    udp.begin(DNS_PORT);
    server.begin();
    apModeActive = true;
    Serial.println("Web server started.");
}

void PortentaWebServerAP::stopAPMode()
{
    if (!apModeActive)
        return;
    Serial.println("Stopping AP...");
    WiFi.end();
    delay(150);
    apModeActive = false;
}

// -------------------------
// LED Status
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
// Begin
// -------------------------
void PortentaWebServerAP::begin()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    LED_Init();
    LED_Test();

    Serial.println("Mounting QSPI Flash FS...");
    int err = fs.mount(&blockDevice);
    if (err)
    {
        Serial.println("Mount failed, formatting...");
        err = fs.reformat(&blockDevice);
        if (err)
        {
            Serial.println("Format failed, stopping...");
            while (1)
                ;
        }
        else
            Serial.println("FS formatted successfully");
    }
    else
        Serial.println("FS mounted successfully");

    if (!connectSavedWiFi())
        startAPMode();
    server.begin();
    Serial.println("Web server started.");
}

// -------------------------
// Main loop
// -------------------------
void PortentaWebServerAP::loop()
{
    updateLED();

    if (apModeActive)
        handleDNS();

    // Scan networks every 10s
    static unsigned long lastScan = 0;
    static String networkList[20];
    if (millis() - lastScan > 10000)
    {
        int networkCount = WiFi.scanNetworks();
        for (int i = 0; i < networkCount && i < 20; i++)
            networkList[i] = WiFi.SSID(i);
        lastScan = millis();
    }

    WiFiClient client = server.available();
    if (!client)
        return;

    Serial.println("New client connected");
    String request = "";
    unsigned long timeout = millis() + 5000;

    while (client.connected() && millis() < timeout)
    {
        while (client.available())
            request += (char)client.read();
    }

    String body = "";
    int bodyIndex = request.indexOf("\r\n\r\n");
    if (bodyIndex >= 0)
        body = request.substring(bodyIndex + 4);

    // GET shortcuts
    if (request.indexOf("GET /L") >= 0)
        LED_SetColor(BLUE);
    if (request.indexOf("GET /H") >= 0)
        LED_SetColor(OFF);
    if (request.indexOf("GET /test") >= 0)
        LED_Test();

    // POST /save
    if (request.startsWith("POST /save"))
    {
        int ssidPos = body.indexOf("ssid=");
        int passPos = body.indexOf("pass=");
        if (ssidPos >= 0 && passPos >= 0)
        {
            String s;
            int amp = body.indexOf('&', ssidPos);
            if (amp > 0 && amp < passPos)
                s = body.substring(ssidPos + 5, amp);
            else
                s = body.substring(ssidPos + 5, passPos - 1);

            int amp2 = body.indexOf('&', passPos);
            String p;
            if (amp2 > 0)
                p = body.substring(passPos + 5, amp2);
            else
                p = body.substring(passPos + 5);

            s = urlDecode(s);
            p = urlDecode(p);
            WifiCredentials creds;
            s.toCharArray(creds.ssid, sizeof(creds.ssid));
            p.toCharArray(creds.pass, sizeof(creds.pass));
            saveCredentials(creds);

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.println("<html><body><h2>Credentials saved. Rebooting...</h2></body></html>");
            client.println();
            client.stop();

            Serial.println("Credentials saved. Rebooting...");
            delay(500);
            NVIC_SystemReset();
            return;
        }
        else
        {
            client.println("HTTP/1.1 400 Bad Request");
            client.println("Content-type:text/html");
            client.println();
            client.println("<html><body><h2>Bad request - missing ssid or pass</h2></body></html>");
            client.println();
            client.stop();
            Serial.println("Bad POST /save request");
            return;
        }
    }

    // Serve portal UI
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><title>WiFi Setup</title>");
    client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
    client.println("<style>");
    client.println("body { font-family: Arial, sans-serif; margin: 0; padding: 10px; text-align: center; }");
    client.println("input, select, button { font-size: 1.2em; padding: 8px; margin: 5px 0; width: 90%; max-width: 300px; }");
    client.println("h2 { font-size: 2em; }");
    client.println("</style></head><body>");
    client.println("<h2>Portenta C3 WiFi Setup</h2>");
    client.println("<form method='POST' action='/save'>");

    int count = WiFi.scanNetworks();
    if (count == 0)
        client.println("<p>No networks found</p>");
    else
    {
        client.println("SSID: <select name='ssid'>");
        for (int i = 0; i < count && i < 20; i++)
        {
            client.print("<option>");
            client.print(WiFi.SSID(i));
            client.println("</option>");
        }
        client.println("</select><br><br>");
    }

    client.println("Password: <input type='password' name='pass'><br><br>");
    client.println("<button type='submit'>Save</button></form>");
    client.println("<p><a href='/L'>Turn LED ON</a></p>");
    client.println("<p><a href='/H'>Turn LED OFF</a></p>");
    client.println("<p><a href='/test'>Test LED Colors</a></p>");
    client.println("</body></html>");
    client.println();

    client.stop();
    Serial.println("Client disconnected");
}
