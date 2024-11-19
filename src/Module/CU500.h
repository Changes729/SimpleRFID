/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _CU500_H
#define _CU500_H
#pragma once
/* Public include ------------------------------------------------------------*/
#include <HardwareSerial.h>

/* Public namespace ----------------------------------------------------------*/
namespace CU500 {

/* Public define -------------------------------------------------------------*/
#define CU500_BAUDRATE 19200

/* Public typedef ------------------------------------------------------------*/
typedef enum : uint8_t {
  CMD_NULL = 0x00,
  CMD_MULT_INVENTORY = 0xDC,
} cmd_t;

typedef void (*handle_callback_t)(cmd_t, uint8_t *data, size_t, void *u_data);

/* Public template -----------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
/* Public class --------------------------------------------------------------*/
class CU500_Reader {
public:
  CU500_Reader(HardwareSerial &serial);
  ~CU500_Reader();

  void init();

  void send_cmd(cmd_t cmd);
  void set_cmd_callback(handle_callback_t cb, void *user_data);

  size_t inventory_count() { return _count; }
  String rfid() { return _rfid; }
  void clear();

protected:
  void resize(size_t size);
  void append(const uint8_t *data, size_t size);

private:
  bool _check_transition_end();
  void _on_transition_end();
  void _handle_cmd(cmd_t cmd, uint8_t *data, size_t size);

private:
  /** transition ****************************************************/
  struct {
    uint8_t uid;
    cmd_t cmd;
  } _transition;
  handle_callback_t _callback;
  void *_user_data;

  /** buffer part ***************************************************/
  uint8_t *_buffer;
  size_t _index;
  size_t _buffer_size;

  /** has a ... *****************************************************/
  HardwareSerial &_serial;

  /** inventory */
  size_t _count;

  /** rfids */
  String _rfid;
};

}; // namespace CU500

#endif /* _CU500_H */
