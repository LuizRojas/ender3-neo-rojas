/* Driver SDIO por polling (barramento de 1 bit) para STM32F103 / GD32F303.
   Pinos da Creality V4.2.2: PC8 D0, PC9 D1, PC10 D2, PC11 D3, PC12 CK, PD2 CMD. */
#include "regs.h"
#include "board.h"
#include "fat.h"

#define HCLK_HZ       64000000u
#define CLKDIV_INIT   158u      /* 64 MHz / (158 + 2) = 400 kHz */
#define CLKDIV_XFER   6u        /* 64 MHz / (6 + 2)   = 8 MHz   */
#define DATA_TIMEOUT  4000000u  /* ciclos de SDIO_CK, ~0,5 s a 8 MHz */

#define R1_ERRORS     0xFDF98008u

enum { RSP_NONE, RSP_R1, RSP_R1B, RSP_R2, RSP_R3, RSP_R6, RSP_R7 };

static uint32_t rca;
static int high_capacity;
static int ready;

static void short_wait(void)
{
    for (volatile int i = 0; i < 64; i++) { }
}

static int cmd(uint32_t idx, uint32_t arg, int rsp)
{
    uint32_t c = idx | SDIO_CMD_CPSMEN;
    if (rsp == RSP_R2) c |= SDIO_CMD_RESP_LONG;
    else if (rsp != RSP_NONE) c |= SDIO_CMD_RESP_SHORT;

    SDIO_ICR = SDIO_ICR_STATIC;
    SDIO_ARG = arg;
    SDIO_CMD = c;

    uint32_t sta, n = 0;
    if (rsp == RSP_NONE) {
        while (!(SDIO_STA & SDIO_STA_CMDSENT))
            if (++n > 1000000u) return -1;
        SDIO_ICR = SDIO_ICR_STATIC;
        return 0;
    }
    do {
        sta = SDIO_STA;
        if (++n > 1000000u) return -1;
    } while (!(sta & (SDIO_STA_CMDREND | SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT)));
    SDIO_ICR = SDIO_ICR_STATIC;

    if (sta & SDIO_STA_CTIMEOUT) return -2;
    /* R2 e R3 nao tem CRC valido do ponto de vista do controlador */
    if ((sta & SDIO_STA_CCRCFAIL) && rsp != RSP_R2 && rsp != RSP_R3) return -3;
    if (rsp == RSP_R1 || rsp == RSP_R1B) {
        if ((SDIO_RESPCMD & 0x3F) != idx) return -4;
        if (SDIO_RESP1 & R1_ERRORS) return -5;
    }
    return 0;
}

static int acmd(uint32_t idx, uint32_t arg, int rsp)
{
    /* Ignora bits de erro do status: um cartao v1 reporta aqui o CMD8 ilegal */
    int r = cmd(55, rca << 16, RSP_R1);
    if (r && r != -5) return -1;
    return cmd(idx, arg, rsp);
}

/* Espera o cartao voltar ao estado "transfer" com READY_FOR_DATA */
static int wait_ready(void)
{
    for (uint32_t i = 0; i < 100000u; i++) {
        if (cmd(13, rca << 16, RSP_R1)) return -1;
        uint32_t st = SDIO_RESP1;
        if ((st & (1u << 8)) && ((st >> 9) & 0xF) == 4) return 0;
    }
    return -1;
}

static void pins_sdio(void)
{
    RCC_APB2ENR |= RCC_APB2_IOPC | RCC_APB2_IOPD;
    /* PC8..PC12: funcao alternativa push-pull 50 MHz (0xB) */
    GPIO_CRH(GPIOC_BASE) = (GPIO_CRH(GPIOC_BASE) & ~0x000FFFFFu) | 0x000BBBBBu;
    /* PD2: funcao alternativa push-pull 50 MHz */
    GPIO_CRL(GPIOD_BASE) = (GPIO_CRL(GPIOD_BASE) & ~0x00000F00u) | 0x00000B00u;
}

