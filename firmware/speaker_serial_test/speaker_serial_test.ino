#include <Arduino.h>

const unsigned long PC_BAUD = 115200;
const unsigned long SPEAKER_BAUD = 9600;

const uint8_t START_PIN = 45;
const uint8_t UP_PIN = 46;
const uint8_t DOWN_PIN = 47;

const uint8_t DEFAULT_VOLUME = 28;
const uint8_t MAX_VOLUME = 28;
const uint8_t ACTIVE_LEVEL = HIGH;

const unsigned long DEBOUNCE_MS = 60;
const unsigned long BOTH_ACTIVE_FAULT_MS = 150;
const unsigned long STEP_GAP_MS = 150;
const unsigned long DEFAULT_SOUND_MS = 2500;

// Set to a real pin if your RS485 module needs DE/RE direction control.
// Keep -1 for auto-direction TTL-to-RS485 modules.
const int8_t RS485_DE_RE_PIN = -1;

const uint8_t UP_SEQUENCE[] = {1, 2, 3, 4, 5};
const uint8_t DOWN_SEQUENCE[] = {6, 7, 8, 9, 10};
const uint8_t UP_SEQUENCE_LEN = sizeof(UP_SEQUENCE) / sizeof(UP_SEQUENCE[0]);
const uint8_t DOWN_SEQUENCE_LEN = sizeof(DOWN_SEQUENCE) / sizeof(DOWN_SEQUENCE[0]);

// Adjust these values to match the real duration of each audio file.
const unsigned long UP_SOUND_MS[] = {
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS
};

const unsigned long DOWN_SOUND_MS[] = {
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS,
  DEFAULT_SOUND_MS
};

enum RemoteSelection {
  SEL_NONE,
  SEL_UP,
  SEL_DOWN,
  SEL_INVALID
};

enum ControllerState {
  STATE_WAIT_START,
  STATE_ARMED,
  STATE_PLAYING_SEQUENCE,
  STATE_FAULT
};

enum FaultCode {
  FAULT_NONE,
  FAULT_BOTH_UP_DOWN_ACTIVE
};

struct DebouncedInput {
  uint8_t pin;
  bool raw;
  bool stable;
  bool previousStable;
  unsigned long lastRawChangeMs;
  unsigned long stableSinceMs;
};

struct SoundSequence {
  const uint8_t *folders;
  const unsigned long *durationsMs;
  uint8_t length;
  uint8_t index;
  bool active;
  unsigned long nextActionMs;
};

DebouncedInput startInput;
DebouncedInput upInput;
DebouncedInput downInput;
SoundSequence currentSequence;

ControllerState controllerState = STATE_WAIT_START;
FaultCode faultCode = FAULT_NONE;
RemoteSelection lastAcceptedSelection = SEL_NONE;

uint8_t currentVolume = DEFAULT_VOLUME;
String inputLine;

unsigned long bothActiveSinceMs = 0;
unsigned long lastStatusMs = 0;

bool isActive(bool value) {
  return value == (ACTIVE_LEVEL == HIGH);
}

bool inputActive(const DebouncedInput &input) {
  return isActive(input.stable);
}

bool risingEdge(const DebouncedInput &input) {
  return inputActive(input) && !isActive(input.previousStable);
}

const char *selectionName(RemoteSelection selection) {
  switch (selection) {
    case SEL_NONE: return "NONE";
    case SEL_UP: return "UP";
    case SEL_DOWN: return "DOWN";
    case SEL_INVALID: return "INVALID";
    default: return "UNKNOWN";
  }
}

