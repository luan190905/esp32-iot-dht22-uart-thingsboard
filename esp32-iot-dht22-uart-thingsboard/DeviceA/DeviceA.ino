// ===== THIET BI A: NUT CAM BIEN (ESP32 + DHT22 + Button + LED) =====
#include <DHT.h>

#define DHT_PIN   15
#define LED_PIN   2
#define BTN_PIN   4
#define RX2_PIN   16
#define TX2_PIN   17
#define DHTTYPE   DHT22

const uint32_t SENSE_PERIOD = 2000;  // chu ky doc cam bien (ms)
const uint32_t LED_ON_TIME  = 500;   // thoi gian led sang (ms)
const uint32_t ACK_TIMEOUT  = 500;   // thoi gian cho bao nhan (ms)
const uint32_t RETRY_PERIOD = 1000;  // khoang cach giua cac lan gui (ms)
const uint8_t  MAX_RETRY    = 3;     // so lan gui lai toi da
const uint32_t DEBOUNCE_MS  = 30;

DHT dht(DHT_PIN, DHTTYPE);

float temperature = NAN, humidity = NAN;
uint32_t tSense = 0, tLedOn = 0;
bool ledState = false;

enum TxState { TX_IDLE, TX_WAIT_ACK, TX_WAIT_RETRY };
TxState txState = TX_IDLE;
uint32_t tLastSend = 0;
uint8_t retryCnt = 0;
uint8_t frame[8];
uint8_t ackState = 0;

int lastBtnRaw = HIGH, btnStable = HIGH;
uint32_t tBtnChange = 0;

uint8_t calcCrc(const uint8_t *d, uint8_t n) {
  uint8_t c = 0;
  for (uint8_t i = 0; i < n; i++) c ^= d[i];
  return c;
}

// Khung: FD AD LEN T_H T_L H_H H_L CRC  (T,H = gia tri x10, int16)
void buildFrame() {
  int16_t t = (int16_t)lroundf(temperature * 10);
  int16_t h = (int16_t)lroundf(humidity * 10);
  frame[0] = 0xFD;
  frame[1] = 0xAD;
  frame[2] = 4;
  frame[3] = (uint8_t)((t >> 8) & 0xFF);
  frame[4] = (uint8_t)(t & 0xFF);
  frame[5] = (uint8_t)((h >> 8) & 0xFF);
  frame[6] = (uint8_t)(h & 0xFF);
  frame[7] = calcCrc(frame, 7);
}

void sendFrame() {
  Serial2.write(frame, sizeof(frame));
  tLastSend = millis();
  Serial.printf("[A] Gui khung (lan %u): ", retryCnt + 1);
  for (uint8_t i = 0; i < sizeof(frame); i++) Serial.printf("%02X ", frame[i]);
  Serial.println();
}

// May trang thai nhan bao nhan FD DE
bool checkAck() {
  bool got = false;
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    if (ackState == 1 && b == 0xDE) { got = true; ackState = 0; }
    else ackState = (b == 0xFD) ? 1 : 0;
  }
  return got;
}

void sensorTask() {
  uint32_t now = millis();
  if (now - tSense >= SENSE_PERIOD) {
    tSense += SENSE_PERIOD;
    digitalWrite(LED_PIN, HIGH);          // bat led bao hieu
    ledState = true;
    tLedOn = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      Serial.println("[A] Loi doc DHT22");
    } else {
      temperature = t; humidity = h;
      Serial.printf("[A] T=%.1f C, H=%.1f %%\n", t, h);
    }
  }
  if (ledState && (now - tLedOn >= LED_ON_TIME)) {
    digitalWrite(LED_PIN, LOW);           // tat led phan con lai cua chu ky
    ledState = false;
  }
}

void onButtonPressed() {
  if (txState != TX_IDLE) { Serial.println("[A] Dang gui, bo qua"); return; }
  if (isnan(temperature) || isnan(humidity)) { Serial.println("[A] Chua co du lieu"); return; }
  buildFrame();
  retryCnt = 0;
  sendFrame();
  txState = TX_WAIT_ACK;
}

void buttonTask() {
  int raw = digitalRead(BTN_PIN);
  if (raw != lastBtnRaw) { tBtnChange = millis(); lastBtnRaw = raw; }
  if ((millis() - tBtnChange > DEBOUNCE_MS) && raw != btnStable) {
    btnStable = raw;
    if (btnStable == LOW) onButtonPressed();   // phat hien nhan xuong
  }
}

void txTask() {
  uint32_t now = millis();
  switch (txState) {
    case TX_WAIT_ACK:
      if (checkAck()) {
        Serial.println("[A] Nhan bao nhan -> ket thuc");
        txState = TX_IDLE;
      } else if (now - tLastSend >= ACK_TIMEOUT) {
        if (retryCnt >= MAX_RETRY) {
          Serial.println("[A] Gui that bai sau 3 lan gui lai");
          txState = TX_IDLE;
        } else {
          Serial.println("[A] Het thoi gian cho bao nhan");
          txState = TX_WAIT_RETRY;
        }
      }
      break;
    case TX_WAIT_RETRY:
      while (Serial2.available()) Serial2.read();  // bo qua du lieu den tre
      if (now - tLastSend >= RETRY_PERIOD) {
        retryCnt++;
        sendFrame();
        txState = TX_WAIT_ACK;
      }
      break;
    default:
      while (Serial2.available()) Serial2.read();
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  dht.begin();
  Serial.println("[A] San sang");
}

void loop() {
  sensorTask();
  buttonTask();
  txTask();
}
