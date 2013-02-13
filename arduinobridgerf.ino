/*
 Created with some help from:
 - RemoteSwitch library by Randy Simons http://randysimons.nl/
 - Details: https://bitbucket.org/fuzzillogic/433mhzforarduino/wiki/Home
 
 Example:
 http://192.168.0.111/?cmd=175682*327 --> Send code 175682 for a period of 327 ms
 http://192.168.0.111 --> See last received code
 
*/

#include <SPI.h>
#include <Ethernet.h>
#include <RemoteReceiver.h>
#include <RemoteTransmitter.h>
#include <NewRemoteReceiver.h>
#include <NewRemoteTransmitter.h>
#include <SensorReceiver.h>
#include <InterruptChain.h>

boolean gflag=false;
String parm;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
// The Server instance on port 80
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 111); //Fallback IP address..
IPAddress lightsHubLocation(192,168,0,108); //Pi, the location of the lighthub
EthernetServer server(80);
EthernetClient client;

boolean lastConnected = false;                 // state of the connection last time through the main loop

//Connections of the RF transmitter to the Arduino board
const int led =  4; 
const int transmitterPin = 8;

//Duration of a RF command
unsigned int period = 326;

unsigned long lastReceivedCode = 0;
unsigned int lastPeriod = 0;

void setup()
{
  pinMode(led, OUTPUT); 
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // start the Ethernet connection:
  Serial.println("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  Serial.print("LightHub Address: ");
  Serial.println(lightsHubLocation);
  Serial.println();
  // start listening for clients
  server.begin();
  
    // Interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  RemoteReceiver::init(-1, 2, showOldCode);
  
  // Again, interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  NewRemoteReceiver::init(-1, 2, showNewCode);

  // And once more, interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  SensorReceiver::init(-1, showTempHumi);

  // On interrupt, call the interrupt handlers of remote and sensor receivers
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, SensorReceiver::interruptHandler);
  
}

void loop()
{
  handleIncomingRequests();
  handleOutgoingRequestReturnData();
}

void handleOutgoingRequestReturnData() {

  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }

   // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
}

void handleIncomingRequests() {
  
  // listen for incoming clients
  EthernetClient incomingClient = server.available();
  if (incomingClient) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    
    while (incomingClient.connected()) {
      if (incomingClient.available()) {
        char c = incomingClient.read();

        // search parameter from "HTTP GET"
        if(gflag){
          if(c != ' '){
              parm += c;
          }else{
              gflag = false;
          }
        }
        if(c == '?' && parm == ""){
          gflag = true;
        }
        //Serial.print(c);

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply

        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header

         if(parm == ""){ // normal HTTP access
            incomingClient.println("HTTP/1.1 200 OK");
            incomingClient.println("Content-Type: text/html");
            incomingClient.println();

            if (lastReceivedCode == 0) {
              incomingClient.print("{\"status\": 0, \"action\": \"learn\", \"message\": \"No code received while starting Arduino, press reset on Arduino and send out RF-command within 4 seconds (while LED is on) to listen for codes. Connections: RF-transmitter on pin 8, RF-receiver on pin 2, LED on pin 4.\"}");
            } else {
              incomingClient.print("{\"status\": 1, \"action\": \"learn\", \"code\": ");
              incomingClient.print(lastReceivedCode);
              incomingClient.print(", \"period\": ");
              incomingClient.print(lastPeriod);
              incomingClient.print("}");
            }

         }else{ // using XMLhttpObject access
  
            unsigned long code = 0;
            int positionIs = parm.indexOf('=');
            if(positionIs != -1){ //We hebben een = gekregen
              
              /*
              Serial.print("positionIs: ");
              Serial.println(positionIs);
              */
              
              int positionCmd = parm.indexOf('cmd'); //Hier wordt het stukje "cmd" uit de URL gehaald
              if(positionCmd != -1){ // We hebben cmd ontvangen
                
                /*
                Serial.print("positionCmd: ");
                Serial.println(positionCmd);
                */
                
                int positionAmp = parm.indexOf("*");
                /*
                Serial.print("positionAmp: ");
                Serial.println(positionAmp);
                */
                
                if(positionAmp != -1) { //Er is een * meegegeven
                  period = (parm.substring(positionAmp+1)).toInt();
                  code = (parm.substring(positionIs+1, positionAmp)).toInt();
                
              
                } else { //Er is geen & meegegeven
                  code = (parm.substring(positionIs+1)).toInt();
                }
                //Serial.print("Code om te sturen: ");
                //Serial.print(code);
                //Serial.print(" met period ");
                //Serial.println(period);
              }

              //Send signal on pin 8, 2^3 times (8 times).
              //RemoteTransmitter::sendCode(8, code, period, 3);
              transmitCode(code, period);

              incomingClient.print("{\"action\": \"code sent\", \"code\": ");
              incomingClient.print(code);
              incomingClient.print(", \"period\": ");
              incomingClient.print(period);
              incomingClient.print("}");
            }
            parm = "";
          }

          break;
        }


        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    incomingClient.stop();
  }
  
}

