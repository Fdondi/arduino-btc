/*
  Repeating WiFi Web Client

 This sketch connects to a a web server and makes a request
 using an Arduino WiFi shield.

 Circuit:
 * WiFi shield attached to pins SPI pins and pin 7

 created 23 April 2012
 modified 31 May 2012
 by Tom Igoe
 modified 13 Jan 2014
 by Federico Vanzati

 http://arduino.cc/en/Tutorial/WiFiWebClientRepeating
 This code is in the public domain.
 */

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

const unsigned long postingInterval = 10L * 1000L; // delay between updates, in milliseconds

float currentPrice = -1;
float averagePrice = -1;
const float exponentialFactor = 0.9; // 1.0 = never change price, 0.0 change immediately to last price

void setup() {


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
  while(!doHttpRequest() || !readHttpJsonResponse()) {
    delay(1000);
  }
  averagePrice = currentPrice;
  Serial.print("First price: ");
  Serial.println(currentPrice);
}

void loop() {
  unsigned long connStart = millis();

  updateAverageWithPreviousCurrentPrice();
  // send HTTP request, reconnecting if needed.
  while(!doHttpRequest() || !readHttpJsonResponse()) {
    delay(1000);
  }
  doTheBlink();

  // wait until the end of the delay time
  unsigned long elapsedTime = millis() - connStart;
  Serial.print("Elapsed: ");
  Serial.println(elapsedTime);
  delay(postingInterval - elapsedTime);
}

void doTheBlink(){
  Serial.print("New price: ");
  Serial.println(currentPrice);
  Serial.print("Previous average: ");
  Serial.println(averagePrice);

  const int scale = 10000;
  const float change = scale * (currentPrice/averagePrice -1);
  if(change > 0){
    Serial.print("INCREASED by ");
  } else {
    Serial.print("DECREASED by ");
  }
  Serial.print(change);
  Serial.print(" per ");
  Serial.println(scale);
}

void updateAverageWithPreviousCurrentPrice(){
  // TODO update to consider how much time since last update
  averagePrice = averagePrice * exponentialFactor + currentPrice * (1.0- exponentialFactor);
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
