/*
  CYD Bluetooth Media Controller
  --------------------------------
  ESP32-2432S028 (Cheap Yellow Display)
  - Bluetooth HID keyboard (media keys)
  - TFT_eSPI for display
  - XPT2046 touchscreen
*/

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <BleKeyboard.h>
#include <SPI.h>

#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TFT_BL 21
#define SCR_W 320
#define SCR_H 240

#define C_BG       0x1082
#define C_SURFACE  0x2124
#define C_BORDER   0x4228
#define C_WHITE    0xFFFF
#define C_MUTED    0x8410
#define C_ACCENT   0x07FF
#define C_VOL_BAR  0x2D6B
#define C_BTN_HIT  0x3186

SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TFT_eSPI tft = TFT_eSPI();
BleKeyboard bleKb("CYD Media", "ESP32", 100);

#define TS_MINX 300
#define TS_MAXX 3800
#define TS_MINY 300
#define TS_MAXY 3800

int  volLevel   = 50;
bool isPlaying  = false;
bool bleReady   = false;
unsigned long lastTouch = 0;
#define DEBOUNCE_MS 300

struct Btn { int x, y, w, h; const char* label; int icon; };

Btn buttons[] = {
  { 30,  95, 70, 70, "PREV",   0 },
  { 125, 85, 90, 90, "PLAY",   1 },
  { 220, 95, 70, 70, "NEXT",   3 },
  { 30,  185, 100, 40, "VOL-", 4 },
  { 190, 185, 100, 40, "VOL+", 5 },
};
#define BTN_COUNT 5

void drawSplash();
void drawUI();
void drawStatusBar();
void drawLabel();
void drawPrevNext();
void drawPlayButton();
void drawVolSection();
void drawVolBar();
void drawRoundBtn(Btn& b, uint16_t bg, uint16_t fg, bool pressed);
void flashButton(int i);
void handleButton(int i);

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);
  drawSplash();
  bleKb.begin();
  unsigned long t = millis();
  while (!bleKb.isConnected() && millis() - t < 15000) {
    delay(200);
    static int dot = 0;
    tft.setTextColor(C_MUTED, C_BG);
    tft.setTextSize(1);
    tft.setCursor(130, 165);
    tft.print(dot == 0 ? "   " : dot == 1 ? ".  " : dot == 2 ? ".. " : "...");
    dot = (dot + 1) % 4;
  }
  bleReady = bleKb.isConnected();
  drawUI();
}

void loop() {
  bool nowReady = bleKb.isConnected();
  if (nowReady != bleReady) {
    bleReady = nowReady;
    drawStatusBar();
  }
  if (!ts.tirqTouched() || !ts.touched()) return;
  if (millis() - lastTouch < DEBOUNCE_MS) return;
  lastTouch = millis();
  TS_Point p = ts.getPoint();
  int tx = map(p.x, TS_MINX, TS_MAXX, 0, SCR_W);
  int ty = map(p.y, TS_MINY, TS_MAXY, 0, SCR_H);
  for (int i = 0; i < BTN_COUNT; i++) {
    Btn& b = buttons[i];
    if (tx >= b.x && tx <= b.x + b.w && ty >= b.y && ty <= b.y + b.h) {
      handleButton(i);
      flashButton(i);
      break;
    }
  }
}

void handleButton(int i) {
  if (!bleReady) return;
  switch (i) {
    case 0: bleKb.write(KEY_MEDIA_PREVIOUS_TRACK); break;
    case 1: bleKb.write(KEY_MEDIA_PLAY_PAUSE); isPlaying = !isPlaying; drawPlayButton(); break;
    case 2: bleKb.write(KEY_MEDIA_NEXT_TRACK); break;
    case 3: bleKb.write(KEY_MEDIA_VOLUME_DOWN); volLevel = max(0, volLevel - 5); drawVolBar(); break;
    case 4: bleKb.write(KEY_MEDIA_VOLUME_UP); volLevel = min(100, volLevel + 5); drawVolBar(); break;
  }
}

void drawSplash() {
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.drawString("CYD Media", SCR_W/2, 100);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED);
  tft.drawString("Pair via Bluetooth...", SCR_W/2, 135);
  tft.drawCircle(SCR_W/2, 60, 22, C_ACCENT);
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(2);
  tft.drawString("BT", SCR_W/2, 60);
}

