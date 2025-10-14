#include <Arduino.h>
#include <HX711.h>
#include <LiquidCrystal_I2C.h>
#include <NimBLEDevice.h>

// Replace with your printer MAC (or leave blank to connect to first found)
static const char *PRINTER_MAC = "58:8C:81:72:AB:0A";

NimBLERemoteService *pPrinterService = nullptr;
NimBLEClient *pClient = nullptr;
// globals
NimBLERemoteCharacteristic *pWriteChar = nullptr;
NimBLERemoteCharacteristic *pNotifyChar = nullptr;
volatile bool ackReceived = false;
std::vector<uint8_t> lastAck; // stores last notification bytes
int currentFrame = 0;
bool printingInProgress = false;

const uint8_t frame1[] = { 0x66, 0x35, 0x00, 0x1b, 0x2f, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x33, 0x01, 0x55, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x03, 0x00, 0xff, 0x3f, 0xff, 0x28, 0x00, 0x00, 0x35, 0x2e, 0x00, 0x00, 0x38, 0xf3, 0x08, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x89, 0x2c, 0x00, 0x11, 0x00, 0x00, 0x63 };

const uint8_t frame2[] = { 0x66, 0x2f, 0x00, 0x1b, 0x2f, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x33, 0x01, 0x55, 0x00, 0x02, 0x00, 0x02, 0x00, 0x38, 0x00, 0x00, 0x00, 0x80, 0x00, 0x02, 0x03, 0x00, 0x00, 0x38, 0x00, 0xc3, 0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xc5, 0x2c, 0x00, 0x11, 0x00, 0x00, 0xb1 };

const uint8_t frame3[] = { 0x66, 0x2f, 0x00, 0x1b, 0x2f, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x33, 0x01, 0x55, 0x00, 0x01, 0x00, 0x02, 0x00, 0x38, 0x00, 0x00, 0x00, 0x80, 0x00, 0x02, 0x03, 0x00, 0x00, 0x38, 0x00, 0xc3, 0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xc5, 0x2c, 0x00, 0x11, 0x00, 0x00, 0xb2 };

const uint8_t frame4[] = { 0x66, 0x44, 0x00, 0x1b, 0x2f, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x33, 0x01, 0x34, 0x00, 0x00, 0x00, 0x02, 0x00, 0x38, 0x00, 0x00, 0x00, 0x80, 0x00, 0x02, 0x03, 0x00, 0x00, 0x38, 0x00, 0xc3, 0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00, 0x08, 0x2f, 0x00, 0xff, 0x3f, 0xff, 0x28, 0x00, 0x00, 0x35, 0x2c, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0xaa };

// Notification callback
void notifyCallback(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t len,
                    bool isNotify) {
  // copy ack
  lastAck.assign(data, data + len);
  ackReceived = true;

  Serial.print("Notification [");
  Serial.print(chr->getUUID().toString().c_str());
  Serial.print("] : ");
  for (size_t i = 0; i < len; ++i)
    Serial.printf("%02X ", data[i]);
  Serial.println();

	// When printing, use each notification as a signal to send the next frame
  if (printingInProgress && chr == pNotifyChar) {
    delay(50);  // slight delay to allow internal buffer clear
    currentFrame++;
    const uint8_t *nextFrame = nullptr;
    size_t lenNext = 0;

    switch (currentFrame) {
      case 1: nextFrame = frame2; lenNext = sizeof(frame2); break;
      case 2: nextFrame = frame3; lenNext = sizeof(frame3); break;
      case 3: nextFrame = frame4; lenNext = sizeof(frame4); break;
      default:
        Serial.println("All frames sent. Printing should complete.");
        printingInProgress = false;
        return;
    }

    Serial.printf("Sending frame %d...\n", currentFrame + 1);
    pWriteChar->writeValue(nextFrame, lenNext, false);
  }
}

