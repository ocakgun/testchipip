#include <vpi_user.h>
#include <svdpi.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <fesvr/context.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "plusarg_file_mem.h"

// We are returning the pointer to the PlusargFileMem object so that we can have
// multiple plusarg memories in one sim.
extern "C" long long plusarg_file_mem_init(const char *filename, unsigned char writeable, int addr_bits, int data_bits)
{
  PlusargFileMem *mem = new PlusargFileMem(filename, (bool)writeable, (1 << addr_bits), data_bits/8);
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

PlusargFileMem::PlusargFileMem(const char *filename, bool writeable, int capacity_words, int data_bytes)
{
  long size;
  _capacity_words = capacity_words;
  _capacity_bytes = capacity_words*data_bytes;
  _data_bytes = data_bytes;

  // open the file
  _file = open(filename, (writeable ? O_RDWR : O_RDONLY));

  // get the file size
  struct stat sb;
  fstat(_file, &sb);
  _memsize = (uint64_t)sb.st_size;

  // Only mmap capacity if the file is larger than the memory
  if (_capacity_bytes < _memsize) _memsize = _capacity_bytes;
  _memblk = mmap(NULL, _memsize, PROT_READ | (writeable ? PROT_WRITE : 0), MAP_SHARED, _file, 0);

  if (_memblk == MAP_FAILED)
  {
    fprintf(stderr, "Error mmapping file %s\n", filename);
    abort();
  }

}

PlusargFileMem::~PlusargFileMem(void)
{
  munmap(_memblk, _memsize);
  close(_file);
}

void PlusargFileMem::read(long long address, long long *data)
{

  if (_data_bytes == 1)
    *data = (0xff & (long long)(((uint8_t *)_memblk)[address]));

  if (_data_bytes == 2)
    *data = (0xffff & (long long)(((uint16_t *)_memblk)[address]));

  if (_data_bytes == 4)
    *data = (0xffffffff & (long long)(((uint32_t *)_memblk)[address]));

  if (_data_bytes == 8)
    *data = (((uint64_t *)_memblk)[address]);

}

void PlusargFileMem::write(long long address, long long data)
{
  /* unimplemented */
}
