#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <DHT.h>

MCUFRIEND_kbv tft;

// --- Modern Color Palette ---
#define BLACK       0x0000
#define DARK_GREY   0x2104
#define LIGHT_GREY  0x7BEF
#define CYAN_ACCENT 0x07FF
#define RED_ACCENT  0xF800
#define GREEN_ACCENT 0x07E0
#define BLUE_ACCENT 0x001F
#define WHITE       0xFFFF
#define ORANGE      0xFD20

// --- Sensor Pins & Setup ---
#define DHTPIN 10
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_PIN A5 
#define BUZZER_PIN 12

// --- ALARM THRESHOLD ---
#define TEMP_THRESHOLD 35.0 // Buzzer activates if temp goes ABOVE this value (in Celsius)

// --- Layout Geometry ---
int gaugeCenterX = 130; 
int gaugeCenterY = 190;  
int gaugeOuterRad = 110;
int gaugeInnerRad = 90;
int needleRadius = 75;  

// State tracking to prevent screen flicker
float oldAngle = -999;
float oldTemp = -999;
float oldHum = -999;
int oldSoil = -999;

void setup() {
  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off at startup

  dht.begin();
  
  // Force ILI9486 Boot (Bypass auto-detect bugs)
  tft.reset();
  tft.begin(0x9486); 
  tft.setRotation(1); // 480x320 Landscape
  tft.fillScreen(BLACK);

  drawStaticUI();
}

void loop() {
  // Read Sensors (DHT11 takes about 250ms to read)
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int rawSoil = analogRead(SOIL_PIN);

  // Convert raw soil analog value (0-1023) to percentage (0-100%)
  int soilPercent = map(rawSoil, 1023, 300, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  // 1. Update Temperature Gauge & Text
  if (!isnan(t) && abs(t - oldTemp) > 0.2) { 
    updateTempGauge(t);
    
    tft.setTextSize(3);
    // Change text color to RED if over threshold
    if (t >= TEMP_THRESHOLD) {
      tft.setTextColor(RED_ACCENT, BLACK);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(95, 205);
    tft.print(t, 1);
    
    oldTemp = t;
  }

  // 2. Update Humidity Card
  if (!isnan(h) && abs(h - oldHum) > 0.5) {
    tft.setTextSize(4);
    tft.setTextColor(CYAN_ACCENT, DARK_GREY);
    tft.setCursor(295, 100);
    tft.print(h, 1);
    oldHum = h;
  }

  // 3. Update Soil Moisture Card & Progress Bar
  if (abs(soilPercent - oldSoil) > 2) {
    tft.setTextSize(3);
    tft.setTextColor(GREEN_ACCENT, DARK_GREY);
    tft.setCursor(295, 220);
    tft.print(soilPercent);
    tft.print("%  "); 
    
    drawSoilBar(soilPercent);
    oldSoil = soilPercent;
  }

  // 4. BUZZER ALARM LOGIC
  if (!isnan(t) && t >= TEMP_THRESHOLD) {
    // Double beep warning
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Wait the remaining time before next sensor read
    delay(1600); 
  } else {
    digitalWrite(BUZZER_PIN, LOW); // Keep quiet
    delay(2000); // Standard 2-second delay for DHT11
  }
}

// ==========================================
// UI DRAWING FUNCTIONS
// ==========================================

void drawStaticUI() {
  // Brand Header
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(120, 15); 
  tft.print("JUST DO ELECTRONICS");
  tft.drawLine(20, 40, 460, 40, LIGHT_GREY);

  // 1. Setup Temp Gauge UI
  drawGaugeTrack();
  tft.setTextSize(2);
  tft.setTextColor(LIGHT_GREY);
  tft.setCursor(95, 290);
  tft.print("TEMP 'C");

  // 2. Setup Humidity Card UI
  tft.fillRoundRect(270, 60, 190, 90, 8, DARK_GREY);
  tft.setTextColor(LIGHT_GREY, DARK_GREY);
  tft.setCursor(285, 75);
  tft.print("HUMIDITY");
  tft.setCursor(415, 115);
  tft.print("%");

  // 3. Setup Soil Moisture Card UI
  tft.fillRoundRect(270, 170, 190, 120, 8, DARK_GREY);
  tft.setTextColor(LIGHT_GREY, DARK_GREY);
  tft.setCursor(285, 185);
  tft.print("SOIL MOISTURE");
  
  // Draw empty progress bar background
  tft.drawRect(285, 260, 160, 15, LIGHT_GREY);
}

void drawGaugeTrack() {
  for (int i = 210; i >= -30; i -= 8) {
    uint16_t color = BLUE_ACCENT;
    if (i < 150) color = GREEN_ACCENT; 
    if (i < 90) color = ORANGE; 
    if (i < 30) color = RED_ACCENT; 

    for (float offset = -1.0; offset <= 1.0; offset += 0.5) {
      float rad = (i + offset) * PI / 180.0;
      int x1 = gaugeCenterX + (gaugeInnerRad * cos(rad));
      int y1 = gaugeCenterY - (gaugeInnerRad * sin(rad));
      int x2 = gaugeCenterX + (gaugeOuterRad * cos(rad));
      int y2 = gaugeCenterY - (gaugeOuterRad * sin(rad));
      tft.drawLine(x1, y1, x2, y2, color);
    }
  }
}

void updateTempGauge(float temp) {
  temp = constrain(temp, 0, 50); // Map 0-50 C to the gauge
  float newAngle = 210.0 - ((temp / 50.0) * 240.0);

  if (abs(newAngle - oldAngle) < 1.0) return; 

  if (oldAngle != -999) drawNeedle(oldAngle, BLACK); 
  
  uint16_t nColor = GREEN_ACCENT;
  if (temp < 20) nColor = BLUE_ACCENT;
  if (temp >= 30) nColor = ORANGE;
  if (temp >= TEMP_THRESHOLD) nColor = RED_ACCENT; // Match gauge needle to alarm state
  
  drawNeedle(newAngle, nColor); 
  oldAngle = newAngle;
}

void drawNeedle(float angle, uint16_t color) {
  float rad = angle * PI / 180.0;
  float radLeft = (angle + 90) * PI / 180.0;
  float radRight = (angle - 90) * PI / 180.0;

  int tipX = gaugeCenterX + (needleRadius * cos(rad));
  int tipY = gaugeCenterY - (needleRadius * sin(rad));
  
  int leftX = gaugeCenterX + (5 * cos(radLeft));
  int leftY = gaugeCenterY - (5 * sin(radLeft));
  
  int rightX = gaugeCenterX + (5 * cos(radRight));
  int rightY = gaugeCenterY - (5 * sin(radRight));

  tft.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, color);
  tft.fillCircle(gaugeCenterX, gaugeCenterY, 8, color); 
  
  if (color != BLACK) { 
    tft.fillCircle(gaugeCenterX, gaugeCenterY, 3, BLACK);
  }
}

void drawSoilBar(int percent) {
  int barWidth = map(percent, 0, 100, 0, 156);
  
  if (barWidth > 0) {
    tft.fillRect(287, 262, barWidth, 11, GREEN_ACCENT);
  }
  if (barWidth < 156) {
    tft.fillRect(287 + barWidth, 262, 156 - barWidth, 11, DARK_GREY);
  }
}