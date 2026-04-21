// Working on creating an animation for a complete transaction
#define TFT
//#define NFC

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

String config_ssid;
String config_password;
String config_device_string;
String config_threshold_inkey;
int config_threshold_amount;
int config_threshold_pin;
int config_threshold_time;

String apiUrl = "/api/v1/ws/";

WebSocketsClient webSocket;

void setup() {
    Serial.begin(115200);
    // Serial.setDebugOutput(true);
    #ifdef TFT
    setupTFT();
    printHome(false, false, false);
    #endif
    setupConfig();
    setupWifi();
    
    pinMode(2, OUTPUT); // To blink on board LED

    if (config_device_string == "") {
        Serial.println("No device string configured!");
        printTFT("No device string!", 21, 95);
        return;
    }

    if (!config_device_string.startsWith("wss://")) {
        Serial.println("Device string does not start with wss://");
        printTFT("no wss://!", 21, 95);
        return;
    }

    String cleaned_device_string = config_device_string.substring(6); // Remove wss://
    String host = cleaned_device_string.substring(0, cleaned_device_string.indexOf('/'));
    String apiPath = cleaned_device_string.substring(cleaned_device_string.indexOf('/'));
    Serial.println("Websocket host: " + host);
    Serial.println("Websocket API Path: " + apiPath);

    if (config_threshold_amount != 0) { // Use in threshold mode
        Serial.println("Using THRESHOLD mode");
        Serial.println("Connecting to websocket: " + host + apiUrl + config_threshold_inkey);
        webSocket.beginSSL(host, 443, apiUrl + config_threshold_inkey);
    } else { // Use in normal mode
        Serial.println("Using NORMAL mode");
        Serial.println("Connecting to websocket: " + host + apiPath);
        webSocket.beginSSL(host, 443, apiPath);
    }
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1000);
    
    // Wait for WebSocket connection before initializing NFC
    Serial.println("Waiting for WebSocket connection before starting NFC...");
    unsigned long startTime = millis();
    bool wsConnected = false;
    
    while (!webSocket.isConnected() && (millis() - startTime) < 10000) {
        webSocket.loop();
        delay(100);
        if (webSocket.isConnected()) {
            wsConnected = true;
            break;
        }
        
        // Show progress every 2 seconds
        if ((millis() - startTime) % 2000 < 100) {
            Serial.println("Still waiting for WebSocket... " + String((millis() - startTime) / 1000) + "s");
        }
    }
    
    if (wsConnected) {
        Serial.println("WebSocket connected! Now initializing NFC...");
        #ifdef NFC
        setupNFC();
        #endif
    } else {
        Serial.println("WebSocket did not connect within 10 seconds");
        Serial.println("Initializing NFC anyway to enable card functionality...");
        #ifdef NFC
        setupNFC();
        #endif
        Serial.println("Note: NFC cards will use default actions until WebSocket connects");
    }
}

void loop() {
    loopWifi();
    webSocket.loop();

    #ifdef TFT
    loopTFT(WiFi.status() == WL_CONNECTED, webSocket.isConnected(), false);
    #endif

    #ifdef NFC
    loopNFC();
    #endif
}

void executePayment(uint8_t *payload) {
  String payloadStr = String((char *)payload);
  Serial.println("[WebSocket] Received message: " + payloadStr);
  
  // Check if this is an NFC-related message that we sent, not a payment response
  if (payloadStr.startsWith("ENCRYPTED_CARD:") || 
      payloadStr.startsWith("NFC_DEFAULT:") ||
      payloadStr.startsWith("NFC_PAYMENT:") ||
      payloadStr.startsWith("LNURL_PAYMENT:") ||
      payloadStr.startsWith("LIGHTNING_PAYMENT:") ||
      payloadStr.startsWith("CARD_DATA:") ||
      payloadStr == "Connected") {
    Serial.println("[WebSocket] This is our outgoing message or connection confirmation, ignoring");
    return;
  }
  
  printTFT("Payment received!", 21, 15);
  flashTFT();

  String parts[3]; // pin, time, comment
  // format: {pin-time-comment} where comment is optional
  int numParts = splitString(payloadStr, '-', parts, 3);
  
  Serial.println("[WebSocket] Split into " + String(numParts) + " parts:");
  for (int i = 0; i < numParts; i++) {
    Serial.println("  Part " + String(i) + ": '" + parts[i] + "'");
  }

  if (numParts < 2) {
    Serial.println("[WebSocket] Invalid payment format - expected pin-time-comment");
    return;
  }

  int pin = parts[0].toInt();
  printTFT("Pin: " + String(pin), 21, 35);

  int time = parts[1].toInt();
  printTFT("Time: " + String(time), 21, 55);

  String comment = "";
  if (numParts == 3) {
      comment = parts[2];
      Serial.println("[WebSocket] received comment: " + comment);
      printTFT("Comment: " + comment, 21, 75);
  }
  Serial.println("[WebSocket] received pin: " + String(pin) + ", duration: " + String(time));

  if (config_threshold_amount != 0) {
      // If in threshold mode we check the "balance" pushed by the
      // websocket and use the pin/time preset
      // executeThreshold();
      return; // Threshold mode not implemented yet
  }

  // the magic happens here
  Serial.println("[WebSocket] Activating relay - Pin: " + String(pin) + " for " + String(time) + "ms");
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delay(time);
  digitalWrite(pin, LOW);
  Serial.println("[WebSocket] Relay activation complete");

  printHome(true, true, false);

}

// long thresholdSum = 0;
// void executeThreshold() {
//     StaticJsonDocument<1900> doc;
//     DeserializationError error = deserializeJson(doc, payloadStr);
//     if (error) {
//         Serial.print("deserializeJson() failed: ");
//         Serial.println(error.c_str());
//         return;
//     }
//     thresholdSum = thresholdSum + doc["payment"]["amount"].toInt();
//     Serial.println("thresholdSum: " + String(thresholdSum));
//     if (thresholdSum >= (config_threshold_amount * 1000)) {
//         pinMode(config_threshold_pin, OUTPUT);
//         digitalWrite(config_threshold_pin, HIGH);
//         delay(config_threshold_time);
//         digitalWrite(config_threshold_pin, LOW);
//         thresholdSum = 0;
//     }
// }

//////////////////WEBSOCKET///////////////////
bool ping_toggle = false;
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_ERROR:
            Serial.printf("[WebSocket] Error: %s\n", payload);
            printHome(true, false, false);
            break;
        case WStype_DISCONNECTED:
            Serial.println("[WebSocket] Disconnected!\n");
            printHome(true, false, false);
            break;
        case WStype_CONNECTED:
            Serial.printf("[WebSocket] Connected to url: %s\n", payload);
            // send message to server when Connected
            webSocket.sendTXT("Connected");
            printHome(true, true, false);
            break;
        case WStype_TEXT:
            executePayment(payload);
            break;
        case WStype_BIN:
            Serial.printf("[WebSocket] Received binary data: %s\n", payload);
            break;
        case WStype_FRAGMENT_TEXT_START:
            break;
        case WStype_FRAGMENT_BIN_START:
            break;
        case WStype_FRAGMENT:
            break;
        case WStype_FRAGMENT_FIN:
            break;
        case WStype_PING:
            Serial.printf("[WebSocket] Ping!\n");
            ping_toggle = !ping_toggle;
            printHome(true, true, ping_toggle);
            // pong will be sent automatically
            break;
        case WStype_PONG:
            // is not used
            Serial.printf("[WebSocket] Pong!\n");
            printHome(true, true, true);
            break;
    }
}
