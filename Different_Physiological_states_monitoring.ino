#include <Wire.h>
#include <WiFi.h>
#include <SHT21.h>
#include <Adafruit_Sensor.h>
#include "time.h"
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>
SHT21 sht;
bool ECGisON = false;
bool old_BTN_state;

// Insert Firebase project API Key
#define API_KEY "AIzaSyAv4xg6FeU-n1PCmfqLy91l6q3q3l3MMw4"

//Insert the host and authentication fot the realtime database
#define FIREBASE_HOST "dailyroutineplanner-1645e-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "HTuTh5Rf2L26riiKdepNbRH3tUfHSt3ILgkpIKti"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "ua3agw@stud.uni-obuda.hu"
#define USER_PASSWORD "777316125@"

//Insert the Wifi ssid and the passward of the provided network
#define WIFI_SSID "Redmi Note 9"
#define WIFI_PASSWORD "Yemen7777"

/////////////////////////////////////////////////////////////////
//initializing the parameters for sending the data
// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String tempPath = "/temperature";
String humPath = "/humidity";
String pulsePath = "/heart_rate";
String oxiPath = "/oxygen_level";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;

int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

/**************************************************************************/
#define BTN 16
#define LED1 17
#define LED2 18
#define LED3 19

#include "MAX30100_PulseOximeter.h"
#define REPORTING_PERIOD_MS 1000
#define LO2 25
#define LO1 26
#define ECG 15
PulseOximeter pox;
uint32_t tsLastReport = 0;
float temp;
float humidity;
uint32_t delayingTime = 300;
uint32_t delayingTime1 = 750;
uint32_t timeRunning = 0;
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 5000;
 String firetemp=String("");
 String fireOxi =String("");
 String fireheart=String("");
 String firehum=String("");

void onBeatDetected()
{
  digitalWrite(LED1,!digitalRead(LED1));
}
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}
void ECG_monitor() {
  if ((digitalRead(25) == 1) || (digitalRead(26) == 1)) {
    Serial.println('!');
  }
  else {
    Serial.println(analogRead(ECG));
  }
  //Wait for a bit to keep serial data from saturating
  delay(1);
}
void temp_and_humi() {
  temp = sht.getTemperature();  // get temp from SHT
  humidity = sht.getHumidity(); // get temp from SHT
  Serial.print("Temp: ");      // printing the readings to the serial output
  Serial.print(temp);
  Serial.print("\t Humidity: ");
  Serial.println(humidity);
  firetemp = String(temp) + String("°C");
  firehum = String(humidity) + String("%");
  pox.update();
}

void pulse_oximeter() {
  pox.update();
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    Serial.print("Heart rate:");
    Serial.print(pox.getHeartRate());
    Serial.print("bpm / SpO2:");
    Serial.print(pox.getSpO2());
    Serial.println("%");
    fireheart = String(pox.getHeartRate()) + String("bpm");
    fireOxi = String ("SpO2:") + String(pox.getSpO2()) + String("%");
    tsLastReport = millis();
  }
}

void connect_to_Wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - timeRunning > delayingTime) {
      timeRunning = millis();
      Serial.print("!.");
    }
  }
  Serial.println();
  Serial.print("Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());  //prints local IP address
  digitalWrite(LED2,!digitalRead(LED2));
}

void Sending_new_readings(){
// Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    //Get current timestamp
    timestamp = getTime();
    Serial.print ("time: ");
    Serial.println (timestamp);
    //Creating a p
    parentPath= databasePath + "/" + String(timestamp);

    json.set(tempPath.c_str(), firetemp );
    json.set(humPath.c_str(), firehum);
    json.set(pulsePath.c_str(), fireheart);
    json.set(oxiPath.c_str(), fireOxi);
    json.set(timePath, String(timestamp));
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  }
}

TaskHandle_t Task1;
TaskHandle_t Task2;

void setup() {
  Serial.begin(115200); 
  pinMode(LO1 , INPUT); // Setup THE leads off detection LO -
  pinMode(LO2 , INPUT); // Setup THE leads off detection LO +
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  pinMode(BTN, INPUT);
  old_BTN_state = digitalRead(BTN);
    timeRunning = millis();
  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                   Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                   10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                   1,           /* priority of the task */
                   &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
 xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                   10000,       /* Stack size of task */
                   NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                   &Task2,      /* Task handle to keep track of created task */
                    1
                    );          /* pin task to core 1 */
   delay(500); 
}
void Task1code( void * pvParameters ){
Serial.print("Initializing pulse oximeter..");
  // Initialize the PulseOximeter instance
  // Failures are generally due to an improper I2C wiring, missing power supply
  // or wrong target chip
  if (!pox.begin()) {
    Serial.println("FAILED");
    //for (;;);
  } else {
    Serial.println("SUCCESS");
  }
  pox.update();
  pox.setOnBeatDetectedCallback(onBeatDetected);
  for(;;){
      bool new_state = digitalRead(BTN);
    if (ECGisON == false) {
    pulse_oximeter();
    if (millis() - timeRunning > delayingTime1) {
        pox.update();
         temp_and_humi();
      timeRunning = millis();
    }
  } 
}
}
//Task2code: blinks an LED every 700 ms
void Task2code( void * pvParameters ){
    connect_to_Wifi();
  // Assign the api key (required)
  config.api_key = API_KEY;
    // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  // Assign the RTDB URL (required)
  config.database_url = FIREBASE_HOST;


  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);
  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    if (millis() - timeRunning > (delayingTime+250)) {
      timeRunning = millis();
      Serial.print('#');
    }
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";

  for(;;){
  bool new_state = digitalRead(BTN);
  if (digitalRead(BTN) != old_BTN_state) {
    old_BTN_state = new_state;
    if (LOW == new_state) {
      ECGisON = !ECGisON;
      Serial.println("Button is pressed now, To stop the ecg monitoring press it again");
    }
  }
  if (ECGisON == true) {
    ECG_monitor();
  }
   Sending_new_readings();
  }
  
}


void loop() {
}