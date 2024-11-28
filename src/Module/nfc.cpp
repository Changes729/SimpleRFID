/** See a brief introduction (right-hand button) */
#include "nfc.h"
/* Private include -----------------------------------------------------------*/
#include "config.h"

#include <PN5180.h>
#include <PN5180ISO15693.h>

/* Private namespace ---------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define BLOCK_SIZE 16
#define LONG_TLV_SIZE 4
#define SHORT_TLV_SIZE 2

/* Private typedef -----------------------------------------------------------*/
/* Private template ----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static PN5180ISO15693 nfc(PN5180_NSS, PN5180_BUSY, PN5180_RST);
static uint8_t _uid[8];
static NdefMessage _current_ndef_msg;

static uint8_t standardpassword[] = {0x0F, 0x0F, 0x0F, 0x0F};
static uint8_t password[] = {0x12, 0x34, 0x56, 0x78};

/* Private class -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void _showIRQStatus(uint32_t irqStatus);
bool decodeTlv(byte *data, int &messageLength, int &messageStartIndex);

/* Private function ----------------------------------------------------------*/
/* Private class function ----------------------------------------------------*/
/**
 * @brief  ...
 * @param  None
 * @retval None
 */
bool nfc_init() {
  bool success = true;

  SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI);
  nfc.begin();
  nfc.reset();

  uint8_t productVersion[2]{};
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
#if 0
  Serial.print(F("Product version="));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);
#endif
  if (0x00 == productVersion[1]) {
    success = false;
  } else {
    nfc.setupRF();
#if 0
    uint32_t byte;
    nfc.readRegister(0x22, &byte);
    Serial.printf("0x22: 0x%X\n", byte);
    nfc.readRegister(0x27, &byte);
    Serial.printf("0x27: 0x%X\n", byte);
    nfc.readEEprom(0x59, (uint8_t *)&byte, 1);
    Serial.printf("EEP 0x59: 0x%X\n", byte);
    // nfc.writeRegisterWithOrMask(0x22, 0x03); // RX_GAIN
    // nfc.writeRegisterWithOrMask(0x27, 0x08);
    // nfc.writeRegisterWithOrMask(0x22, 0x03);
    // nfc.writeRegisterWithOrMask(0x27, 0x08);
    // nfc.writeRegisterWithAndMask(0x22, ~0x03);
    // nfc.writeRegisterWithAndMask(0x27, ~0x08);
    // nfc.readRegister(0x22, &byte);
#endif
  }
  return success;
}

void update_nfc() {
  do {
    /** Inventory --------------------------------------------------*/
    ISO15693ErrorCode rc = nfc.getInventory(_uid);
    if (ISO15693_EC_OK != rc) {
      log_e("Error in getInventory: ");

      uint32_t irqStatus = nfc.getIRQStatus();
#if 0
    _showIRQStatus(irqStatus);
#endif
      if (0 == (RX_SOF_DET_IRQ_STAT & irqStatus)) { // no card detected
        log_d("*** No card detected!");
      }

      nfc.reset();
      nfc.setupRF();
      // nfc.writeRegisterWithOrMask(0x22, 0x03);
      // nfc.writeRegisterWithOrMask(0x27, 0x08);
      break;
    }

    /** get sys info -----------------------------------------------*/
    uint8_t blockSize, numBlocks;
    rc = nfc.getSystemInfo(_uid, &blockSize, &numBlocks);
    if (ISO15693_EC_OK != rc) {
      log_e("[NFC] Error in getSystemInfo: 0x%X", rc);
      break;
    }

    /** read data --------------------------------------------------*/
    uint8_t readBuffer[numBlocks * blockSize]{};
    rc = nfc.readMultipleBlock(_uid, 0, numBlocks, readBuffer, blockSize);
    if (ISO15693_EC_OK == rc) {
#if 0
      Serial.print(F("Read blocks #0-"));
      Serial.print(numBlocks - 1);
      Serial.println(":");
      for (int i = 0; i < numBlocks; i++) {
        if (i < 10)
          Serial.print(" ");
        Serial.print("#");
        Serial.print(i);
        Serial.print(": ");
        uint16_t startAddr = i * blockSize;
        for (int j = 0; j < blockSize; j++) {
          if (readBuffer[startAddr + j] < 16)
            Serial.print("0");
          Serial.print(readBuffer[startAddr + j], HEX);
          Serial.print(" ");
        }
        Serial.print(" ");
        for (int j = 0; j < blockSize; j++) {
          if (isprint(readBuffer[startAddr + j])) {
            Serial.print((char)readBuffer[startAddr + j]);
          } else
            Serial.print(".");
        }
        Serial.println();
      }
#endif
      int messageStartIndex = 0;
      int messageLength = 0;
      if (decodeTlv(readBuffer + 4, messageLength, messageStartIndex)) {
        _current_ndef_msg =
            NdefMessage(readBuffer + 4 + messageStartIndex, messageLength);
      } else {
        _current_ndef_msg = NdefMessage();
      }
#if 0

      Serial.println("print NdefMessage");
      auto &message = _current_ndef_msg;
      Serial.print("\nThis NFC Tag contains an NDEF Message with ");
      Serial.print(message.getRecordCount());
      Serial.print(" NDEF Record");
      if (message.getRecordCount() != 1) {
        Serial.print("s");
      }
      Serial.println(".");

      // cycle through the records, printing some info from each
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++) {
        Serial.print("\nNDEF Record ");
        Serial.println(i + 1);
        NdefRecord record = message.getRecord(i);
        // NdefRecord record = message[i]; // alternate syntax

        Serial.print("  TNF: ");
        Serial.println(record.getTnf());
        Serial.print("  Type: ");
        Serial.println(record.getType()); // will be "" for TNF_EMPTY

        // The TNF and Type should be used to determine how your application
        // processes the payload There's no generic processing for the
        // payload, it's returned as a byte[]
        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);

        // Print the Hex and Printable Characters
        Serial.print("  Payload (HEX): ");
        PrintHexChar(payload, payloadLength);

        // Force the data into a String (might work depending on the content)
        // Real code should use smarter processing
        String payloadAsString = "";
        for (int c = 0; c < payloadLength; c++) {
          payloadAsString += (char)payload[c];
        }
        Serial.print("  Payload (as String): ");
        Serial.println(payloadAsString);

        // id is probably blank and will return ""
        String uid = record.getId();
        if (uid != "") {
          Serial.print("  ID: ");
          Serial.println(uid);
        }
      }
#endif
    } else {
      log_e("[NFC] read multiple block failed: 0x%X", rc);
      break;
    }
  } while (0);
}

