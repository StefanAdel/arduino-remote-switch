#if defined(ARDUINO_UNOR4_WIFI)
  #include <WiFiS3.h>
#endif

#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

#include <WDT.h>


///////please enter your sensitive data in the Secret tab/arduino_secrets.h
extern char ssid[];
extern char pass[];
#include "wifi_secrets.h"


// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.
const char broker[] = "192.168.178.78";
int        port     = 1883;


WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);



ArduinoLEDMatrix matrix;
const uint32_t matrix_happy[] = {
   0x19819,
   0x80000001,
   0x81f8000
};

static unsigned long lastMatrixWrite = 0;

//const char topic_sub[] = "arduino/irrigation/should/#";
const char topic_sub[6][32]  = { "arduino/irrigation/should/1", // mqtt.0.arduino.irrigation.should.1 @iobroker
                                 "arduino/irrigation/should/2",
                                 "arduino/irrigation/should/3",
                                 "arduino/irrigation/should/4",
                                 "arduino/irrigation/should/5",
                                 "arduino/irrigation/should/6" };
const char topic_pub[6][32]  = { "arduino/irrigation/is/1",
                                 "arduino/irrigation/is/2",
                                 "arduino/irrigation/is/3",
                                 "arduino/irrigation/is/4",
                                 "arduino/irrigation/is/5",
                                 "arduino/irrigation/is/6" };

const char topic_pub_rssi[]  = "arduino/irrigation/info/rssi";

void setup();
void loop();
void allRelaisOff();
void sendInfoToMatrix(const char* txt);
void resetMatrix(bool force);
void connectMQTT();
void onMqttMessage(int messageSize);
void publishValues();
void publishStats();


void setup() { 
  // initialize digital pins
  // PIN 8 to 12 (5x output for solenoid valves)
  // PIN 13 for the pump (with led diagnostics onboard)
  pinMode(PIN_D8, OUTPUT);
  pinMode(PIN_D9, OUTPUT);
  pinMode(PIN_D10, OUTPUT);
  pinMode(PIN_D11, OUTPUT);
  pinMode(PIN_D12, OUTPUT);
  pinMode(PIN_D13, OUTPUT);

  // 5V => Relais and LED OFF
  allRelaisOff();

  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  matrix.begin();
  matrix.loadFrame(matrix_happy);

  connectMQTT();

  // set the watchdog timer to 2 seconds
  if(WDT.begin(2000)) {
    Serial.print("WDT interval: ");
    WDT.refresh();
    Serial.print(WDT.getTimeout());
    WDT.refresh();
    Serial.println(" ms");
    WDT.refresh();
  } else {
    Serial.println("Error initializing watchdog");
  }
}

void loop() {
  static int counter = 0;
  resetMatrix(false);
 
  counter = 0;
  while (!mqttClient.connected()){
    allRelaisOff();

    Serial.println("MQTT connection lost, trying reconnect.");
    connectMQTT();
    if (++counter > 30) {
      Serial.println("MQTT connection lost, retried for 30 times. Now I'm just waiting to die.");
      while(1); // wait for watchdog to make hard reset
    }
  } 
  
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // send messagess
  publishValues();

  // send stats
  publishStats();

  //Serial.print("Loop done: ");
  //Serial.println(millis());

  WDT.refresh();
}

void allRelaisOff() {
  digitalWrite(PIN_D8, 1);
  digitalWrite(PIN_D9, 1);
  digitalWrite(PIN_D10, 1);
  digitalWrite(PIN_D11, 1);
  digitalWrite(PIN_D12, 1);
  digitalWrite(PIN_D13, 1);
}


void sendInfoToMatrix(const char* txt) {
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(txt);
  matrix.endText(SCROLL_LEFT);

  lastMatrixWrite = millis();

  Serial.print("Writing \"");
  Serial.print(txt);
  Serial.println("\" to LED Matrix.");
}

