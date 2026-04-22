// ============================================
// USB LINE TESTER - VERSION 8.5 (FINAL UI REFINEMENT)
// Core V5.1 System + Advanced V6.5 UI/Graph
// ============================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ============================================
// PIN DEFINITIONS
// ============================================

// TFT Display pins
#define TFT_CS     10
#define TFT_DC     9
#define TFT_RST    8

// Hardware pins
#define START_BUTTON  5
#define CANCEL_BUTTON 4
#define BUZZER_PIN    3
#define SOURCE_PIN    6

// Analog pins for 8-Line V5.1 Logic
#define PIN_VBUS  A0
#define PIN_DMIN  A1
#define PIN_DPLUS A2
#define PIN_CC1   A3
#define PIN_CC2   A4
#define PIN_TX    A5
#define PIN_RX    A6

// ============================================
// DIODE MODE THRESHOLDS
// ============================================
#define THRESHOLD_SHORT_RAW      0
#define THRESHOLD_HALFSHORT_RAW  120
#define THRESHOLD_OK_MIN_RAW     300
#define THRESHOLD_OK_MAX_RAW     800
#define THRESHOLD_REVERSE_RAW    1000
#define THRESHOLD_OL_RAW         1023

#define CALIBRATION_FACTOR       0.56

// ============================================
// DISPLAY COLORS - INVERTED
// ============================================
#define COLOR_BLACK      0x0000  // White on screen
#define COLOR_WHITE      0xFFFF  // Black on screen
#define COLOR_BG         0xFFFF  
#define COLOR_HEADER_BG  0x3186
#define COLOR_HIGHLIGHT  0x8410
#define COLOR_TEXT       0x0000
#define COLOR_DARK       0x7BEF
#define COLOR_GRAY       0xBDF7
#define COLOR_HEADER_TEXT 0x07FF
#define COLOR_SUCCESS    0xF81F
#define COLOR_OK         0xF81F
#define COLOR_SHORT      0x07FF
#define COLOR_OL         0x07E0
#define COLOR_REVERSE    0xFFE0

// ============================================
// TIMING CONSTANTS (V5.1 Performance)
// ============================================
#define SPLASH_DELAY    3000
#define TEST_TIME_NORMAL 700 
#define TEST_TIME_GRAPH  2000
#define DEBOUNCE_DELAY  50

// ============================================
// MODES & STATE
// ============================================
#define MODE_NORMAL 0
#define MODE_GRAPH  1

int currentMode = MODE_NORMAL;
unsigned long pressStartTime = 0;
bool modeToggled = false;
bool cancelTest = false;

// ============================================
// DATA STRUCTURES
// ============================================
struct LineInfo {
    const char* name;
    uint8_t pin;
    uint16_t statusColor;
    const char* statusText;
    float value;
    bool tested;
    float minVal;
    float maxVal;
};

LineInfo lines[] = {
    {"VBUS", PIN_VBUS, COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"D-",   PIN_DMIN, COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"D+",   PIN_DPLUS, COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"CC1",  PIN_CC1,  COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"CC2",  PIN_CC2,  COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"TX",   PIN_TX,   COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"RX",   PIN_RX,   COLOR_BLACK, "", 0.0, false, 5.0, 0.0},
    {"GND",  255,      COLOR_BLACK, "", 0.0, false, 5.0, 0.0}
};

const int NUM_LINES = 8;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define STATUS_X        50
#define VALUE_X         85

