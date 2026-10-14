// ===== THIET BI B: GATEWAY (ESP32: UART <-> WiFi/ThingsBoard) =====
#include <WiFi.h>
#include <HTTPClient.h>

#define RX2_PIN 16
#define TX2_PIN 17

const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASS = "";
const char *TB_HOST   = "thingsboard.cloud";
const char *TB_TOKEN  = "YOUR_ACCESS_TOKEN";   // Access token cua device tren ThingsBoard

enum RxState { RX_SOF1, RX_SOF2, RX_LEN, RX_DATA, RX_CRC };
RxState rxState = RX_SOF1;
uint8_t buf[8];
uint8_t idx = 0;
uint32_t tLastByte = 0;

const uint8_t ACK_FRAME[2] = {0xFD, 0xDE};
bool uploadPending = false;
float rxTemp = 0, rxHum = 0;

uint8_t calcCrc(const uint8_t *d, uint8_t n) {
  uint8_t c = 0;
  for (uint8_t i = 0; i < n; i++) c ^= d[i];
  return c;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("[B] Ket noi WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " that bai");
}

void uploadToThingsBoard(float t, float h) {
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = String("http://") + TB_HOST + "/api/v1/" + TB_TOKEN + "/telemetry";
  String payload = "{\"temperature\":" + String(t, 1) + ",\"humidity\":" + String(h, 1) + "}";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  Serial.printf("[B] ThingsBoard HTTP code = %d, payload = %s\n", code, payload.c_str());
  http.end();
}

void onFrame(const uint8_t *f) {
  Serial2.write(ACK_FRAME, 2);                 // 1) gui bao nhan FD DE
  Serial2.flush();
  int16_t t = (int16_t)((f[3] << 8) | f[4]);   // 2) giai ma du lieu
  int16_t h = (int16_t)((f[5] << 8) | f[6]);
  rxTemp = t / 10.0f;
  rxHum  = h / 10.0f;
  Serial.printf("[B] Nhan khung tu A: Nhiet do = %.1f C, Do am = %.1f %%\n", rxTemp, rxHum);
  uploadPending = true;                        // 3) day len ThingsBoard trong loop()
}

void processByte(uint8_t b) {
  switch (rxState) {
    case RX_SOF1:
      if (b == 0xFD) { buf[0] = b; rxState = RX_SOF2; }
      break;
    case RX_SOF2:
      if (b == 0xAD) { buf[1] = b; rxState = RX_LEN; }
      else if (b != 0xFD) rxState = RX_SOF1;
      break;
    case RX_LEN:
      if (b == 4) { buf[2] = b; idx = 3; rxState = RX_DATA; }
      else rxState = RX_SOF1;
      break;
    case RX_DATA:
      buf[idx++] = b;
      if (idx >= 7) rxState = RX_CRC;
      break;
    case RX_CRC:
      rxState = RX_SOF1;
      if (b == calcCrc(buf, 7)) { buf[7] = b; onFrame(buf); }
      else Serial.println("[B] Sai CRC, bo qua khung");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  connectWiFi();
  Serial.println("[B] Gateway san sang");
}

void loop() {
  while (Serial2.available()) {
    tLastByte = millis();
    processByte(Serial2.read());
  }
  // Timeout giua cac byte: huy khung dang nhan do
  if (rxState != RX_SOF1 && millis() - tLastByte > 100) rxState = RX_SOF1;

  if (uploadPending) {
    uploadPending = false;
    uploadToThingsBoard(rxTemp, rxHum);
  }
}
