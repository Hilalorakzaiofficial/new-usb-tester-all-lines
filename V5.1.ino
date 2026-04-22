// ============================================
// USB LINE TESTER - VERSION 5.1
// Enhanced Diode Mode with Professional UI
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

// Analog pins for USB line testing - V5.1 Pin Mapping
#define PIN_VBUS  A0   // VBUS (VCC / Sensing input)
#define PIN_DMIN  A1   // D- (Data Negative)
#define PIN_DPLUS A2   // D+ (Data Positive)
#define PIN_CC1   A3   // CC1
#define PIN_CC2   A4   // CC2
#define PIN_TX    A5   // TX
#define PIN_RX    A6   // RX

// ============================================
// DIODE MODE THRESHOLDS - CALIBRATED V5.1
// Hardware measures FORWARD, Display shows REVERSE
// ============================================
#define THRESHOLD_SHORT_RAW      0     // 0 = Direct short
#define THRESHOLD_HALFSHORT_RAW  120   // < 120 = Half short (resistive short)
#define THRESHOLD_OK_MIN_RAW     300   // 300 - 800 = Good diode (normal range)
#define THRESHOLD_OK_MAX_RAW     800
#define THRESHOLD_REVERSE_RAW    1000  // 1000 - 2000 = Reverse bias (display as OL)
#define THRESHOLD_OL_RAW         1023  // > 1023 = Open line

// Calibration multiplier to match professional multimeters
// If raw reads 800, we want display ~450 (like real meters)
#define CALIBRATION_FACTOR       0.56  // 800 * 0.56 ≈ 450

// ============================================
// DISPLAY COLORS - INVERTED FOR DISPLAY HARDWARE V5.1
// White in code = Black on screen, all colors inverted
// Screen: BLACK background, WHITE text, RED accent, GREEN status
// Code: WHITE background (0xFFFF), BLACK text (0x0000), CYAN accent (0x07FF), MAGENTA status (0xF81F)
// ============================================
#define COLOR_BLACK      0x0000  // Shows as WHITE on screen
#define COLOR_WHITE      0xFFFF  // Shows as BLACK on screen (used for background)

// Background colors - code values that show as dark colors on screen
#define COLOR_BG         0xFFFF  // WHITE in code = BLACK background on screen
#define COLOR_HEADER_BG  0x3186  // Dark gray in code = GRAY header background on screen
#define COLOR_HIGHLIGHT  0x8410  // Dark gray in code = light highlight on screen

// Text colors - code values that show as light colors on screen
#define COLOR_TEXT       0x0000  // BLACK in code = WHITE text on screen
#define COLOR_DARK       0x7BEF  // Light gray in code = DARK text on screen
#define COLOR_GRAY       0xBDF7  // Very light gray in code = medium gray on screen

// Accent colors
#define COLOR_HEADER_TEXT 0x07FF  // CYAN in code = YELLOW text on screen (for SLS accent)
#define COLOR_ACCENT     0x0000   // BLACK in code = WHITE accent on screen
#define COLOR_SUCCESS    0xF81F   // MAGENTA in code = GREEN on screen
#define COLOR_WARNING    0x001F    // BLUE in code = YELLOW on screen
#define COLOR_DANGER     0x07FF    // CYAN in code = RED on screen
#define COLOR_INFO       0xFFE0    // YELLOW in code = BLUE on screen

// Status colors for diode readings - INVERTED
#define COLOR_SHORT      0x07FF   // CYAN in code = RED on screen
#define COLOR_HALFSHORT  0x02BC   // Teal in code = ORANGE on screen
#define COLOR_OK         0xF81F   // MAGENTA in code = GREEN on screen
#define COLOR_OL         0x07E0   // GREEN in code = MAGENTA on screen
#define COLOR_REVERSE    0xFFE0   // YELLOW in code = BLUE on screen

