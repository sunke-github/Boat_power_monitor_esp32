
/**
 *优化电量计算频次，esp32版本
**/


/**
 * wifi
 */
//#include <WiFi.h>
//const char* ssid = "ESP32-Access-Point";
//const char* password = "123456789";

//-------------------------------------------
#include "BluetoothSerial.h"
#include "TinyGPS.h"
#include <Wire.h>
#include <DS18B20.h>

/**
*8通道：GPIO32 - GPIO39
*10个通道：GPIO0、GPIO2、GPIO4、GPIO12-GPIO15、GOIO25-GPIO27
*Default is 12-bit resolution.
**/


#include <Ticker.h>  //Ticker Library
#define RXD1 9
#define TXD1 10

//GPS OK
#define RXD2 17
#define TXD2 16

#define pinInterrupt  36   //接中断信号的脚 ADC5,ADC 模式   All ESP32 GPIO pins are interrupt-capable pins.
#define pinSpeed 39    //ADC4
#define pinAmp  26 //ADC0   5v input
#define pinAmp2  25 //ADC0  3.3 input

#define pinVolt3  35  //ADC3
#define pinVolt2  32   //ADC6
#define pinVolt1  33   //ADC7
//Temp OK
#define pinTemp 27        //TEMP  ADC17,普通模式

int delayBLE = 150;  //for BLE.
float currentVolt = 4.19;
float sensitivity = 20;
float ampOffect = 2;

bool isTime = true;
bool isAmp = true;
bool isMaxAmp = true;
bool isPower = true;
bool isSpeedCount = true;
bool isSpeed = true;
bool isMaxGSpeed  = true;
bool isGpsSpeed = true;
bool isSats = true;
bool isMaxPower = true;
bool isMaxRSpeed = true;
bool isTemp = true;
//-------------------------------
float maxAmp = 0;
float totalAmp = 0;
float maxPower = 0;

//speed test.
unsigned long totalCount  = 0;
unsigned long realCount  = 0;
int calcuPowerCount = 0;
float rSpeed = 0;
float maxRSpeed =0;
float maxGSpeed = 0;
double gpsSpeed =0;
int sats = 0;
//----------------------------------------------------

unsigned long beforeCount = 0;
unsigned long beforeSRead = 0;
unsigned long befTime = millis();
unsigned long befTime2 = millis();
unsigned long startTime = millis();


Ticker eventLinker;

DS18B20 ds(pinTemp);      
TinyGPS gps;
BluetoothSerial SerialBT;


//测电压
float getVolt1(){
  float vRead = analogRead(pinVolt1);
  float volt = vRead * 3.3*1.067 / 4095;
  return volt * 2 ;    //扩大2倍，根据自己焊接的电阻自行修改
}

//测电压
float getVolt2(){
  float vRead = analogRead(pinVolt2);
  Serial.println(vRead);
  
  float volt = vRead * 3.3*1.045 / 4095;
  return volt*3 ;    //扩大3倍,根据自己焊接的电阻自行修改
}

//测电压
float getVolt3(){
  float vRead = analogRead(pinVolt3);
  float volt = vRead * 3.3*1.045 / 4095;
  return volt * 4;    //扩大4倍，根据自己焊接的电阻自行修改
}



//测电流
float getAmp(){
  
  // float vRead = analogRead(pinAmp);
  // float volt = vRead * 3.3*1.045 / 4095;  //3.3 基准电压
  // volt = volt*2;  //二分了                 //5v输入
  // float difVolt = volt - 5/2;

  float vRead = analogRead(pinAmp2);
  float volt = vRead * 3.3*1.045 / 4095;  //3.3 基准电压
  float difVolt = volt - 3.3/2;           //3.3v输入   acs724 电流模块

  return difVolt * 1000/40 ;    //40mv/a
}


String output(String label, float number, String unit,bool isDisp ){
  String numStr = String(number);
  String out = numStr+unit+label;
  if(isDisp){
    delay(delayBLE);
    Serial.println(out);
    SerialBT.println(out);
  }
  return out;
}


float calcuRspd(){
  unsigned long currentTime = millis();
  unsigned long timeInter = currentTime - befTime;
  unsigned long countInter = realCount - beforeCount;
//  Serial.println(timeInter);  //打印执行时间间隔
//  digitalWrite(2, !digitalRead(2));
  
  beforeCount = realCount;
  befTime = currentTime;
  rSpeed = (float)countInter*1000/timeInter; 
  if(rSpeed > maxRSpeed){
    maxRSpeed = rSpeed;
  }
  return rSpeed;
}


float calcuPower(){

  unsigned long currentTime = millis();
  unsigned long timeInter = currentTime - befTime2; 
  
  
  befTime2 = currentTime;
  float amp = getAmp();  //float amp = getAmp(); 
  if (amp > maxAmp){
    maxAmp = amp;
  }
  totalAmp = totalAmp + amp  * timeInter/3600;  //mah  
  
  float volt = getVolt3();    
  //功率
  float power = volt * amp; 
  //最大功率
  if (power > maxPower){
    maxPower = power;
  }
  calcuPowerCount+=1;
  return power;
}

