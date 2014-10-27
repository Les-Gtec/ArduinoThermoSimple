//initializes/defines the output pin of the LM35 temperature sensor
const int tempSensorAPin= 0;
const int boilerPowerPin = 19;
const int setPointDownPin = 8;
const int setPointUpPin = 9;
boolean setPointUpCurrentState = LOW;
boolean setPointUpLastState = LOW;
boolean setPointUpDebouncedState = LOW;
boolean setPointDownCurrentState = LOW;
boolean setPointDownLastState = LOW;
boolean setPointDownDebouncedState = LOW;

int debounceInterval = 15;//wait 20 ms for button pin to settle
int tempCheckInterval = 2000;

unsigned long timeOfLastButtonEventUp = 0;//store the last time the button state changed
unsigned long timeOfLastButtonEventDown = 0;
unsigned long timeOfLastTempCheck = 0;

int heatCallCount = 5;
float setTemp = 12.0;
float currentTemp = 10.0;
boolean callForHeat = false;
boolean setPointLocalUpdate = false;
int buttonState = 0;


// include the library code:
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,177);
//byte gateway[] = { 192, 168,   1,   1 };
//byte subnet[]  = { 255, 255, 255,   0 };

// ThingSpeak Settings
IPAddress server(184,106,153,149);
//byte server[]  = { 184, 106, 153, 149 }; // IP Address for the ThingSpeak API
String writeAPIKey = "7GH25707INIBKFUB";    // Write API Key for a ThingSpeak Channel - GlanworthThermo
const int updateInterval = 30000;        // Time interval in milliseconds to update ThingSpeak   
EthernetClient client;

// Variable Setup
long lastConnectionTime = 0; 
boolean lastConnected = false;
int resetCounter = 0;


void setup()
{
//Serial.begin(9600);
pinMode(boilerPowerPin, OUTPUT);
pinMode(setPointDownPin, INPUT);
pinMode(setPointUpPin, INPUT);
 // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  // Print a message to the LCD.
  //lcd.print("hello, world!");
  lcd.print("Cur. Temp:");
  lcd.setCursor(0,1);
  lcd.print("Set Temp:");
  
   // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
 
}

//main loop
void loop()
{
unsigned long currentTime = millis();
if(currentTime - timeOfLastTempCheck > tempCheckInterval){
  //currentTemp = getTempCelcius(tempSensorAPin);
  currentTemp = (int)((getTempCelcius(tempSensorAPin)*2.0)+0.5)/2.0;
  timeOfLastTempCheck = currentTime;
}

lcd.setCursor(10,0);
lcd.print(currentTemp);

lcd.setCursor(10,1);
lcd.print(setTemp);

//Work out if we should call for heat
  if (currentTemp < setTemp && heatCallCount < 10)  {
    heatCallCount++;
  } else if ((currentTemp > (setTemp + 0.5)) && heatCallCount > 0) {
    heatCallCount--; 
  }
    
  if (heatCallCount > 9) {
    digitalWrite(boilerPowerPin, LOW);
  } else if (heatCallCount < 1){
    digitalWrite(boilerPowerPin, HIGH);
  }

//Read set point up and down buttons
  setPointUpCurrentState = digitalRead(setPointUpPin);
  if(setPointUpCurrentState != setPointUpLastState){
    timeOfLastButtonEventUp = currentTime;
  }
  
  if (currentTime - timeOfLastButtonEventUp > debounceInterval){//if enough time has passed
      if (setPointUpCurrentState != setPointUpDebouncedState){//if the current state is still different than our last stored debounced state
        setPointUpDebouncedState = setPointUpCurrentState;//update the debounced state 
        //trigger an event
        if (setPointUpDebouncedState == HIGH){
           setTemp = setTemp+0.5;  
           setPointLocalUpdate = true;
        } else {
         // Serial.println("released");
        }
      }
  }
  setPointUpLastState = setPointUpCurrentState;
  
  setPointDownCurrentState = digitalRead(setPointDownPin);
  
  if(setPointDownCurrentState != setPointDownLastState){
   timeOfLastButtonEventDown = currentTime;
  }
  
   if (currentTime - timeOfLastButtonEventDown > debounceInterval){//if enough time has passed
      if (setPointDownCurrentState != setPointDownDebouncedState){//if the current state is still different than our last stored debounced state
        setPointDownDebouncedState = setPointDownCurrentState;//update the debounced state
        
        //trigger an event
        if (setPointDownDebouncedState == HIGH){
           setTemp = setTemp-0.5;  
           setPointLocalUpdate = true;
        }
      }
   }
   setPointDownLastState = setPointDownCurrentState;
  
    if (client.available())
  {
    char c = client.read();
   // Serial.print(c);
  }
  
// Disconnect from ThingSpeak
  if (!client.connected() && lastConnected)
  {
    delay(1000);
    client.stop();
  }
  
// Update ThingSpeak
  if(!client.connected() && (millis() - lastConnectionTime > updateInterval)){
    lcd.setCursor(0,1);
    lcd.print("UPDATING......");
    if(!setPointLocalUpdate){
       updateThingSpeak("field1="+ String(currentTemp));
    } else {
        updateThingSpeak("field1="+ String(currentTemp) + "&field2="+ String(setTemp));
        setPointLocalUpdate = false;
    };
    lcd.setCursor(0,1);
    lcd.print("Set Temp:");
  }
  
  lastConnected = client.connected();

}

float getTempCelcius(int pin)
{
  int rawvoltage= analogRead(pin);
  float millivolts= (rawvoltage/1024.0) * 5000;
  
  return millivolts/10;  
  
  
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
    
    lastConnectionTime = millis();
    resetCounter = 0;
  } else {
      
    resetCounter++;
    
    if (resetCounter >=5 ) 
      {
        resetEthernetShield();
      }
  }  
      lastConnectionTime = millis(); 
}

void resetEthernetShield()
{
  client.stop();
  delay(1000);
  Ethernet.begin(mac, ip);
  delay(1000);
}



