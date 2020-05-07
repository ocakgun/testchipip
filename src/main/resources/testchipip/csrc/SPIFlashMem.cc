#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstdint>

#include "SPIFlashMem.h"

// CAUTION! This model only supports a small subset of standard QSPI flash
// features. It is useful for modeling a pre-loaded flash memory intended
// to be used as a ROM. You should replace this with a verilog model of
// your specific flash device for more rigorous verification. It also very
// likely contains bugs. Use at your own risk!

SPIFlashMem::SPIFlashMem(const char *filename, uint32_t max_addr)
{
  long size;
  _max_addr = max_addr;

  _file = fopen(filename, "r");
  if (!_file)
  {
    fprintf(stderr, "Could not open %s\n", filename);
    abort();
  }
  if (fseek(_file, 0, SEEK_END))
  {
    perror("fseek");
    abort();
  }
  size = ftell(_file);
  if (size < 0)
  {
    perror("ftell");
    abort();
  }
}

SPIFlashMem::~SPIFlashMem(void)
{
  fclose(_file);
}

uint8_t SPIFlashMem::read(uint32_t address)
{

  uint8_t buf[0];

  if (address > _max_addr)
  {
    fprintf(stderr, "Read out of bounds: 0x%016x >= 0x%016x\n", address, _max_addr);
    abort();
  }

  if (fseek(_file, address, SEEK_SET))
  {
    fprintf(stderr, "Could not seek to 0x%016x\n", address);
    abort();
  }

  if (fread(buf, 1, 1, _file) == 0)
  {
    fprintf(stderr, "Cannot read data at 0x%016x\n", address);
    abort();
  }

  return buf[0];

}

void SPIFlashMem::tick(bool sck, bool cs, bool reset, uint8_t dq_in, uint8_t *dq_out, uint8_t *dq_drive)
{

  // this command has 4 byte address (otherwise 3)
  bool cmd_has_4byte_addr = SPI_CMD_4BYTE_ADDR(_cmd);
  // this command has dummy cycle padding
  bool cmd_has_dummy = SPI_CMD_HAS_DUMMY(_cmd);
  // the data count is aligned to a byte
  bool byte_aligned = ((_data_count & 0x7) == 0);
  // increment the address automatically
  bool incr_addr = ((_state == SPI_STATE_PUT_DATA) && byte_aligned);
  // transition into GET_ADDR state
  bool cmd_done = ((_state == SPI_STATE_GET_CMD) && (_data_count == 8));
  // transition into PUT_DATA or DUMMY states
  bool addr_done = ((_state == SPI_STATE_GET_ADDR) && (_data_count == (cmd_has_4byte_addr ? 40 : 32)));
  // transition into PUT_DATA state
  bool dummy_done = (_dummy_count == (SPI_DUMMY_CYCLES - 1));
  // next value of cmd to latch on the rising clock edge
  uint8_t next_cmd = (cmd_done ? (_data_buf & 0xff) : _cmd);
  // the next value of cmd is valid
  bool next_cmd_valid = SPI_CMD_VALID(next_cmd);

  // next value of address register to latch on the rising clock edge
  uint32_t next_addr = _addr;
  if (_state != SPI_STATE_DUMMY)
  {
    if (addr_done)
    {
      next_addr = _data_buf;
      if (cmd_has_4byte_addr) next_addr &= 0x00ffffff;
    }
    else if (incr_addr)
    {
      next_addr = _addr + 1;
    }
  }

  // State machine
  uint8_t next_state = _state;
  switch(_state)
  {
    case SPI_STATE_STANDBY:
      if (!cs) next_state = SPI_STATE_GET_CMD;
      break;
    case SPI_STATE_GET_CMD:
      if (cmd_done)
      {
        next_state = SPI_STATE_ERROR;
        if (next_cmd_valid) next_state = SPI_STATE_GET_ADDR;
      }
      break;
    case SPI_STATE_GET_ADDR:
      if (addr_done)
      {
        next_state = SPI_STATE_PUT_DATA;
        if (cmd_has_dummy) next_state = SPI_STATE_DUMMY;
      }
      break;
    case SPI_STATE_DUMMY:
      if (dummy_done) next_state = SPI_STATE_PUT_DATA;
      break;
    case SPI_STATE_PUT_DATA:
    case SPI_STATE_ERROR:
      if (cs) next_state = SPI_STATE_STANDBY;
      break;
  }

  // if we are currently doing quad I/O
  bool quad_io =
    ((SPI_CMD_QUAD_O(next_cmd) && (next_state == SPI_STATE_GET_ADDR)) ||
     (SPI_CMD_QUAD_I(next_cmd) && (next_state == SPI_STATE_PUT_DATA)));
  // the next value of data_buf to latch on the rising clock edge
  uint8_t next_data_buf = (quad_io ?
    (((_data_buf & 0x0fffffff) << 4) | (dq_in & 0xf)) :
    (((_data_buf & 0x7fffffff) << 1) | (dq_in & 0x1)));
  // how much to increment the counter by
  uint8_t quad_incr = (quad_io ? 4 : 1);

  // outputs

  if (_drive_dq)
  {
    if (quad_io)
    {
      *dq_out = (_data_out >> 6); // dq[1] <= _data_out[7]
      *dq_drive = 0x2; // only drive dq[1]
    }
    else
    {
      *dq_out = (_data_out >> 4); // dq[3:0] <= _data_out[7:4]
      *dq_drive = 0xf; // drive all
    }
  }
  else
  {
    *dq_out = 0x0; // don't care
    *dq_drive = 0x0; // drive none
  }

  // sequential stuff
  if (reset || cs)
  {
    _data_count = 0;
    _state = SPI_STATE_STANDBY;
    _dummy_count = 0;
    _drive_dq = 0;
    _cmd = 0;
  }
  else
  {
    if (clock)
    {
      // Capture edge
      if (_state == SPI_STATE_DUMMY)
      {
        _dummy_count += 1;
      }
      else
      {
        _data_count += quad_incr;
        _dummy_count = 0;
      }
      _data_buf = next_data_buf;
      _state = next_state;
      _addr = next_addr;
      _cmd = next_cmd;
    }
    else
    {
      // Launch edge
      _drive_dq = (next_state == SPI_STATE_PUT_DATA);
      if ((next_state == SPI_STATE_PUT_DATA) && byte_aligned)
      {
        _data_out = read(next_addr);
      }
      else
      {
        _data_out = ((_data_out << quad_incr) & 0xff);
      }
    }
  }

}
