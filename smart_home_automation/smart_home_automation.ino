/*
 * Smart Home Automation
 * 
 * Created on : Nov 23, 2022 
 *
 * Author: Parikshit Pagare
 * github.com/parikshitpagare
 * linkedin.com/in/parikshitpagare 
 * 
 * Project link: github.com/parikshitpagare
 * 
 * MIT License 
 */

#include "BluetoothSerial.h"
#include <Wire.h>
#include "DHT.h"
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Ticker.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

/* Using core 1 of ESP32 */
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

/* Sensor pins */
#define DHTPIN 33                                         // DHT11 temperature sensor (GPIO 33)
#define DHTTYPE DHT11
#define lightSensor 26                                    // LDR sensor (GPIO 25 )
#define smokeSensor 25                                    // MQ2 smoke and gas sensor (GPIO 26 )
#define touchSensor 4                                     // Touch sensor (GPIO 4)
#define echo 2                                            // Ultrasonic sensor echo pin (GPIO 32)
#define trigger 15                                        // Ultrasonic sensor trigger pin (GPIO 33)
         
/* Relay pins */          
#define fanRelay 17                                       // Relay for fan (GPIO )
#define lightRelay 16                                     // Relay for light (GPIO )

/* Buzzer pins */
#define smokeBuzzer 14                                    // Buzzer for alerting smoke or gas (GPIO )
#define touchBuzzer 27                                    // Buzzer for alerting touch (GPIO )

/* Led pins */
#define smokeLed 5                                        // Led for alerting smoke (GPIO )
#define touchLed 19                                       // Led for alerting touch (GPIO )
#define ultrasonicLed 18                                  // Led for alerting when someone in range (GPIO )

/* Setting OLED display parameters */
#define SCREEN_WIDTH 128                                  // OLED display width, in pixels
#define SCREEN_HEIGHT 64                                  // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C                               // i2c address for OLED display
#define OLED_RESET 4 

/* Defining objects */
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BluetoothSerial SerialBT;                                 
Ticker ultrasonic;

/* Defining queues */
static QueueHandle_t tempReading;
static QueueHandle_t lightReading;
static QueueHandle_t smokeAlarm;
static QueueHandle_t touchAlarm;

/* Defining task handles */
TaskHandle_t autoFan_handle = NULL;
TaskHandle_t autoLight_handle = NULL;

/* Status for OLED display indicators */
bool fanStatus = false;
bool lightStatus = false;
bool smokeStatus = false;
bool touchStatus = false;
bool ultrasonicStatus = false;

