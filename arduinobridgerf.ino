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
 *  ==How to use==
 *  Send an UDP Package to port 8888 to execute (send) a command over RF.
 *  Listen for UDP Packages on port 8889 to receive RF-commands
 * 
 * A command is built with 5 comma separated values:
 *  - Direction of the message: O or I (outgoing, incoming);
 *  - Library that applies: 1, 2 or 3 (NewRemoteSwitch, RemoteSwitch or RemoteSensor)
 *  - Code: xxxx (any value)
 *  - Period: xxx (microseconds)
 *  - Additional details: 0000 (4 digits, containing: Type (0=Off, 1=On, 2=Dim), Unit (which light), Dim value (always 0 for now), Group (always 0 for now)
 * 
 * E.g: O,1,2559398,254,1200 (outgoing, NewRemoteswitch, code, period, type 1 / unit 2 / dim 0 / group 0) --> Turn light 2 of remote 2559398 ON.
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

//Connections of the RF transmitter and LED to the Arduino board (receiver is 2)
const int led =  4; 
const int transmitterPin = 8;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

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
  RemoteReceiver::init(-1, 2, handleOldCode);
  
  // Again, interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  NewRemoteReceiver::init(-1, 2, handleNewCode);

  // On interrupt, call the interrupt handlers of remote and sensor receivers
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, NewRemoteReceiver::interruptHandler);
}

void loop() {
    
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();

  if(packetSize) {
    
    //Because handling incoming UDP Request without deinitializing the interupts will fail, we deinit them...
    RemoteReceiver::deinit();
    NewRemoteReceiver::deinit();
    
    // read the packet into packetBufffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.print("Binnen gekregen: ");
    Serial.println(packetBuffer);
    
    handleCommand(packetBuffer);
        
    //and reinit our interupts of course.
    RemoteReceiver::init(-1, 2, handleOldCode);
    NewRemoteReceiver::init(-1, 2, handleNewCode);

  }

  delay(10);
}

void handleCommand(char* receivedCommand) {
  Serial.print("Verwerken: ");
  Serial.println(receivedCommand);
  
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
    Serial.println(str);
  }
  if (commandValues[0] == "O") {
      Serial.println("Transmitting over RF:");
      Serial.println(receivedCode);
      Serial.println(period);
      transmitCode(receivedCode, period); 
  }
}

// shows the received code sent from an old-style remote switch
void handleOldCode(unsigned long receivedCode, unsigned int period) {
  // Print the received code.
  Serial.print("Code: ");
  Serial.print(receivedCode);
  Serial.print(", period: ");
  Serial.print(period);
  Serial.println("us.");
  
  //Broadcasting received RF-signal
  Udp.beginPacket(broadcast, 8889);
  Udp.write("I,2,");

  char charBuf[50];
  String codeString(receivedCode);
  codeString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  Udp.write(",");

  String periodString(period);
  periodString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  Udp.write(",0000");
  Udp.endPacket(); 
}

// Shows the received code sent from an new-style remote switch
void handleNewCode(NewRemoteCode receivedCode) {
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
  
  
  //Broadcasting received RF-signal
  Udp.beginPacket(broadcast, 8889);
  Udp.write("I,1,");

  char charBuf[50];
  String codeString(receivedCode.address);
  codeString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  Udp.write(",");

  String periodString(receivedCode.period);
  periodString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  Udp.write(",");
  
  //Type
  String typeString(receivedCode.switchType);
  typeString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  
  //Unit
  String unitString(receivedCode.unit);
  unitString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  
  Udp.endPacket(); 
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