// ============================================
// VERSION INFO
// ============================================
#define VERSION_MAJOR   5
#define VERSION_MINOR   1
#define VERSION_STRING  "v5.1"
// ============================================
#define SPLASH_DELAY    3000  // 3 seconds
#define TEST_TIME_MS    700  // 0.70 seconds per line (~5-6 seconds total)
#define BEEP_DURATION   100
#define DEBOUNCE_DELAY  50

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
};

// Initialize line data with V5.1 structure
// Lines: VBUS, D-, D+, CC1, CC2, TX, RX, GND
LineInfo lines[] = {
    {"VBUS", PIN_VBUS,  COLOR_BLACK, "", 0.0, false},
    {"D-",   PIN_DMIN,  COLOR_BLACK, "", 0.0, false},
    {"D+",   PIN_DPLUS, COLOR_BLACK, "", 0.0, false},
    {"CC1",  PIN_CC1,   COLOR_BLACK, "", 0.0, false},
    {"CC2",  PIN_CC2,   COLOR_BLACK, "", 0.0, false},
    {"TX",   PIN_TX,    COLOR_BLACK, "", 0.0, false},
    {"RX",   PIN_RX,    COLOR_BLACK, "", 0.0, false},
    {"GND",  255,       COLOR_BLACK, "", 0.0, false}  // GND is reference
};

const int NUM_LINES = 8;

// ============================================
// GLOBAL OBJECTS
// ============================================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Screen dimensions for 1.8" ST7735
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 160

// Layout constants - V5.1: smaller text to fit 8 lines on one screen
// Header: 0-16, Column labels: 16-26, Lines: 26-154 (8 lines x ~16px), Footer: 154-160
#define HEADER_Y        0
#define COL_LABEL_Y     17
#define LINES_START_Y   26    // Start of first line row
#define LINE_HEIGHT     16    // Reduced from 18 to fit 8 lines
#define FOOTER_Y        154
#define STATUS_X        48    // Status column X position
#define VALUE_X         84    // Value column X position

// ============================================
// SETUP
// ============================================
void setup() {
    // Initialize pins
    pinMode(START_BUTTON, INPUT_PULLUP);
    pinMode(CANCEL_BUTTON, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SOURCE_PIN, OUTPUT);
    
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(SOURCE_PIN, LOW);
    
    // Initialize display
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);  // Portrait mode for full 128x160 resolution
    
    // Show splash screen
    showSplashScreen();
    
    // Initial beep
    beep(1);
    
    // Draw main interface
    drawMainInterface();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // Check for CANCEL button at any time
    if (digitalRead(CANCEL_BUTTON) == LOW) {
        delay(DEBOUNCE_DELAY);
        if (digitalRead(CANCEL_BUTTON) == LOW) {
            handleCancel();
        }
    }
    
    // Check for START button to begin test
    if (digitalRead(START_BUTTON) == LOW) {
        delay(DEBOUNCE_DELAY);
        if (digitalRead(START_BUTTON) == LOW) {
            runTestSequence();
        }
    }
    
    delay(10);  // Small delay to prevent busy-waiting
}

// ============================================
// DISPLAY FUNCTIONS
// ============================================

void drawLogo(int centerX, int centerY, int size) {
    // Draw "SLS" with individual letter colors
    // S = White (code BLACK), L = Red (code CYAN), S = White (code BLACK)
    tft.setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    
    // Calculate total width of "SLS"
    tft.getTextBounds("SLS", 0, 0, &x1, &y1, &w, &h);
    int startX = centerX - w/2;
    int letterY = centerY - h/2;
    
    // Get width of single "S"
    uint16_t sW, sH, lW, lH;
    tft.getTextBounds("S", 0, 0, &x1, &y1, &sW, &sH);
    tft.getTextBounds("L", 0, 0, &x1, &y1, &lW, &lH);
    
    // Draw first S in White (code BLACK)
    tft.setTextColor(COLOR_BLACK);
    tft.setCursor(startX, letterY);
    tft.print("S");
    
    // Draw L in Red (code CYAN)
    tft.setTextColor(0x07FF);  // Cyan in code = Red on screen
    tft.setCursor(startX + sW - 2, letterY);
    tft.print("L");
    
    // Draw second S in White (code BLACK)
    tft.setTextColor(COLOR_BLACK);
    tft.setCursor(startX + sW + lW - 4, letterY);
    tft.print("S");
}

