/* * SHARKOS 2.0 - SUPERMINI S3 "STRETCH" BUILD
 * TARGET: ESP32-S3 (SuperMini)
 */

// 1. FORCE OVERRIDE (MUST BE AT THE VERY TOP)
#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED
#define ILI9341_DRIVER
#define TFT_MOSI 3
#define TFT_SCLK 5
#define TFT_CS   4
#define TFT_DC   21
#define TFT_RST  10
#define TFT_BL   2
#define SPI_FREQUENCY 27000000 
#endif

#include <Keypad.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_now.h>
#include <TJpg_Decoder.h>

/* --- 2. HARDWARE & LOGIC DEFINES --- */
#define CASIO_ASH 0xDEFB
#define CASIO_INK 0x0000
#define CHORD_KEY '7'      
#define PANIC_KEY 'A'      
#define JUMP_HOLD_TIME 2000 
#define SD_CS  1           

/* --- 3. GLOBAL STATE --- */
struct SharkOS {
    int currentRoom = 1;
    bool isBeastMode = false;
    bool panicActive = false;
    unsigned long lastActivity = 0;
    String calcDisplay = "0.";
    String queryBuffer = "";
    bool needsRedraw = true;
    int activeDecker = 0; 
} shark;

TFT_eSPI tft = TFT_eSPI();

/* --- 4. KEYPAD CONFIG --- */
const byte ROWS = 5; 
const byte COLS = 6; 
char keys[ROWS][COLS] = {
  {'7','8','9','D','A',' '}, {'4','5','6','X','/',' '},
  {'1','2','3','+','-',' '}, {'0','.','^','=','E',' '},
  {'U','L','R','V','M','S'} 
};

byte rowPins[ROWS] = {6, 7, 8, 9, 1}; 
byte colPins[COLS] = {0, 47, 48, 35, 36, 37}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

/* --- 5. GHOSTNET DATA --- */
typedef struct struct_message {
    char msg[64];
    int val;
} struct_message;
struct_message incomingData;

// Updated callback for ESP32 Core 3.0+
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming, int len) {
    memcpy(&incomingData, incoming, sizeof(incomingData));
    shark.needsRedraw = true;
}

/* --- 6. UTILITIES --- */
void proPrint(String text, int x, int y, uint16_t color) {
    text.replace("therefore", ".:."); 
    text.replace("*", "x");           
    tft.setTextColor(color);
    tft.setCursor(x, y); 
    tft.print(text);
}

void forceGlobalPanic() {
    shark.isBeastMode = false; 
    shark.currentRoom = 1;
    shark.calcDisplay = "0."; 
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(160); 
    digitalWrite(TFT_BL, LOW); 
    tft.fillScreen(CASIO_ASH); 
    shark.needsRedraw = true;
}

/* --- 7. ROOM ENGINES --- */
void drawRoom2_GreatHall(int action) {
    if (action == 1 && shark.activeDecker > 0) { shark.activeDecker--; shark.needsRedraw = true; }
    if (action == 3 && shark.activeDecker < 4) { shark.activeDecker++; shark.needsRedraw = true; }
    if (action == 5) { shark.currentRoom = shark.activeDecker + 4; shark.needsRedraw = true; return; }

    if (!shark.needsRedraw) return;
    tft.fillScreen(CASIO_ASH);
    const char* options[] = {"4: SOLVER", "5: MATCHER", "6: LIBRARY", "7: GHOST", "8: SYSTEM"};
    for(int i=0; i<5; i++) {
        int y = 40 + (i*35);
        if(i == shark.activeDecker) tft.fillRect(10, y-5, 220, 30, CASIO_INK);
        tft.setTextColor(i == shark.activeDecker ? CASIO_ASH : CASIO_INK);
        tft.drawString(options[i], 20, y, 2);
    }
    shark.needsRedraw = false;
}

void drawRoom5_Matcher() {
    if (!shark.needsRedraw) return;
    tft.fillScreen(CASIO_ASH);
    tft.fillRect(0, 0, 240, 25, CASIO_INK);
    tft.setTextColor(CASIO_ASH); 
    tft.drawCentreString("VEC-MATCH ENGINE", 120, 5, 1);
    proPrint("SCALAR DETECTED: AUTO-ADJUST", 10, 50, CASIO_INK);
    shark.needsRedraw = false;
}

void drawRoom7_Ghost() {
    if (!shark.needsRedraw) return;
    tft.fillScreen(CASIO_ASH);
    tft.drawFastHLine(0, 30, 240, CASIO_INK);
    tft.setTextColor(CASIO_INK);
    tft.drawString("GHOSTNET MSG:", 10, 10, 2);
    tft.setCursor(10, 50); 
    tft.print(incomingData.msg);
    shark.needsRedraw = false;
}

/* --- 8. MAIN --- */
unsigned long jumpTimer = 0;

void setup() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); 
    pinMode(SD_CS, OUTPUT); 
    digitalWrite(SD_CS, HIGH);
    
    tft.init(); 
    tft.setRotation(1);
    
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }
    
    tft.fillScreen(CASIO_ASH);
    shark.lastActivity = millis();
}

void loop() {
    char key = keypad.getKey();
    int action = 0; 
    if (key == 'U') action = 1; 
    if (key == 'V') action = 3; 
    if (key == 'E') action = 5;

    // Jump Handshake Logic
    if (key == CHORD_KEY) {
        if (jumpTimer == 0) jumpTimer = millis();
        if (millis() - jumpTimer > JUMP_HOLD_TIME && !shark.isBeastMode) {
            shark.isBeastMode = true; 
            shark.currentRoom = 2; 
            shark.needsRedraw = true;
        }
    } else if (key == NO_KEY) { 
        jumpTimer = 0; 
    }

    if (key == PANIC_KEY && shark.isBeastMode) forceGlobalPanic();

    if (!shark.isBeastMode) {
        if (key != NO_KEY && key != CHORD_KEY) { 
            shark.calcDisplay += key;
            shark.needsRedraw = true; 
        }
        if (shark.needsRedraw) {
            tft.fillScreen(CASIO_ASH);
            tft.setTextColor(CASIO_INK); 
            tft.drawString(shark.calcDisplay, 20, 40, 4);
            shark.needsRedraw = false;
        }
    } else {
        if (key == 'M') { shark.currentRoom = 2; shark.needsRedraw = true; }
        switch(shark.currentRoom) {
            case 2: drawRoom2_GreatHall(action); break;
            case 5: drawRoom5_Matcher(); break;
            case 7: drawRoom7_Ghost(); break;
        }
    }
    yield();
}
