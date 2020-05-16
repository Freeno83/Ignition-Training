/*
Written by: Nicholas Robinson
Last modified: 15th May 2020

***Function***
Provide a device which can be controlled by Ignition for training
Allows "VFD like" control of a DC motor via modbus TCP
 */

#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

char ssid[] = "Kaisoku";
char pass[] = "Tsunagaru";

//Declare a ModbusIP instance

ModbusIP modbusTCPServer;

//Modbus coil mapping

const int modAddrunStop = 0X00;
const int modAddfwdRev = 0X01;

//Modbus holding register mapping

const int modAddrampSpeed = 0X00;
const int modAddtargetSpeed = 0X01;
const int modAddcurrSpeed = 0X02;
const int modAddsetSpeed = 0X03;

//Declare a MotorShield instance on pin 3

Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *myMotor = AFMS.getMotor(3);

//Program global variables

int runStop;            //0 = stop, 1 = run
int fwdRev;             //Motor desired direction
int fwdRevButton;       //Button used to control direction
int prevFwdRev;         //Record of the previous fwdRev state
int dirChangeReq;       //Flags a direction change request
int switchingFlag;      //Indicates a direction change is in progress  
int switchSpeed;        //Temp holder for target speed during a direction change
int currSpeed;          //Current motor speed
int targetSpeed = 0;    //target motor speed
int SetSpeed = 0;       //prevent target speed from changing during direction change
int speedIncrement;     //Step value for motor speed change based on remote input
int rampSpeed;          //Delay time for auto ramp up/down

void setup(){
  Serial.begin(115200);
  AFMS.begin();
  myMotor->setSpeed(targetSpeed);
  myMotor->run(FORWARD);
  myMotor->run(RELEASE);

//Connect to WiFi
  startWifi();

//Set this device as a server (slave)
//Declare modbus coils and holding registers
  modbusTCPServer.slave(502);
  modbusTCPServer.addCoil(0x00, 0, 5);
  modbusTCPServer.addHreg(0x00, 0, 5);
}

void loop(){
  modbusTCPServer.task();

//This section gets/puts data to modbus registers
  runStop = modbusTCPServer.Coil(modAddrunStop);
  fwdRevButton = modbusTCPServer.Coil(modAddfwdRev);
  rampSpeed = modbusTCPServer.Hreg(modAddrampSpeed);
  targetSpeed = modbusTCPServer.Hreg(modAddtargetSpeed);
  SetSpeed = modbusTCPServer.Hreg(modAddsetSpeed);
  modbusTCPServer.Hreg(modAddcurrSpeed, currSpeed);

//These two lines keep set speed from changing during direction change
  if(targetSpeed != 1)
    targetSpeed = SetSpeed;

// this line decrease speed increment as ramp speed slows
  speedIncrement = map(rampSpeed, 1, 20, 15, 5);  

//This section controls the motor direction and direction switching
  if(fwdRevButton == LOW)
    fwdRev = FORWARD;
  if(fwdRevButton == HIGH)
    fwdRev = BACKWARD;

  if(runStop == HIGH){
    if(currSpeed == 0)
      myMotor->run(fwdRev);
    if((fwdRev != prevFwdRev) && (currSpeed > 1))
      dirChangeReq = true;
    else
      dirChangeReq = false;

    if((dirChangeReq == true) && (switchingFlag == true)){
      switchingFlag = false;
      targetSpeed = switchSpeed;
      modbusTCPServer.Hreg(modAddtargetSpeed, switchSpeed);
    }
    if((dirChangeReq == true) && (switchingFlag == false)){
      switchingFlag = true;
      switchSpeed = targetSpeed;
      targetSpeed = 1;
      modbusTCPServer.Hreg(modAddtargetSpeed, 1);
    }
    if((currSpeed == 1) && (switchingFlag == true)){  
      myMotor->run(fwdRev);
      switchingFlag = false;
      targetSpeed = switchSpeed;
      modbusTCPServer.Hreg(modAddtargetSpeed, switchSpeed);
    }

//This section controls the motor speed
    if(currSpeed != targetSpeed){
      if(currSpeed < targetSpeed)
        currSpeed += 1;
      if(currSpeed > targetSpeed)
        currSpeed -= 1;
      delay(rampSpeed);
    }
    myMotor->setSpeed(currSpeed);
  }
   
//This section controls motor stopping  
  if(runStop == LOW){
    if(currSpeed != 0){
      currSpeed -= 1;
      delay(rampSpeed);
      myMotor->setSpeed(currSpeed);
      if(currSpeed == 0)
        myMotor->run(RELEASE);
      }
    }
   
//This line records the previous direction value
//Needed to identify direction change requests
  prevFwdRev = fwdRev;          
}

//Wifi connection 
void startWifi(){
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
