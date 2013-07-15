/**
 * @author Rogier van den Berg / http://www.rogiervandenberg.nl
 *
 * This project is to send/receive RF-signals (KaKu, Elro, etc).
 * Its purpose is to function as a proxy/bridge, so you can toggle lights or receive commands in your domotica solution.
 *
 * It is designed to work with combined with https://github.com/bslagter/hue-control
 * 
 * I am using the interpretation of RF-signals from the RemoteSwitch Libraries by Randy Simons,
 * as found at https://bitbucket.org/fuzzillogic/433mhzforarduino/wiki/Home
 * 
 *
 ==How to use==
 * A command is built with 5 comma separated values:
 *  - Direction of the message: O or I (outgoing, incoming);
 *  - Library that applies: 1, 2 or 3 (NewRemoteSwitch, RemoteSwitch or RemoteSensor)
 *  - Code: xxxx (any value)
 *  - Period: xxx (microseconds)
 *  - Additional details: 0000 (4 digits, containing: Type (0=Off, 1=On, 2=Dim), 2 digits for Unit (which light), Dim value (always 0 for now)
 * 
 * E.g: O,1,2559398,254,1020 (outgoing, NewRemoteswitch, code, period, type 1 / unit 02 / dim 0) --> Turn light 2 of remote 2559398 ON.
 */

#include <RemoteReceiver.h>
#include <RemoteTransmitter.h>
#include <NewRemoteReceiver.h>
#include <NewRemoteTransmitter.h>
#include <InterruptChain.h>
#include <String.h>

String incomingPacketBuffer = ""; //buffer to hold incoming packet,
String outgoingPacketBuffer = ""; //buffer to hold outgoing packet,
char packetBuffer[50]; //buffer to hold incoming packet,

//Connections of the RF transmitter and LED to the Arduino board (receiver is 2)
const int led =  4; 
const int transmitterPin = 8;

boolean sending = false;
boolean serialMessageToSend = false;
boolean stringComplete = false;  // whether the string is complete

void setup()
{
  pinMode(led, OUTPUT); 
  
  incomingPacketBuffer.reserve(50);
  outgoingPacketBuffer.reserve(50);
  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  
    // Interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  RemoteReceiver::init(-1, 2, handleOldRfRemotes);
  
  // Again, interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  NewRemoteReceiver::init(-1, 2, handleNewRfRemotes);

  // On interrupt, call the interrupt handlers of remote and sensor receivers
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler);
}

void loop() {
    
  if (stringComplete) {
    //Serial.println(incomingPacketBuffer); 
    incomingPacketBuffer.toCharArray(packetBuffer, 50);
    handleCommand(packetBuffer);
    // clear the string:
    incomingPacketBuffer = "";
    stringComplete = false;
  }
  
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    incomingPacketBuffer += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
  
//If we receive a command, parse it to do something with it
void handleCommand(char* receivedCommand) {
  char *p = receivedCommand;
  
   char *str;
  int count = 0;
  String commandValues[5];
  
  unsigned long receivedCode = 0;
  unsigned int period = 0;
  
  while ((str = strtok_r(p, ",", &p)) != NULL) {
    
    if (count == 2)
    {
      receivedCode = atol(str); //Convert to long integer
    }
    if (count == 3)
    {
      period = atoi(str); //Convert to integer
    }
    
    commandValues[count++] = str;
  }
  
  if (commandValues[0] == "O" && commandValues[1] == "2") {
    /*
    Serial.print("Sending out over RF: ");
    Serial.print(receivedCode);
    Serial.print(" / ");
    Serial.println(period);
    */
    RemoteReceiver::disable();
    RemoteTransmitter::sendCode(8, receivedCode, period, 3);
    RemoteReceiver::enable();
  } else if (commandValues[0] == "O" && commandValues[1] == "1") {
    Serial.println("new kakuding");
  }
}



// shows the received code sent from an old-style remote switch
void handleOldRfRemotes(unsigned long receivedCode, unsigned int period) {
  
  // Print the received code.
  /*
  Serial.print("Interupted! Code: ");
  Serial.print(receivedCode);
  Serial.print(", period: ");
  Serial.print(period);
  Serial.println("us.");
  */
  String codeString(receivedCode);
  String periodString(period);
  
  String command = "I,2," + codeString + "," + periodString + ",0000";
  Serial.println(command);
}


// shows the received code sent from an old-style remote switch
void handleNewRfRemotes(NewRemoteCode receivedCode) {
  /*
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
  */
  
  String codeString(receivedCode.address);
  String periodString(receivedCode.period);
  String typeString(receivedCode.switchType);
  String unitString(receivedCode.unit);
  
  if (unitString.length() == 1) {
    unitString = "0" + unitString;
  }
  
  String command = "I,1," + codeString + "," + periodString + "," + typeString + unitString + "0";
  Serial.println(command);
}


void transmitCodeNewKaKu(NewRemoteCode receivedCode) {  
  NewRemoteReceiver::disable();
  interrupts();
  NewRemoteTransmitter transmitter(receivedCode.address, 11, receivedCode.period);
  if (receivedCode.switchType == 2) {
    transmitter.sendDim(receivedCode.unit, receivedCode.dimLevel);	
  } 
  else {
    if (receivedCode.groupBit) {
      transmitter.sendGroup(receivedCode.switchType);
    } 
    else {
      transmitter.sendUnit(receivedCode.unit, receivedCode.switchType);
    }
  }
  NewRemoteReceiver::enable();
}