// Explicitly write CCC descriptor (0x2902) with Write Request (response
// expected)
bool enableNotificationsExplicit(NimBLERemoteCharacteristic *notifyChr,
                                 uint32_t timeoutMs = 2000) {
  if (!notifyChr)
    return false;

  NimBLERemoteDescriptor *ccc =
      notifyChr->getDescriptor(NimBLEUUID((uint16_t)0x2902));
  if (!ccc) {
    Serial.println("CCC descriptor (0x2902) not found.");
    return false;
  }

  // value 0x0001 -> enable notifications (little endian)
  uint8_t val[2] = {0x01, 0x00};

  Serial.println(
      "Writing CCC descriptor to enable notifications (Write Request) ...");
  bool ok = ccc->writeValue(val, 2, true); // true => request/with response

  if (!ok) {
    Serial.println("Descriptor write failed (sync call returned false).");
    return false;
  }

  // Optionally wait briefly for the server response - NimBLE handles write
  // response sync, but we could check subscription by subscribing via NimBLE
  // API too:
  bool subOk = notifyChr->subscribe(
      true, notifyCallback); // subscribe wrapper sets CCC and callback
  if (!subOk) {
    Serial.println(
        "subscribe() failed (but descriptor write may have succeeded).");
  } else {
    Serial.println("subscribe() succeeded.");
  }
  return true;
}

// send a single chunk and wait for an ACK notification (optional)
bool sendChunkWaitAck(const uint8_t *data, size_t len,
                      uint32_t timeoutMs = 1000) {
  if (!pWriteChar) {
    Serial.println("Write characteristic missing!");
    return false;
  }
  // clear ack flag
  ackReceived = false;
  lastAck.clear();

  // writeWithoutResponse (false -> no response)
  bool wrote = pWriteChar->writeValue((uint8_t *)data, len, false);
  if (!wrote) {
    Serial.println("writeValue (no-response) returned false.");
    return false;
  }
  // Wait for ackReceived to become true (from notifyCallback)
  uint32_t start = millis();
  while (!ackReceived && (millis() - start) < timeoutMs) {
    delay(5);
  }
  if (!ackReceived) {
    Serial.println("ACK timeout");
    return false;
  }
  // Optionally inspect lastAck bytes for success code
  Serial.print("ACK bytes: ");
  for (auto &b : lastAck)
    Serial.printf("%02X ", b);
  Serial.println();
  return true;
}

// send big buffer chunked according to MTU - 3 (safe)
bool sendLargeBufferWithAck(const uint8_t *buf, size_t buflen) {
  // compute chunk size from MTU; NimBLE client has getMTU()
  uint16_t mtu = pClient->getMTU(); // negotiated MTU
  size_t chunk = (mtu > 3) ? (mtu - 3) : 20;
  Serial.printf("Using chunk size: %u (MTU %u)\n", (unsigned)chunk,
                (unsigned)mtu);

  for (size_t offset = 0; offset < buflen; offset += chunk) {
    size_t thisLen = std::min(chunk, buflen - offset);
    if (!sendChunkWaitAck(buf + offset, thisLen, 1000)) {
      Serial.printf("Chunk at offset %u failed\n", (unsigned)offset);
      return false;
    }
    delay(5); // small spacing — tweak if needed
  }
  return true;
}

// Scan callback
class PrinterAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    Serial.print("Found device: ");
    Serial.println(advertisedDevice->toString().c_str());

    if (PRINTER_MAC[0] != '\0' &&
        advertisedDevice->getAddress().toString() == std::string(PRINTER_MAC)) {
      Serial.println("Found target printer, stopping scan...");
      NimBLEDevice::getScan()->stop();
    }
  }
};

void beginBLESniffer() {
  Serial.println("Starting BLE sniffer...");
  NimBLEDevice::init("ESP32-BLE-Sniffer");

  // Scan
  NimBLEScan *pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new PrinterAdvertisedDeviceCallbacks());
  pScan->setInterval(45);
  pScan->setWindow(15);
  pScan->setActiveScan(true);
  pScan->start(5, false);

  // Connect
  NimBLEAddress addr(std::string(PRINTER_MAC), BLE_ADDR_PUBLIC);
  pClient = NimBLEDevice::createClient();

  Serial.print("Connecting to printer: ");
  Serial.println(addr.toString().c_str());

  if (!pClient->connect(addr)) {
    Serial.println("Failed to connect.");
    return;
  }
  Serial.println("Connected!");

  // Negotiate MTU
  uint16_t mtu = pClient->getMTU();
  Serial.printf("Negotiated MTU: %u\n", mtu);

  // Find printer service (UUID 0xABF0)
  pPrinterService = pClient->getService("ABF0");
  if (!pPrinterService) {
    Serial.println("Printer service (0xABF0) not found!");
    return;
  }
  Serial.println("Printer service found.");

  // Get write characteristic (ABF1)
  pWriteChar = pPrinterService->getCharacteristic("ABF1");
  if (!pWriteChar) {
    Serial.println("Write characteristic ABF1 not found!");
    return;
  }
  Serial.println("Write characteristic ABF1 found.");

  // Get notify characteristic (ABF2)
  pNotifyChar = pPrinterService->getCharacteristic("ABF2");
  if (!pNotifyChar) {
    Serial.println("Notify characteristic ABF2 not found!");
    return;
  }
  Serial.println("Notify characteristic ABF2 found.");

  // Subscribe to ABF2 notifications
  if (pNotifyChar->canNotify()) {
    if (pNotifyChar->subscribe(true, notifyCallback)) {
      Serial.println("Subscribed to ABF2 notifications.");
    } else {
      Serial.println("Subscribe to ABF2 failed.");
    }
  }
}


