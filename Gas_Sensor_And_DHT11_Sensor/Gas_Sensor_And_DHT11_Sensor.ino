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

#define GAS_PIN A5 
#define BUZZER_PIN 12
#define BUTTON_PIN 11

// --- ALARM THRESHOLDS ---
#define TEMP_THRESHOLD 35.0  
#define GAS_THRESHOLD 400    

// --- Layout Geometry (Dashboard) ---
int gaugeCenterX = 130; 
int gaugeCenterY = 190;  
int gaugeOuterRad = 110;
int gaugeInnerRad = 90;
int needleRadius = 75;  

// --- State Tracking & Timers ---
unsigned long lastReadTime = 0;
unsigned long lastDebounceTime = 0;
bool lastBtnState = HIGH;
int currentScreen = 0; // 0 = Dashboard, 1 = Graph

float oldAngle = -999;
float oldTemp = -999;
float oldHum = -999;
int oldGas = -999;

// --- Graph Data Storage ---
// 41 points creates exactly 40 intervals for perfect 400-pixel scaling
#define MAX_POINTS 41 
float tempHistory[MAX_POINTS];
float humHistory[MAX_POINTS];
int gasHistory[MAX_POINTS];
int dataCount = 0;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Enable internal pull-up for button

  dht.begin();
  
  tft.reset();
  tft.begin(0x9486); 
  tft.setRotation(1); 
  tft.fillScreen(BLACK);

  // Initialize data arrays to 0
  for(int i=0; i<MAX_POINTS; i++) {
    tempHistory[i] = 0; humHistory[i] = 0; gasHistory[i] = 0;
  }

  drawDashboardStaticUI();
}

void loop() {
  // ==========================================
  // 1. CHECK PUSH BUTTON (NON-BLOCKING)
  // ==========================================
  bool currentBtnState = digitalRead(BUTTON_PIN);
  
  if (currentBtnState == LOW && lastBtnState == HIGH) {
    if (millis() - lastDebounceTime > 200) { // Debounce button
      lastDebounceTime = millis();
      currentScreen = (currentScreen == 0) ? 1 : 0; // Toggle screen
      
      tft.fillScreen(BLACK); // Clear screen for transition
      
      if (currentScreen == 0) {
        drawDashboardStaticUI();
        oldAngle = -999; oldTemp = -999; oldHum = -999; oldGas = -999;
      } else {
        drawGraphStaticUI();
        drawGraphLines(false); // Draw current history instantly
      }
    }
  }
  lastBtnState = currentBtnState;

  // ==========================================
  // 2. READ SENSORS EVERY 2 SECONDS
  // ==========================================
  if (millis() - lastReadTime >= 2000) {
    lastReadTime = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int rawGas = analogRead(GAS_PIN);

    // --- ALARM LOGIC ---
    bool tempAlarm = (!isnan(t) && t >= TEMP_THRESHOLD);
    bool gasAlarm = (rawGas >= GAS_THRESHOLD);

    if (tempAlarm || gasAlarm) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100); 
      digitalWrite(BUZZER_PIN, LOW);
      delay(50);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
    }

    // --- DATA SAVING & UI UPDATING ---
    if (!isnan(t) && !isnan(h)) {
      
      // Erase old graph lines before shifting data
      if (currentScreen == 1) {
        drawGraphLines(true); 
      }

      saveDataPoint(t, h, rawGas);

      // Draw the newly updated UI
      if (currentScreen == 0) {
        updateDashboard(t, h, rawGas);
      } else if (currentScreen == 1) {
        drawGraphLines(false); 
      }
    }
  }
}

// ==========================================
// DATA HANDLING FUNCTIONS
// ==========================================

void saveDataPoint(float t, float h, int gas) {
  for (int i = 0; i < MAX_POINTS - 1; i++) {
    tempHistory[i] = tempHistory[i + 1];
    humHistory[i] = humHistory[i + 1];
    gasHistory[i] = gasHistory[i + 1];
  }
  int idx = (dataCount < MAX_POINTS) ? dataCount : MAX_POINTS - 1;
  tempHistory[idx] = t;
  humHistory[idx] = h;
  gasHistory[idx] = gas;
  
  if (dataCount < MAX_POINTS) dataCount++;
}

