

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
#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <Narcoleptic.h>

//Pin Assignment
#define DHTPIN 2
#define SDPIN  4

//Configuration
#define DHTTYPE DHT22

//Global variables

int mode;
DHT dht(DHTPIN, DHTTYPE);
byte mac[] = {   0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char server[] = "infocology.co.uk"; // Google
char auth_name[] = "token";
char auth_key[]  = "$1$jdgjNCEX$.4kM0/YL4a97jn579QDl60";
String request = "";

EthernetClient client;



//Function Prototypes
void(* resetFunc) (void) = 0;  //soft reset


void setup() {
  Serial.begin(9600); 
  while(!Serial){
  //  ;
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
      //Serial.println(sd_check);
      delay(2000);
      if (sd_check == false) {
        /*error error = {
          3, 1        };
        //error_condition(error); */
      }
    }
    if (sd_check == false) {
      /*error error = {
        3, 2      };
      error_condition(error);*/
    }
  }



  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    //Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  //Serial.print("MY IP is ");
  //Serial.println(Ethernet.localIP());
  //Serial.println("connecting...");




  //Start DHT 
  dht.begin();

}

void loop() {
  lta data = lta_get_data();
  data_post(data);
  error_blink(1);
  Serial.println("Starting sleep.");
  Narcoleptic.delay(6000);
  Serial.println("Ending sleep.");
}

int mode_select() {
  return 1; 
}

boolean sd_post(lta data) {
  //Generate the data string
  char humidity[6];
  char temperature[6];
  dtostrf(data.humidity, 1, 2, humidity);
  dtostrf(data.temperature, 1, 2, temperature);
  request.concat(data.time);
  request += ",";
  request +=  humidity;
  request += ",";
  request +=  temperature;
  request += ",";
  //request +=  data.ethernet;
  //request += ",";
  //Serial.println();
  //Serial.println();
  //Serial.println(request);

  //Append to file
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(request);
    dataFile.close();
    //Serial.println("Data saved to data.txt");
    return true;
  } 
  else {
    //Serial.println("Failed to open data.txt");
   /* error error = {
      4, 1    };
    error_condition(error);*/
    return false;
  }

  return false; 
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


  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    //Serial.println("connected");
    // Make a HTTP request:

    request = "";
    request += "GET ";
    request += "/other/drupal7/arduino/post?";

    request += auth_name;
    request += "=";
    request += auth_key;

    char humidity[6];
    char temperature[6];
    dtostrf(data.humidity, 1, 2, humidity);
    dtostrf(data.temperature, 1, 2, temperature);


    request += "&field_temp=";
    request += temperature;

    request += "&field_humidity=";
    request += humidity;
    
    request += "&field_time=";
    request += data.time; 

    request += " HTTP/1.0";

    //Serial.print("Requesting: ");
   // Serial.println(request);

    client.println(request);
    client.println ("Host: infocology.co.uk");
    //client.println("Connection: close");
    client.println();
  } 
  else {
    // kf you didn't get a connection to the server:
   // Serial.println("connection failed");
    return false;
  }
  client.stop();
  return true;
}

boolean init_sd() {
  //Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(SDPIN)) {
    //Serial.println("Card failed, or not present");
    // don't do anything more:
    return false;
  }
  //Serial.println("card initialized.");  
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
    //Serial.println("Failed to read from DHT");
    /*error error =  {
      1, 1    };
    error_condition(error);*/
    fail = true;
  } 
  else {
   // Serial.print("Humidity: "); 
    //Serial.print(h);
    //Serial.print(" %\t");
    //Serial.print("Temperature: "); 
   // Serial.print(t);
   // Serial.println(" *C");
    fail = false;
  }
  lta data = {
    RTC.get(),
    h,
    t
  };
  return data;

}


















/*
void error_condition(error error) {
  //Serial.println("Error condition encountered:");
  //Serial.println(error.condition);

  //Log error to SD card
  request = "";
  request += error.condition;
  request += ",";
  request += error.level;

  File errorFile = SD.open("error.log", FILE_WRITE);
  if (errorFile) {
    errorFile.println(request);
    errorFile.close();
  }

}*/

void error_blink(int repeat) {
  pinMode(9, OUTPUT);
  for (int i =0 ; i < repeat; i++) {
    digitalWrite(9, HIGH);
    delay(200);
    digitalWrite(9, LOW);
    delay(200);
  }
}

