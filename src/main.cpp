/** See a brief introduction (right-hand button) */
/* Private include -----------------------------------------------------------*/
#include "config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#ifdef ESP32
#include <AsyncTCP.h>

#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include "Module/CU500.h"
#include "Module/nfc.h"
#include "Web/network_settings.h"
#include "config.h"
#include "network_manager.h"
#include "system_operation.h"
#include "web_server.h"

/* Private namespace ---------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
typedef enum {
  TYPE_PN5180,
  TYPE_CU500,
} rfid_device_t;

/* Private template ----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static unsigned long _disconnect_ts = 0;
static unsigned long _encoder_ts = 0;
static int _curr_angle = 0;
static uint16_t _last_touched;
constexpr unsigned long _SEND_TS = 200; /* 200 ms */

/** Debug part -----------------------------------------------------*/
static bool _debug_enable = false;
static bool _sys_enable = true;
static IPAddress _debug_ip{0, 0, 0, 0};
static String _debug_ip_str = _debug_ip.toString();
static uint32_t _debug_port = 3333;
static String _latest_rfid_index{};
static unsigned _latest_ts;

/* Private class -------------------------------------------------------------*/
static rfid_device_t rfid_device;
static CU500::CU500_Reader rfid_reader{Serial1};
static unsigned rfid_record{};

/* Private function prototypes -----------------------------------------------*/
static void check_network_state();
static void send_box_msg(const String &str);
static void send_msg(const String &str, const char *server_ip, int server_port);

/* Private function ----------------------------------------------------------*/
/* Private class function ----------------------------------------------------*/
void setup() {
  /** Params init */
  log_i("init start");
  WebServer::create();
  NetworkManager::create();

  /** Serial init */
  log_i("init serial");
  Serial.begin(115200);

  /** LittleFS init */
  log_i("init littlefs");
  if (!LittleFS.begin(false, "")) {
    log_d("LittleFS mount failed");
    return;
  }

  init_rs_cfg();
  SYSTEM::init();

  log_i("init network");
#if 0
  NetworkManager::instance().config_dhcpcd("/etc/dhcpcd.conf");
  NetworkManager::instance().config_wpa_supplicant("/etc/wpa_supplicant.conf");
  NetworkManager::instance().begin();

  // this resets all the neopixels to an off state

  WebServer::instance().on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello, world");
  });

  NetworkSettings::init();
  WebServer::instance().begin();
#endif
  log_i("init nfc");
  {
    if (nfc_init()) {
      rfid_device = TYPE_PN5180;
      log_d("PN5180 MODE\n");
    } else {
      Serial1.begin(19200, SERIAL_8N1, 34, 33);
      rfid_reader.init();
      rfid_reader.set_cmd_callback([](CU500::cmd_t cmd, uint8_t *data, size_t,
                                      void *u_data) { rfid_record = 0; },
                                   nullptr);
      rfid_record = 0;
      rfid_device = TYPE_CU500;
      log_d("CU500 MODE\n");
    }
  }
  log_i("init end");
}

void loop() {
  SYSTEM::loop();

  if (rfid_device == TYPE_CU500) {
    static unsigned ts = 0;
    if (rfid_record == 0) {
      rfid_reader.send_cmd(CU500::CMD_MULT_INVENTORY);
      rfid_record = millis();
    } else if (millis() - rfid_record > 200) {
      rfid_reader.clear();
      rfid_record = 0;
    }

    const auto &new_rfid = rfid_reader.rfid();
    if (!_latest_rfid_index.equals(new_rfid)) {
      _latest_rfid_index = new_rfid;
      if (!_latest_rfid_index.isEmpty()) {
        Serial.println(_latest_rfid_index.c_str());
      }
    }
  } else if (rfid_device == TYPE_PN5180) {
    String new_rfid{};
    if (has_uid(new_rfid)) {
      if (!_latest_rfid_index.equals(new_rfid)) {
        _latest_rfid_index = new_rfid;
        if (!_latest_rfid_index.isEmpty()) {
          analogWrite(4, 128);
          Serial.println(_latest_rfid_index.c_str());
          delay(300);
        }
      }
      _latest_ts = millis();
    } else if (millis() - _latest_ts > 300) {
      _latest_rfid_index = "";
    }
    analogWrite(4, 0);
    update_nfc();
  }

  bool send_json = false;
  while (Serial.available()) {
    send_json = (Serial.read() == '\n');
  }

  if (send_json && has_uid()) {
    String output;
    JsonDocument json_doc;
    auto &ndef_msg = get_ndef_message();
    auto size = ndef_msg.getRecordCount();
    for (int i = 0; i < size; ++i) {
      auto record = ndef_msg.getRecord(i);
      int payload_length = record->getPayloadLength();
      char buffer[payload_length + 1]{'\0'};
      memcpy(buffer, record->getPayload(), payload_length);
      switch (record->getTnf()) {
      case 2:
        json_doc[i] = String(buffer);
        break;
      case 1: /** NOTE: not support now. https://; plain text; */
      default:
        // Serial.println(record->getTnf());
        // Serial.println(buffer);
        break;
      }
    }
    serializeJson(json_doc, output);
    Serial.println(output);
  }
#if 0
  check_network_state();
#endif
}

bool debug_enable() { return _debug_enable; }

void set_debug(bool enable) { _debug_enable = enable; }

bool system_enable() { return _sys_enable; }

void set_system(bool enable) { _sys_enable = enable; }

IPAddress debug_ip() { return _debug_ip; }

uint32_t debug_port() { return _debug_port; }

void update_debug_client(IPAddress address, uint32_t port = 3333) {
  _debug_ip = address;
  _debug_ip_str = _debug_ip.toString();
  _debug_port = port;
}

static void check_network_state() {
  auto &network_manager = NetworkManager::instance();
  if (network_manager.getMode() == WIFI_AP) {
    /** nothing todo. */
  } else if (!network_manager.isConnected()) {
    if (_disconnect_ts == 0) {
      _disconnect_ts = millis();
    } else if (millis() - _disconnect_ts > 5000) {
      network_manager.startAP();
      _disconnect_ts = 0;
    }
  } else { /** if(network_manager.isConnected()) */
    _disconnect_ts = 0;
  }
}

static void send_box_msg(const String &str) {
  send_msg(str, rs_cfg().ip_str.c_str(), rs_cfg().port);
}

static void send_msg(const String &str, const char *server_ip,
                     int server_port) {
  assert(server_ip != nullptr);
  auto &udp = NetworkManager::instance();
  udp.beginPacket(server_ip, server_port);
  udp.printf("%s\n", str.c_str());
  udp.endPacket();
}
