// include the library code:
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>


//initializes/defines the output pins for sensors and buttins
const int tempSensorAPin= 0;
const int boilerPowerPin = 19;
const int setPointDownPin = 17;
const int setPointUpPin = 16;

//Initialize the variables required for switch debouncing
boolean setPointUpCurrentState = LOW;
boolean setPointUpLastState = LOW;
boolean setPointUpDebouncedState = LOW;
boolean setPointDownCurrentState = LOW;
boolean setPointDownLastState = LOW;
boolean setPointDownDebouncedState = LOW;
int debounceInterval = 15;//wait 20 ms for button pin to settle
unsigned long timeOfLastButtonEventUp = 0;//store the last time the button state changed
unsigned long timeOfLastButtonEventDown = 0;

//Intialize variables required for temperature sensing
unsigned long timeOfLastTempCheck = 0;
int tempCheckInterval = 2000;
int heatCallCount = 5;
float setTemp = 12.0;
float currentTemp = 10.0;
boolean setPointLocalUpdate = false;
float lastSentTemp = 10.0;


// initialize the LCD library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// Initialize Ethernet Adapter
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
//  IPAddress ip(192,168,1,177);
EthernetClient client;

// Intialize ThingSpeak Settings
IPAddress server(184,106,153,149); // IP Address for the ThingSpeak API
String writeAPIKey = "7GH25707INIBKFUB";    // Write API Key for a ThingSpeak Channel - GlanworthThermo
const int writeUpdateInterval = 32 * 1000;        // Time interval in milliseconds to update ThingSpeak
const int readUpdateInterval = 11 * 1000;        // Time interval in milliseconds to read ThingSpeak

// Thing Speak Variable Setup
boolean isRead = false;
boolean capChar = false;
String JSONResponse = "";
int startChar = 0;
int endChar = 0;
String sSetTemp = "";
long lastWriteConnectionTime = 0;
long lastReadConnectionTime = 0;
boolean lastConnected = false;
int resetCounter = 0;


void setup()
{
  //set up digital pins
  pinMode(boilerPowerPin, OUTPUT);
  pinMode(setPointDownPin, INPUT);
  pinMode(setPointUpPin, INPUT);
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  lcd.print("Cur. Temp:");
  lcd.setCursor(0,1);
  lcd.print("Set Temp:");
  
   // start the Ethernet connection
   Ethernet.begin(mac);

   //Set the analog ref for tenperature sensing
   analogReference(INTERNAL);
 }

