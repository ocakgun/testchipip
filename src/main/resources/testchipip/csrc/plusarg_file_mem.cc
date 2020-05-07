#include <vpi_user.h>
#include <svdpi.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstdint>
#include <vector>
#include <queue>
#include <fesvr/context.h>

#include "plusarg_file_mem.h"

// We are returning the pointer to the PlusargFileMem object so that we can have
// multiple plusarg memories in one sim.
extern "C" long long plusarg_file_mem_init(const char *filename, int addr_bits, int data_bits)
{
  PlusargFileMem *mem = new PlusargFileMem(filename, (1 << addr_bits), data_bits/8);
  return (long long)mem;
}

extern "C" void plusarg_file_mem_read(long long mem, long long address, long long *data)
{
  // Cast the pointer back to PlusargFileMem pointer
  ((PlusargFileMem *)mem)->read(address, data);
}

extern "C" void plusarg_file_mem_write(long long mem, long long address, long long data)
{
  // Cast the pointer back to PlusargFileMem pointer
  ((PlusargFileMem *)mem)->write(address, data);
}

PlusargFileMem::PlusargFileMem(const char *filename, int capacity_words, int data_bytes)
{
  long size;
  _capacity_words = capacity_words;
  _capacity_bytes = capacity_words*data_bytes;
  _data_bytes = data_bytes;

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

PlusargFileMem::~PlusargFileMem(void)
{
  fclose(_file);
}

void PlusargFileMem::read(long long address, long long *data)
{

  long long buf[0];

  long long addr2 = address;

  if (address >= _capacity_words)
  {
    fprintf(stderr, "Read out of bounds: 0x%016x >= 0x%016x\n", address, _capacity_words);
    *data = 0xffffffffffffffffL;
    return;
  }

  if (fseek(_file, addr2, SEEK_SET))
  {
    fprintf(stderr, "Could not seek to 0x%016x\n", addr2);
    *data = 0xffffffffffffffffL;
    return;
  }

  if (fread(data, 1, 1, _file) == 0)
  {
    fprintf(stderr, "Cannot read data at 0x%016x\n", addr2);
    *data = 0xffffffffffffffffL;
  }

}

void PlusargFileMem::write(long long address, long long data)
{
  /* unimp */
}
