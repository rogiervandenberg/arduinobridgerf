/**
 * @author Rogier van den Berg / http://www.rogiervandenberg.nl
 *
 * This project is to send/receive RF-signals (KaKu, Elro, etc).
 * Its purpose is to function as a proxy/bridge, so you can toggle lights or receive commands in your domotica solution.
 *
 * It is designed to work with combined with https://github.com/bslagter/hue-control
 * You need an Arduino with Ethternet for this to work.
 * 
 * I am using the interpretation of RF-signals from the RemoteSwitch Libraries by Randy Simons,
 * as found at https://bitbucket.org/fuzzillogic/433mhzforarduino/wiki/Home
 * 
 * For processing the raw RF-signals for the New style KaKu I am using the code from Tymmo at http://gathering.tweakers.net/forum/list_message/37257667#37257667
 *
 ==How to use==
 *  Send an UDP Package to port 8888 to execute (send) a command over RF.
 *  Listen for UDP Packages on port 8889 to receive RF-commands
 * 
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
#include <SPI.h>
#include <Ethernet.h>
#include <String.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //Mac Address (IP will be dynamic by DHCP)
IPAddress broadcast(255, 255, 255, 255); //Where to broadcast incoming signals to
unsigned int localPort = 8888;      // local port to listen on
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
char outgoingPacketBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold outgoing packet,

//Connections of the RF transmitter and LED to the Arduino board (receiver is 2)
const int led =  4; 
const int transmitterPin = 8;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

boolean sending = false;
boolean udpMessageToSend = false;

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
  Serial.println();
  
  Udp.begin(localPort);
  
    // Interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  RemoteReceiver::init(-1, 2, handleOldRfRemotes);
  
  // Again, interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  NewRemoteReceiver::init(-1, 2, handleNewRfRemotes);

  // On interrupt, call the interrupt handlers of remote and sensor receivers
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler);
}

void loop() {
    
  sendUDPMessage();
  
  // if there's data available, read a packet
  if(Udp.parsePacket()) {
    sending = true;
    // read the packet into packetBufffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.print("Received over UDP: ");
    Serial.println(packetBuffer);
    handleCommand(packetBuffer);
    sending = false;
  }
}

//This message checks if there is a message to send, if yes, it will be sent
void sendUDPMessage() {
  if (udpMessageToSend == true) {
    Serial.print("We send this message out over UDP: ");
    Serial.println(outgoingPacketBuffer);
    sending = true;
    //Broadcasting RF-signal
    Udp.beginPacket(broadcast, 8889);
    Udp.write(outgoingPacketBuffer);
    Udp.endPacket(); 
    
    udpMessageToSend = false;
    sending = false;
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
  if (commandValues[0] == "O") {
    Serial.print("Sending out over RF: ");
    Serial.print(receivedCode);
    Serial.print(" / ");
    Serial.println(period);
    RemoteReceiver::disable();
    RemoteTransmitter::sendCode(8, receivedCode, period, 3);
    RemoteReceiver::enable();
  }
}



// shows the received code sent from an old-style remote switch
void handleOldRfRemotes(unsigned long receivedCode, unsigned int period) {
  
  // Print the received code.
  Serial.print("Interupted! Code: ");
  Serial.print(receivedCode);
  Serial.print(", period: ");
  Serial.print(period);
  Serial.println("us.");
  
  String codeString(receivedCode);
  String periodString(period);
  
  String command = "I,2," + codeString + "," + periodString + ",0000";
  if(udpMessageToSend == false) {
    udpMessageToSend = true;
    command.toCharArray(outgoingPacketBuffer, 50);
  }
}


// shows the received code sent from an old-style remote switch
void handleNewRfRemotes(NewRemoteCode receivedCode) {
  
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
  
  
  String codeString(receivedCode.address);
  String periodString(receivedCode.period);
  String typeString(receivedCode.switchType);
  String unitString(receivedCode.unit);
  
  if (unitString.length() == 1) {
    unitString = "0" + unitString;
  }
  
  String command = "I,1," + codeString + "," + periodString + "," + typeString + unitString + "0";
  if(udpMessageToSend == false) {
    udpMessageToSend = true;
    command.toCharArray(outgoingPacketBuffer, 50);
  }
}


void transmitCode(unsigned long receivedCode, unsigned int period) {  
  
  Serial.print("We gaan sturen: ");
  Serial.print(receivedCode);
  Serial.print(" / ");
  Serial.println(period);

  
  RemoteReceiver::disable();
  interrupts();
  RemoteTransmitter::sendCode(8, receivedCode, period, 3);
  RemoteReceiver::enable();
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