// ============================================
// SETUP
// ============================================
void setup() {
    pinMode(START_BUTTON, INPUT_PULLUP);
    pinMode(CANCEL_BUTTON, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SOURCE_PIN, OUTPUT);
    
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(SOURCE_PIN, LOW);
    
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    
    showSplashScreen();
    beep(1);
    drawMainInterface();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    int btnState = digitalRead(START_BUTTON);
    
    if (btnState == LOW) {
        if (pressStartTime == 0) {
            pressStartTime = millis();
            modeToggled = false;
        }
        if (!modeToggled && (millis() - pressStartTime > 1000)) {
            toggleMode();
            modeToggled = true;
            while(digitalRead(START_BUTTON) == LOW);
            pressStartTime = 0;
        }
    } else {
        if (pressStartTime > 0) {
            unsigned long dur = millis() - pressStartTime;
            pressStartTime = 0;
            if (!modeToggled && dur > DEBOUNCE_DELAY) {
                if (currentMode == MODE_NORMAL) runTestSequence();
                else runGraphSequence();
            }
        }
    }
    
    if (digitalRead(CANCEL_BUTTON) == LOW) {
        delay(DEBOUNCE_DELAY);
        if (digitalRead(CANCEL_BUTTON) == LOW) handleCancel();
    }
}

// ============================================
// UI FUNCTIONS
// ============================================

void drawLogo(int centerX, int centerY) {
    tft.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds("SLS", 0, 0, &x1, &y1, &w, &h);
    int startX = centerX - w/2;
    int letterY = centerY - h/2;
    uint16_t sW, sH, lW, lH;
    tft.getTextBounds("S", 0, 0, &x1, &y1, &sW, &sH);
    tft.getTextBounds("L", 0, 0, &x1, &y1, &lW, &lH);
    tft.setTextColor(COLOR_BLACK); tft.setCursor(startX, letterY); tft.print("S");
    tft.setTextColor(0x07FF); tft.setCursor(startX + sW - 2, letterY); tft.print("L");
    tft.setTextColor(COLOR_BLACK); tft.setCursor(startX + sW + lW - 4, letterY); tft.print("S");
}

void showSplashScreen() {
    tft.fillScreen(COLOR_BG);
    drawLogo(64, 25);
    tft.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds("MR", 0, 0, &x1, &y1, &w, &h);
    int mrWidth = w;
    tft.getTextBounds(".HILAL", 0, 0, &x1, &y1, &w, &h);
    int startX = (128 - (mrWidth + w - 4)) / 2;
    tft.setTextColor(0x07FF); tft.setCursor(startX, 50); tft.print("MR");
    tft.setTextColor(COLOR_BLACK); tft.setCursor(startX + mrWidth - 4, 50); tft.print(".HILAL");
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLACK); setTextCentered("USB LINE TESTER", 75);
    tft.setTextColor(0x07FF); setTextCentered("V8.5", 92);
    tft.setTextColor(COLOR_BLACK); setTextCentered("SKYLAB SERVICES", 110);
    delay(SPLASH_DELAY);
}

void drawMainInterface() {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, 128, 20, 0x07FF);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLACK);
    setTextCentered("USB TESTER V8.5", 6);
    tft.setCursor(5, 24); tft.print("LINE");
    tft.setCursor(STATUS_X + 8, 24); tft.print("STAT");
    tft.setCursor(VALUE_X + 2, 24); tft.print("VAL");
    tft.drawLine(0, 32, 128, 32, COLOR_DARK);
    for (int i = 0; i < NUM_LINES; i++) drawLineRow(i, false);
}

void drawLineRow(int i, bool highlight) {
    int yPos = 34 + (i * 15);
    tft.fillRect(0, yPos, 128, 14, highlight ? COLOR_HIGHLIGHT : COLOR_BG);
    tft.setTextColor(COLOR_BLACK);
    tft.setCursor(5, yPos + 3); tft.print(lines[i].name);
    if (lines[i].tested) {
        tft.setTextColor(lines[i].statusColor);
        tft.setCursor(STATUS_X, yPos + 3); tft.print(lines[i].statusText);
        tft.setTextColor(COLOR_BLACK);
        tft.setCursor(VALUE_X, yPos + 3); reportValueFormat(lines[i].value);
    } else {
        tft.setTextColor(COLOR_DARK);
        tft.setCursor(STATUS_X, yPos + 3); tft.print("---");
        tft.setCursor(VALUE_X, yPos + 3); tft.print("----");
    }
    tft.drawLine(0, yPos + 14, 128, yPos + 14, COLOR_GRAY);
}

