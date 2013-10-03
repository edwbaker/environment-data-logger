//Ecological Data Logger
//======================
//
//Ed Baker (ebaker.me.uk)

//Libraries
#include "DHT.h"
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include "lta_Struct.h"
#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <Sleep_n0m1.h>

//DHT (Digital Humidity and Temperature Sensor)
#define DHTPIN 2
#define DHTTYPE DHT22

//SD Card
#define SDPIN  4

//Global variables
int mode;  //The mode that the device is running in

EthernetClient client;
byte mac[] = {   0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "infocology.co.uk"; // Google
char auth_name[] = "token";
char auth_key[]  = "$1$jdgjNCEX$.4kM0/YL4a97jn579QDl60";

DHT dht(DHTPIN, DHTTYPE);
Sleep sleep;

//Function Prototypes
void(* softReset) (void) = 0;  //soft reset


void setup() {
  //Serial connection
  Serial.begin(9600); 
  while(!Serial){
    ;
  }
  serial_boilerplate();
  
  //Choose run mode
  mode = mode_select();
  mode = 4; //TODO remove this when hardware switch enabled 

  //Check for SD card
  if (mode == 2 || mode == 3 || mode == 4) { 
    Serial.println("Initalising SD card...");
    //Pin 53 on mega must be output (10 on other baords)
    pinMode(53, OUTPUT);
    int i=0;
    boolean sd_check = false;
    while (sd_check == false && i < 10) {
      i++;
      sd_check = init_sd();
      delay(2000);
      if (sd_check == false) {
        String error_msg = ("SD card initialisation failed on attempt ");
        error_msg.concat(i);
        error error = {
          error_msg, 
          3 //WARNING
        };
        error_condition(error);
      }
    }
    if (sd_check == false) {
      int warning;
      if (mode == 2 || mode == 3) {
        warning = 1;  //FATAL IN THESE CASES
      } else {
        warning = 2;  //ERROR - USE OF ETHERNET IS EXPECTED
      }
      error error = {
        "SD card initialisation has failed.", 
        3 //WARNING
      };
      error_condition(error);
      Serial.println("SD card initalisation has failed.");
    }
  }

  // start the Ethernet connection:
  if (mode == 1 || mode == 3 || mode == 4) { 
    if (Ethernet.begin(mac) == 0) {
      String error_msg = "Failed to configure Ethernet using DHCP";
      int warning;
      if (mode == 1 || mode == 4) {
        warning = 1; //FATAL
      } else {
        warning = 2; //ERROR
      }
      error error = {
        error_msg,
        warning
      };
      error_condition(error);
    }
    // give the Ethernet shield a second to initialize:
    //delay(1000);
    Serial.print("MY IP is ");
    Serial.println(Ethernet.localIP());
  }

  //Start DHT
  Serial.println("Connecting to humidity and temperature sensor");
  dht.begin();
  int i =0;
  boolean connected = true;
  if (isnan(dht.readTemperature())) { connected = false; }
  if (isnan(dht.readHumidity())) { connected = false; }
  if (dht.readTemperature() == 0 && dht.readHumidity() == 0) { connected = false; }
  while (!connected) {
    i++;
    int warning;
    if (i < 10) {
      warning = 3;
    } else {
      warning = 1;
    }
    String error_msg = "Cannot connect to sensor. Try ";
    error_msg.concat(i);
    error error = {
      error_msg,
      warning
    };
    error_condition(error);
    connected = true;
    if (isnan(dht.readTemperature())) { connected = false; }
    if (isnan(dht.readHumidity())) { connected = false; }
    if (dht.readTemperature() == 0 && dht.readHumidity() == 0) { connected = false; }
  }
  Serial.println("Connected.");
  Serial.println();
  Serial.println("Starting program loop.");
}

void loop() {
  lta data = lta_get_data();
  data_post(data);
  error_blink(1); //Should move to data_post function
  Serial.println("Starting sleep.");
  delay(100);
  sleep.pwrDownMode();
  sleep.sleepDelay(600000);
  Serial.println("Ending sleep.");
}

int mode_select() {
  //There should be a hardware switch for this
  int mode = 1;
  String text;
  switch (mode) {
    case 1:
      text = "Ethernet only.";
      break;
    case 2:
      text = "SD card only.";
      break;
    case 3:
      text = "Ethernet if available, otherwise to SD card.";
      break;
    case 4:
      text = "Ethernet and SD card.";
      break;
    default:
      mode = 1;
      Serial.println("Unknown mode specified. Defaulting to mode 1.");
  }
  Serial.print("Mode ");
  Serial.print(mode);
  Serial.print(" selected: ");
  Serial.println(text);
  
  return mode; 
}

boolean sd_post(lta data) {
  //Generate new line of data
  char humidity[6];
  char temperature[6];
  dtostrf(data.humidity, 1, 2, humidity);
  dtostrf(data.temperature, 1, 2, temperature);
  String request;
  request.concat(data.time);
  request += ",";
  request +=  humidity;
  request += ",";
  request +=  temperature;
  request += ",";
  request +=  data.ethernet;
  request += ",";
  request += data.fail;

  //Append to file
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(request);
    dataFile.close();
    Serial.println("Data saved to data.txt");
    return true;
  } 
  else {
   error error = {
     "Failed to open data.txt.",
     2    };
    error_condition(error);
    return false;
  }
}

