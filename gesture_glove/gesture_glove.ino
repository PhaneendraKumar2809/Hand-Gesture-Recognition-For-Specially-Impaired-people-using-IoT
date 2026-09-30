#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ================= DISPLAY =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;

bool displayOk = false;
bool mpuOk = false;

// ================= Finger Inputs =================
const int potPins[5] = {34, 35, 32, 33, 25};

const int POT_ON_THRESHOLD[5]  = {2200, 2100, 2150, 2250, 2000};
const int POT_OFF_THRESHOLD[5] = {1800, 1700, 1750, 1850, 1600};

bool fingerActive[5] = {false, false, false, false, false};

// ================= Base Tilt =================
int detectedBase = 0;
int stableBase = 0;
unsigned long baseTimer = 0;
const unsigned long BASE_STABLE_TIME = 200;

// ================= Runtime =================
int currentGesture = 0;

// ================= MPU STORAGE =================
float last_ax = 0, last_ay = 0, last_az = 0;

// ================= MESSAGE STORAGE =================
const char* const baseMessages[6][32] PROGMEM = {
  {
    "No Gesture","Water","Hungry","Medicine","Bathroom","Pain",
    "Nausea","Dizzy","Sit","Lie Down","Adjust Pillow","Tired",
    "Clean","Thirsty","Snack","Call Nurse",
    "Vitals","Cold","Hot","Blanket",
    "Towel","Wash Face","Brush Teeth","Comb",
    "Clothes","Shoes On","Shoes Off","Glasses",
    "Can't Hear","Can't See","Weak","EMERGENCY"
  },
  {
    "Hello","Thank You","Yes","No","Happy","Sad",
    "Angry","Worried","Confused","Excited","Frustrated","Lonely",
    "Bored","Love You","Miss You","Good Morning",
    "Good Night","See Later","Come","Go Away",
    "Wait","Hurry","Slow","Help Please",
    "Don't Know","Maybe","Later","Now",
    "Tomorrow","Yesterday","Goodbye","Sorry"
  },
  {
    "Light ON","Light OFF","Fan ON","Fan OFF","TV ON","TV OFF",
    "AC ON","AC OFF","Open Door","Close Door","Open Window","Close Window",
    "Phone","Book","Pen","Paper",
    "Laptop","Charger","Curtain Up","Curtain Down",
    "Music","Radio","News","Weather",
    "Call Family","Time","Lock","Unlock",
    "Read","Stop","Menu","Back"
  },
  {
    "Rice","Bread","Water","Milk","Tea","Coffee",
    "Juice","Soup","Vegetable","Meat","Fish","Chicken",
    "Egg","Fruit","Dessert","Salt",
    "Sugar","Spice","Butter","Oil",
    "Hot Food","Cold Food","Soft Food","Sweet",
    "Salty","Sour","Bitter","Too Hot",
    "Too Cold","Delicious","More","Enough"
  },
  {
    "Headache","Fever","Cough","Sore Throat","Chest Pain","Back Pain",
    "Leg Pain","Arm Pain","Stomach Ache","Muscle Pain","Joint Pain","Numbness",
    "Tingling","Weakness","Fatigue","Dizziness",
    "Vomiting","Constipation","Diarrhea","Bleeding",
    "Infection","Allergy","Asthma","Diabetes",
    "Blood Pressure","Heart Problem","Doctor","Ambulance",
    "Hospital","Medication","Urgent","Critical"
  },
  {
    "Work","Meeting","Email","Call","Video Call","Report",
    "Print","Save","Delete","Edit","Review","Submit",
    "Cancel","Pause","Resume","Complete",
    "Deadline","Priority","Schedule","Calendar",
    "Timer","Alarm","Gym","Exercise",
    "Stretch","Meditate","Game","Movie",
    "Music","Podcast","Travel","Vacation"
  }
};

