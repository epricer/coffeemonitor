#include "HX711ADC.h"
#include <Adafruit_SSD1306.h>
#include <math.h>
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

// initialize the scale analog-digital converter
HX711ADC scale(D3, D2);		

// tare values
float offsetA = 0;
float offsetB = 0;

bool debug = false;

String previousA = "";
String previousB = "";
String settledA = "";
String settledB = "";
long brewTimeAMillis = 0;
long brewTimeBMillis = 0;

// calibrated from strain gauge.
const float rawToGramA = -515.f; 
const float rawToGramB = -55.f;

// used for a cup of coffee
const long GRAMS_PER_CUP = 227;

// weight of coffee carafe
const long CARAFE_WEIGHT_GRAMS = 2325;

// constant of the number 
const byte NUM_READS = 5;

// when switching channels the, the HX711 needs some time to 'settle'
// see http://www.dfrobot.com/image/data/SEN0160/hx711_english.pdf
const long OUTPUT_SETTLING_TIME = 500; 

// time to wait with no changes before publishing new values
const long SETTLE_TIME_MILLIS = 1000 * 5; 

// when a pot is empty, and then exceeds this threshold, assume a fresh pot
const float FULL_POT_THRESHOLD_CUPS = 8;

const long MILLIS_PER_MINUTE = 60 * 1000;
const long MILLIS_PER_HOUR = MILLIS_PER_MINUTE * 60;

unsigned long lastChangeMillis = true;

int getDark(String arg);
int getLight(String arg);
int calculateScale(String arg);

void setup() {
  Particle.variable("acups", settledA);
  Particle.variable("bcups", settledB);
  Particle.function("getdark", getDark);
  Particle.function("getlight", getLight);
  Particle.function("pubscale", pubScale);
  Particle.function("debugmode", debugMode);
  initDisplay();
  tareBothChannels();
}

void initDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();
}

void tareBothChannels() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("Clear the\nscales");
  display.display();
  delay(3000);

  offsetA = readChannelA();
  offsetB = readChannelB();

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Tare\nComplete");
  display.display();
  delay(500);

}

long readChannelA() {
  scale.set_gain(byte(128));
  scale.read_average(1);
  delay(OUTPUT_SETTLING_TIME);
  long rawValue = scale.read_average(NUM_READS);
  return rawValue;
}

float readCupsA() {
  return (((readChannelA() - offsetA) / rawToGramA) - CARAFE_WEIGHT_GRAMS) / GRAMS_PER_CUP;
}

long readChannelB() {
  scale.set_gain(byte(32));
  scale.read_average(1);
  delay(OUTPUT_SETTLING_TIME);
  long rawValue = scale.read_average(NUM_READS);
  return rawValue;
}

float readCupsB() {
  return (((readChannelB() - offsetB) / rawToGramB) - CARAFE_WEIGHT_GRAMS) / GRAMS_PER_CUP;
}

void displaySupplies(String aSupply, String bSupply) {
  display.clearDisplay();
  renderScaleValueAndAge(bSupply, millisToDuration(millis() - brewTimeBMillis), aSupply, millisToDuration(millis() - brewTimeAMillis));
  display.display();
}

String millisToDuration(long millis) {
    if (millis < MILLIS_PER_MINUTE) {
        return "Fresh";
    } 
    
    if (millis < MILLIS_PER_HOUR) {
        int minutes = millis / MILLIS_PER_MINUTE;
        char content[12];
        sprintf(content, "%d min. ", minutes);
        return String(content);
    }
    
    int hours = millis / MILLIS_PER_HOUR;
    if (hours == 1) return "1 hour ";
    char content[12];
    sprintf(content, "%d hours", hours);
    return String(content);
}

void renderLabeledValue(String label, String value, int column, int textSize) {
  display.setTextSize(textSize);
  display.setCursor(column * 64, 0);
  display.print(label);
  display.setCursor(column * 64, 26);
  display.print(value);
}

void renderScaleValueAndAge(String aValue, String aAge, String bValue, String bAge) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(aAge);

  display.setCursor(64, 0);
  display.print(bAge);

  display.setTextSize(4);
  display.setCursor(10, 26);
  display.print(aValue);
  
  display.setCursor(74, 26);
  display.print(bValue);

}




void renderFooter(String text, int column, int textSize) {
  display.setTextSize(textSize);
  display.setCursor(column * 64, 44);
  display.print(text);
}

void renderSupply(String label, String value, int column, int textSize) {
  display.setTextSize(textSize);
  display.setCursor(column * 64, 0);
  display.print(label);
  display.setCursor(column * 64, 26);
  display.print(value);
}

void recordSupplies(String aCups, String bCups) {
  if ((aCups != previousA) || (bCups != previousB)) {
    lastChangeMillis = millis();
  }

  if (millis() - lastChangeMillis > SETTLE_TIME_MILLIS) {
    if (aCups != settledA) {
      // settled on a change:
      Particle.publish("CoffeeQuantityChanged", aCups + " cups of dark remain", PRIVATE);
      Particle.publish("A_UNITS", aCups);
      if (aCups.toFloat() > FULL_POT_THRESHOLD_CUPS && settledA.toFloat() < 1) {
        Particle.publish("FreshPot", "Huzzah! There is a fresh pot of dark (coffee)", PRIVATE);
          brewTimeAMillis = millis();
      }
      
    }
    if (bCups != settledB) {
      // settled on a change:
      Particle.publish("CoffeeQuantityChanged", bCups + " cups of light remain", PRIVATE);
      Particle.publish("B_UNITS", bCups);
      if (bCups.toFloat() > FULL_POT_THRESHOLD_CUPS && settledB.toFloat() < 1) {
        Particle.publish("FreshPot", "Huzzah! There is a fresh pot of light (coffee)", PRIVATE);
          brewTimeBMillis = millis();
      }
    }
    settledA = aCups;
    settledB = bCups;
  }
  previousA = aCups;
  previousB = bCups;
}

int getDark(String arg) {
  return round(max(0, readCupsA()));
}

int getLight(String arg) {
  return round(max(0, readCupsB()));
}

void loop() {
    if (!debug) {
      String aCups = String(max(0.0, readCupsA()), 0);
      String bCups = String(max(0.0, readCupsB()), 0);
      displaySupplies(aCups, bCups);
      recordSupplies(aCups, bCups);
      // update the lcd panel
      display.display();
    } else {
        display.clearDisplay();
        renderLabeledValue("Raw A", String(readChannelA()), 0, 1);
        renderLabeledValue("Raw B", String(readChannelB()), 1, 1);
        display.display();

    }
}

int pubScale(String arg) {
  char content[63];
  sprintf(content, "Raw A Channel: %d Raw B Channel: %d", readChannelA() - offsetA, readChannelB() - offsetB);
  Particle.publish("Scale", content);
  return 1;
}

int debugMode(String arg) {
   debug = !debug;
   return 1;
}
