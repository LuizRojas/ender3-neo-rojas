/* Logica de atualizacao: procura um .bin no cartao e grava em APP_BASE. */
#include "board.h"
#include "fat.h"
#include <string.h>

static uint32_t buf[128];

static int header_ok(const uint8_t *p, uint32_t size)
{
    uint32_t sp = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    uint32_t pc = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
    if (sp <= RAM_BASE || sp > RAM_MAX_END || (sp & 3)) return 0;
    /* Thumb, dentro da area da aplicacao. Recusa firmware compilado para 0x08000000. */
    if (!(pc & 1) || pc < APP_BASE + 8 || pc >= APP_BASE + size) return 0;
    return 1;
}

int app_is_valid(void)
{
    return header_ok(flash_ptr(APP_BASE), flash_end() - APP_BASE);
}

/* Aceita so arquivos que parecem firmware para 0x08007000 e cabem na flash */
static int bin_filter(fat_file_t *f)
{
    if (f->size < 8 || f->size > flash_end() - APP_BASE) return 0;
    int n = fat_read_sector(f, (uint8_t *)buf);
    if (n < 8) return 0;
    return header_ok((const uint8_t *)buf, f->size);
}

/* 0 = igual ao que esta na flash, 1 = diferente, -1 = erro de leitura */
static int compare(fat_file_t *f)
{
    fat_rewind(f);
    uint32_t off = 0;
    for (;;) {
        int n = fat_read_sector(f, (uint8_t *)buf);
        if (n < 0) return -1;
        if (n == 0) return 0;
        if (memcmp(buf, flash_ptr(APP_BASE + off), (uint32_t)n)) return 1;
        off += (uint32_t)n;
    }
}

static int program(fat_file_t *f)
{
    static uint32_t first[128];
    uint32_t first_len = 0;
    uint32_t off = 0;

    fat_rewind(f);
    if (flash_unlock()) return -1;
    for (;;) {
        int n = fat_read_sector(f, (uint8_t *)buf);
        if (n < 0) goto fail;
        if (n == 0) break;
        uint32_t addr = APP_BASE + off;
        if (addr % PAGE_SIZE == 0 && flash_erase_page(addr)) goto fail;
        if (n & 1) ((uint8_t *)buf)[n] = 0xFF;   /* completa a meia-palavra */
        if (off == 0) {
            /* O primeiro setor (vetores) e gravado por ultimo: se faltar energia
               no meio, a aplicacao fica invalida e o bootloader tenta de novo. */
            memcpy(first, buf, 512);
            first_len = (uint32_t)n;
        } else if (flash_program(addr, (const uint8_t *)buf, (uint32_t)(n + 1) & ~1u)) {
            goto fail;
        }
        off += (uint32_t)n;
    }
    if (first_len == 0) goto fail;
    if (flash_program(APP_BASE, (const uint8_t *)first, (first_len + 1) & ~1u)) goto fail;
    flash_lock();
    return 0;
fail:
    flash_lock();
    return -1;
}

int update_from_sd(void)
{
    static fat_file_t f;
    int r = UPD_NO_CARD;

    if (sd_init() || fat_mount()) goto out;
    if (fat_find_bin(&f, bin_filter)) { r = UPD_NO_FILE; goto out; }

    int c = compare(&f);
    if (c < 0) { r = UPD_ERROR; goto out; }
    if (c == 0) { r = UPD_SAME; goto out; }

    beep(80);
    if (program(&f) || compare(&f) != 0) { r = UPD_ERROR; goto out; }
    r = UPD_FLASHED;
out:
    sd_deinit();
    return r;
}
