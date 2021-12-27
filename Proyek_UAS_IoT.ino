#include "DHT.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
 
#define WIFI_AP "SetiawanT"
#define WIFI_PASSWORD "idet5525"
#define TOKEN "ProyekIoT"

char thingsboardServer[] = "192.168.1.101";

WiFiClient wifiClient;
 
int status = WL_IDLE_STATUS;
 
PubSubClient client(wifiClient);
 
boolean gpioStatus = false;

#define LEDGREENPIN 33
#define LEDREDPIN 25

// DHT
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//LDR
#define LIGHT_SENSOR_PIN 34

//ultrasonic
const int trigPin = 5;
const int echoPin = 18;

//define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

long duration;
float distanceCm;
float distanceInch;

//button
#define BTNPIN 27

unsigned long lastSend;

//-----------------------------------------------------------------------------------------------------
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  dht.begin();
  delay(10);
  pinMode(LEDGREENPIN,OUTPUT);
  digitalWrite(LEDGREENPIN,LOW);
  pinMode(LEDREDPIN,OUTPUT);
  digitalWrite(LEDREDPIN,HIGH);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); 
  
  InitWiFi();
  client.setServer(thingsboardServer,1883);
  lastSend = 0;
}

//-----------------------------------------------------------------------------------------------------
void loop() {
  // put your main code here, to run repeatedly:
  if(!client.connected()){
        reconnect();
    }
    client.setCallback(callback);
    buttonPress();
    
    if(gpioStatus == true)
    {
      if ( millis() - lastSend > 1000 ) { // Update and send only after 1 seconds
        getAndSendLDRData();
        getAndSendUltrasonicData();
        getAndSendTemperatureAndHumidityData();
        lastSend = millis();
      }
    }
    
    client.loop();
}

//-----------------------------------------------------------------------------------------------------
//LDR
void getAndSendLDRData()
{
  Serial.println("Collecting light data.");
  
  // reads the input on analog pin (value between 0 and 4095)
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  String state;
  
  Serial.print("Light Value = ");
  Serial.print(analogValue);   // the raw analog reading
  Serial.print(" %\t");
  Serial.print("State = ");
  // We'll have a few threshholds, qualitatively determined
  if (analogValue < 40) {
    state = "Dark";
  } else if (analogValue < 800) {
    state = "Dim";
  } else if (analogValue < 2000) {
    state = "Light";
  } else if (analogValue < 3200) {
    state = "Bright";
  } else {
    state = "Very bright";
  }
  Serial.println(state);

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"light_value\":"; payload += analogValue;
  payload += "}";
  
  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "v1/devices/me/telemetry", attributes );
}

//-----------------------------------------------------------------------------------------------------
//Ultrasonic Sensor
void getAndSendUltrasonicData()
{
  Serial.println("Collecting distance data.");

  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;
  
  // Convert to inches
  distanceInch = distanceCm * CM_TO_INCH;
  
  // Prints the distance in the Serial Monitor
  Serial.print("Distance (cm): ");
  Serial.print(distanceCm);
  Serial.print(" %\t");
  Serial.print("Distance (inch): ");
  Serial.println(distanceInch);
  
  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"distance_cm\":"; payload += distanceCm; payload += ",";
  payload += "\"distance_inch\":"; payload += distanceInch;
  payload += "}";
  
  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "v1/devices/me/telemetry", attributes );
}

//-----------------------------------------------------------------------------------------------------
//DHT 11
void getAndSendTemperatureAndHumidityData()
{
    Serial.println("Collecting temperature data.");
    
    // Reading temperature or humidity takes about 250 milliseconds!
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    
    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
    }
    
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println(" *C ");
    
    String temperature = String(t);
    String humidity = String(h);
    
    // Prepare a JSON payload string
    String payload = "{";
    payload += "\"temperature\":"; payload += temperature; payload += ",";
    payload += "\"humidity\":"; payload += humidity;
    payload += "}";
    
    // Send payload
    char attributes[100];
    payload.toCharArray( attributes, 100 );
    client.publish( "v1/devices/me/telemetry", attributes );
}

//-----------------------------------------------------------------------------------------------------
void buttonPress()
{
  int Push_button_state = digitalRead(BTNPIN);

  if ( Push_button_state == HIGH )
  { 
      if(gpioStatus==true){
            digitalWrite(LEDGREENPIN,LOW);
            digitalWrite(LEDREDPIN,HIGH);
            gpioStatus=false;
            }
      else{
            digitalWrite(LEDGREENPIN,HIGH);
            digitalWrite(LEDREDPIN,LOW);
            gpioStatus=true;
          } 
      delay(300);
      client.publish("v1/devices/me/attributes", get_gpio_status().c_str()); 
  }
  delay(200);
}

//-----------------------------------------------------------------------------------------------------
void callback(const char* topic, byte* payload, unsigned int length){
    //Serial.println("On message");
    char json[length + 1];
    strncpy (json, (char*)payload, length);
    json[length] = '\0';
  
    Serial.println("Topic: ");
    Serial.println(topic);
    
    Serial.println("Message: ");
    
    Serial.println(json);
    
    StaticJsonBuffer<200> jsonBuffer;
    
    JsonObject  &data = jsonBuffer.parseObject((char*)json);
    
    if (!data.success()){
    Serial.println("parseObject() failed");
    return;
    }
    String methodName = String((const char*)data["method"]);
    if(methodName.equals("Sensor")){
      if(data["params"]==true){
          digitalWrite(LEDGREENPIN,HIGH);
          digitalWrite(LEDREDPIN,LOW);
          gpioStatus=true;
          }
      else{
          digitalWrite(LEDGREENPIN,LOW);
          digitalWrite(LEDREDPIN,HIGH);
          gpioStatus=false;
          }
    }
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str()); 
 }

//-----------------------------------------------------------------------------------------------------
void reconnect(){
// Loop until we're reconnected
  while (!client.connected()){
    status = WiFi.status();
    if( status != WL_CONNECTED){
        WiFi.begin(WIFI_AP, WIFI_PASSWORD);
        while (WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
      }
    Serial.println("Connected to AP");
    }
  Serial.print("Connecting to Thingsboard node ...");
  
// Attempt to connect (clientId, username, password)
  if(client.connect("ESP32 Device",TOKEN,NULL)){
    Serial.println( "[DONE]" );
    client.subscribe("v1/devices/me/rpc/request/+");
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
    } 
  else{
    Serial.print( "[FAILED] [ rc = " );
    Serial.print( client.state());
    Serial.println(" : retrying in 5 seconds]");
    // Wait 5 seconds before retrying
    delay( 5000 );
    }
  }
}

//-----------------------------------------------------------------------------------------------------
String get_gpio_status(){
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  
  JsonObject& data = jsonBuffer.createObject();
  
  data["Sensor"]=gpioStatus ? true : false;
  
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.println("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

//-----------------------------------------------------------------------------------------------------
void InitWiFi(){
  Serial.println("Connecting to AP ...");
// attempt to connect to WiFi network
  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
    }
  Serial.println("Connected to AP");
}