void reportValueFormat(float val) {
    int mv = (int)(val * 1000);
    tft.print(mv/1000); tft.print(".");
    if (mv%1000 < 100) tft.print("0");
    if (mv%1000 < 10) tft.print("0");
    tft.print(mv%1000);
}

// --- GRAPH MODE (FULL SCREEN SCOPE) ---

void toggleMode() {
    currentMode = (currentMode == MODE_NORMAL) ? MODE_GRAPH : MODE_NORMAL;
    beep(2);
    if (currentMode == MODE_GRAPH) tft.setRotation(1);
    else tft.setRotation(0);
    if (currentMode == MODE_NORMAL) drawMainInterface();
    else drawGraphInterface();
}

void drawGraphInterface() {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, 160, 20, 0x07FF);
    tft.setTextColor(COLOR_BLACK);
    setTextCentered("Testing Report", 6);
    drawGraphGrid();
}

void drawGraphGrid() {
    int gW = 160, gH = 92; // Reduced height to fit header
    int gY = 20; 
    
    // Vertical Grid Lines
    for (int x = 0; x <= gW; x += 16) tft.drawLine(x, gY, x, gY + gH, COLOR_GRAY);
    
    // Horizontal Grid Lines at Fixed 0.5V steps (0, 0.5, 1.0, 1.5, 2.0)
    for (int i = 0; i <= 4; i++) {
        int yPos = gY + 5 + (i * 20);
        tft.drawLine(0, yPos, gW, yPos, COLOR_GRAY);
    }
    tft.drawLine(0, gY + 85, gW, gY + 85, COLOR_GRAY); // Baseline grid
    
    tft.drawRect(0, gY, gW, gH, COLOR_BLACK);
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLACK);
    // Labels 0.0 to 2.0 strictly fixed
    tft.setCursor(2, gY + 2);   tft.print("2.0");
    tft.setCursor(2, gY + 22);  tft.print("1.5");
    tft.setCursor(2, gY + 42);  tft.print("1.0");
    tft.setCursor(2, gY + 62);  tft.print("0.5");
    tft.setCursor(2, gY + 81);  tft.print("0.0"); 
}

// ============================================
// CORE LOGIC
// ============================================

bool checkDoubleClickCancel() {
    static unsigned long lastP = 0; static int cCount = 0;
    if (digitalRead(START_BUTTON) == LOW) {
        delay(20);
        if (digitalRead(START_BUTTON) == LOW) {
            cCount++;
            if (cCount >= 2 && (millis() - lastP < 500)) { cCount = 0; return true; }
            lastP = millis();
            while(digitalRead(START_BUTTON) == LOW);
        }
    }
    return false;
}

void runTestSequence() {
    beep(1); cancelTest = false;
    for (int i = 0; i < NUM_LINES; i++) lines[i].tested = false;
    drawMainInterface();
    digitalWrite(SOURCE_PIN, HIGH);
    for (int i = 0; i < NUM_LINES; i++) {
        if (checkDoubleClickCancel()) { cancelTest = true; break; }
        drawLineRow(i, true);
        if (lines[i].pin == 255) processGND(i);
        else updateLineData(i, analogRead(lines[i].pin));
        unsigned long t = millis();
        while(millis() - t < TEST_TIME_NORMAL) {
            if (checkDoubleClickCancel()) { cancelTest = true; break; }
            delay(10);
        }
        if (cancelTest) break;
        drawLineRow(i, false);
    }
    digitalWrite(SOURCE_PIN, LOW);
    if (cancelTest) handleCancel(); else beep(2);
}

