/*IOT project:: This code is a part of big project in which data being measured is sent to an outer website
designed by the other coworkers*/
/*Project_Name:: Daily routine planner*/
/*Author:: Abd-Alfatah Alodainy*/
/*E-mail:: ua3agw@stud.uni-obuda.hu*/
/* This program is used to Run an Arduino based device in which 3  sensors are connected to a  microcontroller
to measure the different human physiological states (HR, TEMP, HUMIDITY, O2 level and support the ECG function) */

/*Necessary libraries */
#include <Wire.h>
#include <WiFiManager.h>
#include <SHT21.h>
#include <Adafruit_Sensor.h>
#include "time.h"
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>
//Define SHT21 SENSOR objects
SHT21 sht;
// Key of firebase project API
#define API_KEY "AIzaSyAv4xg6FeU-n1PCmfqLy91l6q3q3l3MMw4"

//Host and authentication foR the realtime database
#define FIREBASE_HOST "dailyroutineplanner-1645e-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "HTuTh5Rf2L26riiKdepNbRH3tUfHSt3ILgkpIKti"

// Authorized Email and Corresponding Password for the user in the you entered in firebase
#define USER_EMAIL "ua3agw@stud.uni-obuda.hu"
#define USER_PASSWORD "777316125@"

/////////////////////////////////////////////////////////////////
//initializing the parameters for sending the data
// Define Firebase objects, like data and authentication 
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID ::UID is a unique carchecters that is automatically made by firebase for every user
String uid;

// Database main path (it is being updated in setup with the user UID)
String databasePath;
// Database child nodes
String tempPath = "/temperature";
String humPath = "/humidity";
String pulsePath = "/heart_rate";
String oxiPath = "/oxygen_level";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;
//declaration of time stamp of the Arduino running time:::: to be used in case of continous monitoring  
int timestamp;
//Firebase json file initializing 
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

/***************************************************************************/
//LEDs ot show different opperation signs
#define BTN 16
#define LED2 17 // if it is ON then Wifi is connected
#define LED3 18 // if it is blinking then data is being sent
#define LED1 19 //if it is blinking then hear rate is being measured 
#define LED4 4 // if it is ON then ECG is connected
/**************************************************************************/
// TIME OF HEART RATE SENISNG
#define REPORTING_PERIOD_MS 1000
//initializing the pulse_oximeter senosr 
#include "MAX30100_PulseOximeter.h"
PulseOximeter pox;
uint32_t tsLastReport = 0;
////initializing the temprature varilable
float temp;
float humidity;
//Non blocking delays variable
uint16_t delayingTime = 300;
uint16_t delayingTime1 = 750;
uint16_t timeRunning = 0;
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 5000;
//initializing the strings for the firebase path variable 
String firetemp=String("");
String fireOxi =String("");
String fireheart=String("");
String firehum=String("");
//blinking and updating the HR in case of sensing any beats from huamn finger
void onBeatDetected()
{
  pox.update();
  digitalWrite(LED1,!digitalRead(LED1));
}
//getting the timestamp of the Arduino
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}
// temp and hum measuring
void temp_and_humi() {
  if (millis() - timeRunning > 85){
  temp = sht.getTemperature();  // get temp from SHT
  humidity = sht.getHumidity(); // get temp from SHT
 /* Serial.print("Temp: ");      // printing the readings to the serial output
  Serial.print(temp);
  Serial.print("\t Humidity: ");
  Serial.println(humidity);*/
  firetemp = String(temp) + String("°C");
  firehum = String(humidity) + String("%");
  }
  pox.update();
}
// HR and O2 measuring 
void pulse_oximeter() {
  pox.update();
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    // printing the readings to the serial monitor
   /* Serial.print("Heart rate:");
    Serial.print(pox.getHeartRate());
    Serial.print("bpm / SpO2:");
    Serial.print(pox.getSpO2());
    Serial.println("%");*/
    fireheart = String(pox.getHeartRate()) + String("bpm");
    fireOxi = String ("SpO2:") + String(pox.getSpO2()) + String("%");
    tsLastReport = millis();
  }
}
//WiFi connection 
void connect_to_Wifi() {
      WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("ESP32","AbdAlfatah"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart(); //This can be used in case we want to restart the device each time the connection failed
    } 
    else {
        //here it means that we are connected to the WiFi    
        Serial.println("connected...yeey :)");
        digitalWrite( LED2, !digitalRead(LED2));//this LED  will tell us whehter the device is connected to wifi or not
        //if the LED is ON then it is connected  
    }
}
//Data transfere from the controlleres to the firebase
void Sending_new_readings(){
// Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    //Get current timestamp
    timestamp = getTime();
    /*Serial.print ("time: ");
    Serial.println (timestamp);*/ //print the current time stamp to the serial monitor
    //Creating path in the database for data sending
    parentPath= databasePath + "/" + String(timestamp);

    json.set(tempPath.c_str(), firetemp );
    json.set(humPath.c_str(), firehum);
    json.set(pulsePath.c_str(), fireheart);
    json.set(oxiPath.c_str(), fireOxi);
    json.set(timePath, String(timestamp));
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    digitalWrite(LED3,!digitalRead(LED3)); //THIS led will blink in case of data being sent
  }
}

TaskHandle_t Task1;
TaskHandle_t Task2;

void setup() {
  Serial.begin(115200); 
  /*pinMode(LO1 , INPUT); // Setup THE leads off detection LO -
  pinMode(LO2 , INPUT); // Setup THE leads off detection LO +*/
  pinMode(LED1,OUTPUT);  //LEDs setup
  pinMode(LED2,OUTPUT);
  pinMode(LED3,OUTPUT);
  pinMode(LED4,OUTPUT);
  pinMode(BTN, INPUT);
  timeRunning = millis();
  //This task will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                   Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                   10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                   1,           /* priority of the task */
                   &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //This task will be executed in the Task2code() function, with priority 1 and executed on core 1
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
//task 1 code (In core 0 we will run the all sensors)
void Task1code( void * pvParameters ){
  //Serial printing
//Serial.print("Initializing pulse oximeter..");
  // Initialize the PulseOximeter instance
  // Failures are generally due to an improper I2C wiring, missing power supply
  // or wrong target chip
  if (!pox.begin()) {
    Serial.println("FAILED");
  } else {
    Serial.println("SUCCESS");
  }
  pox.update();
  pox.setOnBeatDetectedCallback(onBeatDetected);
  for(;;){
    pulse_oximeter();
    if (millis() - timeRunning > delayingTime1) {
        pox.update();
        temp_and_humi();
      timeRunning = millis();
  } 
}
}
//Task2code: Connect to wifi and send the data to the firebase

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
 // Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    if (millis() - timeRunning > (delayingTime+250)) {
      timeRunning = millis();
      //Serial.print('#');
    }
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  /*Serial.print("User UID: ");
  Serial.println(uid);*/

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";

  for(;;){

   Sending_new_readings();

  }
}


void loop() {
  /* Here should be empty because we used the dual core feature of the ESP32 in the setup
  and there is not any need for further code here
  
  */
}
/* I have commented all the serial printing for testing purpose you have to uncomment all of them*/