int getNdefStartIndex(byte *data) {

  for (int i = 0; i < BLOCK_SIZE; i++) {
    if (data[i] == 0x0) {
      // do nothing, skip
    } else if (data[i] == 0x3) {
      return i;
    } else {
#ifdef NDEF_USE_SERIAL
      log_e("Unknown TLV 0x%X", data[i]);
#endif
      return -2;
    }
  }

  return -1;
}

bool decodeTlv(byte *data, int &messageLength, int &messageStartIndex) {
  int i = getNdefStartIndex(data);

  if (i < 0 || data[i] != 0x3) {
#ifdef NDEF_USE_SERIAL
    log_e("Error. Can't decode message length.");
#endif
    return false;
  } else {
    if (data[i + 1] == 0xFF) {
      messageLength = ((0xFF & data[i + 2]) << 8) | (0xFF & data[i + 3]);
      messageStartIndex = i + LONG_TLV_SIZE;
    } else {
      messageLength = data[i + 1];
      messageStartIndex = i + SHORT_TLV_SIZE;
    }
  }

  return true;
}

bool has_uid(uint8_t *buffer, size_t size) {
  bool has_uid_ = false;
  for (auto byte : _uid) {
    if (byte != 0x00) {
      has_uid_ = true;
      break;
    }
  }

  if (has_uid_ && buffer != nullptr) {
    memcpy(buffer, _uid, min(size, sizeof(_uid)));
  }

  return has_uid_;
}

bool has_uid(String &out, const char seq) {
  bool has_uid_ = has_uid();
  char buff[3]{};
  if (has_uid_) {
    out.clear();
    for (int i = 0; i < sizeof(_uid); ++i) {
      /** big-endian */
      snprintf(buff, sizeof(buff), "%02X", _uid[i]);
      out += buff;
      if (isalpha(seq) && i != 7) {
        out += seq;
      }
    }
  }

  return has_uid_;
}

const NdefMessage &get_ndef_message() { return _current_ndef_msg; }

static void _uid_to_string(String &str) { str.clear(); }

static void _showIRQStatus(uint32_t irqStatus) {
  Serial.print(F("IRQ-Status 0x"));
  Serial.print(irqStatus, HEX);
  Serial.print(": [ ");
  if (irqStatus & (1 << 0))
    Serial.print(F("RQ "));
  if (irqStatus & (1 << 1))
    Serial.print(F("TX "));
  if (irqStatus & (1 << 2))
    Serial.print(F("IDLE "));
  if (irqStatus & (1 << 3))
    Serial.print(F("MODE_DETECTED "));
  if (irqStatus & (1 << 4))
    Serial.print(F("CARD_ACTIVATED "));
  if (irqStatus & (1 << 5))
    Serial.print(F("STATE_CHANGE "));
  if (irqStatus & (1 << 6))
    Serial.print(F("RFOFF_DET "));
  if (irqStatus & (1 << 7))
    Serial.print(F("RFON_DET "));
  if (irqStatus & (1 << 8))
    Serial.print(F("TX_RFOFF "));
  if (irqStatus & (1 << 9))
    Serial.print(F("TX_RFON "));
  if (irqStatus & (1 << 10))
    Serial.print(F("RF_ACTIVE_ERROR "));
  if (irqStatus & (1 << 11))
    Serial.print(F("TIMER0 "));
  if (irqStatus & (1 << 12))
    Serial.print(F("TIMER1 "));
  if (irqStatus & (1 << 13))
    Serial.print(F("TIMER2 "));
  if (irqStatus & (1 << 14))
    Serial.print(F("RX_SOF_DET "));
  if (irqStatus & (1 << 15))
    Serial.print(F("RX_SC_DET "));
  if (irqStatus & (1 << 16))
    Serial.print(F("TEMPSENS_ERROR "));
  if (irqStatus & (1 << 17))
    Serial.print(F("GENERAL_ERROR "));
  if (irqStatus & (1 << 18))
    Serial.print(F("HV_ERROR "));
  if (irqStatus & (1 << 19))
    Serial.print(F("LPCD "));
  Serial.println("]");
}