// ==========================================
// SCREEN 1: DASHBOARD UI FUNCTIONS
// ==========================================

void drawDashboardStaticUI() {
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(125, 15); 
  tft.print("JUST DO ELECTRONICS");
  tft.drawLine(20, 40, 460, 40, LIGHT_GREY);

  drawGaugeTrack();
  tft.setTextSize(2);
  tft.setTextColor(LIGHT_GREY);
  tft.setCursor(95, 290);
  tft.print("TEMP 'C");

  tft.fillRoundRect(270, 60, 190, 90, 8, DARK_GREY);
  tft.setTextColor(LIGHT_GREY, DARK_GREY);
  tft.setCursor(285, 75);
  tft.print("HUMIDITY");
  tft.setCursor(415, 115);
  tft.print("%");

  tft.fillRoundRect(270, 170, 190, 120, 8, DARK_GREY);
  tft.setTextColor(LIGHT_GREY, DARK_GREY);
  tft.setCursor(285, 185);
  tft.print("GAS LEVEL");
  tft.drawRect(285, 260, 160, 15, LIGHT_GREY);
}

void updateDashboard(float t, float h, int rawGas) {
  if (abs(t - oldTemp) > 0.2) { 
    updateTempGauge(t);
    tft.setTextSize(3);
    if (t >= TEMP_THRESHOLD) tft.setTextColor(RED_ACCENT, BLACK);
    else tft.setTextColor(WHITE, BLACK);
    tft.setCursor(95, 205);
    tft.print(t, 1);
    oldTemp = t;
  }

  if (abs(h - oldHum) > 0.5) {
    tft.setTextSize(4);
    tft.setTextColor(CYAN_ACCENT, DARK_GREY);
    tft.setCursor(295, 100);
    tft.print(h, 1);
    oldHum = h;
  }

  if (abs(rawGas - oldGas) > 4) {
    tft.setTextSize(3);
    bool isGasAlarm = (rawGas >= GAS_THRESHOLD);
    if (isGasAlarm) tft.setTextColor(RED_ACCENT, DARK_GREY);
    else tft.setTextColor(GREEN_ACCENT, DARK_GREY);
    
    tft.setCursor(295, 220);
    tft.print(rawGas);
    tft.print("  "); 
    
    int gasPercent = constrain(map(rawGas, 0, 1023, 0, 100), 0, 100);
    drawGasBar(gasPercent, isGasAlarm);
    oldGas = rawGas;
  }
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
  temp = constrain(temp, 0, 50); 
  float newAngle = 210.0 - ((temp / 50.0) * 240.0);
  if (abs(newAngle - oldAngle) < 1.0) return; 

  if (oldAngle != -999) drawNeedle(oldAngle, BLACK); 
  uint16_t nColor = GREEN_ACCENT;
  if (temp < 20) nColor = BLUE_ACCENT;
  if (temp >= 30) nColor = ORANGE;
  if (temp >= TEMP_THRESHOLD) nColor = RED_ACCENT; 
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
  if (color != BLACK) tft.fillCircle(gaugeCenterX, gaugeCenterY, 3, BLACK);
}

void drawGasBar(int percent, bool isAlarm) {
  int barWidth = map(percent, 0, 100, 0, 156);
  uint16_t barColor = isAlarm ? RED_ACCENT : GREEN_ACCENT;
  if (barWidth > 0) tft.fillRect(287, 262, barWidth, 11, barColor);
  if (barWidth < 156) tft.fillRect(287 + barWidth, 262, 156 - barWidth, 11, DARK_GREY);
}

// ==========================================
// SCREEN 2: 3-BOX GRAPH UI FUNCTIONS
// ==========================================