void drawUI() {
  tft.fillScreen(C_BG);
  drawStatusBar();
  drawLabel();
  drawPrevNext();
  drawPlayButton();
  drawVolSection();
}

void drawStatusBar() {
  tft.fillRect(0, 0, SCR_W, 22, C_SURFACE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED, C_SURFACE);
  tft.drawString("CYD Media Control", 8, 11);
  tft.setTextDatum(MR_DATUM);
  if (bleReady) {
    tft.setTextColor(0x07E0, C_SURFACE);
    tft.drawString("BT connected", SCR_W - 8, 11);
  } else {
    tft.setTextColor(0xF800, C_SURFACE);
    tft.drawString("BT disconnected", SCR_W - 8, 11);
  }
}

void drawLabel() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("tap to send media key to your PC", SCR_W/2, 38);
}

void drawPrevNext() {
  drawRoundBtn(buttons[0], C_SURFACE, C_WHITE, false);
  int bx = buttons[0].x + buttons[0].w/2;
  int by = buttons[0].y + buttons[0].h/2;
  tft.fillTriangle(bx+10, by-14, bx+10, by+14, bx-8, by, C_WHITE);
  tft.fillRect(bx-14, by-14, 6, 28, C_WHITE);
  drawRoundBtn(buttons[2], C_SURFACE, C_WHITE, false);
  bx = buttons[2].x + buttons[2].w/2;
  by = buttons[2].y + buttons[2].h/2;
  tft.fillTriangle(bx-10, by-14, bx-10, by+14, bx+8, by, C_WHITE);
  tft.fillRect(bx+8, by-14, 6, 28, C_WHITE);
}

void drawPlayButton() {
  Btn& b = buttons[1];
  int cx = b.x + b.w/2;
  int cy = b.y + b.h/2;
  int r  = b.w/2;
  tft.fillCircle(cx, cy, r, C_ACCENT);
  tft.fillCircle(cx, cy, r - 4, C_SURFACE);
  if (isPlaying) {
    tft.fillRect(cx - 14, cy - 16, 10, 32, C_WHITE);
    tft.fillRect(cx + 4,  cy - 16, 10, 32, C_WHITE);
  } else {
    tft.fillTriangle(cx - 12, cy - 18, cx - 12, cy + 18, cx + 18, cy, C_WHITE);
  }
}

void drawVolSection() {
  drawRoundBtn(buttons[3], C_SURFACE, C_WHITE, false);
  int bx = buttons[3].x + buttons[3].w/2;
  int by = buttons[3].y + buttons[3].h/2;
  tft.fillRect(bx - 10, by - 3, 20, 6, C_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_MUTED, C_SURFACE);
  tft.setTextSize(1);
  tft.drawString("VOL-", bx, by + 14);
  drawRoundBtn(buttons[4], C_SURFACE, C_WHITE, false);
  bx = buttons[4].x + buttons[4].w/2;
  by = buttons[4].y + buttons[4].h/2;
  tft.fillRect(bx - 10, by - 3, 20, 6, C_WHITE);
  tft.fillRect(bx - 3,  by - 10, 6, 20, C_WHITE);
  tft.setTextColor(C_MUTED, C_SURFACE);
  tft.drawString("VOL+", bx, by + 14);
  drawVolBar();
}

void drawVolBar() {
  int barX = 140, barY = 193, barW = 40, barH = 24;
  tft.fillRoundRect(barX, barY, barW, barH, 4, C_BORDER);
  int fill = map(volLevel, 0, 100, 0, barW);
  if (fill > 0) tft.fillRoundRect(barX, barY, fill, barH, 4, C_VOL_BAR);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE, fill > barW/2 ? C_VOL_BAR : C_BORDER);
  char buf[4]; itoa(volLevel, buf, 10);
  tft.drawString(buf, barX + barW/2, barY + barH/2);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("volume", barX + barW/2, barY - 10);
}

void drawRoundBtn(Btn& b, uint16_t bg, uint16_t fg, bool pressed) {
  uint16_t col = pressed ? C_BTN_HIT : bg;
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, col);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, C_BORDER);
}

void flashButton(int i) {
  Btn& b = buttons[i];
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, C_BTN_HIT);
  delay(80);
  if (i == 0) { drawPrevNext(); }
  else if (i == 1) { drawPlayButton(); }
  else if (i == 2) { drawPrevNext(); }
  else { drawVolSection(); }
}