//main loop
void loop()
{
  unsigned long currentTime = millis();
  
  //Are we due a temperature check
  if(currentTime - timeOfLastTempCheck > tempCheckInterval){
    //Get temp and set resolution to 0.5 Degrees
    currentTemp = (int)((getTempCelcius(tempSensorAPin)*2.0)+0.5)/2.0;
    timeOfLastTempCheck = currentTime;
  }

  lcd.setCursor(10,0);
  lcd.print(currentTemp);

  lcd.setCursor(10,1);
  lcd.print(setTemp);

  //Work out if we should call for heat iorning out for odd values (need 5 in a row to actually call)
  if (currentTemp < setTemp && heatCallCount < 10)  {
    heatCallCount++;
  } else if ((currentTemp > (setTemp + 1.0)) && heatCallCount > 0) {
      heatCallCount--; 
  }

  if (heatCallCount > 9) {
    digitalWrite(boilerPowerPin, LOW);
  } else if (heatCallCount < 1){
    digitalWrite(boilerPowerPin, HIGH);
  }

  //Read set point up and down buttons with debounce
  //Temp Up Button
  setPointUpCurrentState = digitalRead(setPointUpPin);
  if(setPointUpCurrentState != setPointUpLastState){
    timeOfLastButtonEventUp = currentTime;
  }
  if (currentTime - timeOfLastButtonEventUp > debounceInterval){
    if (setPointUpCurrentState != setPointUpDebouncedState){
      setPointUpDebouncedState = setPointUpCurrentState;
      if (setPointUpDebouncedState == HIGH){
        setTemp = setTemp+0.5;  
        setPointLocalUpdate = true;
      }
    }
  }
  setPointUpLastState = setPointUpCurrentState;

  //Temp DOwn Button
  setPointDownCurrentState = digitalRead(setPointDownPin);
  if(setPointDownCurrentState != setPointDownLastState){
    timeOfLastButtonEventDown = currentTime;
  }
  if (currentTime - timeOfLastButtonEventDown > debounceInterval){
    if (setPointDownCurrentState != setPointDownDebouncedState){
      setPointDownDebouncedState = setPointDownCurrentState;
      if (setPointDownDebouncedState == HIGH){
        setTemp = setTemp-0.5;  
        setPointLocalUpdate = true;
      }
    }
  }
  setPointDownLastState = setPointDownCurrentState;
  //END of Buttin check code

  //Verfiy if we have a message from web connection and captue relevant JSON
  if (client.available()){
    char c = client.read();
    if (c == '[') {
      capChar = true;
    }
    if (capChar) JSONResponse += (c);
    if (c == ']') {
      capChar = false;
    }
  }

  // Disconnect from ThingSpeak
  if (!client.connected() && lastConnected){
    delay(1000);
    //If we read a set temp process it
    if(isRead){
      startChar = JSONResponse.indexOf("field2\":\"")+9;
      endChar = startChar + 5;
      sSetTemp = JSONResponse.substring(startChar, endChar);
      char carray[sSetTemp.length() + 1]; //determine size of the array
      sSetTemp.toCharArray(carray, sizeof(carray)); //put readStringinto an array
      setTemp = atof(carray); //convert the array into an Float 
      JSONResponse = "";
      isRead = false;
    }
    client.stop();
  }

  // read set point is delaty past and local update not happened
  if(!setPointLocalUpdate && !client.connected() && (millis() - lastReadConnectionTime > readUpdateInterval)){
    readThingSpeak();
  }

  
   // Update ThingSpeak if temperature or set point has changed since last update
  if(currentTemp != lastSentTemp || setPointLocalUpdate){
    if(!client.connected() && (millis() - lastWriteConnectionTime > writeUpdateInterval)){
      if(!setPointLocalUpdate){
        updateThingSpeak("field1="+ String(currentTemp) + "&field2="+ String(setTemp));
      } else {
        updateThingSpeak("field1="+ String(currentTemp) + "&field2="+ String(setTemp));
      setPointLocalUpdate = false;
      };
      lastSentTemp = currentTemp;
    }
  }

  lastConnected = client.connected();

}

float getTempCelcius(int pin)
{
  int rawvoltage= analogRead(pin);
  //Uses 1.1v internal refernce
  float tempDegC = rawvoltage/9.31;
  return tempDegC;  
}


void updateThingSpeak(String tsData)
{
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");
    client.print(tsData);
    
    lastWriteConnectionTime = millis();
    resetCounter = 0;
  } else {
    resetCounter++;
    if (resetCounter >=5 ){
      resetEthernetShield();
    }
  }  
  lastWriteConnectionTime = millis(); 
}

void readThingSpeak()
{
  if (client.connect(server, 80)){         
    client.print("GET /channels/16469/feed.json?results=1 HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    client.print("Content-Type: application/xml\n");
    client.print("\n\n");

    lastReadConnectionTime = millis();
    
    if (client.connected()){
      isRead = true;
      resetCounter = 0;
    } else {
      resetCounter++;
      if (resetCounter >=5 ) {
        resetEthernetShield();
      }
    }
  } else {
    resetCounter++;
    if (resetCounter >=5 ) {
      resetEthernetShield();
    }
    lastReadConnectionTime = millis(); 
  }
}

void resetEthernetShield()
{
  client.stop();
  delay(1000);
  Ethernet.begin(mac);
  delay(1000);
}
