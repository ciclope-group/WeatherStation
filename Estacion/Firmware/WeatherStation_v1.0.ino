#include <stdio.h>
#include <string.h>
#include <Wire.h>//comunicacion I2C
#include <SD.h>
#include <FreqCount.h>
#include <SparkFunDS1307RTC.h>//RTC
#include <BMP085.h>//sensor temp/pres
#include <hh10d.hpp>//sensor humedad

#define FREQ 500
#define INTERVAL 8// es el periodo entre publicacion de info en segundos) 
unsigned long previousSegs = 0;
unsigned long currentSegs;
//////SD//////
File FileRead,FileWrite;
String path;
String anio,mes,dia,hora;

//////XBEE//////
/*Serial1 para comunicación*/
char datos[72];//72 bytes de envío de datos
String strDatos,h,m,s;

//////RTC1307//////
#define PRINT_EU_DATE//disposicion de la fecha dia/mes/año
#define SQW_INPUT_PIN 2   // Input pin to read SQW
#define SQW_OUTPUT_PIN 13 // LED to indicate SQW's state

//////BMP085//////
BMP085 bmp085;//objeto sensor
float temp=0.0;
float pres=0.0;

//////HH10D//////
const int HH10D_OFFSET(7732);
const int HH10D_SENSITIVITY(402);
float hum;

//////SPARKFUN STATION////// ANEMOMETRO+VELETA+PLUVIOMETRO
#define pinAnem 2
#define pinPluviometro 3
#define pinVeleta 4
#define pinReset 14
volatile int cuentaAnem=0;//frecuencia recibida por el Anemometro
volatile int cuentaPluv=0;//frecuencia recibida del Pluviometro
float VelSp = 0.0;//Km/h del viento
float pluv = 0.0;//precipitacion en mm
String dir;// N NW W SW S SE E NE,también indica NNW, SSE, etc

/******SETUP******/
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  initSD();
  initXbee();//SERIAL 1 se inicializa, para la comunicacion por XBEE
  initRTC();
  initBmp085();
  initHH10D();
  initSparkStation();
  Serial1.println("Sensores inicializados");  
  delay(2000);
}
/******END OF SETUP******/

/******LOOP******/
void loop() {
  // put your main code here, to run repeatedly:
  clockRTC();
  sensorBmp085();
  sensorHH10D();
  sensorSparkStation();

  currentSegs = (millis() / 1000);//trabaja cada intervalo establecido
  if(currentSegs - previousSegs >= INTERVAL) {
    previousSegs = currentSegs;
    printData();//serial
    sendData();//xbee
    saveSD();//guarda los datos en la SD
    clearCounters(); //para los contadores de frecuencia del anemómetro, pluviómetro y contador de ciclos
  }
}
/******END OF LOOP******/
//////XBEE//////
void initXbee(){
  Serial1.begin(9600);
  Wire.begin();  
  Serial1.println("Comunicacion inicializada");
}
void sendData(){//se envía info en cada ciclo (FREQ veces por cada Serial.print del Arduino
    if (rtc.hour() < 10) h = "0" + String(rtc.hour());//conversion a string de m
    else h = String(rtc.hour());
  
    if (rtc.minute() < 10) m = "0" + String(rtc.minute());
    else m = String(rtc.minute());
  
    if (rtc.second() < 10) s = "0" + String(rtc.second());
    else s = String(rtc.second());
  float v=(VelSp/(FREQ));
  float pl=(pluv/(FREQ));
  
  strDatos="######" + h + ":" + m + ":" + s + "_T_" + String(temp) + "_P_" + String(pres) + "_H_" + String(hum) + "_V_" + String(v) + "_PL_" + String(pl) + "_D_" + String(dir) + "\n";
  strDatos.toCharArray(datos,72);
  //sprintf(info,"######%d:%d:%d %d grados %d KPa %d humedad  Km/h\n",rtc.hour(),rtc.minute(),rtc.second(),(int)temp,(int)pres,(int)hum);
  //Serial1.setTimeout(5000);
  Serial1.write("//////");
  delay(1000);
  Serial1.write(datos); 
}

