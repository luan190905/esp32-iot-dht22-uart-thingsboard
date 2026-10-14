# Mô phỏng hệ thống IoT ESP32: DHT22 – UART – ThingsBoard

Mô hình mô phỏng trên [Velxio.dev](https://velxio.dev) gồm 2 ESP32:

- **Thiết bị A (node cảm biến):** đọc DHT22 mỗi 2 s, LED báo hiệu (sáng 500 ms). Nhấn nút thì gửi khung dữ liệu qua UART tới B, chờ báo nhận 500 ms, gửi lại tối đa 3 lần (cách nhau 1 s).
- **Thiết bị B (gateway):** nhận khung, kiểm tra CRC, gửi báo nhận, in ra Serial Monitor và đẩy lên ThingsBoard.

🔗 **Dự án mô phỏng:** [DÁN LINK VELXIO TẠI ĐÂY]

![Mô hình mô phỏng](docs/images/mo_hinh.png)

## Cấu trúc thư mục

```
DeviceA/DeviceA.ino   # Firmware thiết bị A
DeviceB/DeviceB.ino   # Firmware thiết bị B (gateway)
docs/                 # Báo cáo và hình ảnh
```

## Đấu nối

| Thiết bị | Linh kiện | Chân ESP32 | Ghi chú |
|---|---|---|---|
| A | DHT22 (DATA) | GPIO15 | VCC 3V3, GND |
| A | LED (R 220 Ω) | GPIO2 | |
| A | Button | GPIO4 | `INPUT_PULLUP`, nhấn = LOW; 1 chân bên trái → GPIO4, 1 chân bên phải → GND |
| A | UART2 TX / RX | GPIO17 / GPIO16 | TX(A) → RX(B), RX(A) ← TX(B) |
| B | UART2 TX / RX | GPIO17 / GPIO16 | Nối chéo với A, chung GND |

## Khung dữ liệu (A → B)

| Byte | 0 | 1 | 2 | 3–4 | 5–6 | 7 |
|---|---|---|---|---|---|---|
| Nội dung | `0xFD` | `0xAD` | `0x04` | Nhiệt độ ×10 (int16) | Độ ẩm ×10 (int16) | CRC (XOR byte 0–6) |

Báo nhận (B → A): `0xFD 0xDE`.

## Cách chạy

1. Tạo dự án Velxio với 2 board ESP32, đấu dây như bảng trên.
2. Dán `DeviceA.ino` vào board A, `DeviceB.ino` vào board B. Thêm thư viện **DHT sensor library** (Adafruit).
3. Trên ThingsBoard tạo device, lấy **Access Token**, điền vào `TB_TOKEN` trong `DeviceB.ino`.
   > ⚠️ Không commit token thật lên GitHub. Chỉ giữ `YOUR_ACCESS_TOKEN` trong repo.
4. Chạy mô phỏng, nhấn nút ở board A và xem Serial Monitor của cả hai board.

## Báo cáo

Xem [docs/TenSV_MaSV.docx](docs/TenSV_MaSV.docx).

## Tác giả

- Họ tên: [Điền]
- MSSV: [Điền]