const char *stateName(ControllerState state) {
  switch (state) {
    case STATE_WAIT_START: return "WAIT_START";
    case STATE_ARMED: return "ARMED";
    case STATE_PLAYING_SEQUENCE: return "PLAYING_SEQUENCE";
    case STATE_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

const char *faultName(FaultCode fault) {
  switch (fault) {
    case FAULT_NONE: return "NONE";
    case FAULT_BOTH_UP_DOWN_ACTIVE: return "BOTH_UP_DOWN_ACTIVE";
    default: return "UNKNOWN";
  }
}

uint8_t speakerXor(uint8_t folder, uint8_t volume) {
  return 0x01 ^ 0x51 ^ folder ^ 0x00 ^ volume;
}

void setRs485Transmit(bool enabled) {
  if (RS485_DE_RE_PIN < 0) {
    return;
  }

  digitalWrite(RS485_DE_RE_PIN, enabled ? HIGH : LOW);
}

void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void beginInput(DebouncedInput &input, uint8_t pin) {
  input.pin = pin;
  pinMode(input.pin, INPUT);

  bool nowRaw = digitalRead(input.pin);
  unsigned long nowMs = millis();

  input.raw = nowRaw;
  input.stable = nowRaw;
  input.previousStable = nowRaw;
  input.lastRawChangeMs = nowMs;
  input.stableSinceMs = nowMs;
}

void updateInput(DebouncedInput &input, unsigned long nowMs) {
  input.previousStable = input.stable;

  bool newRaw = digitalRead(input.pin);

  if (newRaw != input.raw) {
    input.raw = newRaw;
    input.lastRawChangeMs = nowMs;
  }

  if ((nowMs - input.lastRawChangeMs) >= DEBOUNCE_MS && input.stable != input.raw) {
    input.stable = input.raw;
    input.stableSinceMs = nowMs;

    Serial.print(F("[INPUT] D"));
    Serial.print(input.pin);
    Serial.print(F(" = "));
    Serial.println(inputActive(input) ? F("ACTIVE") : F("INACTIVE"));
  }
}

RemoteSelection readRemoteSelection() {
  bool up = inputActive(upInput);
  bool down = inputActive(downInput);

  if (up && down) {
    return SEL_INVALID;
  }

  if (up) {
    return SEL_UP;
  }

  if (down) {
    return SEL_DOWN;
  }

  return SEL_NONE;
}

void sendSpeakerCommand(uint8_t folder, uint8_t volume) {
  uint8_t frame[7] = {
    0x01,
    0x51,
    folder,
    0x00,
    volume,
    speakerXor(folder, volume),
    0x02
  };

  Serial.print(F("TX:"));
  for (uint8_t i = 0; i < sizeof(frame); i++) {
    Serial.print(' ');
    printHexByte(frame[i]);
  }
  Serial.println();

  while (Serial3.available()) {
    Serial3.read();
  }

  setRs485Transmit(true);
  Serial3.write(frame, sizeof(frame));
  Serial3.flush();
  setRs485Transmit(false);
}

void printFolderName(uint8_t folder) {
  if (folder == 0) {
    Serial.print(F("STOP"));
    return;
  }

  Serial.print(F("AW"));
  if (folder < 100) {
    Serial.print('0');
  }
  if (folder < 10) {
    Serial.print('0');
  }
  Serial.print(folder);
}

void stopSequence() {
  currentSequence.active = false;
  currentSequence.index = 0;
}

void startSequence(const uint8_t *folders, const unsigned long *durationsMs, uint8_t length, const char *name) {
  currentSequence.folders = folders;
  currentSequence.durationsMs = durationsMs;
  currentSequence.length = length;
  currentSequence.index = 0;
  currentSequence.active = true;
  currentSequence.nextActionMs = millis();
  controllerState = STATE_PLAYING_SEQUENCE;

  Serial.print(F("[SEQ] Start "));
  Serial.println(name);
}

void updateSequence(unsigned long nowMs) {
  if (!currentSequence.active || nowMs < currentSequence.nextActionMs) {
    return;
  }

  if (currentSequence.index >= currentSequence.length) {
    sendSpeakerCommand(0, currentVolume);
    stopSequence();
    controllerState = STATE_ARMED;
    lastAcceptedSelection = readRemoteSelection();
    Serial.println(F("[SEQ] Done"));
    return;
  }

  uint8_t folder = currentSequence.folders[currentSequence.index];
  unsigned long durationMs = currentSequence.durationsMs[currentSequence.index];

  Serial.print(F("[SEQ] Play "));
  printFolderName(folder);
  Serial.println();

  sendSpeakerCommand(folder, currentVolume);

  currentSequence.index++;
  currentSequence.nextActionMs = nowMs + durationMs + STEP_GAP_MS;
}

void cancelSequenceForManualCommand() {
  if (currentSequence.active) {
    Serial.println(F("[SEQ] Cancelled by Serial Monitor"));
  }
  stopSequence();

  if (controllerState == STATE_PLAYING_SEQUENCE) {
    controllerState = STATE_ARMED;
    lastAcceptedSelection = readRemoteSelection();
  }
}

void enterFault(FaultCode fault) {
  stopSequence();
  sendSpeakerCommand(0, currentVolume);
  controllerState = STATE_FAULT;
  faultCode = fault;

  Serial.print(F("[FAULT] "));
  Serial.println(faultName(fault));
}

void handleStartLatchStop() {
  if (inputActive(startInput) || controllerState == STATE_WAIT_START) {
    return;
  }

  stopSequence();
  sendSpeakerCommand(0, currentVolume);

  controllerState = STATE_WAIT_START;
  faultCode = FAULT_NONE;
  lastAcceptedSelection = SEL_NONE;
  bothActiveSinceMs = 0;

  Serial.println(F("[STOP] START latch lost, back to WAIT_START"));
}

void runSafetyChecks(unsigned long nowMs) {
  RemoteSelection selection = readRemoteSelection();

  if (selection == SEL_INVALID) {
    if (bothActiveSinceMs == 0) {
      bothActiveSinceMs = nowMs;
    }
    if (nowMs - bothActiveSinceMs >= BOTH_ACTIVE_FAULT_MS && controllerState != STATE_FAULT) {
      enterFault(FAULT_BOTH_UP_DOWN_ACTIVE);
    }
  } else {
    bothActiveSinceMs = 0;
  }
}

void clearFaultIfSafe() {
  if (controllerState != STATE_FAULT) {
    return;
  }

  if (inputActive(startInput) && readRemoteSelection() != SEL_INVALID) {
    faultCode = FAULT_NONE;
    controllerState = STATE_ARMED;
    lastAcceptedSelection = readRemoteSelection();
    bothActiveSinceMs = 0;

    Serial.print(F("[FAULT CLEARED] baseline="));
    Serial.println(selectionName(lastAcceptedSelection));
  }
}

void handleRemoteState() {
  if (controllerState == STATE_FAULT) {
    clearFaultIfSafe();
    return;
  }

  if (controllerState == STATE_WAIT_START) {
    if (inputActive(startInput)) {
      lastAcceptedSelection = readRemoteSelection();
      controllerState = STATE_ARMED;
      Serial.print(F("[ARMED] baseline="));
      Serial.println(selectionName(lastAcceptedSelection));
    }
    return;
  }

  if (controllerState == STATE_PLAYING_SEQUENCE) {
    return;
  }

  RemoteSelection selection = readRemoteSelection();

  if (selection == SEL_NONE) {
    lastAcceptedSelection = SEL_NONE;
    return;
  }

  if (selection != lastAcceptedSelection) {
    if (selection == SEL_UP) {
      startSequence(UP_SEQUENCE, UP_SOUND_MS, UP_SEQUENCE_LEN, "UP 1-5");
    } else if (selection == SEL_DOWN) {
      startSequence(DOWN_SEQUENCE, DOWN_SOUND_MS, DOWN_SEQUENCE_LEN, "DOWN 6-10");
    }
    lastAcceptedSelection = selection;
  }
}

void readSpeakerReply() {
  static unsigned long lastByteMs = 0;
  static uint8_t buffer[16];
  static uint8_t length = 0;

  while (Serial3.available()) {
    if (length < sizeof(buffer)) {
      buffer[length++] = Serial3.read();
    } else {
      Serial3.read();
    }
    lastByteMs = millis();
  }

  if (length > 0 && millis() - lastByteMs > 30) {
    Serial.print(F("RX:"));
    for (uint8_t i = 0; i < length; i++) {
      Serial.print(' ');
      printHexByte(buffer[i]);
    }
    Serial.println();
    length = 0;
  }
}

bool isNumberLine(const String &line) {
  if (line.length() == 0) {
    return false;
  }

  for (uint8_t i = 0; i < line.length(); i++) {
    if (!isDigit(line.charAt(i))) {
      return false;
    }
  }

  return true;
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  1..255  play only one AW folder and cancel any sequence"));
  Serial.println(F("  0       stop playing"));
  Serial.println(F("  v0..v28 set volume"));
  Serial.println(F("  u       simulate remote UP sequence AW001..AW005"));
  Serial.println(F("  d       simulate remote DOWN sequence AW006..AW010"));
  Serial.println(F("  ?       help"));
  Serial.print(F("Current volume: "));
  Serial.println(currentVolume);
}

void handleLine(String line) {
  line.trim();
  line.toLowerCase();

  if (line.length() == 0) {
    return;
  }

  if (line == "?") {
    printHelp();
    return;
  }

  if (line == "u") {
    startSequence(UP_SEQUENCE, UP_SOUND_MS, UP_SEQUENCE_LEN, "SERIAL UP 1-5");
    return;
  }

  if (line == "d") {
    startSequence(DOWN_SEQUENCE, DOWN_SOUND_MS, DOWN_SEQUENCE_LEN, "SERIAL DOWN 6-10");
    return;
  }

  if (line.charAt(0) == 'v') {
    int requestedVolume = line.substring(1).toInt();
    if (requestedVolume < 0 || requestedVolume > MAX_VOLUME) {
      Serial.println(F("ERR volume must be 0..28"));
      return;
    }

    currentVolume = (uint8_t)requestedVolume;
    Serial.print(F("Volume set to "));
    Serial.println(currentVolume);
    return;
  }

  if (!isNumberLine(line)) {
    Serial.println(F("ERR unknown command. Type ? for help"));
    return;
  }

  int folder = line.toInt();
  if (folder < 0 || folder > 255) {
    Serial.println(F("ERR folder must be 0..255"));
    return;
  }

  cancelSequenceForManualCommand();
  sendSpeakerCommand((uint8_t)folder, currentVolume);

  Serial.print(F("Manual play: "));
  printFolderName((uint8_t)folder);
  Serial.println();
}

void readPcSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      handleLine(inputLine);
      inputLine = "";
      continue;
    }

    if (inputLine.length() < 32) {
      inputLine += ch;
    } else {
      inputLine = "";
      Serial.println(F("ERR input too long"));
    }
  }
}