void showSplashScreen() {
    tft.fillScreen(COLOR_BG);
    
    // Draw Logo at top
    drawLogo(SCREEN_WIDTH/2, 25, 25);
    
    // MR.HILAL - MR in Red, .HILAL in White
    tft.setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    
    // Calculate positions
    tft.getTextBounds("MR", 0, 0, &x1, &y1, &w, &h);
    int mrWidth = w;
    tft.getTextBounds(".HILAL", 0, 0, &x1, &y1, &w, &h);
    int hilalWidth = w;
    int totalWidth = mrWidth + hilalWidth - 4;
    int startX = (SCREEN_WIDTH - totalWidth) / 2;
    
    // MR in Red (code CYAN = 0x07FF)
    tft.setTextColor(0x07FF);
    tft.setCursor(startX, 50);
    tft.print("MR");
    
    // .HILAL in White (code BLACK)
    tft.setTextColor(COLOR_BLACK);
    tft.setCursor(startX + mrWidth - 4, 50);
    tft.print(".HILAL");
    
    // USB LINE TESTER in White (code BLACK)
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLACK);
    setTextCentered("USB LINE TESTER", 75);
    
    // V5.1 in Red (code CYAN)
    tft.setTextColor(0x07FF);
    setTextCentered("V5.1", 92);
    
    // SKYLAB SERVICES in White (code BLACK)
    tft.setTextColor(COLOR_BLACK);
    setTextCentered("SKYLAB SERVICES", 110);
    
    delay(SPLASH_DELAY);
}

void drawMainInterface() {
    tft.fillScreen(COLOR_BG);
    
    // Draw header with RED background (code CYAN) and WHITE text (code BLACK)
    tft.fillRect(0, 0, SCREEN_WIDTH, 16, 0x07FF);  // CYAN in code = RED on screen
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLACK);  // BLACK in code = WHITE text on screen
    setTextCentered("USB TESTER V5.1", 4);
    
    // Draw accent separator
    tft.drawLine(0, 16, SCREEN_WIDTH, 16, COLOR_GRAY);
    tft.drawLine(0, 17, SCREEN_WIDTH, 17, COLOR_DARK);
    
    // Draw column headers
    tft.setTextColor(COLOR_BLACK);  // White on screen
    tft.setCursor(5, COL_LABEL_Y);
    tft.print("LINE");
    tft.setCursor(STATUS_X + 4, COL_LABEL_Y);
    tft.print("STAT");
    tft.setCursor(VALUE_X + 2, COL_LABEL_Y);
    tft.print("VAL");
    
    // Draw subtle grid lines
    for (int i = 0; i <= NUM_LINES; i++) {
        int yLine = LINES_START_Y + LINE_HEIGHT - 2 + (i * LINE_HEIGHT);
        tft.drawLine(0, yLine, SCREEN_WIDTH, yLine, COLOR_DARK);
    }
    
    // Draw initial line rows
    for (int i = 0; i < NUM_LINES; i++) {
        drawLineRow(i, false);
    }
    
    // Draw footer
    drawFooter();
}

