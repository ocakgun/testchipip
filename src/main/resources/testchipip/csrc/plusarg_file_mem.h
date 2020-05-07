#ifndef __PLUSARG_FILE_MEM_H__
#define __PLUSARG_FILE_MEM_H__

class PlusargFileMem
{
  public:
    PlusargFileMem(const char *filename, int capacity_words, int data_bytes);
    ~PlusargFileMem(void);

    void read(long long address, long long *data);
    void write(long long address, long long data);

  private:
    FILE *_file;
    long long _capacity_words;
    long long _capacity_bytes;
    int _data_bytes;
};

#endif /* __SPI_FLASH_H__ */
