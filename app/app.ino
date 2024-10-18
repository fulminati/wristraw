#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

const String appTitle = "ESP8266 WiFi";
const String hostname = "esp8266.local.cloud";

String configErrorMessage;
String configNetworksOptions;

ESP8266WebServer webServer(80);
ESP8266WebServer discoverServer(64138);
IPAddress configHotSpotIpAddress(192, 168, 24, 1);
IPAddress configHotSpotNetmask(255, 255, 255, 0);

/**
 * Application bootstrap.
 */
void appSetup(void) {
    Serial.println("[App] Setup...");
}

/**
 * Application runtime loop.
 */
void appLoop(void) {
    //Serial.println("[App] Loop...");
}

/**
 * Application runtime loop.
 */
void appRoutes(void) {
    Serial.println("[App] Routes...");
}

/**
 * System bootstrap.
 */
void setup(void) {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    WiFi.hostname(hostname);
    delay(100);

    Serial.println("Disconnecting previously connected WiFi");
    WiFi.disconnect();
    delay(200);

    Serial.println("Reading SSID and passphrase from EEPROM");
    EEPROM.begin(512);
    delay(10);
    String ssid = dataReadAsString(0, 32);
    String passphrase = dataReadAsString(32, 96);
    Serial.println("- SSID: " + ssid);
    Serial.println("- Passphrase: " + passphrase);

    Serial.println("Perform WiFi connection with EEPROM");
        WiFi.begin(ssid.c_str(), passphrase.c_str());
        if (testWifi()) {
        Serial.println("Successfully connected.");
        discoverServerStart();
        defaultWebServerRegisterRoutes();
        webServer.begin();
        appSetup();
        return;
    }

    Serial.println("Turning on the config HotSpot.");
    discoverServerStart();
    configWebServerRegisterRoutes();
    webServer.begin();
    configHotSpotSetup();
    while ((WiFi.status() != WL_CONNECTED)) {
        delay(100);
        webServer.handleClient();
        discoverServer.handleClient();
    }
}

/**
 * System main loop.
 */
void loop(void) {
    if ((WiFi.status() == WL_CONNECTED)) {
        appLoop();
        delay(100);
        webServer.handleClient();
        discoverServer.handleClient();
    }
}

/**
 * Test WiFi status for connection.
 */
bool testWifi(void) {
    int c = 0;
    Serial.println("Waiting for WiFi to connect...");
    while (c < 20) {
        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }
        delay(500);
        Serial.print(".");
        c++;
    }
    Serial.println("");
    Serial.println("Connection timed out.");
    return false;
}

/**
 * Setup the HotSpot to access on config area.
 */
void configHotSpotSetup(void) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.softAPConfig(configHotSpotIpAddress, configHotSpotIpAddress, configHotSpotNetmask);
    delay(100);
    configScanNetworks();
    delay(100);
    WiFi.softAP(appTitle + " " + ESP.getChipId(), "");
}

/**
 *
 */
void configWebServerRegisterRoutes(void) {
    Serial.println("Register config web server routes");
    webServer.on("/", []() {
        String configIndexHtml = "";
        webServer.send(200, "text/html", configIndexHtml);
    });
    webServer.on("/config", []() {
        webServer.send(200, "text/html", configFormHtml());
    });
    webServer.on("/scan", []() {
        configScanNetworks();
        webServer.send(200, "text/html", configFormHtml());
    });
    webServer.on("/connect", []() {
        int statusCode;
        bool validData = false;
        String ssid = webServer.arg("ssid");
        String passphrase = webServer.arg("passphrase");
        String content;
        if (ssid.length() > 0 && passphrase.length() > 0) {
            dataErase(0, 96);
            dataSaveAsString(0, ssid);
            dataSaveAsString(32, passphrase);
            dataCommit();
            WiFi.begin(ssid.c_str(), passphrase.c_str());
            if (testWifi()) {
                String clientIpAddress = getClientIpAddress();
                content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}" + clientIpAddress;
                statusCode = 200;
                validData = true;
            } else {
                content = "{\"Error\":\"404 not found\"}";
                statusCode = 404;
            }
        } else {
            content = "{\"Error\":\"404 not found\"}";
            statusCode = 404;
        }
        webServer.send(statusCode, "application/json", content);
        if (validData) {
            delay(500);
            ESP.reset();
        }
    });
}

/**
 *
 */
void discoverServerStart(void) {
    discoverServer.on("/_discover", []() {
        String discoverInfo = "{\"name\":\"" + appTitle + "\"}";
        discoverServer.sendHeader("Access-Control-Allow-Origin", "*");
        discoverServer.sendHeader("Access-Control-Allow-Methods", "*");
        discoverServer.send(200, "application/json", discoverInfo);
    });
    discoverServer.onNotFound([]() {
        discoverServer.send(403, "text/html", "<h1>Forbidden</h1>");
    });
    discoverServer.begin();
}

/**
 * Scan networks and prepare the list for the login form.
 */
void configScanNetworks(void) {
    int countNetworks = WiFi.scanNetworks();
    Serial.println("Networks scan completed.");
    if (countNetworks == 0) {
        Serial.println("No WiFi Networks found");
        configNetworksOptions = "<option value=0>No networks found</option>";
        configNetworksOptions += "<option value=-1>Scan for networks</option>";
    } else {
        Serial.print(countNetworks);
        Serial.println(" Networks found");
        configNetworksOptions = "<option value=0>Select a network</option>";
        for (int i = 0; i < countNetworks; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            delay(10);
            configNetworksOptions += "<option value=\"" + WiFi.SSID(i) + "\">";
            configNetworksOptions += WiFi.SSID(i);
            configNetworksOptions += " (";
            configNetworksOptions += WiFi.RSSI(i);
            configNetworksOptions += ")";
            configNetworksOptions += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
            configNetworksOptions += "</option>";
        }
    }
}

/**
 *
 */
String configFormHtml(void) {
    String configFormHtml = "";
    return configFormHtml;
}

/**
 *
 */
void defaultWebServerRegisterRoutes(void) {
    webServer.on("/welcome", []() {
        String welcomeHtml = "";
        webServer.send(200, "text/html", welcomeHtml);
    });
    webServer.on("/reset", []() {
        dataErase(0, 96);
        dataCommit();
        webServer.send(200, "text/html", "<h1>Reset ok!</h1>");
        delay(500);
        ESP.reset();
    });
    webServer.onNotFound([]() {
        webServer.sendHeader("Location", "/welcome", true);
        webServer.send(302, "text/plane", "");
    });
    appRoutes();
}

/**
 * Get IP address assigned by DHCP to the device.
 */
String getClientIpAddress(void) {
    IPAddress ipAddress = WiFi.localIP();
    return String(ipAddress[0]) + '.' + String(ipAddress[1]) + '.' + String(ipAddress[2]) + '.' + String(ipAddress[3]);
}

/**
 * Read data from EEPROM.
 */
String dataReadAsString(int from, int to) {
    String data = "";
    for (int i = from; i < to; ++i) {
        data += char(EEPROM.read(i));
    }
    return data;
}

/**
 * Store data on EEPROM.
 */
void dataSaveAsString(int offset, String value) {
    for (int i = 0; i < value.length(); ++i) {
        EEPROM.write(offset + i, value[i]);
    }
}

/**
 * Erase EEPROM segment.
 */
void dataErase(int from, int to) {
    for (int i = from; i < to; ++i) {
        EEPROM.write(i, 0);
    }
}

/**
 * Commit data on EEPROM.
 */
void dataCommit(void) {
    EEPROM.commit();
}