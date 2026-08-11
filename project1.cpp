#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <DHT.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// PIN DEFINITIONS
// =====================================================

// MQ-4 Methane
#define MQ4_PIN 34

// DHT22
#define DHT_PIN 4
#define DHT_TYPE DHT22

// LDR
#define LDR_PIN 33

// LEDs
#define GREEN_LED 14
#define YELLOW_LED 27
#define RED_LED 26

// Buzzer
#define BUZZER_PIN 25

// Relay
#define RELAY_PIN 13

// Emergency Push Button
#define BUTTON_PIN 16

// =====================================================
// RFID RC522 SPI PINS
// =====================================================

#define RFID_SS 15
#define RFID_RST 5
#define RFID_SCK 18
#define RFID_MOSI 23
#define RFID_MISO 19

// =====================================================
// OBJECTS
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);

Adafruit_MPU6050 mpu;

MFRC522 rfid(RFID_SS, RFID_RST);

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// THRESHOLDS
// =====================================================

// These are prototype thresholds.
// They should NOT be treated as real mine-safety limits.

const int METHANE_WARNING = 1800;
const int METHANE_CRITICAL = 2800;

const float TEMP_WARNING = 35.0;
const float TEMP_CRITICAL = 45.0;

const float VIBRATION_WARNING = 2.5;
const float VIBRATION_CRITICAL = 4.0;

const int LIGHT_WARNING = 1200;

// =====================================================
// VARIABLES
// =====================================================

String currentWorker = "No Worker";

int methaneValue = 0;
int lightValue = 0;

float temperature = 0;
float humidity = 0;

float vibration = 0;

bool emergencyPressed = false;

enum SafetyState
{
  SAFE,
  WARNING,
  CRITICAL
};

SafetyState currentState = SAFE;

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" COAL MINE SAFETY MONITORING");
  Serial.println("=================================");

  // -------------------------------
  // GPIO
  // -------------------------------

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Turn everything OFF
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // -------------------------------
  // DHT22
  // -------------------------------

  dht.begin();

  // -------------------------------
  // I2C
  // -------------------------------

  Wire.begin(21, 22);

  // -------------------------------
  // OLED
  // -------------------------------

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED not found!");
  }
  else
  {
    Serial.println("OLED initialized.");

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("COAL MINE SAFETY");

    display.setCursor(0, 20);
    display.println("System Starting...");

    display.display();

    delay(2000);
  }

  // -------------------------------
  // MPU6050
  // -------------------------------

  if (!mpu.begin())
  {
    Serial.println("MPU6050 not found!");
  }
  else
  {
    Serial.println("MPU6050 initialized.");
  }

  // -------------------------------
  // RFID
  // -------------------------------

  SPI.begin(
    RFID_SCK,
    RFID_MISO,
    RFID_MOSI,
    RFID_SS
  );

  rfid.PCD_Init();

  Serial.println("RFID initialized.");

  Serial.println("---------------------------------");
  Serial.println("System Ready.");
  Serial.println("Tap RFID card...");
  Serial.println("---------------------------------");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // 1. Read sensors

  readSensors();

  // 2. Check RFID

  checkRFID();

  // 3. Check emergency button

  checkEmergencyButton();

  // 4. Determine safety condition

  determineSafety();

  // 5. Control LEDs, buzzer and relay

  controlSafetyOutputs();

  // 6. Display information

  updateOLED();

  // 7. Print information

  printSerialData();

  delay(1000);
}

// =====================================================
// READ ALL SENSORS
// =====================================================

void readSensors()
{
  // -------------------------------
  // MQ-4
  // -------------------------------

  methaneValue = analogRead(MQ4_PIN);

  // -------------------------------
  // LDR
  // -------------------------------

  lightValue = analogRead(LDR_PIN);

  // -------------------------------
  // DHT22
  // -------------------------------

  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (!isnan(newTemperature))
  {
    temperature = newTemperature;
  }

  if (!isnan(newHumidity))
  {
    humidity = newHumidity;
  }

  // -------------------------------
  // MPU6050
  // -------------------------------

  sensors_event_t acceleration;
  sensors_event_t gyro;
  sensors_event_t temp;

  mpu.getEvent(
    &acceleration,
    &gyro,
    &temp
  );

  // Calculate acceleration magnitude

  vibration = sqrt(
    acceleration.acceleration.x *
    acceleration.acceleration.x +

    acceleration.acceleration.y *
    acceleration.acceleration.y +

    acceleration.acceleration.z *
    acceleration.acceleration.z
  );
}

// =====================================================
// RFID
// =====================================================

