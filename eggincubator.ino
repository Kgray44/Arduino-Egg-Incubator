#include "DFRobot_RGBLCD1602.h"
#include "DFRobot_AHT20.h"
#include <AccelStepper.h>
#include <EEPROM.h>

#define heater1 22
#define heater2 23
#define heater3 24
#define heater4 25

#define m1en 2
#define m1step 3
#define m1dir 4
#define m2en 5
#define m2step 6
#define m2dir 7
#define buzzer 8
#define blower1 9
#define blower2 10

#define touch A0

#define turningtime 120000 //2 min;   -1 means always turning, else input a millis value

float timeoffset;// = 2.1;


const byte motor1Char[8] = {
  0b00110,
  0b00110,
  0b00100,
  0b01110,
  0b01110,
  0b01110,
  0b01110,
  0b01110
};

const byte motor2Char[8] = {
  0b01100,
  0b01100,
  0b00100,
  0b01110,
  0b01110,
  0b01110,
  0b01110,
  0b01110
};

float temp=0;
float hum=0;

int normalDayCount = 18;
int hatchDayCount = 3;
unsigned long lastmillis=0;
unsigned long daymillis=86400000;
unsigned long lastdaymillis=0;
unsigned long lasticonmillis=0;
unsigned long endmillis=0;
unsigned long fanonmillis=40000;
unsigned long fanoffmillis=80000;
unsigned long lastfanmillis=0;
bool fanon = true;

int day=1;

bool icon = false;

bool mode = false;

int touchcount=0;

float lasthour=0;

DFRobot_RGBLCD1602 lcd(/*lcdCols*/16,/*lcdRows*/2);  //16 characters and 2 lines of show
DFRobot_AHT20 aht20;
AccelStepper stepper(1,m1step,m1dir); // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

void setup() {
	Serial.begin(115200);
  
  setupPinmodes();

  startSensor();

  lcd.init();
  lcd.customSymbol(1,motor1Char);
  lcd.customSymbol(2,motor2Char);
  lcd.setRGB(0,255,0);
  lcd.setCursor(0,0);
  lcd.print("Egg Incubator");
  lcd.setCursor(0,1);
  lcd.print("Version 1.0.0");

  delay(2000);

  lcd.clear();
  digitalWrite(blower1, HIGH);
  digitalWrite(m1en,LOW);// Low Level Enable
  digitalWrite(m2en,LOW);// Low Level Enable

  stepper.setAcceleration(25);
  stepper.setMaxSpeed(160);

  //EEPROM.write(8, 26);
  //EEPROM.write(7,1);

  day = EEPROM.read(7);
  float r = EEPROM.read(8);
  timeoffset = r/10;
}

void loop() {
  if (!stepper.isRunning()){
    checkTH();
    if (!mode){
      lcd.setCursor(13,0);
      lcd.print("   ");
      displayTH();
    }
    else {
      lcd.setCursor(14,0);
      lcd.print("  ");
      displayInfo();  
    }
  }
  else {
    if (millis() - lasticonmillis > 300){
      //lcd.setCursor(13,0);
      //lcd.print("  ");
      lcd.setCursor(14,0);
      lcd.write(icon ? 1 : 2);
        //lcd.write(2);
      icon = !icon;
      lasticonmillis = millis();
    }
  }
  Serial.println("Touch: " + String(digitalRead(touch)));
  if (digitalRead(touch) == HIGH){
    while(digitalRead(touch) == HIGH){touchcount++;delay(1);}
    if (touchcount < 5000){
      mode = !mode;
      digitalWrite(buzzer, HIGH);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Mode: ");
      lcd.print(mode ? "Info" : "Temp&Hum");
      delay(500);
      digitalWrite(buzzer, LOW);
      delay(2000);
      lcd.clear();
      touchcount=0;
    }
    else if (touchcount >= 5000) {
      digitalWrite(buzzer, HIGH);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("RST to Day 1!?");
      lcd.setCursor(0,1);
      lcd.print("Hold 10 Sec = Y");
      delay(500);
      digitalWrite(buzzer, LOW);
      touchcount=0;

      while(digitalRead(touch) == LOW);
      while(digitalRead(touch) == HIGH){touchcount++;delay(1);}
      if (touchcount >= 10000){
        day = 1;
        EEPROM.write(7, day);//reset day to 1
        EEPROM.write(8, 0);//reset hour to 0
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Please Reset!");
        lcd.setCursor(0,1);
        lcd.print("Unplug...");
        while(true);  
      }
    }
  }

  if (day <= normalDayCount){
    Serial.println("Normal");
    standardMode();
  }
  else if (day > normalDayCount && day <= normalDayCount+hatchDayCount){
    hatchMode();
  }
  else {
    endMode();
  }

  stepper.run();

  if ((millis()+((timeoffset*1000)*60*60)) - lastdaymillis > daymillis){
    day++;
    EEPROM.write(7, day);
    lastdaymillis = millis();
  }
}