static void pins_reset(void)
{
    /* volta para entrada flutuante (valor de reset 0x4) */
    GPIO_CRH(GPIOC_BASE) = (GPIO_CRH(GPIOC_BASE) & ~0x000FFFFFu) | 0x00044444u;
    GPIO_CRL(GPIOD_BASE) = (GPIO_CRL(GPIOD_BASE) & ~0x00000F00u) | 0x00000400u;
}

int sd_init(void)
{
    ready = 0;
    rca = 0;
    high_capacity = 0;

    pins_sdio();
    RCC_AHBENR |= RCC_AHBENR_SDIO;
    short_wait();

    SDIO_POWER = 0;
    short_wait();
    SDIO_CLKCR = CLKDIV_INIT;
    short_wait();
    SDIO_POWER = 3;
    short_wait();
    SDIO_CLKCR = CLKDIV_INIT | SDIO_CLKCR_CLKEN;
    SDIO_MASK = 0;
    delay_ms(2);   /* >= 74 ciclos de clock antes do CMD0 */

    if (cmd(0, 0, RSP_NONE)) return -1;

    int v2 = 0;
    if (cmd(8, 0x1AA, RSP_R7) == 0) {
        if ((SDIO_RESP1 & 0xFFF) != 0x1AA) return -2;
        v2 = 1;
    }

    uint32_t ocr = 0;
    uint32_t i;
    for (i = 0; i < 1000; i++) {
        if (acmd(41, 0x80100000u | (v2 ? 0x40000000u : 0), RSP_R3)) return -3;
        ocr = SDIO_RESP1;
        if (ocr & 0x80000000u) break;
        delay_ms(1);
    }
    if (i == 1000) return -4;
    high_capacity = (ocr & 0x40000000u) != 0;

    if (cmd(2, 0, RSP_R2)) return -5;
    if (cmd(3, 0, RSP_R6)) return -6;
    rca = SDIO_RESP1 >> 16;

    SDIO_CLKCR = CLKDIV_XFER | SDIO_CLKCR_CLKEN;
    short_wait();

    if (cmd(7, rca << 16, RSP_R1B)) return -7;
    if (wait_ready()) return -8;
    if (!high_capacity && cmd(16, 512, RSP_R1)) return -9;

    ready = 1;
    return 0;
}

void sd_deinit(void)
{
    SDIO_CLKCR = 0;
    short_wait();
    SDIO_POWER = 0;
    short_wait();
    SDIO_ICR = SDIO_ICR_STATIC;
    RCC_AHBENR &= ~RCC_AHBENR_SDIO;
    pins_reset();
    ready = 0;
}

int disk_read(uint32_t lba, uint8_t *buf)
{
    uint32_t *p = (uint32_t *)buf;   /* buffers sao sempre alinhados em 4 */
    uint32_t n = 0, sta;

    if (!ready) return -1;
    for (int tries = 0; tries < 3; tries++) {
        SDIO_ICR = SDIO_ICR_STATIC;
        SDIO_DTIMER = DATA_TIMEOUT;
        SDIO_DLEN = 512;
        SDIO_DCTRL = SDIO_DCTRL_BS512 | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DTEN;
        if (cmd(17, high_capacity ? lba : lba * 512, RSP_R1)) {
            SDIO_DCTRL = 0;
            wait_ready();
            continue;
        }
        n = 0;
        do {
            sta = SDIO_STA;
            if (sta & SDIO_STA_RXDAVL) {
                uint32_t w = SDIO_FIFO;
                if (n < 128) p[n++] = w;
            }
        } while (!(sta & (SDIO_STA_DATAEND | SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT |
                          SDIO_STA_RXOVERR | SDIO_STA_STBITERR)));
        while ((SDIO_STA & SDIO_STA_RXDAVL) && n < 128) p[n++] = SDIO_FIFO;
        SDIO_ICR = SDIO_ICR_STATIC;
        SDIO_DCTRL = 0;
        if ((sta & SDIO_STA_DATAEND) && !(sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT |
                SDIO_STA_RXOVERR | SDIO_STA_STBITERR)) && n == 128)
            return 0;
        wait_ready();
    }
    return -1;
}