void runGraphSequence() {
    beep(1); cancelTest = false;
    digitalWrite(SOURCE_PIN, HIGH);
    for (int i = 0; i < NUM_LINES; i++) {
        if (cancelTest) break;
        tft.fillRect(0, 20, 160, 108, COLOR_BG); // Keep header area
        drawGraphGrid();
        
        tft.setTextColor(0x07FF);
        char msg[32]; sprintf(msg, "Testing: %s", lines[i].name);
        tft.setCursor(35, 116); tft.print(msg);
        
        float minV = 5.0, maxV = 0.0;
        int gW = 160, gY = 20;
        int lx = 0, ly = gY + 85; 
        
        unsigned long st = millis();
        while (millis() - st < TEST_TIME_GRAPH) {
            if (checkDoubleClickCancel()) { cancelTest = true; break; }
            uint16_t r = (lines[i].pin == 255) ? 1023 : analogRead(lines[i].pin);
            float baseV = ((1023 - r) * CALIBRATION_FACTOR) / 1000.0;
            if (baseV < 0.02) baseV = 0.0;
            float anim = (baseV > 0.02) ? (baseV * 0.12 * sin((millis()-st)/150.0)) : 0;
            float plotV = baseV + anim;
            if (plotV > 2.0) plotV = 2.0;

            if (baseV < minV) minV = baseV;
            if (baseV > maxV) maxV = baseV;
            
            int cx = map(millis() - st, 0, TEST_TIME_GRAPH, 0, gW);
            // Scaling 0-2.0V to 80px range (gY+85 to gY+5)
            int cy = (gY + 85) - (int)(plotV * 40.0);
            if (cy < 5) cy = 5; if (cy > 105) cy = 105;

            if (cx > 0) tft.drawLine(lx, ly, cx, cy, COLOR_SUCCESS);
            lx = cx; ly = cy;
            delay(30);
            if (millis() - st >= 1970) updateLineData(i, r);
        }
        lines[i].minVal = (minV > 4.5) ? 0.0 : minV; lines[i].maxVal = maxV;
    }
    digitalWrite(SOURCE_PIN, LOW);
    if (cancelTest) handleCancel(); else showFinalReport();
}

void updateLineData(int i, uint16_t raw) {
    if (raw == THRESHOLD_SHORT_RAW) { lines[i].statusColor = COLOR_SHORT; lines[i].statusText = "SHORT"; }
    else if (raw >= THRESHOLD_OL_RAW) { lines[i].statusColor = COLOR_OL; lines[i].statusText = "OL"; }
    else if (raw >= THRESHOLD_REVERSE_RAW) { lines[i].statusColor = COLOR_REVERSE; lines[i].statusText = "ABOVE"; }
    else { lines[i].statusColor = COLOR_OK; lines[i].statusText = "OK"; }
    lines[i].value = ((1023 - raw) * CALIBRATION_FACTOR) / 1000.0;
    if (lines[i].value < 0) lines[i].value = 0;
    lines[i].tested = true;
}

void processGND(int i) { lines[i].tested = true; lines[i].statusColor = COLOR_SHORT; lines[i].statusText = "SHORT"; lines[i].value = 0.0; }

void showFinalReport() {
    tft.fillScreen(COLOR_BG);
    tft.fillRect(0, 0, 160, 20, 0x07FF);
    tft.setTextColor(COLOR_BLACK); setTextCentered("Testing Report", 6);
    tft.setTextSize(1);
    for (int i = 0; i < NUM_LINES; i++) {
        int y = 24 + (i * 12);
        tft.setCursor(5, y); tft.setTextColor(COLOR_BLACK); tft.print(lines[i].name); tft.print(":");
        tft.setCursor(38, y); tft.setTextColor(lines[i].statusColor);
        tft.print(lines[i].minVal, 2); tft.print("-"); tft.print(lines[i].maxVal, 2);
        tft.setCursor(100, y); tft.print("("); reportValueFormat(lines[i].value); tft.print(")");
    }
    tft.setTextColor(COLOR_BLACK); setTextCentered("PRESS START FOR HOME", 120);
}

void handleCancel() {
    digitalWrite(SOURCE_PIN, LOW); beep(1); delay(100); beep(1);
    if (currentMode == MODE_NORMAL) drawMainInterface(); else drawGraphInterface();
}

void beep(int count) {
    for (int i = 0; i < count; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); if (i < count - 1) delay(100); }
}

void setTextCentered(const char* text, int y) {
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((tft.width() - w) / 2, y); tft.print(text);
}