void drawLineRow(int lineIndex, bool highlight) {
    int yPos = LINES_START_Y + (lineIndex * LINE_HEIGHT);
    
    // Clear row area with highlight or normal background
    if (highlight) {
        tft.fillRect(0, yPos, SCREEN_WIDTH, LINE_HEIGHT - 1, COLOR_HIGHLIGHT);
    } else {
        // All rows use black background
        tft.fillRect(0, yPos, SCREEN_WIDTH, LINE_HEIGHT - 1, COLOR_BG);
    }
    
    LineInfo& line = lines[lineIndex];
    
    // Line name - White text on black background (text size 1 = smallest)
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLACK);  // Black in code = White on screen
    tft.setCursor(5, yPos + 3);
    tft.print(line.name);
    
    // Status and Value
    if (line.tested) {
        // Calculate status text width for centering
        int16_t x1, y1;
        uint16_t statusW, h;
        tft.getTextBounds(line.statusText, 0, 0, &x1, &y1, &statusW, &h);
        
        // Status centered in the STAT column area
        int statusCenterX = STATUS_X + 10;
        int statusX = statusCenterX - (statusW / 2);
        
        // Status with its color (already inverted for correct display)
        tft.setTextColor(line.statusColor);
        tft.setCursor(statusX, yPos + 3);
        tft.print(line.statusText);
        
        // Value (format: 0.000) - White text
        tft.setTextColor(COLOR_BLACK);  // Black in code = White on screen
        tft.setCursor(VALUE_X + 2, yPos + 3);
        
        // Convert to millivolts for display
        int displayMillivolts = (int)(line.value * 1000);
        if (displayMillivolts < 0) displayMillivolts = 0;
        if (displayMillivolts > 1999) displayMillivolts = 1999;
        
        int whole = displayMillivolts / 1000;
        int frac = displayMillivolts % 1000;
        
        tft.print(whole);
        tft.print(".");
        tft.print(frac / 100);
        tft.print((frac / 10) % 10);
        tft.print(frac % 10);
    } else {
        // Not tested yet - centered in status area
        int16_t x1, y1;
        uint16_t w, h;
        tft.getTextBounds("---", 0, 0, &x1, &y1, &w, &h);
        int notTestedX = STATUS_X + 10 - (w / 2);
        
        tft.setTextColor(COLOR_DARK);  // Gray on screen
        tft.setCursor(notTestedX, yPos + 3);
        tft.print("---");
        tft.setCursor(VALUE_X + 2, yPos + 3);
        tft.print("----");
    }
}

void updateLineStatus(int lineIndex, uint16_t rawADC) {
    LineInfo& line = lines[lineIndex];
    
    // V5.1 Diode Mode Logic
    // Hardware measures FORWARD bias, Display shows REVERSE bias values
    // With calibration to match professional multimeters
    
    int displayValue;
    
    if (rawADC == THRESHOLD_SHORT_RAW) {
        // Direct short (0)
        displayValue = 0;
        line.statusColor = COLOR_SHORT;
        line.statusText = "SHORT";
        
    } else if (rawADC < THRESHOLD_HALFSHORT_RAW) {
        // Half short / resistive short (< 120) - show as OK
        displayValue = (1000 - rawADC);  // Invert for display
        line.statusColor = COLOR_OK;
        line.statusText = "OK";
        
    } else if (rawADC >= THRESHOLD_OK_MIN_RAW && rawADC <= THRESHOLD_OK_MAX_RAW) {
        // Normal diode range (300 - 800)
        displayValue = (1000 - rawADC);  // Invert: 300→700, 800→200
        line.statusColor = COLOR_OK;
        line.statusText = "OK";
        
    } else if (rawADC >= THRESHOLD_REVERSE_RAW && rawADC < THRESHOLD_OL_RAW) {
        // Reverse bias range (1000 - 1023) - show as ABOVE
        line.value = 0.0;  // Force 0.000 display
        line.statusColor = COLOR_REVERSE;
        line.statusText = "ABOVE";
        
    } else if (rawADC >= THRESHOLD_OL_RAW) {
        // Open line - force value to 0.000
        line.value = 0.0;
        line.statusColor = COLOR_OL;
        line.statusText = "OL";
        
    } else {
        // Transition zone (120-300 or 800-1000) - show as OK
        displayValue = (1000 - rawADC);
        line.statusColor = COLOR_OK;
        line.statusText = "OK";
    }
    
    // Apply calibration factor to match professional multimeters
    // For OK range: display inverted reverse bias values (300-800 raw → 700-200 display)
    if (rawADC >= THRESHOLD_OK_MIN_RAW && rawADC <= THRESHOLD_OK_MAX_RAW) {
        // Inverted display for OK range
        int invertedValue = (1000 - rawADC);  // 300→700, 800→200
        line.value = (invertedValue * CALIBRATION_FACTOR) / 1000.0;
    } else if (rawADC < THRESHOLD_HALFSHORT_RAW) {
        // Half short - also inverted
        int invertedValue = (1000 - rawADC);
        line.value = (invertedValue * CALIBRATION_FACTOR) / 1000.0;
    } else {
        // Transition zone
        int invertedValue = (1000 - rawADC);
        line.value = (invertedValue * CALIBRATION_FACTOR) / 1000.0;
    }
    // Note: OL and RV cases already set line.value = 0.0 above
    
    line.tested = true;
    
    // Redraw this specific row
    drawLineRow(lineIndex, false);
}