// reset (last) error on matrix display after 30 min
void resetMatrix(bool force = false) {
  static const unsigned long RESET_INTERVAL = 30*60*1000; // 30 min
	static unsigned long currTime;
  static bool loadFrame;

  loadFrame = false;

  if (!force && lastMatrixWrite > 0) {
    currTime = millis();  
    if (currTime >= (lastMatrixWrite + RESET_INTERVAL)) {
      loadFrame = true;
      lastMatrixWrite = 0;
    }
  }

  if (force || loadFrame) {
    matrix.loadFrame(matrix_happy);
    Serial.println("Resetting LED Matrix to happy.");
  }
}

  
void connectMQTT(){
   // attempt to connect to WiFi network:
  Serial.print("Attempting to (re-) connect to WPA SSID: ");
  Serial.println(ssid);
  Serial.print("Before disconnect: WiFi.status() == ");
  Serial.println(WiFi.status());

  WiFi.disconnect();
  mqttClient.stop();

  Serial.print("After disconnect: WiFi.status() == ");
  Serial.println(WiFi.status());

  sendInfoToMatrix("?WiFi");
  do {
    WiFi.begin(ssid, pass);

    Serial.print(".");

    delay(2000);

  } while (WiFi.status() != WL_CONNECTED);
  resetMatrix(true);

  Serial.println("You're connected to the network");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Remote IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.println();

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("arduino-irrigation");

  // Behavior: The broker discards any previous session state upon connection and creates a brand new one.
  //    When the client disconnects, the session is completely destroyed.
  // Impact: The client must re-subscribe to its topics every time it connects.
  //    Any messages sent with QoS 1 or 2 while the client is offline are lost.
  mqttClient.setCleanSession(true); 

  // keepalive ist 60 secs by default
  // connection timeout is 30 secs by default

  // You can provide a username and password for authentication
  // mqttClient.setUsernamePassword("username", "password");

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  while (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    sendInfoToMatrix("!MQTT");

    delay(2000);
  }
  resetMatrix(true);
 
  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  // subscribe to topics
  Serial.println("Subscribing to topic(s)...:");
  for (int i = 0; i < 6; i++) {
      Serial.print(topic_sub[i]);
      if (mqttClient.subscribe(topic_sub[i], 2) == 1)
        Serial.println(" successful.");
      else
        Serial.println(" failed.");
  }
  Serial.println();
}


void onMqttMessage(int messageSize) {
    static int ionr = 0;
    static arduino::String str;

    str = mqttClient.messageTopic();

    Serial.print("Received a message with content '");   
    Serial.print(str);
    Serial.print("', length ");
    Serial.print(messageSize);
    Serial.println(" command:");

    ionr = ((int) str.charAt(str.length()-1)) - 48; // "arduino/irrigation/should/2" ==> 2
    if (ionr < 1 || ionr > 6)
    {
      Serial.print(ionr);
      Serial.println( "is somehow wrong. [1;6] allowed. Skipping message");
      
      // throw away the rest!
      while(mqttClient.available())
        mqttClient.read();
    }

    // use the Stream interface to print the contents
    if (mqttClient.available()) {
      // read first byte
      int i = mqttClient.read();
      if (i == -1 ) {
        Serial.println("Empty message received. Ignoring.");
        // debug.. => check retur of read() most likey -1 is file closed
      } else {
        Serial.print("Write OUTPUT ");
        Serial.print(ionr);

        if (((char) i) == '1') {
          digitalWrite(7+ionr, 1);   // pins 8 to 13. ionr is 1 to 6. => ionr 1 == GPIO_PIN8
          Serial.println(" to HIGH");
        } else {
          digitalWrite(7+ionr, 0);   // pins 8 to 13. ionr is 1 to 6. => ionr 1 == GPIO_PIN8
          Serial.println(" to LOW");
        }      

        // throw away the rest (if existing)
        while(mqttClient.available())
          mqttClient.read();
      }
    }
    Serial.println();
}


void publishValues()
{
	static const unsigned long REFRESH_INTERVAL = 2 * 1000; // 2 sek
	static unsigned long lastRefreshTime = 0;
  static unsigned long currTime;
  
  currTime = millis();
	
	if(currTime - lastRefreshTime >= REFRESH_INTERVAL)
	{
		lastRefreshTime = currTime;

    Serial.print ("Publishing values (time: ");
    Serial.print(currTime);
    Serial.print("):");

    for (int i = 0; i < 6; i++) {
      mqttClient.beginMessage(topic_pub[i]);  // creates a new message to be published.

      if (digitalRead (8+i)) {
        Serial.print (" 1");
        mqttClient.print("1");                // prints the content of message between the ().
      } else {
        Serial.print (" 0");
        mqttClient.print("0");
      }

      mqttClient.endMessage();                // publishes the message to the broker.
    }

    Serial.println();
	}
}


void publishStats() {
	static const unsigned long REFRESH_INTERVAL = 30 * 1000; // 30 sek
	static unsigned long lastRefreshTime = 0;
  static unsigned long currTime = 0;
  static int32_t rssi = 0;
  
  currTime = millis();
	
	if(currTime - lastRefreshTime >= REFRESH_INTERVAL)
	{
		lastRefreshTime = currTime;
    rssi = WiFi.RSSI();

    Serial.print("Publishing stats (time: ");
    Serial.print(currTime);
    Serial.println("):");
    Serial.print("\tRSSI: ");
    Serial.println(rssi, 10);

    mqttClient.beginMessage(topic_pub_rssi);
    mqttClient.print(rssi, 10);
    mqttClient.endMessage();
	}
}