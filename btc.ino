#include <SPI.h>
#include <WiFi101.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h" 
char ssid[] = SECRET_SSID;    // network SSID
char pass[] = SECRET_PASS;    // network password (use for WPA, or use as key for WEP)
int keyIndex = 0;             // network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

// Initialize the WiFi client library
WiFiClient client;

// server address:
char server[] = "api.coindesk.com";

const unsigned long updateInterval = 10L * 1000L; // delay between updates, in milliseconds
unsigned long nextUpdate = 0;


float currentPrice = -1;
float averagePrice = -1;
const float exponentialFactor = 0.9; // 1.0 = never change price, 0.0 change immediately to last price

#define LED_RED 8
#define LED_GREEN 10

class ActiveLed{
  byte active_led :7;
  bool is_on      :1;
public:
  ActiveLed() : active_led(0), is_on(false) {}
  void on(){
    digitalWrite(active_led, HIGH);
    is_on = true;
  }
  void off(){
    digitalWrite(active_led, LOW);
    is_on = false;
  }
  void switch_led(byte new_led){
    off(); // switch off old led
    active_led = new_led;
    off(); // new led starts off
  }
  void toggle(){
    if(is_on){ off(); } else { on(); }
  }
};

class BlinkingLed: public ActiveLed {
  unsigned long blinkInterval = 0;
  unsigned long nextBlink = 0;
public:
  void blink(){
    toggle();
    nextBlink = millis() + blinkInterval;
  }
  void setBlinkInterval(unsigned long newInterval){
    blinkInterval = newInterval;
    nextBlink = millis() + blinkInterval;
    on();
  }
  bool is_overdue(unsigned long currentTime = millis()){
    return currentTime > nextBlink;
  }
} blinking_led;

void setup() {
  // Initialize pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to WiFi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  // you're connected now, so print out the status:
  printWiFiStatus();

  // ensure we have a connection and price
  readNewPrice();
  averagePrice = currentPrice;
  Serial.print("First price: ");
  Serial.println(currentPrice);
}

void loop() {
  const unsigned int currentTime = millis();
  if (currentTime > nextUpdate){
    blinking_led.off();
    nextUpdate = currentTime + updateInterval;
    updateAverageWithPreviousCurrentPrice();
    // send HTTP request, reconnecting if needed.
    readNewPrice();
    blinking_led.setBlinkInterval(getBlinkInterval());
  } else if(blinking_led.is_overdue(currentTime)){
    blinking_led.blink();
  }
}

void readNewPrice(){
  //Set inbuilt led to signal that data is refreshing
  digitalWrite(LED_BUILTIN, HIGH);
  while(!doHttpRequest() || !readHttpJsonResponse()) {
    delay(1000);
  }

  Serial.print("New price: ");
  Serial.println(currentPrice);

  // Turn off led now that we're done
  digitalWrite(LED_BUILTIN, LOW);
}

unsigned long getBlinkInterval(){
  const int scale = 10000;
  const float change = scale * (currentPrice/averagePrice -1);
  if(change > 0){
    blinking_led.switch_led(LED_GREEN);
    Serial.print("INCREASED by ");
  } else {
    blinking_led.switch_led(LED_RED);
    Serial.print("DECREASED by ");
  }
  Serial.print(change);
  Serial.print(" per ");
  Serial.println(scale);

  // Blink a number of times equal to "change"
  // 2 because 1 blink is 2 toggles, one on and one off
  const float blinksPerInterval = 2 * abs(change);
  return updateInterval / blinksPerInterval;
}

void updateAverageWithPreviousCurrentPrice(){
  // TODO update to consider how much time since last update
  averagePrice = averagePrice * exponentialFactor + currentPrice * (1.0- exponentialFactor);
  Serial.print("Average: ");
  Serial.println(averagePrice);
}

bool readHttpJsonResponse() {
  // get rid of headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break; // headers are done
    }
  }

  // if there is data, read it.
  while (client.available()) {
    String jsonResponse = client.readString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonResponse);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      Serial.println("Original was:");
      Serial.println(jsonResponse);
      return false;
    }
    currentPrice = doc["bpi"]["USD"]["rate_float"];
  }
  return true;
}

// this method makes a HTTP connection to the server:
bool doHttpRequest() {
  // Check if the client is already connected
  if (!client.connected()) {
    // Not connected, so try to connect
    if (!client.connect(server, 80)) {
      Serial.println("Connection failed");
      return false;
    }
  }

  // if there's a successful connection:
  Serial.println("Launching connection");
  Serial.println("");
  // send the HTTP GET request:
  client.println("GET /v1/bpi/currentprice.json HTTP/1.1");
  client.print("Host: ");
  client.println(server);
  client.println("User-Agent: ArduinoWiFi/1.1");
  client.println("Connection: keep-alive");
  client.println();

  return true;
}


void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
