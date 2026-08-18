# STM AMR Crane Remote + Voice Speaker

โปรเจกต์นี้ใช้ Arduino Mega 2560 เป็นตัวกลางระหว่างรีโมทเครน F21-2S, AMR และลำโพง Voice Announcer ผ่าน RS485

## เป้าหมาย

- รับสัญญาณจากรีโมทเครนแบบ relay latch
- ต้องกด START/ENABLE ก่อน จึงจะยอมรับคำสั่ง UP/DOWN
- UP ใช้เป็นคำสั่ง AMR จาก A ไป B
- DOWN ใช้เป็นคำสั่ง AMR จาก B ไป A
- สั่งลำโพงผ่าน TTL to RS485 ด้วย Serial3 ของ Arduino Mega
- sketch ปัจจุบันรวม remote input และ speaker control ในไฟล์เดียว
- พิมพ์เลขใน Serial Monitor เพื่อเล่นเสียงเดี่ยว หรือกด remote UP/DOWN เพื่อเล่นเสียงเป็นชุด

## โครงสร้างโปรเจกต์

```text
STM_AMR/
  README.md
  docs/
    BOM_STM_AMR.xlsx
    manuals/
      AW-S24AF-AT.pdf
      Voice Announcer User Manual (24A Series).pdf
    speaker-protocol.md
    wiring.md
  firmware/
    speaker_serial_test/
      speaker_serial_test.ino
```

## Hardware Mapping ปัจจุบัน

| Function | Wire | Arduino Mega pin | Note |
| --- | --- | --- | --- |
| START / ENABLE / heartbeat from remote | Orange | D45 | ต้อง active ก่อนรับ UP/DOWN |
| UP / A to B | Yellow/actual UP output | D46 | ยืนยันสีจากการวัดจริงอีกครั้ง |
| DOWN / B to A | Green/actual DOWN output | D47 | ยืนยันสีจากการวัดจริงอีกครั้ง |
| RS485 UART RX | TTL-RS485 module TX/RO | RX3 / D15 | Arduino Serial3 |
| RS485 UART TX | TTL-RS485 module RX/DI | TX3 / D14 | Arduino Serial3 |
| Speaker RS485 A | Module A | Speaker A+ | ถ้าสื่อสารไม่ได้ ลองสลับ A/B |
| Speaker RS485 B | Module B | Speaker B- | ถ้าสื่อสารไม่ได้ ลองสลับ A/B |

รายละเอียดวงจรอยู่ที่ [docs/wiring.md](docs/wiring.md)

## BOM

- [docs/BOM_STM_AMR.xlsx](docs/BOM_STM_AMR.xlsx) - Excel BOM แบบ minimal เฉพาะอุปกรณ์ที่ใช้จริงตอนนี้, wiring summary, และ commissioning checklist

## Diagrams

- [docs/diagrams/remote-speaker-flowchart.svg](docs/diagrams/remote-speaker-flowchart.svg) - flowchart การทำงานของ firmware ตั้งแต่ START, UP/DOWN, fault, sequence, และ Serial Monitor
- [docs/diagrams/remote-speaker-block-diagram.svg](docs/diagrams/remote-speaker-block-diagram.svg) - block diagram ภาพรวม remote, Arduino Mega, TTL-to-RS485, และ speaker

## Speaker Protocol ที่อ่านจาก Manual

manual ระบุ default custom serial protocol สำหรับ RS232/RS485:

- Baud rate: `9600`
- Data bits: `8`
- Stop bit: `1`
- Parity: none
- Command length: 7 bytes
- Play folders: `AW001` ถึง `AW255`
- `0x00` = stop playing
- Volume: `0` ถึง `28`

รูปแบบ command:

```text
0x01 0x51 folder 0x00 volume xor 0x02
```

ตัวอย่างจาก manual: เล่น `AW002` ที่ volume `28`

```text
01 51 02 00 1C 4E 02
```

รายละเอียดเพิ่มอยู่ที่ [docs/speaker-protocol.md](docs/speaker-protocol.md)

## Remote + Speaker Controller

เปิด sketch นี้ใน Arduino IDE หรือ VS Code Arduino extension:

[firmware/speaker_serial_test/speaker_serial_test.ino](firmware/speaker_serial_test/speaker_serial_test.ino)

ตั้งค่า Serial Monitor:

- Baud: `115200`
- Line ending: `Newline` หรือ `Both NL & CR`

พฤติกรรมจากรีโมท:

```text
กด START ก่อน 1 ครั้ง     ระบบเข้าสู่ ARMED
กด UP หลัง ARMED          เล่น AW001 -> AW002 -> AW003 -> AW004 -> AW005
กด DOWN หลัง ARMED        เล่น AW006 -> AW007 -> AW008 -> AW009 -> AW010
UP และ DOWN พร้อมกัน      เข้า fault
กด STOP / D45 ดับ          หยุด sequence, ส่ง stop speaker, กลับ WAIT_START
```

คำสั่งจาก Serial Monitor:

```text
1       เล่น AW001
2       เล่น AW002
50      เล่น AW050
0       stop playing
v20     ตั้ง volume เป็น 20
v28     ตั้ง volume สูงสุดตาม manual
u       จำลอง sequence UP: AW001 ถึง AW005
d       จำลอง sequence DOWN: AW006 ถึง AW010
?       แสดง help
```

หมายเหตุ: manual ที่มีระบุ reply ว่า speaker รับคำสั่ง แต่ยังไม่เจอ packet ที่บอกว่าเสียงเล่นจบจริง ดังนั้นใน sketch ใช้เวลาหน่วงต่อเสียงจาก `UP_SOUND_MS[]` และ `DOWN_SOUND_MS[]` แทน ถ้าไฟล์เสียงจริงยาว/สั้นไม่เท่ากัน ให้ปรับ array นี้ใน sketch

หลัง sequence จากรีโมทเล่นเสียงสุดท้ายครบเวลาแล้ว sketch จะส่ง `0` เพื่อ stop speaker กันเสียงสุดท้ายวนค้าง เพราะ manual ระบุ default playback เป็น loop in folder

## หมายเหตุสำคัญ

- Arduino Mega ใช้ Serial3 ที่ pin `TX3=D14`, `RX3=D15`
- ถ้า TTL-to-RS485 module ไม่ใช่แบบ auto-direction ต้องเพิ่มขา `DE/RE` และแก้ sketch ให้ control ทิศทางส่ง/รับ
- ห้ามให้ 24V จากรีโมทเข้าขา D45/D46/D47 ของ Arduino
- สัญญาณ remote input แนะนำให้มี `R series 1k` ก่อนเข้าขา Arduino และ `pull-down 10k` ลง GND Arduino ทุกช่อง
- `.venv/` เกิดจากการติดตั้ง Python helper เพื่ออ่านข้อความ PDF ในรอบนี้ ไม่เกี่ยวกับ firmware โดยตรง