boolean data_post(lta data) {
  //First - attempt to post to internet
  boolean ethernet =  false;
  if (mode == 1 || mode == 3 || mode == 4) {
    //data.ethernet = true;

    int j = 0;
    while (ethernet == false && j < 5) {
      ethernet = ethernet_post(data);
      j++;
    }
    if (!ethernet) {
      //data.ethernet = false; 
    }
  }
  //Then SD card 
  if (mode == 2 || (mode == 3 && !ethernet) || mode == 4) {
    //data.ethernet = false;
    boolean sd = false;
    int i = 0;
    while (sd == false && i < 5) {
      sd = sd_post(data);
      i++;
    }
  }
}

boolean ethernet_post(lta data) {
  char humidity[6];
  char temperature[6];
  dtostrf(data.humidity, 1, 2, humidity);
  dtostrf(data.temperature, 1, 2, temperature);
  if (client.connect(server, 80)) {
    Serial.println("Connected to remote computer.");

    String request = "";
    request += "GET ";
    request += "/other/drupal7/arduino/post?";
    request += auth_name;
    request += "=";
    request += auth_key;
    request += "&field_temp=";
    request += temperature;
    request += "&field_humidity=";
    request += humidity;
    request += "&field_time=";
    request += data.time; 
    request += " HTTP/1.0";

   //Send request to remote computer
    client.println(request);
    client.println ("Host: infocology.co.uk");
    client.println();
  } 
  else {
    error error = {
      "Failed to make connection to remote computer.",
      2
    };
    error_condition(error);
    return false;
  }
  client.stop();
  return true;
}

boolean init_sd() {
  // make sure that the default chip select pin is set to output, even if you don't use it:
  pinMode(SDPIN, OUTPUT);
  // see if the card is present and can be initialized:
  if (!SD.begin(SDPIN)) {
    return false;
  } 
  return true;
}

lta lta_get_data(){
  uint32_t time     = RTC.get();
  float    h        = dht.readHumidity();
  float    t        = dht.readTemperature();
  boolean  ethernet = false;
  boolean  fail     = false;
  
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h) || (t == 0 && h == 0)) {
    String error_msg = "Failed to read from DHT";
    error error =  {
      error_msg, 
      2
    };
    error_condition(error);
    fail = true;
  } 
  lta data = {
    time,
    h,
    t,
    ethernet,
    fail
  };
  return data;
}

void error_condition(error error) {
  Serial.println("Error condition encountered:");
  Serial.println(error.condition);
  Serial.print("Error level ");
  Serial.println(error.level);

  //Log error to SD card
  String request = "";
  request += error.condition;
  request += ",";
  request += error.level;

  File errorFile = SD.open("error.log", FILE_WRITE);
  if (errorFile) {
    errorFile.println(request);
    errorFile.close();
  }
  
  //Respond to error condition as appropriate
  switch (error.level) {
    case 1:
      error_blink(5);
      softReset();
      break;
    case 2:
      error_blink(4);
      break;
    case 3:
      error_blink(3);
      break;
    case 4:
      error_blink(2);
      break;
  }
}

void error_blink(int repeat) {
  pinMode(9, OUTPUT);
  for (int i =0 ; i < repeat; i++) {
    digitalWrite(9, HIGH);
    delay(50);
    digitalWrite(9, LOW);
    delay(50);
  }
}

void serial_boilerplate() {
  Serial.println("Ecological Data Logger v0.1");
  Serial.println("===========================");
  Serial.println();
  Serial.println("Developed by Ed Baker: http://ebaker.me.uk");
}
