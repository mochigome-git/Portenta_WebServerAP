#include <Portenta_WebServerAP.h>

PortentaWebServerAP wsAP;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    wsAP.begin();
    if (!wsAP.connectSavedWiFi())
    {
        wsAP.startAPMode();
    }
}

void loop()
{
    wsAP.loop();
}