void checkRFID()
{
  // No new card
  if (!rfid.PICC_IsNewCardPresent())
  {
    return;
  }

  if (!rfid.PICC_ReadCardSerial())
  {
    return;
  }

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10)
    {
      uid += "0";
    }

    uid += String(
      rfid.uid.uidByte[i],
      HEX
    );
  }

  uid.toUpperCase();

  Serial.print("RFID UID: ");
  Serial.println(uid);

  // ---------------------------------
  // Worker identification
  // ---------------------------------

  if (uid == "12345678")
  {
    currentWorker = "Worker 1";
  }
  else if (uid == "AABBCCDD")
  {
    currentWorker = "Worker 2";
  }
  else
  {
    currentWorker = "Unknown";
  }

  Serial.print("Worker: ");
  Serial.println(currentWorker);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// =====================================================
// EMERGENCY BUTTON
// =====================================================

void checkEmergencyButton()
{
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    emergencyPressed = true;

    Serial.println("!!! EMERGENCY BUTTON PRESSED !!!");
  }
  else
  {
    emergencyPressed = false;
  }
}

// =====================================================
// SAFETY ANALYSIS
// =====================================================

void determineSafety()
{
  // Emergency button has highest priority

  if (emergencyPressed)
  {
    currentState = CRITICAL;
    return;
  }

  // -------------------------------
  // Critical conditions
  // -------------------------------

  if (
    methaneValue >= METHANE_CRITICAL ||
    temperature >= TEMP_CRITICAL ||
    vibration >= VIBRATION_CRITICAL
  )
  {
    currentState = CRITICAL;
    return;
  }

  // -------------------------------
  // Warning conditions
  // -------------------------------

  if (
    methaneValue >= METHANE_WARNING ||
    temperature >= TEMP_WARNING ||
    vibration >= VIBRATION_WARNING ||
    lightValue <= LIGHT_WARNING
  )
  {
    currentState = WARNING;
    return;
  }

  // -------------------------------
  // Everything normal
  // -------------------------------

  currentState = SAFE;
}

// =====================================================
// OUTPUT CONTROL
// =====================================================

void controlSafetyOutputs()
{
  // Turn everything OFF first

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // -------------------------------
  // SAFE
  // -------------------------------

  if (currentState == SAFE)
  {
    digitalWrite(GREEN_LED, HIGH);
  }

  // -------------------------------
  // WARNING
  // -------------------------------

  else if (currentState == WARNING)
  {
    digitalWrite(YELLOW_LED, HIGH);

    // Slow warning beep

    tone(BUZZER_PIN, 1000, 200);
  }

  // -------------------------------
  // CRITICAL
  // -------------------------------

  else if (currentState == CRITICAL)
  {
    digitalWrite(RED_LED, HIGH);

    digitalWrite(RELAY_PIN, HIGH);

    // Continuous alarm

    tone(BUZZER_PIN, 2000);
  }
}

// =====================================================
// OLED DISPLAY
// =====================================================

void updateOLED()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // -------------------------------
  // Title
  // -------------------------------

  display.setCursor(0, 0);
  display.println("COAL MINE SAFETY");

  // -------------------------------
  // Worker
  // -------------------------------

  display.setCursor(0, 10);
  display.print("Worker: ");
  display.println(currentWorker);

  // -------------------------------
  // Methane
  // -------------------------------

  display.setCursor(0, 20);
  display.print("CH4: ");
  display.println(methaneValue);

  // -------------------------------
  // Temperature
  // -------------------------------

  display.setCursor(0, 30);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  // -------------------------------
  // Humidity
  // -------------------------------

  display.setCursor(0, 40);
  display.print("Hum: ");
  display.print(humidity);
  display.println("%");

  // -------------------------------
  // Status
  // -------------------------------

  display.setCursor(0, 50);

  if (currentState == SAFE)
  {
    display.print("STATUS: SAFE");
  }
  else if (currentState == WARNING)
  {
    display.print("STATUS: WARNING");
  }
  else
  {
    display.print("STATUS: CRITICAL");
  }

  display.display();
}

// =====================================================
// SERIAL MONITOR
// =====================================================

void printSerialData()
{
  Serial.println();
  Serial.println("========== SENSOR DATA ==========");

  Serial.print("Worker       : ");
  Serial.println(currentWorker);

  Serial.print("Methane      : ");
  Serial.println(methaneValue);

  Serial.print("Temperature  : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity     : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Light        : ");
  Serial.println(lightValue);

  Serial.print("Vibration    : ");
  Serial.println(vibration);

  Serial.print("Safety       : ");

  if (currentState == SAFE)
  {
    Serial.println("SAFE");
  }
  else if (currentState == WARNING)
  {
    Serial.println("WARNING");
  }
  else
  {
    Serial.println("CRITICAL");
  }

  Serial.println("=================================");
}