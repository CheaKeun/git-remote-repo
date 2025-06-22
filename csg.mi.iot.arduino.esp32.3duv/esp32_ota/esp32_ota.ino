#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <PubSubClient.h>

const char *HOST = "3DUVIOT_A200";                                            // <------ 디바이스마다 다르게 수정
const char *SSID = "iptime_MI";                    // Network SSID (name) Test : iptime_MI_K  , insert  : MI_WL_A020
const char *WIFI_PWD = "iot123sys";                // Network password (use for WPA, or use as key for WEP)

const char *MQTT_SERVER = "192.168.0.57";       // MQTT Server address 203.228.107.124
//const char *MQTT_SERVER = "172.30.22.65";        // MQTT Server address 203.228.107.124
const int MQTT_PORT = 1883;                        // Port number
const char *MQTT_USER = "";                        // User ID
const char *MQTT_PWD = "";                         // Password

const char *SENSOR_ID = "3DUVIOT-A100";            // Test : VIB_SENSOR_01  , insert  : VIBIOT-A001

#define OUT_TOPIC_BUFFER_SIZE (50) // Vibration OutTopic
char OutTopic[OUT_TOPIC_BUFFER_SIZE];
#define SENSOR_BUFFER_SIZE (150) // Vibration Sensor to MQTT
char Sensor[SENSOR_BUFFER_SIZE];
#define MAC_ADDR_BUFFER_SIZE (50) // Mac Address
char MacAddr[MAC_ADDR_BUFFER_SIZE];

WebServer server(80);

int wifiStatus = WL_IDLE_STATUS; // WiFi Status

WiFiClient nanoclient;
PubSubClient client(nanoclient);

/*
 * Login page
 */

const char *loginIndex =
    "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
    "<tr>"
    "<td colspan=2>"
    "<center><font size=4><b>IoT Firmware Download Login Page</b></font></center>"
    "<br>"
    "</td>"
    "<br>"
    "<br>"
    "</tr>"
    "<tr>"
    "<td>Username:</td>"
    "<td><input type='text' size=25 name='userid'><br></td>"
    "</tr>"
    "<br>"
    "<br>"
    "<tr>"
    "<td>Password:</td>"
    "<td><input type='Password' size=25 name='pwd'><br></td>"
    "<br>"
    "<br>"
    "</tr>"
    "<tr>"
    "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
    "</tr>"
    "</table>"
    "</form>"
    "<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='iotsys' && form.pwd.value=='iot123sys')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
    "</script>";

/*
 * Server Index Page
 */

const char *serverIndex =
    "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
    "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
    "<script>"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    " $.ajax({"
    "url: '/update',"
    "type: 'POST',"
    "data: data,"
    "contentType: false,"
    "processData:false,"
    "xhr: function() {"
    "var xhr = new window.XMLHttpRequest();"
    "xhr.upload.addEventListener('progress', function(evt) {"
    "if (evt.lengthComputable) {"
    "var per = evt.loaded / evt.total;"
    "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
    "}"
    "}, false);"
    "return xhr;"
    "},"
    "success:function(d, s) {"
    "console.log('success!')"
    "},"
    "error: function (a, b, c) {"
    "}"
    "});"
    "});"
    "</script>";

/*
 * setup function
 */
void setup(void)
{
    Serial.begin(115200);

    // Connect to WiFi network
    WiFi.begin(SSID, WIFI_PWD);
    Serial.println("");

    byte addr[6];
    WiFi.macAddress(addr);
    snprintf(MacAddr, MAC_ADDR_BUFFER_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    snprintf(OutTopic, OUT_TOPIC_BUFFER_SIZE, "iot/3duv/%s", MacAddr);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    /*use mdns for host name resolution*/
    if (!MDNS.begin(HOST))
    { // http://esp32.local
        Serial.println("Error setting up MDNS responder!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");
    /*return index page which is stored in serverIndex */
    server.on("/", HTTP_GET, []()
              {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "<center><font size=4><b>[ " + String(HOST) + " ]</b></font></center>" + String(loginIndex)); });
    server.on("/serverIndex", HTTP_GET, []()
              {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex); });
    /*handling uploading firmware file */
    server.on("/update", HTTP_POST, []()
              {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart(); }, []()
              {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } });
    server.begin();
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);

    Serial.print("Message:");

    String message;
    for (int i = 0; i < length; i++)
    {
        message = message + (char)payload[i]; // Conver *byte to String
    }
    Serial.println(message);
    Serial.println("-----------------------");
}

void reconnect_wifi()
{
    // Loop until we're reconnected
    while (wifiStatus != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(SSID);
        // Connect to WPA/WPA2 network:
        wifiStatus = WiFi.begin(SSID, WIFI_PWD);
        // wait 10 seconds for connection:
        delay(10000);
        // you're connected now, so print out the data:
        if (wifiStatus == WL_CONNECTED)
        {
            Serial.println("You're connected to the network");
        }
        else
        {
            wifiStatus = WiFi.status();
            Serial.print("failed =");
            Serial.println(wifiStatus);
            break;
        }
    }
}

void reconnect_mqtt()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(MacAddr))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            wifiStatus = WiFi.status(); // Turnoff wifi module
            // Wait 5 seconds before retrying
            delay(5000);
            break;
        }
    }
}

void loop(void)
{
    server.handleClient();

    // mqtt Setup and Connection
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqttCallback);

    wifiStatus = WiFi.status();
    if (wifiStatus != WL_CONNECTED)
    {
        reconnect_wifi();
    }
    if (!client.connected())
    {
        reconnect_mqtt();
    }

    char *json = "{ \"sensor_id\":\"%s\", \"mW\": %3.8f}";
    float mw = (analogRead(A0) / 4095) * 500;
    if (mw < 0)
        mw = 0;

    snprintf(Sensor, SENSOR_BUFFER_SIZE, json, SENSOR_ID, mw);
    Serial.println(Sensor);
    Serial.print("ADC raw value : ");
    Serial.println(analogRead(A0));

    // MQTT Publish
    client.loop();
    client.publish(OutTopic, Sensor); // publish
    delay(1000);
}
