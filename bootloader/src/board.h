/* Interface entre a logica de atualizacao e o hardware. */
#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>

#define BOOT_BASE     0x08000000u
#define APP_BASE      0x08007000u   /* 28 KB: mesmo layout do bootloader da Creality */
#define PAGE_SIZE     2048u
#define RAM_BASE      0x20000000u
#define RAM_MAX_END   0x20018000u   /* aceita firmwares de 64 KB e 96 KB de RAM */

uint32_t flash_end(void);
int  flash_unlock(void);
void flash_lock(void);
int  flash_erase_page(uint32_t addr);
int  flash_program(uint32_t addr, const uint8_t *data, uint32_t len);
const uint8_t *flash_ptr(uint32_t addr);

int  sd_init(void);
void sd_deinit(void);

void beep(uint32_t ms);
void delay_ms(uint32_t ms);

/* Resultado de update_from_sd() */
enum {
    UPD_NO_CARD = 1,     /* cartao ausente ou sem FAT valida */
    UPD_NO_FILE,         /* nenhum .bin de firmware valido */
    UPD_SAME,            /* firmware do cartao ja esta gravado */
    UPD_FLASHED,         /* gravado e verificado */
    UPD_ERROR            /* erro de leitura ou gravacao */
};

int  update_from_sd(void);
int  app_is_valid(void);

#endif
