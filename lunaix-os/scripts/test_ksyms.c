#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

struct ksyms_meta
{
    unsigned int entries;
    unsigned int label_off_base;
};

struct ksyms_entry
{
    unsigned int addr;
    unsigned int off;
};

void
main()
{
    int fd = open("ksyms", O_RDONLY);
    void* data = mmap(0, 0x6000, PROT_READ, MAP_PRIVATE, fd, 0);

    printf("mapped at: %p\n", data);

    if (data == 0) {
        printf("unable to map, %d", errno);
        return;
    }

    close(fd);

    struct ksyms_meta* meta = (struct ksyms_meta*)data;

    printf(
      "entires: %d, label_base: %p\n", meta->entries, meta->label_off_base);

    struct ksyms_entry* entries = (struct ksyms_entry*)(meta + 1);
    for (int i = 0; i < meta->entries; i++) {
        struct ksyms_entry* entry = &entries[i];
        printf(
          "addr: %p, off: %p, label: %s\n",
          entry->addr,
          entry->off,
          (char*)((unsigned long)data + meta->label_off_base + entry->off));
    }
}
