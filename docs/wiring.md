# Wiring Notes

## Remote Receiver to Arduino Mega

ใช้ Arduino Mega 2560 อ่าน relay output จากรีโมทเครนเป็น digital input แบบ external pull-down

```text
Remote relay COM / Red  -> Arduino 5V
START / Orange          -> R 1k -> D45
UP output               -> R 1k -> D46
DOWN output             -> R 1k -> D47

D45 -> R 10k -> Arduino GND
D46 -> R 10k -> Arduino GND
D47 -> R 10k -> Arduino GND
```

## Current Pin Map

| Signal | Arduino pin | Logic |
| --- | --- | --- |
| START / ENABLE | D45 | HIGH = armed/available |
| UP / A to B | D46 | HIGH = selected/latch active |
| DOWN / B to A | D47 | HIGH = selected/latch active |

Notes:

- ยึด output จริงจากการวัดด้วย multimeter เป็นหลัก ถ้าสีสายไม่ตรงกับ label ให้แก้ตามผลวัด
- Input ทั้งหมดต้องสูงสุดประมาณ 5V เท่านั้น
- ถ้า UP และ DOWN active พร้อมกัน ให้ software ถือเป็น fault/invalid
- START เป็นเงื่อนไข armed: เปิดเครื่องมาเจอ UP/DOWN ค้างอยู่ก่อน ไม่ควรสั่ง AMR ทันที

## Arduino Mega to TTL-RS485 Module

Arduino Mega Serial3:

```text
Arduino TX3 / D14 -> RS485 module DI/RXD/TX input side
Arduino RX3 / D15 -> RS485 module RO/TXD/RX output side
Arduino GND       -> RS485 module GND
```

RS485 bus:

```text
RS485 module A -> Speaker A+
RS485 module B -> Speaker B-
```

ถ้าสื่อสารไม่ได้:

- ลองสลับ A/B
- ตรวจว่า module เป็น auto-direction หรือไม่
- ถ้าไม่ใช่ auto-direction ต้องต่อ DE/RE เข้าขา digital output ของ Arduino แล้วเปิดส่งก่อน `Serial3.write()`

## Speaker Power

```text
Speaker +24V -> 24V supply +
Speaker GND  -> 24V supply 0V
```

อย่าเอา 24V เข้า pin logic ของ Arduino