// ================= FILTER =================
int readFiltered(int pin) {
  int sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += analogRead(pin);
  }
  return sum / 5;
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  // CSV HEADER
  Serial.println("timestamp,flex1_raw,flex2_raw,flex3_raw,flex4_raw,flex5_raw,flex1_state,flex2_state,flex3_state,flex4_state,flex5_state,ax,ay,az,base,gesture,predicted,expected");

  Wire.begin(21, 22);

  displayOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  mpuOk = mpu.begin();

  if (mpuOk) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  analogSetWidth(12);
  analogSetAttenuation(ADC_11db);

  if (!displayOk || !mpuOk) {
    while (true);
  }

  displayStartup();
}

// ================= LOOP =================
void loop() {

  currentGesture = readGestures();
  stableBase = detectBaseTilt();

  int safeBase = constrain(stableBase, 0, 5);
  int safeGesture = constrain(currentGesture, 0, 31);

  char buffer[50];
  strcpy_P(buffer, (char*)pgm_read_ptr(&(baseMessages[safeBase][safeGesture])));

  String finalMsg = String(buffer);

  updateDisplay(finalMsg);
  logCSV(finalMsg);
}

// ================= GESTURES =================
int readGestures() {

  int gesture = 0;

  for (int i = 0; i < 5; i++) {
    int value = readFiltered(potPins[i]);

    if (!fingerActive[i] && value >= POT_ON_THRESHOLD[i])
      fingerActive[i] = true;
    else if (fingerActive[i] && value <= POT_OFF_THRESHOLD[i])
      fingerActive[i] = false;

    if (fingerActive[i])
      gesture |= (1 << i);
  }

  static unsigned long lastChangeTime = 0;
  static int lastGesture = 0;

  if (gesture != lastGesture) {
    lastChangeTime = millis();
    lastGesture = gesture;
  }

  if (millis() - lastChangeTime > 100)
    return gesture;

  return currentGesture;
}

// ================= BASE =================
int detectBaseTilt() {

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  // store for logging
  last_ax = ax;
  last_ay = ay;
  last_az = az;

  float axAbs = abs(ax);
  float ayAbs = abs(ay);
  float azAbs = abs(az);

  int newBase;

  if (azAbs > axAbs && azAbs > ayAbs)
    newBase = (az > 0) ? 0 : 1;
  else if (ayAbs > axAbs && ayAbs > azAbs)
    newBase = (ay > 0) ? 2 : 3;
  else
    newBase = (ax > 0) ? 4 : 5;

  if (newBase != detectedBase) {
    detectedBase = newBase;
    baseTimer = millis();
  }

  if (millis() - baseTimer > BASE_STABLE_TIME)
    stableBase = detectedBase;

  return stableBase;
}

// ================= DISPLAY =================
void updateDisplay(String msg) {

  static String lastMsg = "";

  if (msg == lastMsg) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Base:");
  display.print(stableBase);
  display.print(" G:");
  display.println(currentGesture);

  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.println(msg);

  display.display();

  lastMsg = msg;
}

// ================= CSV LOGGER =================
void logCSV(String msg) {

  static unsigned long lastLog = 0;

  if (millis() - lastLog < 200) return;
  lastLog = millis();

  Serial.print(millis()); Serial.print(",");

  int rawValues[5];
  for (int i = 0; i < 5; i++) {
    rawValues[i] = readFiltered(potPins[i]);
    Serial.print(rawValues[i]); Serial.print(",");
  }

  for (int i = 0; i < 5; i++) {
    Serial.print(fingerActive[i]); Serial.print(",");
  }

  Serial.print(last_ax); Serial.print(",");
  Serial.print(last_ay); Serial.print(",");
  Serial.print(last_az); Serial.print(",");

  Serial.print(stableBase); Serial.print(",");
  Serial.print(currentGesture); Serial.print(",");

  Serial.print(msg); Serial.print(",");
  Serial.println("NA"); // expected label later
}

// ================= STARTUP =================
void displayStartup() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Gesture");
  display.println("System Ready");
  display.display();
  delay(1500);
}
