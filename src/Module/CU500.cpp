/** See a brief introduction (right-hand button) */
#include "CU500.h"
/* Private include -----------------------------------------------------------*/
/* Private namespace ---------------------------------------------------------*/
namespace CU500 {

/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private template ----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private class -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Private class function ----------------------------------------------------*/
/**
 * @brief  ...
 * @param  None
 * @retval None
 */
CU500_Reader::CU500_Reader(HardwareSerial &serial)
    : _transition{0, CMD_NULL}, _callback{nullptr}, _buffer{nullptr}, _index{0},
      _buffer_size{0}, _serial(serial), _count{0} {
  /** NOTE: init() should not be called on here. */
}

CU500_Reader::~CU500_Reader() {
  _serial.onReceive(nullptr);

  free(_buffer);
  _buffer = nullptr;
  _buffer_size = 0;
}

void CU500_Reader::init() {
  _serial.begin(CU500_BAUDRATE);
  _serial.onReceive([this]() {
    size_t receive = _serial.available();
    uint8_t buffer[receive];

    _serial.readBytesUntil('\0', buffer, receive);
    append(buffer, receive);
    if (_check_transition_end()) {
      _on_transition_end();
    }
  });
}

void CU500_Reader::set_cmd_callback(handle_callback_t cb, void *user_data) {
  _callback = cb;
  _user_data = user_data;
}

void CU500_Reader::send_cmd(cmd_t cmd) {
  constexpr uint8_t DATA_BEGIN = 3;

  uint8_t data_len = 0;
  uint8_t total_len = 4 + data_len;

  uint8_t uid = 0x01;
  uint8_t data_buffer[data_len] = {};
  uint8_t buffer[total_len] = {[0] = total_len, [1] = uid, [2] = cmd};

  memccpy(buffer + DATA_BEGIN, data_buffer, data_len, sizeof(uint8_t));

  uint8_t bcc = 0;
  for (int i = 0; i < total_len - 1; ++i) {
    bcc += buffer[i];
  }
  buffer[total_len - 1] = ~bcc;

  _transition.cmd = cmd;
  _transition.uid = uid;
  _serial.write(buffer, total_len);
  _serial.flush();
}

void CU500_Reader::resize(size_t size) {
  if (size > _buffer_size) {
    _buffer = (uint8_t *)realloc(_buffer, size);
    _buffer_size = size;
  }
}

void CU500_Reader::clear() { _index = 0; }

void CU500_Reader::append(const uint8_t *data, size_t size) {
  if (_index + size > _buffer_size) {
    resize(_index + size);
  }

  memccpy(_buffer + _index, data, size, sizeof(uint8_t));
  _index += size;
}

bool CU500_Reader::_check_transition_end() {
  return (_index > 0 && _index >= _buffer[0]);
}

void CU500_Reader::_on_transition_end() {
  constexpr uint8_t DATA_BEGIN = 4;
  constexpr uint8_t DATA_OK = 0x00;
  assert(_buffer_size > 0);

  uint8_t len = _buffer[0];
  uint8_t uid = _buffer[1];
  uint8_t cmd = _buffer[2];
  uint8_t sw = _buffer[3];
  assert(_buffer_size >= len);

  if (uid == _transition.uid && cmd == _transition.cmd) {
    uint8_t bcc = 0;
    for (int i = 0; i < len - 1; ++i) {
      bcc += _buffer[i];
    }
    if (_buffer[len - 1] == uint8_t(~bcc)) {
      _handle_cmd(_transition.cmd, _buffer + DATA_BEGIN, len - 5);
    }
  }

  /** transition end */
  {
    _transition.uid = 0;
    _transition.cmd = CMD_NULL;

    clear();
  }
}

void CU500_Reader::_handle_cmd(cmd_t cmd, uint8_t *data, size_t size) {
  if (cmd == CU500::CMD_MULT_INVENTORY) {
    _count = size / 8;

    char buffer[size * 2 + 1]{'\0'};
    for (size_t i = 0; i < size; ++i) {
      snprintf(buffer + i * 2, (size - i) * 2 + 1, "%02X", data[i]);
    }

    _rfid = String(buffer);
  }

  if (_callback != nullptr) {
    _callback(cmd, data, size, _user_data);
  }
}

}; // namespace CU500