//////SERIAL DATA//////
void printData(){
    Serial.print("Temperatura: ");//grados centigrados
    Serial.print(temp);
    Serial.print("  Presion: ");//KILO pascales
    Serial.print(pres);
    Serial.print(" Humedad Rel. ");//tanto por ciento
    Serial.print(hum); 
    Serial.print("%");
    Serial.print(" ");
    printTime();//RTC
    Serial.print((VelSp/(FREQ)));//km/h
    Serial.print(" ");
    Serial.print(pluv/(FREQ));//mm por minuto de precipitacion
    Serial.print(" ");
    Serial.println(dir);//punto cardinal de donde proviene el viento   
}

//////SD CARD//////
void initSD(){
  SD.begin();//sin parámetro se define por defecto CS el pin 53 en Mega
  Serial.println("SD Card inicializado");
}
void saveSD(){//guarda en la SD los datos que se transmiten
    if (rtc.year() < 10) anio = "0" + String(rtc.year());//conversion a string de m
    else anio = String(rtc.year());
  
    if (rtc.month() < 10) mes = "0" + String(rtc.month());
    else mes = String(rtc.month());
  
    if (rtc.date() < 10) dia = "0" + String(rtc.date());
    else dia = String(rtc.date());
  
    path = dia + "_" + mes + "_" + anio + ".txt";
    //path=(String(rtc.year()) + "_" + String(rtc.month()) + "_" + String(rtc.date()) + ".txt");
    FileWrite=SD.open(path,FILE_WRITE);
    FileWrite.print("#");
    if(rtc.hour()<10){
      FileWrite.print("0");FileWrite.print(rtc.hour());
    }
    else FileWrite.print(rtc.hour());
    
    FileWrite.print(":"); 
    
    if(rtc.minute()<10){
      FileWrite.print("0");FileWrite.print(rtc.minute());
    }
    else FileWrite.print(rtc.minute());
    
    FileWrite.print(":");

    if(rtc.second()<10){
      FileWrite.print("0");FileWrite.print(rtc.second());
    }
    else FileWrite.print(rtc.second());
    
    FileWrite.print("_T_");//grados centigrados
    FileWrite.print(temp);
    FileWrite.print("_P_");//KILO pascales
    FileWrite.print(pres);
    FileWrite.print("_H_");//tanto por ciento
    FileWrite.print(hum);
    FileWrite.print("_V_");
    FileWrite.print(VelSp/(FREQ));//km/h
    FileWrite.print("_PL_");
    FileWrite.print(pluv/(FREQ));//mm por minuto de precipitacion
    FileWrite.print("_D_");
    FileWrite.println(dir);//punto cardinal de donde proviene el viento

    FileWrite.close();  
}

//////RTC1307//////
void initRTC(){
  pinMode(SQW_INPUT_PIN, INPUT_PULLUP);
  pinMode(SQW_OUTPUT_PIN, OUTPUT);
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));  
  rtc.begin(); // Call rtc.begin() to initialize the library
  rtc.writeSQW(SQW_SQUARE_1);
  Serial.println("Reloj RTC1307 inicializado");
  //rtc.setTime(00, 30, 18, 7, 8, 3, 17);  // Uncomment to manually set time
  //rtc.set24Hour(); // Use rtc.set12Hour to set to 12-hour mode
}
void clockRTC(){
  rtc.update();
}
void printTime(){
  Serial.print(String(rtc.hour()) + ":"); // Print hour
  if (rtc.minute() < 10)
    Serial.print('0'); // Print leading '0' for minute
  Serial.print(String(rtc.minute()) + ":"); // Print minute
  if (rtc.second() < 10)
    Serial.print('0'); // Print leading '0' for second
  Serial.print(String(rtc.second())); // Print second
  if (rtc.is12Hour()) // If we're in 12-hour mode
  {
    // Use rtc.pm() to read the AM/PM state of the hour
    if (rtc.pm()) Serial.print(" PM"); // Returns true if PM
    else Serial.print(" AM");
  }
  Serial.print(" ");
#ifdef PRINT_EU_DATE//self changed
  Serial.print(String(rtc.date()) + "/" +   // Print month
                 String(rtc.month()) + "/");  // Print date
#else//USA Time
  Serial.print(String(rtc.month()) + "/" +    // (or) print date
                 String(rtc.date()) + "/"); // Print month
#endif
  Serial.print(String(rtc.year()));        // Print year
  Serial.print(" ");
}