// shows the received code sent from an old-style remote switch
void showOldCode(unsigned long receivedCode, unsigned int period) {
  // Print the received code.
  Serial.print("Code: ");
  Serial.print(receivedCode);
  Serial.print(", period: ");
  Serial.print(period);
  Serial.println("us.");
  
   for (int i=0; i <= 15; i++){
      digitalWrite(led, HIGH);
  delay(50);
    digitalWrite(led, LOW);
  delay(50);
   } 

httpRequest(); //Doe Request bij binnenkomende data (test)

/*
if(!client.connected()){
  RemoteReceiver::disable();
  interrupts();
  httpRequest(); //HIER WAS IK, HIJ HANGT BIJ DOEN VAN REQUEST...
  RemoteReceiver::enable();
}
*/

}

// Shows the received code sent from an new-style remote switch
void showNewCode(NewRemoteCode receivedCode) {
  // Print the received code.
  Serial.print("Addr ");
  Serial.print(receivedCode.address);
  
  if (receivedCode.groupBit) {
    Serial.print(" group");
  } else {
    Serial.print(" unit ");
    Serial.print(receivedCode.unit);
  }
  
  switch (receivedCode.switchType) {
    case 0:
      Serial.print(" off");
      break;
    case 1:
      Serial.print(" on");
      break;
    case 2:
      Serial.print(" dim level");
      Serial.print(receivedCode.dimLevel);
      break;
  }
  
  Serial.print(", period: ");
  Serial.print(receivedCode.period);
  Serial.println("us.");
}

// Shows the received sensor data
void showTempHumi(byte *data) {
  // is data a ThermoHygro-device?
  if ((data[3] & 0x1f) == 0x1e) {
    // Yes!
    byte channel, randomId;
    int temp;
    short int humidity;

    // Decode the data
    SensorReceiver::decodeThermoHygro(data, channel, randomId, temp, humidity);

    // Print temperature. Note: temp is 10x the actual temperature!
    Serial.print("Temperature: ");
    Serial.print(temp / 10); // units
    Serial.print('.');
    Serial.print(temp % 10); // decimal

    // Print humidity
    Serial.print(" deg, Humidity: ");
    Serial.print(humidity);
    Serial.print("% REL");

    // Print channel
    Serial.print(", Channel: ");
    Serial.println(channel, DEC);   
  }
}

void transmitCode(unsigned long receivedCode, unsigned int period) {  
  // Disable the receiver; otherwise it might pick up the retransmit as well.
  RemoteReceiver::disable();
  
  // Need interrupts for delay()
  interrupts();
    
  // Retransmit the signal 8 times ( == 2^3) on pin 11. Note: no object was created!
  RemoteTransmitter::sendCode(8, receivedCode, period, 3);
  
  RemoteReceiver::enable();
}

void transmitCodeNewKaKu(NewRemoteCode receivedCode) {  
  // Disable the receiver; otherwise it might pick up the retransmit as well.
  NewRemoteReceiver::disable();

  // Need interrupts for delay()
  interrupts();

  // Create a new transmitter with the received address and period, use digital pin 11 as output pin

  NewRemoteTransmitter transmitter(receivedCode.address, 11, receivedCode.period);

  // Switch type 0 = switch off, type 1 = switch on, type 2 = set dim level.
  if (receivedCode.switchType == 2) {
    // Dimmer signal received
    transmitter.sendDim(receivedCode.unit, receivedCode.dimLevel);	
  } 
  else {
    // On/Off signal received

    if (receivedCode.groupBit) {
      // Send to the group
      transmitter.sendGroup(receivedCode.switchType);
    } 
    else {
      // Send to a single unit
      transmitter.sendUnit(receivedCode.unit, receivedCode.switchType);
    }
  }

  NewRemoteReceiver::enable();
}

// this method makes a HTTP connection to the server:
void httpRequest() {
  // if there's a successful connection:
  if (client.connect(lightsHubLocation, 80)) {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET / HTTP/1.1");
    client.print("Host: ");
    client.println(lightsHubLocation);
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();

  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println("disconnecting.");
    client.stop();
  }
}