void setupPinmodes(){
  pinMode(heater1, OUTPUT);
  pinMode(heater2, OUTPUT);
  pinMode(heater3, OUTPUT);
  pinMode(heater4, OUTPUT);

  pinMode(m1en, OUTPUT);
  pinMode(m1step, OUTPUT);
  pinMode(m1dir, OUTPUT);
  pinMode(m2en, OUTPUT);
  pinMode(m2step, OUTPUT);
  pinMode(m2dir, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(blower1, OUTPUT);
  pinMode(blower2, OUTPUT);

  pinMode(touch, INPUT);
}

void startSensor(){
  uint8_t status;
  while((status = aht20.begin()) != 0){
    Serial.print("AHT20 sensor initialization failed. error status : ");
    Serial.println(status);
    digitalWrite(buzzer, HIGH);
    delay(500);
    digitalWrite(buzzer, LOW);
    delay(500);
  }
}

void standardMode(){
  setStandardHeater();
  setStandardRotation();
  digitalWrite(blower1, HIGH);
}

void setStandardHeater(){// the temperature we want is 95.5˚F
  if (temp < 96.50){// if temperature is less than 0.75˚ too warm
    if (temp > 96){
      digitalWrite(blower2, HIGH);
      digitalWrite(heater1, HIGH);
      digitalWrite(heater2, HIGH);
      digitalWrite(heater3, LOW);
      digitalWrite(heater4, LOW);
    }
    else if (temp <= 96 && temp >= 95.75){
      digitalWrite(heater1, HIGH);
      digitalWrite(heater2, HIGH);
      digitalWrite(heater3, HIGH);
      digitalWrite(heater4, LOW);
      if (fanon){
        if (millis() - lastfanmillis > fanonmillis){
          analogWrite(blower2, 80);
          fanon = false;
          lastfanmillis = millis();
        }
      }
      else {
        if (millis() - lastfanmillis > fanoffmillis){
          analogWrite(blower2, 255);
          fanon = true;
          lastfanmillis = millis();
        }
      }
    }
    else if (temp < 95.75 && temp >= 88){
      digitalWrite(heater1, HIGH);
      digitalWrite(heater2, HIGH);
      digitalWrite(heater3, HIGH);
      digitalWrite(heater4, HIGH);
      if (fanon){
        if (millis() - lastfanmillis > fanonmillis){
          analogWrite(blower2, 80);
          fanon = false;
          lastfanmillis = millis();
        }
      }
      else {
        if (millis() - lastfanmillis > fanoffmillis){
          analogWrite(blower2, 255);
          fanon = true;
          lastfanmillis = millis();
        }
      }
    }
    else {
      digitalWrite(blower2, HIGH);
      digitalWrite(heater1, HIGH);
      digitalWrite(heater2, HIGH);
      digitalWrite(heater3, HIGH);
      digitalWrite(heater4, HIGH);
    }
  }
  else { // if temperature is ≥ 0.75˚ too warm, shut off the heater (keep 1 on to just keep the heater warmed up)
    digitalWrite(blower2, LOW);
    digitalWrite(heater1, HIGH);
    digitalWrite(heater2, LOW);
    digitalWrite(heater3, LOW);
    digitalWrite(heater4, LOW);
  }
}

void setStandardRotation(){
  if (!stepper.isRunning()){
    if (turningtime == -1){
      stepper.move(800);

      if (hum < 25 || hum > 60){
        digitalWrite(buzzer, HIGH);
        delay(500);
        digitalWrite(buzzer, LOW);
      }
    }
    else {
      if (millis() - lastmillis > turningtime){
        stepper.move(800);
        lastmillis = millis();

        if (hum < 25 || hum > 60){
          digitalWrite(buzzer, HIGH);
          delay(500);
          digitalWrite(buzzer, LOW);
        }
      }
    }
  }
}

void hatchMode(){
  setStandardHeater();
  digitalWrite(blower1, HIGH);
}

void endMode(){
  endmillis = millis();

  digitalWrite(heater1, LOW);
  digitalWrite(heater2, LOW);
  digitalWrite(heater3, LOW);
  digitalWrite(heater4, LOW);
  digitalWrite(blower1, LOW);
  digitalWrite(blower2, LOW);
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Done!");
  lcd.print(" Check eggs");
  while(true){lcd.setCursor(0,1);lcd.print("               ");lcd.setCursor(0,1);lcd.print((millis()-endmillis)/100);delay(1000);}  
}

void displayTH(){
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(temp);
  lcd.setCursor(0,1);
  lcd.print("Hum:");
  lcd.print(hum);
  lcd.print(" Day ");
  lcd.print(day);
  if (day < normalDayCount){
    if (hum < 25){
      lcd.setRGB(255,0,0);
    }
    else if (hum > 60){
      lcd.setRGB(255,0,0);
    }
    else {
      lcd.setRGB(0,255,0);
    }
  }
  else {
    if (hum < 55){
      lcd.setRGB(255,0,0);
    }
    else if (hum > 95){
      lcd.setRGB(255,0,0);
    }
    else {
      lcd.setRGB(0,255,0);
    }
  }
}

void displayInfo(){
  lcd.setCursor(0,0);
  lcd.print("Day ");
  lcd.print(day);
  lcd.print(" Hum:");
  lcd.print(hum,1);
  lcd.setCursor(0,1);
  if (day < normalDayCount){
    if (hum < 25){
      lcd.print("Add Water!");
      lcd.setRGB(255,0,0);
    }
    else if (hum > 60){
      lcd.print("Less Water!");
      lcd.setRGB(255,0,0);
    }
    else {
      lcd.print("Doing Well!");
      lcd.setRGB(0,255,0);
    }
  }
  else {
    if (hum < 55){
      lcd.print("Add Water!");
      lcd.setRGB(255,0,0);
    }
    else if (hum > 95){
      lcd.print("Less Water!");
      lcd.setRGB(255,0,0);
    }
    else {
      lcd.print("Doing Well!");
      lcd.setRGB(0,255,0);
    }
  }
  lcd.setCursor(11,1);
  float currenthour = millis()+((timeoffset*1000)*60*60); //millis
  currenthour = currenthour/1000; //seconds
  currenthour = currenthour/60; // minutes
  currenthour = currenthour/60; // hours
  if (lasthour != currenthour){
    int d = currenthour*10;
    EEPROM.write(8,d);
  }
  lasthour = currenthour;
  lcd.print(currenthour, 1);
}

void checkTH(){
  if(aht20.startMeasurementReady(/* crcEn = */true)){
    temp = aht20.getTemperature_F();
    temp = temp - 4; //temperature error correction;  currently reads 4˚ too hot
    hum = aht20.getHumidity_RH();
  }
}
