#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// I2C Pins for ESP32
#define SDA_PIN 21
#define SCL_PIN 23

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// OLED SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Keypad 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {14, 18, 25, 27};
byte colPins[COLS] = {12, 13, 32, 33};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// DHT22
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Relay pins
#define HEATER1_RELAY 5
#define HEATER2_RELAY 19
#define FAN_RELAY 26

// Global variables
String input = "";
int setTemp = 0;
int setTimer = 0;
bool manualMode = false;
bool fanStatus = false;
unsigned long startTime = 0;

float temperature = NAN;
float humidity = NAN;

unsigned long lastDHTRead = 0;
unsigned long lastOLEDUpdate = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(HEATER1_RELAY, OUTPUT);
  pinMode(HEATER2_RELAY, OUTPUT);
  pinMode(FAN_RELAY, OUTPUT);
  turnOffHeater();
  turnOffFan();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM ON!");
  delay(2000);
  lcd.clear();
  lcd.print("Set Temp:");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (1);
  }
  display.clearDisplay();
  display.display();

  dht.begin();
}

void loop() {
  if (millis() - lastDHTRead >= 2000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
      Serial.print("Temp: ");
      Serial.print(temperature);
      Serial.print(" C, Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");
    } else {
      Serial.println("DHT read failed!");
      temperature = NAN;
      humidity = NAN;
    }
    lastDHTRead = millis();
  }

  char key = keypad.getKey();
  if (key) handleKeypadInput(key);

  if (manualMode) runManualMode();

  if (millis() - lastOLEDUpdate >= 1000) {
    updateOLED();
    lastOLEDUpdate = millis();
  }
}

void handleKeypadInput(char key) {
  switch (key) {
    case '*':
      input = "";
      lcd.clear();
      lcd.print("Input Reset");
      delay(1000);
      lcd.clear();
      lcd.print("Set Temp:");
      break;

    case 'B':
      if (input.length() > 0) {
        setTemp = input.toInt();
        if (setTemp > 80) {
          lcd.clear();
          lcd.print("Max temp 80C!");
          delay(2000);
          lcd.clear();
          lcd.print("Set Temp:");
          input = "";
          return;
        }

        lcd.clear();
        lcd.print("Set Timer:");
        input = "";
        while (true) {
          char timerKey = keypad.getKey();
          if (timerKey == '#') break;
          if (isDigit(timerKey)) {
            input += timerKey;
            lcd.setCursor(0, 1);
            lcd.print(input);
          }
        }

        setTimer = input.toInt();
        startTime = millis();
        manualMode = true;
        turnOnHeater();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Running...");
        lcd.setCursor(0, 1);
        lcd.print("Mode: Manual");
      }
      input = "";
      break;

    case 'C':
      stopSystem();
      manualMode = false;
      lcd.clear();
      lcd.print("System Stopped");
      delay(2000);
      lcd.clear();
      lcd.print("Set Temp:");
      break;

    case 'D':
      fanStatus = !fanStatus;
      if (fanStatus) {
        turnOnFan();
        lcd.clear();
        lcd.print("Fan On");
      } else {
        turnOffFan();
        lcd.clear();
        lcd.print("Fan Off");
      }
      delay(1000);
      lcd.clear();
      lcd.print("Set Temp:");
      break;

    default:
      if (isDigit(key)) {
        if (input.length() < 2) {
          input += key;
          lcd.setCursor(0, 1);
          lcd.print(input);
        }
      }
      break;
  }
}

void runManualMode() {
  if (isnan(temperature)) return;

  unsigned long elapsed = (millis() - startTime) / 1000;
  if (elapsed >= setTimer * 60) {
    manualMode = false;
    stopSystem();
    lcd.clear();
    lcd.print("Finished!");
    delay(3000);
    lcd.clear();
    lcd.print("Set Temp:");
    return;
  }

  if (temperature > 80) {
    turnOffHeater();
    lcd.clear();
    lcd.print("Max Temp 80C!");
    delay(2000);
    lcd.clear();
    lcd.print("Set Temp:");
    manualMode = false;
    return;
  }

  if (temperature < setTemp - 0.5) {
    turnOnHeater();
  } else if (temperature > setTemp + 5) {
    turnOffHeater();
  }
}

void stopSystem() {
  turnOffHeater();
  turnOffFan();
}

void turnOnHeater() {
  digitalWrite(HEATER1_RELAY, LOW);
  digitalWrite(HEATER2_RELAY, LOW);
  Serial.println("Heater ON");
}

void turnOffHeater() {
  digitalWrite(HEATER1_RELAY, HIGH);
  digitalWrite(HEATER2_RELAY, HIGH);
  Serial.println("Heater OFF");
}

void turnOnFan() {
  digitalWrite(FAN_RELAY, LOW);
}

void turnOffFan() {
  digitalWrite(FAN_RELAY, HIGH);
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (isnan(temperature) || isnan(humidity)) {
    display.setCursor(0, 0);
    display.println("DHT Sensor Error");
  } else {
    display.setCursor(0, 0);
    display.printf("Temp: %.1f C", temperature);

    display.setCursor(0, 10);
    display.printf("Humidity: %.1f %%", humidity);

    display.setCursor(0, 20);
    display.printf("Target: %d C", setTemp);

    unsigned long elapsed = (millis() - startTime) / 1000;
    int hrs = elapsed / 3600;
    int mins = (elapsed % 3600) / 60;
    int secs = elapsed % 60;

    display.setCursor(0, 30);
    display.printf("Elapsed: %02d:%02d:%02d", hrs, mins, secs);
    
    display.setCursor(0, 40);
    display.printf("Fan: %s", fanStatus ? "ON" : "OFF");
  }

  display.display();
}