void drawGraphStaticUI() {
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(150, 10); 
  tft.print("DATA HISTORY");
  
  tft.setTextSize(1);
  int axisX = 45; // Shifted right slightly for wider labels
  int endX = axisX + 400; // 400px wide graph

  // Box 1: Temperature (Y: 50 to 120) Scale: 0 to 50
  tft.setTextColor(RED_ACCENT); tft.setCursor(2, 35); tft.print("TMP 'C");
  tft.drawLine(axisX, 50, axisX, 120, WHITE);   
  tft.drawLine(axisX, 120, endX, 120, WHITE); 
  // Temp Ticks
  tft.setTextColor(WHITE);
  tft.drawLine(axisX - 4, 51, axisX, 51, WHITE);  tft.setCursor(axisX - 20, 47); tft.print("50");
  tft.drawLine(axisX - 4, 85, axisX, 85, WHITE);  tft.setCursor(axisX - 20, 81); tft.print("25");
  tft.drawLine(axisX - 4, 119, axisX, 119, WHITE); tft.setCursor(axisX - 14, 115); tft.print("0");

  // Box 2: Humidity (Y: 140 to 210) Scale: 0 to 100
  tft.setTextColor(CYAN_ACCENT); tft.setCursor(2, 125); tft.print("HUM %");
  tft.drawLine(axisX, 140, axisX, 210, WHITE);   
  tft.drawLine(axisX, 210, endX, 210, WHITE);  
  // Hum Ticks
  tft.setTextColor(WHITE);
  tft.drawLine(axisX - 4, 141, axisX, 141, WHITE); tft.setCursor(axisX - 26, 137); tft.print("100");
  tft.drawLine(axisX - 4, 175, axisX, 175, WHITE); tft.setCursor(axisX - 20, 171); tft.print("50");
  tft.drawLine(axisX - 4, 209, axisX, 209, WHITE); tft.setCursor(axisX - 14, 205); tft.print("0");

  // Box 3: Gas (Y: 230 to 300) Scale: 0 to 1K
  tft.setTextColor(GREEN_ACCENT); tft.setCursor(2, 215); tft.print("GAS");
  tft.drawLine(axisX, 230, axisX, 300, WHITE);   
  tft.drawLine(axisX, 300, endX, 300, WHITE);  
  // Gas Ticks
  tft.setTextColor(WHITE);
  tft.drawLine(axisX - 4, 231, axisX, 231, WHITE); tft.setCursor(axisX - 20, 227); tft.print("1K");
  tft.drawLine(axisX - 4, 265, axisX, 265, WHITE); tft.setCursor(axisX - 26, 261); tft.print("500");
  tft.drawLine(axisX - 4, 299, axisX, 299, WHITE); tft.setCursor(axisX - 14, 295); tft.print("0");

  // --- X-AXIS TIME SCALE (Bottom) ---
  tft.setTextColor(LIGHT_GREY);
  for (int i = 0; i <= 4; i++) {
    int xTick = axisX + (i * 100); 
    tft.drawLine(xTick, 300, xTick, 305, WHITE); 
    
    tft.setCursor(xTick - 10, 310);
    if (i == 4) {
      tft.print("Now");
    } else {
      tft.print("-");
      tft.print(80 - (i * 20)); 
      tft.print("s");
    }
  }
}

void drawGraphLines(bool eraseOld) {
  if (dataCount < 2) return; 

  int axisX = 45;
  int xSpacing = 10; // 40 intervals * 10 = 400px width

  for (int i = 0; i < dataCount - 1; i++) {
    int x1 = axisX + (i * xSpacing);
    int x2 = axisX + ((i + 1) * xSpacing);

    // Box 1: Temp (Plotting area Y = 119 to 51)
    int y1Temp = constrain(map(tempHistory[i], 0, 50, 119, 51), 51, 119);
    int y2Temp = constrain(map(tempHistory[i+1], 0, 50, 119, 51), 51, 119);
    tft.drawLine(x1, y1Temp, x2, y2Temp, eraseOld ? BLACK : RED_ACCENT);

    // Box 2: Humidity (Plotting area Y = 209 to 141)
    int y1Hum = constrain(map(humHistory[i], 0, 100, 209, 141), 141, 209);
    int y2Hum = constrain(map(humHistory[i+1], 0, 100, 209, 141), 141, 209);
    tft.drawLine(x1, y1Hum, x2, y2Hum, eraseOld ? BLACK : CYAN_ACCENT);

    // Box 3: Gas Level (Plotting area Y = 299 to 231)
    int y1Gas = constrain(map(gasHistory[i], 0, 1023, 299, 231), 231, 299);
    int y2Gas = constrain(map(gasHistory[i+1], 0, 1023, 299, 231), 231, 299);
    tft.drawLine(x1, y1Gas, x2, y2Gas, eraseOld ? BLACK : GREEN_ACCENT);
  }
}