bool isGPSAvailable(){
  
  bool newData = false;
  // For one second we parse GPS data and report some key values
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (Serial2.available())
    {
      char c = Serial2.read();
      //Serial.write(c); // uncomment this line if you want to see the GPS data flowing
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }
  return newData;
}


/**
 * Floats use the FPU while doubles are calculated in software. 
 * For reasons, using the FPU inside an interrupt handler is currently not supported.
 * 
 */
void onChange(){
  unsigned long vRead = analogRead(pinSpeed);  
  if(abs(beforeSRead-vRead) > 500){
    totalCount =  totalCount + 1;
    realCount = totalCount/2;
    beforeSRead = vRead;
//    digitalWrite(2, !digitalRead(2));
  }
}

/**
 * get gps data.
 */
void gpsEvent(){
  if (isGPSAvailable()){
    gpsSpeed = gps.f_speed_mps();
    if(gpsSpeed > maxGSpeed && gpsSpeed < 1000){
      maxGSpeed = gpsSpeed;
    }
    sats = gps.satellites();
  }
  digitalWrite(2, !digitalRead(2));
}

/**
 * get power and r speed.
 */
void timeEvent(){
  calcuPower();  
  calcuRspd();
}


void printEvent(){
  String comdata = "";
  while (Serial.available() > 0){
    comdata += char(Serial.read());
    delay(2);
  }
  for(int i=0;i<comdata.length();i++){
    switch(String(comdata[i]).toInt()){
      case 1:
        isTime = ! isTime;
        break;
      case 2:
        isAmp = ! isAmp;
        isMaxAmp =! isMaxAmp;
        isPower = ! isPower;
        isMaxPower =! isMaxPower;
        break;
      case 3:
        isSpeedCount =! isSpeedCount;
        isSpeed = ! isSpeed;
        isMaxRSpeed =! isMaxRSpeed;
        break;
      case 4:
        isSats =! isSats;
        isGpsSpeed =! isGpsSpeed;
        isMaxGSpeed =! isMaxGSpeed;
        break;
      case 5:
        break;
      case 6:
        break;
      case 7:
        isTemp = ! isTemp;
        break;
    }
  }
  
  //------------------------------------------
  float runTime = float(millis()-startTime)/1000;
  //-----------------------------------------------  
  float amp = getAmp();     //float amp = getAmp();
  //-------------------------------------------
  float volt1 = getVolt1();
  float volt2 = getVolt2();
  float volt3 = getVolt3();
  //---------------------------------------------
  float power = calcuPower(); 
  //-------------------------------------------------------
  float temp = 0;
//-------------------------------------------------------
  while(ds.selectNext()){  
    temp = ds.getTempC();
  }
//----------------------------------------------------------------------------------------
  //BLE协议规定每个蓝牙数据包长度不能超过20byte，每一包数据发送间隔需要超过150ms，否则容易丢包，我们编程时需要注意两点
  //可能需要做调整
  
  String out1 = output(": Time",runTime,"s",isTime);  //1
  String out2 = output(": Amp",amp," a",isAmp);      //2
  String out3 = output(": Volt1",volt1," v",true);   //3
  String out4 = output(": Volt2",volt2-volt1," v",true);    //4
  String out5 = output(": Volt3",volt3-volt2," v",true);    //5 
  String out6 = output(": Power",power," w",isPower);     //6
  String out7 = output(": Count",realCount," r",isSpeedCount);  //7
  String out8 = output(": RSpeed",rSpeed," r/s",isSpeed);    //8
  String out9 = output(": MaxRSpeed",maxRSpeed," r/s",isMaxRSpeed);   //9
  String out10= output(": MaxAmp",maxAmp," A",isMaxAmp);   //10
  String out11= output(": MaxPower",maxPower," w",isMaxPower);  //11
  String out12= output(": TotalAmp",totalAmp," mah",true);  //12
  String out13= output(": Sats",sats," ",isSats);   //13
  String out14= output(": Speed",gpsSpeed," m/s",isGpsSpeed);   //14
  String out15= output(": MaxSpeed",maxGSpeed," m/s",isMaxGSpeed);//15
  String out16 = output(": Temp",temp," ℃",isTemp);
  String out17= output("--------",0,"------------",true);//16
}


void setup() {
  eventLinker.attach_ms(50, timeEvent);  //秒

  pinMode(2, OUTPUT);
  pinMode(pinInterrupt, INPUT); 
  attachInterrupt(pinInterrupt, onChange, CHANGE);  // RISING, CHANGE,LOW HIGH
  Serial.begin(9600,SERIAL_8N1);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  SerialBT.begin("ESP32test"); //Bluetooth device name
  
//  WiFi.softAP(ssid, password);
//  IPAddress IP = WiFi.softAPIP();
//  Serial.print("AP IP address: ");
//  Serial.println(IP);
  
  delay(1000);
}


void loop() {
  printEvent();
  gpsEvent();
}