void printStatus(unsigned long nowMs) {
  if (nowMs - lastStatusMs < 1000) {
    return;
  }
  lastStatusMs = nowMs;

  Serial.print(F("[HB] state="));
  Serial.print(stateName(controllerState));
  Serial.print(F(" fault="));
  Serial.print(faultName(faultCode));
  Serial.print(F(" start="));
  Serial.print(inputActive(startInput) ? 1 : 0);
  Serial.print(F(" up="));
  Serial.print(inputActive(upInput) ? 1 : 0);
  Serial.print(F(" down="));
  Serial.print(inputActive(downInput) ? 1 : 0);
  Serial.print(F(" sel="));
  Serial.println(selectionName(readRemoteSelection()));
}







void setup() {
  Serial.begin(PC_BAUD);
  Serial3.begin(SPEAKER_BAUD, SERIAL_8N1);

  beginInput(startInput, START_PIN);
  beginInput(upInput, UP_PIN);
  beginInput(downInput, DOWN_PIN);

  if (RS485_DE_RE_PIN >= 0) {
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    setRs485Transmit(false);
  }

  Serial.println(F("STM AMR remote + speaker controller"));
  Serial.println(F("D45=START, D46=UP, D47=DOWN"));
  Serial.println(F("Serial3 TX3=D14 RX3=D15, speaker 9600 8N1"));
  Serial.println(F("Press remote START before remote UP/DOWN can trigger sequences"));
  printHelp();
}



//loop
void loop() {
  unsigned long nowMs = millis();

  updateInput(startInput, nowMs);
  updateInput(upInput, nowMs);
  updateInput(downInput, nowMs);

  handleStartLatchStop();

  readPcSerial();
  readSpeakerReply();
  runSafetyChecks(nowMs);
  handleRemoteState();
  updateSequence(nowMs);
  printStatus(nowMs);
}