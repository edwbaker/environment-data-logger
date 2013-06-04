//Light Trap Augmentor

//TODO: RTC circuit
//      - timestamps of readings
//      - timestamps of errors
//TODO: Time of creation of SD files

//Libraries
#include "DHT.h"
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include "lta_Struct.h"

//Pin Assignment
#define DHTPIN 2
#define SDPIN  4

//Configuration
#define DHTTYPE DHT22

//Global variables
int mode;
DHT dht(DHTPIN, DHTTYPE);
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "www.google.fail"; // Google

//Function Prototypes
void(* resetFunc) (void) = 0;  //soft reset


void setup() {
  Serial.begin(9600); 
  while(!Serial){
    ;
  }
  
  //Choose run mode
  mode = mode_select();
  
  //Check for SD card
  if (mode == 2 || mode == 3 || mode == 4) { 
    int i=0;
    boolean sd_check = false;
    while (sd_check == false && i < 10) {
      i++;
      sd_check = init_sd();
      Serial.println(sd_check);
      delay(2000);
      if (sd_check == false) {
        error error = {3, 1};
        error_condition(error); 
      }
    }
    if (sd_check == false) {
     error error = {3, 2};
     error_condition(error);
    }
  }
  
  //Start DHT 
  dht.begin();
  

}

void loop() {
  lta data = lta_get_data();
  if (!data.fail) {
    delay(1700);
    data_post(data);
  }
}

int mode_select() {
 //   1 - Ethernet only
 //   2 - SD card only
 //   3 - Try Ethernet, SD if fail
 //   4 - Ethernet and SD card always
 return 4; 
}

boolean sd_post(lta data) {
 //Generate the data string
 char humidity[6];
 char temperature[6];
 dtostrf(data.humidity, 1, 2, humidity);
 String dataString = "";
 dataString +=  humidity;
 dataString += ",";
 dataString +=  temperature;
 dataString += ",";
 dataString +=  data.ethernet;
 dataString += ",";
 dataString +=  data.fail;
 dataString += ",";
 
 //Append to file
 File dataFile = SD.open("data.txt", FILE_WRITE);
 if (dataFile) {
  dataFile.println(dataString);
  dataFile.close();
  Serial.println("Data saved to data.txt");
  return true;
 } else {
  Serial.println("Failed to open data.txt");
  error error = {4, 1};
  error_condition(error);
  return false;
 }
 
 return false; 
}

boolean data_post(lta data) {
  //First - attempt to post to internet
  boolean ethernet =  false;
  if (mode == 1 || mode == 3 || mode == 4) {
    data.ethernet = true;

    int j = 0;
    while (ethernet == false && j < 5) {
      ethernet = ethernet_post(data);
      j++;
    }
    if (!ethernet) {
      data.ethernet = false; 
    }
  }
  //Then SD card 
  if (mode == 2 || (mode == 3 && !ethernet) || mode == 4) {
    data.ethernet = false;
    boolean sd = false;
    int i = 0;
    while (sd == false && i < 5) {
      sd = sd_post(data);
      i++;
    }
  }
}

boolean ethernet_post(lta data) {
     //Indicate error condition
    error error = {2, 1};
    error_condition(error); 
 return false; //For debugging
 EthernetClient client; 
   // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.print("MY IP is ");
  Serial.println(Ethernet.localIP());
  Serial.println("connecting...");

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
  } 
  else {
    // kf you didn't get a connection to the server:
    Serial.println("connection failed");
    return false;
  }
  // if there are incoming bytes available 
  // from the server, read them and print them:
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }
  return true;
}

boolean init_sd() {
 Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(SDPIN)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return false;
  }
  Serial.println("card initialized.");  
  return true;
}

lta lta_get_data(){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  boolean ethernet = false;
  boolean fail = false;
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h) || (t == 0 && h == 0)) {
    Serial.println("Failed to read from DHT");
    error error =  {1, 1};
    error_condition(error);
    fail = true;
  } else {
    Serial.print("Humidity: "); 
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: "); 
    Serial.print(t);
    Serial.println(" *C");
    fail = false;
  }
  lta data = {
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
  if (error.condition == 1) {
    if (!isnan(error.level)) {
      error_blink_pair(error.level); 
    }
  }
  
  if (error.condition == 2) {
    if (!isnan(error.level)) {
      error_blink(error.level); 
    }
  }
  
  if (error.condition == 3) {
    if (error.level == 1) {
      error_blink(error.level);
    }
    if (error.level == 2) {
      error_fatal_reset();
    }
  }
  
  if (error.condition == 4) {
    if (error.level == 1) {
      error_blink(2);
    }
    if (error.level == 2) {
      error_fatal_reset();
    }
  }
  
  //Log error to SD card
  String errorString = "";
  errorString += error.condition;
  errorString += ",";
  errorString += error.level;
  
  File errorFile = SD.open("error.log", FILE_WRITE);
  if (errorFile) {
    errorFile.println(errorString);
    errorFile.close();
  }
 
}

void error_blink(int repeat) {
  pinMode(9, OUTPUT);
  for (int i =0 ; i < repeat; i++) {
   digitalWrite(9, HIGH);
   delay(200);
   digitalWrite(9, LOW);
   delay(200);
  }
}

void error_blink_pair(int repeat) {
  pinMode(9, OUTPUT);
  for (int i =0 ; i < repeat; i++) {
   digitalWrite(9, HIGH);
   delay(50);
   digitalWrite(9, LOW);
   delay(50);
   digitalWrite(9, HIGH);
   delay(50);
   digitalWrite(9, LOW);
   delay(200);
  }
}

void error_blink_fatal(){
  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);
  delay(3000);
}

void error_fatal_reset() {
  error_blink_fatal();
  resetFunc(); 
}


