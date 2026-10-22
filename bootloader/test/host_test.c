/* Teste no PC: simula o cartao (imagem de disco) e a flash (array em RAM). */
#include "board.h"
#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLASH_KB 512
static uint8_t flash[FLASH_KB * 1024];
static FILE *img;
static int unlocked;

int disk_read(uint32_t lba, uint8_t *buf)
{
    if (fseek(img, (long)lba * 512, SEEK_SET)) return -1;
    return fread(buf, 1, 512, img) == 512 ? 0 : -1;
}
int sd_init(void) { return img ? 0 : -1; }
void sd_deinit(void) { }
void beep(uint32_t ms) { (void)ms; }
void delay_ms(uint32_t ms) { (void)ms; }
uint32_t flash_end(void) { return BOOT_BASE + sizeof flash; }
const uint8_t *flash_ptr(uint32_t a)
{
    if (a < BOOT_BASE || a >= flash_end()) { fprintf(stderr, "leitura fora da flash %08x\n", a); exit(3); }
    return flash + (a - BOOT_BASE);
}
int flash_unlock(void) { unlocked = 1; return 0; }
void flash_lock(void) { unlocked = 0; }
int flash_erase_page(uint32_t a)
{
    if (!unlocked || a < APP_BASE || a % PAGE_SIZE || a >= flash_end()) { fprintf(stderr, "erase invalido %08x\n", a); exit(3); }
    memset(flash + (a - BOOT_BASE), 0xFF, PAGE_SIZE);
    return 0;
}
int flash_program(uint32_t a, const uint8_t *d, uint32_t n)
{
    if (!unlocked || a < APP_BASE || a + n > flash_end() || (n & 1)) { fprintf(stderr, "program invalido %08x\n", a); exit(3); }
    for (uint32_t i = 0; i < n; i++) {
        if (flash[a - BOOT_BASE + i] != 0xFF) { fprintf(stderr, "gravando sem apagar %08x\n", a + i); exit(3); }
        flash[a - BOOT_BASE + i] = d[i];
    }
    return 0;
}

/* uso: host_test <imagem> <resultado esperado> [arquivo esperado na flash] [estado inicial da flash] */
int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    memset(flash, 0xFF, sizeof flash);
    memset(flash, 0xAA, APP_BASE - BOOT_BASE);           /* "bootloader" */
    if (argc > 4) {
        FILE *s = fopen(argv[4], "rb");
        if (!s) return 2;
        fread(flash, 1, sizeof flash, s);
        fclose(s);
    }
    img = strcmp(argv[1], "-") ? fopen(argv[1], "rb") : NULL;
    int r = update_from_sd();
    if (r != atoi(argv[2])) { printf("FALHOU: resultado %d, esperado %s\n", r, argv[2]); return 1; }
    for (uint32_t i = 0; i < APP_BASE - BOOT_BASE; i++)
        if (flash[i] != 0xAA) { printf("FALHOU: bootloader alterado\n"); return 1; }
    if (argc > 3 && strcmp(argv[3], "-")) {
        FILE *e = fopen(argv[3], "rb");
        static uint8_t exp[FLASH_KB * 1024];
        size_t n = fread(exp, 1, sizeof exp, e);
        fclose(e);
        if (memcmp(exp, flash + (APP_BASE - BOOT_BASE), n)) { printf("FALHOU: conteudo diferente\n"); return 1; }
        if (!app_is_valid()) { printf("FALHOU: app invalida\n"); return 1; }
    }
    FILE *o = fopen("build/flash_after.bin", "wb");
    fwrite(flash, 1, sizeof flash, o);
    fclose(o);
    printf("ok (resultado %d)\n", r);
    return 0;
}