void startPrintJob() {
  if (!pWriteChar) {
    Serial.println("No write characteristic available!");
    return;
  }
  currentFrame = 0;
  printingInProgress = true;
  Serial.println("Starting print job (frame 1)...");
  pWriteChar->writeValue(frame1, sizeof(frame1), false);
}

// Defines pin numbers
const int trigPin = 12; // Connects to the Trig pin of the HC-SR04P
const int echoPin = 13; // Connects to the Echo pin of the HC-SR04P

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 18;
const int LOADCELL_SCK_PIN = 19;

// Defines variables
long duration;  // Variable for the duration of sound wave travel
int distanceMM; // Variable for the distance measurement in centimeters

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

HX711 scale;
const float CALIBRATION_FACTOR = 1100;
int reading;
int lastReading;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  lcd.init();
  lcd.backlight();

  beginBLESniffer();
	Serial.println("Attempting to get print characteristic (0x002a)...");
	BLERemoteCharacteristic* printChar = pPrinterService->getCharacteristic(BLEUUID((uint16_t)0xabf1));
	if (printChar && printChar->canWriteNoResponse()) {
		Serial.println("Found 0x002a characteristic, sending replay frames...");
		startPrintJob();
	} else {
		Serial.println("Characteristic 0x002a not found or not writable.");
	}
  // scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  // scale.set_scale(CALIBRATION_FACTOR); // this value is obtained by calibrating
                                       // the scale with known weights
  // scale.tare();                        // reset the scale to 0
}

void loop() {

  // Clears the trigPin by setting it LOW for a short period
  // digitalWrite(trigPin, LOW);
  // delayMicroseconds(2);
  //
  // // Sets the trigPin HIGH for 10 microseconds to send a pulse
  // digitalWrite(trigPin, HIGH);
  // delayMicroseconds(10);
  // digitalWrite(trigPin, LOW);
  //
  // // Reads the echoPin, returns the sound wave travel time in microseconds
  // duration = pulseIn(echoPin, HIGH);
  //
  // // Calculates the distance in centimeters
  // // Speed of sound in air is approximately 343 meters/second or 0.343
  // // mm/microsecond The pulse travels to the object and back, so divide by 2
  // distanceMM = duration * 0.343 / 2;
  //
  // // Displays the distance on the Serial Monitor
  // lcd.setCursor(0, 0);
  // lcd.print("Distance:       ");
  // lcd.setCursor(10, 0);
  // lcd.print(distanceMM);
  // lcd.print(" mm");

  // Example payload from Wireshark (QR command)
  // uint8_t qr1[] = {0x66, 0x06, 0x00, 0x10, 0x00, 0x84};
  //
  // if (pWriteChar) {
  //   sendChunkWaitAck(qr1, sizeof(qr1));
  // }

  // if (scale.wait_ready_timeout(200)) {
  //   reading = round(scale.get_units());
  //   Serial.print("Weight: ");
  //   Serial.println(reading);
  //   if (reading != lastReading) {
  //     lcd.setCursor(0, 1);
  //     lcd.print("Weight:         ");
  //     lcd.setCursor(8, 1);
  //     lcd.print(reading);
  //     lcd.print(" g");
  //   }
  //   lastReading = reading;
  // } else {
  //   Serial.println("HX711 not found.");
  // }
  //
  delay(1000); // Small delay to allow for stable readings
}
