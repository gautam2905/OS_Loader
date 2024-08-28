#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "loader.h"

Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;

/*
 * Release memory and other cleanups
 */
void loader_cleanup() {
    // Free memory allocated for ehdr and phdr
    if (ehdr) {
        free(ehdr);
        ehdr = NULL;
    }
    if (phdr) {
        free(phdr);
        phdr = NULL;
    }
    // Close the file descriptor
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char** exe) {
    fd = open(exe[0], O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // 1. Load entire binary content into the memory from the ELF file.
    ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
    if (read(fd, ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    // 2. Iterate through the PHDR table and find the section of PT_LOAD
    //    type that contains the address of the entrypoint method in fib.c
    phdr = (Elf32_Phdr *)malloc(ehdr->e_phnum * sizeof(Elf32_Phdr));
    if (lseek(fd, ehdr->e_phoff, SEEK_SET) == -1) {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    if (read(fd, phdr, ehdr->e_phnum * sizeof(Elf32_Phdr)) != ehdr->e_phnum * sizeof(Elf32_Phdr)) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    Elf32_Phdr *entry_phdr = NULL;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            entry_phdr = &phdr[i];
            break;
        }
    }
    if (!entry_phdr) {
        fprintf(stderr, "No PT_LOAD segment found.\n");
        exit(EXIT_FAILURE);
    }

    // 3. Allocate memory of the size "p_memsz" using mmap function
    //    and then copy the segment content
    void *segment = mmap(NULL, entry_phdr->p_memsz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, fd, entry_phdr->p_offset);
    if (segment == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 4. Navigate to the entrypoint address into the segment loaded in the memory in above step
    void (*entry_point)(void) = (void (*)(void))(segment + entry_phdr->p_vaddr);

    // 5. Call the "_start" method and print the value returned from the "_start"
    int result = entry_point();
    printf("User _start return value = %d\n", result);

    // 6. Clean up
    munmap(segment, entry_phdr->p_memsz);
    loader_cleanup();
}