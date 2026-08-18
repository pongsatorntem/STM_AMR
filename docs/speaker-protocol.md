# Speaker RS485 Protocol Notes

สรุปจาก manual ใน [docs/manuals](manuals)

## Default Custom Protocol

Manual `AW-S24AF-AT.pdf` ระบุ default custom protocol สำหรับ RS232/RS485:

```text
Baud rate : 9600
Data bits : 8
Stop bit  : 1
Parity    : none
```

Master command มี 7 bytes:

```text
[0] 0x01    Initial code
[1] 0x51    Function code / play command
[2] Data1   Music folder number
[3] 0x00    Standby/reserved
[4] Data2   Volume
[5] XOR     XOR of bytes [0] through [4]
[6] 0x02    End code
```

## Folder Number

| Data1 | Meaning |
| --- | --- |
| `0x00` | Stop playing |
| `0x01` | Play folder AW001 |
| `0x02` | Play folder AW002 |
| `0x0F` | Play folder AW015 |
| `0x1F` | Play folder AW031 |
| `0xFF` | Play folder AW255 |

ดังนั้นจำนวนเสียง/folder ที่ protocol เรียกได้คือสูงสุด 255 folder (`AW001` ถึง `AW255`) ถ้ามีไฟล์เสียงอยู่จริงใน speaker storage

## Volume

`Data2` อยู่ในช่วง `0x00` ถึง `0x1C`

| Data2 | Volume |
| --- | --- |
| `0x00` | 0 / quiet |
| `0x01` | 1 |
| `0x0F` | 15 |
| `0x1C` | 28 / max |

## XOR

XOR คำนวณจาก 5 bytes แรกของ command:

```text
xor = 0x01 ^ 0x51 ^ folder ^ 0x00 ^ volume
```

ตัวอย่างจาก manual: play `AW002`, volume `28`

```text
01 51 02 00 1C 4E 02
```

ตรวจ XOR:

```text
0x01 ^ 0x51 ^ 0x02 ^ 0x00 ^ 0x1C = 0x4E
```

## Reply

Manual ระบุว่า alarm/speaker จะ reply กลับทุกครั้งที่ master ส่ง command:

```text
0x01 0x52 Data1 0x00 Data2 XOR 0x02
```

ใน sketch ทดสอบจะพิมพ์ bytes ที่ได้รับกลับมาใน Serial Monitor เพื่อช่วย debug

## Modbus RTU

Manual มี Modbus RTU ด้วย แต่ระบุว่าเป็น customer request only/ต้องตรง firmware ที่สั่งมา จึงเริ่มจาก custom protocol ก่อน เพราะ manual บอกว่าเป็น default

ค่าที่ manual ระบุสำหรับ Modbus:

```text
Slave address default: 0x01
Baud: 9600 8N1
Function code: 03/06/10 ใน manual 24A, และบางหน้าเน้น 06
Register 3: audio folder number 0-255
Register 4: volume 0-28
```

ถ้า custom protocol ไม่ตอบ ค่อยทดสอบ Modbus RTU เป็นทางเลือกถัดไป