/*
* ---------------------------------------------------------------------------------------------------------------------------------
* Setup  
* ---------------------------------------------------------------------------------------------------------------------------------
*/

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);                                     // Serial baud rate
  Wire.begin();
  SerialBT.begin("ESP32");
  Serial.println("The device started, now you can pair it with bluetooth!");
  dht.begin();
  ultrasonic.attach(1, ultrasonicDetect);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);      // Initialize OLED with I2C address 0x3C
  
  /* Defining pin modes */  
  pinMode(fanRelay, OUTPUT);
  pinMode(lightRelay, OUTPUT);
  pinMode(smokeLed, OUTPUT);
  pinMode(touchLed, OUTPUT);
  pinMode(ultrasonicLed, OUTPUT);
  pinMode(smokeBuzzer, OUTPUT);
  pinMode(touchBuzzer, OUTPUT);
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

  /* Relays off at start */
  digitalWrite(fanRelay, HIGH);                              
  digitalWrite(lightRelay, HIGH);                            
  
  /* Buzzers off at start */
  digitalWrite(touchBuzzer, LOW);                            
  digitalWrite(smokeBuzzer, LOW);                            

  /* Leds off at start */
  digitalWrite(smokeLed, LOW);                           
  digitalWrite(touchLed, LOW);                                   
  digitalWrite(ultrasonicLed, LOW);                                

  /* OLED display at start */
  introDisplay();

  /* Creating queues */
  tempReading = xQueueCreate(10, sizeof(int));
  lightReading = xQueueCreate(10, sizeof(int));
  
  /* Creating tasks */
  xTaskCreatePinnedToCore (tempRead, "Temp read", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore (autoFan, "Auto fan", 2048, NULL, 1, &autoFan_handle, app_cpu); 
  xTaskCreatePinnedToCore (lightRead, "Light read", 1024, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore (autoLight, "Auto light", 1024, NULL, 1, &autoLight_handle, app_cpu); 
  xTaskCreatePinnedToCore (smokeDetect, "Smoke detect", 1024, NULL, 1, NULL, app_cpu);   
  xTaskCreatePinnedToCore (touchDetect, "Touch read", 1024, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore (switchControl, "Switch control", 4096, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore (indicatorDisplay, "OLED display", 4096*2, NULL, 1, NULL, app_cpu);
    
/* Suspending auto mode tasks at start */
  vTaskSuspend (autoFan_handle);
  vTaskSuspend (autoLight_handle);
}

void loop() {
  vTaskDelay(500 / portTICK_PERIOD_MS);
}

/*
* ---------------------------------------------------------------------------------------------------------------------------------
* Temperature monitoring and fan control 
* ---------------------------------------------------------------------------------------------------------------------------------
*/

/* Task for temperature sensing using DHT11 */
void tempRead(void *parameter) {
 int t = 0;
  
  while (true) {
    t = dht.readTemperature();
    
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }
    
    /* Send temperature values via bluetooth */
    SerialBT.print("#");
    SerialBT.print(t);
    SerialBT.print("?");
    
    /* Print temperature and humidity values on serial monitor */
    Serial.print("Temperature: "); 
    Serial.print(t); 
    Serial.println(" °C"); 
    
    xQueueSend (tempReading, (void*)&t, 10);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

/* Task for fan control in auto mode */
void autoFan(void *parameter) {
  int tempValue;
  
  while (true) { 
    xQueueReceive(tempReading, (void *)&tempValue, portMAX_DELAY);    
    
    if (tempValue >= 33) {
      SerialBT.print ("Fan on?"); 
      digitalWrite(fanRelay,LOW); ;
      fanStatus = true;
    }
    else if (tempValue < 33) {
      SerialBT.print ("Fan off?");
      digitalWrite(fanRelay,HIGH);
      fanStatus = false; 
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

/*
* ---------------------------------------------------------------------------------------------------------------------------------
* LDR based bulb control 
* ---------------------------------------------------------------------------------------------------------------------------------
*/

/* Task for light intensity sensing using LDR */
void lightRead(void *parameter) {
  int lightValue;
  
  while (true) {
    lightValue = analogRead(lightSensor); 
    Serial.print("Light intensity: ");
    Serial.println(lightValue);

    xQueueSend (lightReading, (void*)&lightValue, 10);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  } 
}

/* Task for bulb control in auto mode */
void autoLight(void *parameter) {
  int lightValue;
  
  while (true) { 
    xQueueReceive(lightReading, (void *)&lightValue, portMAX_DELAY);    
    
    if (lightValue >= 2200) {
      SerialBT.print("Bulb on?");
      digitalWrite(lightRelay,LOW);
      lightStatus = true;
    }
    else if (lightValue < 2200) {
      SerialBT.print("Bulb off?");
      digitalWrite(lightRelay,HIGH); 
      lightStatus = false;
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  } 
}

/*
* ---------------------------------------------------------------------------------------------------------------------------------
* Safety and Security system
* ---------------------------------------------------------------------------------------------------------------------------------
*/

/* Task for detecting smoke or gas using MQ2 sensor */
void smokeDetect(void *parameter) {
  int smokeValue;
  
  while (true) {
    smokeValue = analogRead(smokeSensor); 
    Serial.print("Smoke: ");
    Serial.println(smokeValue);
    
    if (smokeValue >= 3200) {
      SerialBT.print("Smoke active?");
      digitalWrite(smokeLed, HIGH);
      digitalWrite(smokeBuzzer, HIGH);
      smokeStatus = true;      
    }
    else if (smokeValue < 3200) {
      SerialBT.print("Smoke inactive?");
      digitalWrite(smokeLed, LOW);
      digitalWrite(smokeBuzzer, LOW);
      smokeStatus = false; 
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);      
  }
}

/* Task for detecting touch using inbuilt touch sensor */
void touchDetect(void *parameter) {
  int touchValue;
  
  while (true) {
    touchValue = (touchRead(touchSensor));  
    if (touchValue < 20) {
      SerialBT.print("Touch active?");
      digitalWrite(touchLed, HIGH);
      digitalWrite(touchBuzzer, HIGH);
      touchStatus = true; 
    }
  }
}

/* Task for finding distance using Ultrasonic sensor */
void ultrasonicDetect() { 
  int distance;
  int duration;

  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);

  duration = pulseIn(echo, HIGH);
  distance = (duration / 2) * 0.0343;

  Serial.print("Distance: ");
  Serial.println(distance);

  if (distance > 20) {
    SerialBT.print("Ultrasonic inactive?");
    digitalWrite(ultrasonicLed, LOW);
    ultrasonicStatus = false;
  }
  else if (distance <= 20) {
    SerialBT.print("Ultrasonic active?");
    digitalWrite(ultrasonicLed, HIGH);
    ultrasonicStatus = true;
  }
}

/*
* ---------------------------------------------------------------------------------------------------------------------------------
* App based switch controls
* ---------------------------------------------------------------------------------------------------------------------------------
*/

/* Task for controlling relays and alarms using app */
void switchControl(void *parameter) {
  char input;
  
  while (true) {
    if (SerialBT.available() > 0) {
      input = SerialBT.read();
      switch (input) {
        /* Select manual mode */
        case 'M': {
          vTaskSuspend (autoFan_handle);
          vTaskSuspend (autoLight_handle);
          break;
        }
        /* Switch on fan */
        case 'F': {
          SerialBT.print("Fan on?");
          digitalWrite(fanRelay, LOW);
          fanStatus = true;
          break;
        }
        /* Switch off fan */
        case 'Y': {
          SerialBT.print("Fan off?");
          digitalWrite(fanRelay, HIGH);  
          fanStatus = false;       
          break;
        }
        /* Switch on light */
        case 'L': {
          SerialBT.print("Bulb on?");
          digitalWrite(lightRelay, LOW);
          lightStatus = true;
          break;
        }
        /* Switch off light  */
        case 'Z': {
          SerialBT.print("Bulb off?");
          digitalWrite(lightRelay, HIGH);
          lightStatus = false;
          break;
        }
        /* Select automatic mode */
        case 'A': {
          vTaskResume(autoFan_handle);
          vTaskResume(autoLight_handle);
          break;
        }
        /* Select off mode */
        case 'O': {
          vTaskSuspend(autoFan_handle);
          vTaskSuspend(autoLight_handle);
          SerialBT.print("Fan off?");
          SerialBT.print("Bulb off?");
          digitalWrite(fanRelay, HIGH);
          digitalWrite(lightRelay, HIGH);
          fanStatus = false;
          lightStatus = false;
          break;
        }
        /* Turn off touch alarm */
        case 'T': {
          SerialBT.print("Touch inactive?");
          digitalWrite(touchBuzzer, LOW);
          digitalWrite(touchLed, LOW);
          touchStatus = false; 
          break;
        }
      }
    }   
  }
}

/* Task for temperature display on OLED */
void indicatorDisplay(void *parameter) {
  int tempValue;

  const unsigned char fanIndicator [] PROGMEM = {
    0x01, 0xf8, 0x00, 0x07, 0x0e, 0x00, 0x0d, 0xc3, 0x00, 0x1b, 0xe1, 0x80, 0x11, 0xe0, 0x80, 0x30, 
	  0xe0, 0xc0, 0x30, 0x66, 0xc0, 0x30, 0x7f, 0xc0, 0x33, 0xff, 0xc0, 0x33, 0xde, 0xc0, 0x17, 0xc0, 
	  0x80, 0x1b, 0x81, 0x80, 0x0f, 0x83, 0x00, 0x06, 0x06, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 
	  0x00, 0xf0, 0x00, 0x01, 0xf8, 0x00, 0x07, 0xfe, 0x00, 0x07, 0xfe, 0x00
  };
  
  const unsigned char lightIndicator [] PROGMEM = {
    0x00, 0x60, 0x00, 0x01, 0xfc, 0x00, 0x07, 0xc6, 0x00, 0x07, 0xc2, 0x00, 0x0f, 0xf1, 0x00, 0x0f, 
  	0xf1, 0x00, 0x0f, 0xfb, 0x00, 0x0f, 0xff, 0x00, 0x0f, 0xff, 0x00, 0x07, 0xfe, 0x00, 0x07, 0xfe, 
  	0x00, 0x03, 0xfc, 0x00, 0x01, 0xfc, 0x00, 0x01, 0xf8, 0x00, 0x01, 0x00, 0x00, 0x01, 0x80, 0x00, 
  	0x01, 0xf8, 0x00, 0x01, 0xf8, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00
  };

  const unsigned char smokeIndicator [] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x30, 0x00, 0x00, 0x38, 0x00, 0x00, 0x38, 0x00, 0x02, 
	  0x78, 0x00, 0x07, 0xf8, 0x00, 0x07, 0xfa, 0x00, 0x07, 0x3e, 0x00, 0x07, 0x3f, 0x00, 0x03, 0x0f, 
	  0x00, 0x17, 0x0f, 0x80, 0x1e, 0x0f, 0x80, 0x1e, 0x07, 0x80, 0x1e, 0x07, 0x80, 0x1e, 0x07, 0x80, 
	  0x0e, 0x07, 0x80, 0x0f, 0x0f, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x60, 0x00
  };
  
  const unsigned char touchIndicator [] PROGMEM = {
    0x00, 0x60, 0x00, 0x01, 0xf8, 0x00, 0x03, 0xfc, 0x00, 0x07, 0x8e, 0x00, 0x07, 0x0e, 0x00, 0x06, 
	  0x06, 0x00, 0x06, 0x06, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0f, 0x00, 0x1f, 0xff, 
	  0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 
	  0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x80
  };

  const unsigned char ultrasonicIndicator [] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x01, 0xf8, 0x00, 0x03, 0xfc, 0x00, 0x03, 0xfc, 0x00, 0x03, 
  	0xfc, 0x00, 0x03, 0xfc, 0x00, 0x03, 0xfc, 0x00, 0x03, 0xfc, 0x00, 0x01, 0xf8, 0x00, 0x01, 0xf8, 
  	0x00, 0x01, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x07, 0xfe, 0x00, 0x1f, 0xff, 0x80, 
	  0x3f, 0xff, 0xc0, 0x7f, 0xff, 0xe0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0
  };
  
  while (true) {
    xQueueReceive(tempReading, (void*)&tempValue, portMAX_DELAY); 
    
    display.clearDisplay();                                     // Clear the display
    
    /* Displaying temperature value */
    display.setTextColor(WHITE);                                // Set the color
    display.setTextSize(2);                                     // Set the font size
    display.setCursor(10,10);                                    // Set the cursor coordinates
    display.print("Temp ");
    display.print(tempValue);
    display.print((char)247);
    display.print("C");
    
    if (fanStatus == true) {
      display.drawBitmap(2, 36, fanIndicator, 20, 20, WHITE);
    }
    else if (fanStatus == false) {
      display.setCursor(6, 38);
      display.print("-");
    }

    if (lightStatus == true) {
      display.drawBitmap(28, 36, lightIndicator, 20, 20, WHITE);
    }
    else if (lightStatus == false) {
      display.setCursor(32, 38);
      display.print("-");
    }
    
    if (smokeStatus == true) {
      display.drawBitmap(54, 36, smokeIndicator, 20, 20, WHITE);
    }
    else if (smokeStatus == false) {
      display.setCursor(58, 38);
      display.print("-");
    }
    
    if (touchStatus == true) {
      display.drawBitmap(80, 36, touchIndicator, 20, 20, WHITE);
    }
    else if (touchStatus == false) {
      display.setCursor(84, 38);
      display.print("-");
    }
    
    if (ultrasonicStatus == true) {
      display.drawBitmap(106, 36, ultrasonicIndicator, 20, 20, WHITE);
    }
    else if (ultrasonicStatus == false) {
      display.setCursor(110, 38);
      display.print("-");
    }
    
    display.display();

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}  

void introDisplay() {
  display.clearDisplay();
  
  const unsigned char intro [] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x7b, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0xde, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xe3, 0x80, 0x00, 0x01, 0x80, 0x00, 0x01, 0xc7, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xe3, 0xbf, 0x00, 0x07, 0xe0, 0x00, 0xfd, 0xc7, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0xbf, 0x00, 0x0f, 0xf0, 0x00, 0xfd, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0xbf, 0x00, 0x1e, 0x78, 0x00, 0xfd, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0xb8, 0x00, 0x7c, 0x3e, 0x00, 0x1d, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0xb8, 0x00, 0xf0, 0x0f, 0x00, 0x1d, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0xb8, 0x03, 0xe0, 0x07, 0xc0, 0x1d, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x07, 0xc0, 0x03, 0xe0, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0f, 0x00, 0x00, 0xf0, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x3e, 0x00, 0x00, 0x7c, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x7c, 0x00, 0x00, 0x3e, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0xfe, 0x00, 0x00, 0x7f, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0xfe, 0x00, 0x00, 0x7f, 0x01, 0xdb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xdb, 0x80, 0x0e, 0x07, 0xe0, 0x70, 0x01, 0xfb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xdf, 0x80, 0x0e, 0x1f, 0xf8, 0x70, 0x01, 0xdb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xdf, 0x80, 0x0e, 0x3e, 0x7c, 0x70, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xdf, 0x80, 0x0e, 0x78, 0x1e, 0x70, 0x01, 0xdb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xdf, 0x80, 0x0e, 0x73, 0xce, 0x70, 0x01, 0xfb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xdf, 0x80, 0x0e, 0x67, 0xe6, 0x70, 0x01, 0xfb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xcb, 0x80, 0x0e, 0x47, 0xe6, 0x70, 0x01, 0xfb, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0e, 0x06, 0x60, 0x70, 0x01, 0xd3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0e, 0x06, 0x60, 0x70, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0e, 0x06, 0x60, 0x70, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0e, 0x06, 0x60, 0x70, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0f, 0xe6, 0x67, 0xf0, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0f, 0xf6, 0x6f, 0xf0, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x0f, 0xf6, 0x6f, 0xf0, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x00, 0x06, 0x60, 0x00, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x00, 0x06, 0x7c, 0x00, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x80, 0x00, 0x06, 0x7f, 0xe0, 0x01, 0xc3, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xe3, 0x80, 0x00, 0x06, 0x77, 0xf0, 0x01, 0xc7, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xe3, 0x80, 0x00, 0x06, 0x67, 0x7e, 0x01, 0xc7, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xf3, 0x80, 0x00, 0x06, 0x67, 0x7f, 0x01, 0xcf, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xfc, 0x3e, 0x67, 0x7f, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x7e, 0x67, 0x73, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xfc, 0xfe, 0x27, 0x73, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6, 0x07, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6, 0x02, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0xe6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x01, 0xf6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0xf6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  
  display.drawBitmap(0, 0, intro, 128, 64, WHITE);
  
  display.display();
  delay(4000);
  display.clearDisplay();

  display.setTextColor(WHITE);                                // Set the color
  display.setTextSize(2);                                     // Set the font size
  display.setCursor(6,10);                                    // Set the cursor coordinates
  display.print("Smart Home");
  display.setCursor(6,40);
  display.print("Automation");

  display.display();
  delay(4000);
  display.clearDisplay();
}