void drawFooter() {
    // Footer removed - no display
}

void setTextCentered(const char* text, int y) {
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SCREEN_WIDTH - w) / 2, y);
    tft.print(text);
}

// ============================================
// TESTING FUNCTIONS
// ============================================
void runTestSequence() {
    // Beep at start
    beep(1);
    
    // Reset all lines
    for (int i = 0; i < NUM_LINES; i++) {
        lines[i].tested = false;
        lines[i].statusText = "";
        lines[i].value = 0.0;
    }
    
    // Redraw interface
    drawMainInterface();
    
    // Enable source pin
    digitalWrite(SOURCE_PIN, HIGH);
    
    // Test each line step-by-step
    for (int i = 0; i < NUM_LINES; i++) {
        // Check for cancel during testing
        if (checkCancel()) {
            digitalWrite(SOURCE_PIN, LOW);
            return;
        }
        
        // Highlight current line being tested
        drawLineRow(i, true);
        
        // Read ADC value (except for GND which is reference)
        uint16_t rawADC;
        if (lines[i].pin != 255) {
            rawADC = analogRead(lines[i].pin);
        } else {
            // GND - force SHORT status with 0.000 value
            lines[i].tested = true;
            lines[i].statusColor = COLOR_SHORT;  // Red for short
            lines[i].statusText = "SHORT";
            lines[i].value = 0.0;
            drawLineRow(i, true);
            delay(100);
            unsigned long startTime = millis();
            while (millis() - startTime < TEST_TIME_MS) {
                if (checkCancel()) {
                    digitalWrite(SOURCE_PIN, LOW);
                    return;
                }
                delay(10);
            }
            drawLineRow(i, false);
            continue;  // Skip normal processing for GND
        }
        
        // Update status
        updateLineStatus(i, rawADC);
        
        // Small delay for visual effect
        delay(100);
        
        // Test time per line (distributed check for cancel)
        unsigned long startTime = millis();
        while (millis() - startTime < TEST_TIME_MS) {
            if (checkCancel()) {
                digitalWrite(SOURCE_PIN, LOW);
                return;
            }
            delay(10);
        }
    }
    
    // Disable source pin
    digitalWrite(SOURCE_PIN, LOW);
    
    // Completion beep (2 times)
    beep(2);
}

bool checkCancel() {
    if (digitalRead(CANCEL_BUTTON) == LOW) {
        delay(DEBOUNCE_DELAY);
        if (digitalRead(CANCEL_BUTTON) == LOW) {
            return true;
        }
    }
    return false;
}

void handleCancel() {
    // Visual feedback with V5.1 colors
    tft.fillRect(0, FOOTER_Y + 2, SCREEN_WIDTH, 10, COLOR_DANGER);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(5, FOOTER_Y + 4);
    tft.print("CANCELLED");
    
    // Ensure source is off
    digitalWrite(SOURCE_PIN, LOW);
    
    delay(500);
    
    // Redraw footer
    drawFooter();
}

// ============================================
// BUZZER FUNCTION
// ============================================
void beep(int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(BEEP_DURATION);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < count - 1) {
            delay(BEEP_DURATION);
        }
    }
}
