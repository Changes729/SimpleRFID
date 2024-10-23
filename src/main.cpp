/** See a brief introduction (right-hand button) */
/* Private include -----------------------------------------------------------*/
#include "config.h"

#include <Arduino.h>
#include <LittleFS.h>

#ifdef ESP32
#include <AsyncTCP.h>

#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include "Web/network_settings.h"
#include "config.h"
#include "network_manager.h"
#include "system_operation.h"
#include "web_server.h"

/* Private namespace ---------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
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

/* Private class -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void check_network_state();
static void send_box_msg(const String &str);
static void send_msg(const String &str, const char *server_ip, int server_port);

/* Private function ----------------------------------------------------------*/
/* Private class function ----------------------------------------------------*/
void setup() {
  /** Params init */
  WebServer::create();
  NetworkManager::create();

  /** Serial init */
  Serial.begin(115200);
  while (!Serial) {
  }

  /** LittleFS init */
  if (!LittleFS.begin(false, "")) {
    Serial.println("LittleFS mount failed");
    return;
  }

  SYSTEM::init();
  init_rs_cfg();

  NetworkManager::instance().config_dhcpcd("/etc/dhcpcd.conf");
  NetworkManager::instance().config_wpa_supplicant("/etc/wpa_supplicant.conf");
  NetworkManager::instance().begin();

  // this resets all the neopixels to an off state

  WebServer::instance().on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello, world");
  });

  NetworkSettings::init();
  WebServer::instance().begin();
}

void loop() {
  SYSTEM::loop();
  check_network_state();

  do {
    if (NetworkManager::instance().is_connect() ||
        NetworkManager::instance().isConnected()) {
    } else {
    }

  } while (0);
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