//////BMP085//////
void initBmp085(){
  bmp085.Init();//inicialización comunicacion I2C
  Serial.println("Sensor BMP085 incializado");
}
void sensorBmp085(){
  temp=bmp085.GetTemperature();
  pres=bmp085.GetPressure();
}

//////HH10D//////
void initHH10D(){
  HH10D::initialize();
  Serial.println("Sensor HH10D incializado");
}
void sensorHH10D(){
  hum=HH10D::getHumidity(HH10D_SENSITIVITY, HH10D_OFFSET);
}

//////SPARKFUN STATION//////
void initSparkStation() {
  pinMode(pinPluviometro,INPUT);
  pinMode(pinAnem,INPUT);
  digitalWrite(pinAnem, HIGH);
  attachInterrupt(digitalPinToInterrupt(2), countAnemometer, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), countPluviometer, FALLING);
  pinMode(pinReset, OUTPUT);
  digitalWrite(pinReset, HIGH);
  Serial.println("SparkStation inicializado");
}
void countAnemometer(){
  cuentaAnem++;
}
void countPluviometer(){
  cuentaPluv++;
}
void clearCounters(){
    cuentaAnem=0;
    VelSp=0.0;
    cuentaPluv=0;
}
void sensorSparkStation(){
  VelSp= (cuentaAnem*2.4); //km/h 
  dir=WindDirection(pinVeleta);
  pluv = (cuentaPluv * 0.2794 * 0.5 * 60);//mm por ciclo(por minuto)
  //cada cierre de contacto del reed del Pluv provoca dos falling por lo que cuenta
  //dos cierres, asi que lo que dividimos entre 2
}
String WindDirection(int pinAnalog){
  String veleta;
  float s;
  int i = analogRead(pinAnalog); 
  s=5.0/1024.0*(float)i;
  
  if(s>=3.83&&s<=3.87){
    veleta="N";
  }
  else if(s>=3.43&&s<=3.47){
    veleta="NNW";
  }
  else if((s>=4.33)&&(s<=4.36)){
    veleta="NW";
  }
  else if((s>=4.04 )&&(s<= 4.07)){
    veleta="NWW";
  }
  else if(s>=4.61&&s<=4.64){
    veleta="W";
  }
  else if((s>=2.93 )&&(s<=2.96 )){
    veleta="WSW";
  }
  else if(s>=3.08&&s<=3.11){
    veleta="SW";
  }
  else if((s>=1.19 )&&(s<= 1.22)){
    veleta="SWS";
  }
  else if((s>=1.40 )&&(s<=1.43 )){
    veleta="S";
  }
  else if((s>= 0.60)&&(s<=0.63 )){
    veleta="SSE";
  }
  else if((s>= 0.89)&&(s<=0.92 )){
    veleta="SE";
  }
  else if((s>=0.30 )&&(s<=0.33 )){
    veleta="SEE";
  }
  else if(s>=0.44&&s<=0.47){
    veleta="E";
  }
  else if((s>=0.39 )&&(s<=0.42 )){
    veleta="ENE";
  }
  else if((s>= 2.26)&&(s<=2.29 )){
    veleta="NE";
  }
  else if((s>=1.98)&&(s<=2.01)){
    veleta="NEN";
  }
  else{
    veleta="N/A";
  }
  return veleta;
}
