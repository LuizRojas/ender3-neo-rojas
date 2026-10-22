/* Registradores minimos do STM32F103 / GD32F303 (layout compativel). */
#ifndef REGS_H
#define REGS_H
#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(addr))

/* RCC */
#define RCC_BASE      0x40021000u
#define RCC_CR        REG(RCC_BASE + 0x00)
#define RCC_CFGR      REG(RCC_BASE + 0x04)
#define RCC_CIR       REG(RCC_BASE + 0x08)
#define RCC_AHBENR    REG(RCC_BASE + 0x14)
#define RCC_APB2ENR   REG(RCC_BASE + 0x18)

#define RCC_CR_HSION    (1u << 0)
#define RCC_CR_HSIRDY   (1u << 1)
#define RCC_CR_PLLON    (1u << 24)
#define RCC_CR_PLLRDY   (1u << 25)
#define RCC_AHBENR_SDIO (1u << 10)
#define RCC_APB2_IOPC   (1u << 4)
#define RCC_APB2_IOPD   (1u << 5)

/* GPIO */
#define GPIOC_BASE    0x40011000u
#define GPIOD_BASE    0x40011400u
#define GPIO_CRL(b)   REG((b) + 0x00)
#define GPIO_CRH(b)   REG((b) + 0x04)
#define GPIO_IDR(b)   REG((b) + 0x08)
#define GPIO_BSRR(b)  REG((b) + 0x10)
#define GPIO_BRR(b)   REG((b) + 0x14)

/* FLASH (FMC no GD32) */
#define FLASH_BASE_R  0x40022000u
#define FLASH_ACR     REG(FLASH_BASE_R + 0x00)
#define FLASH_KEYR    REG(FLASH_BASE_R + 0x04)
#define FLASH_SR      REG(FLASH_BASE_R + 0x0C)
#define FLASH_CR      REG(FLASH_BASE_R + 0x10)
#define FLASH_AR      REG(FLASH_BASE_R + 0x14)

#define FLASH_SR_BSY      (1u << 0)
#define FLASH_SR_PGERR    (1u << 2)
#define FLASH_SR_WRPRTERR (1u << 4)
#define FLASH_SR_EOP      (1u << 5)
#define FLASH_CR_PG       (1u << 0)
#define FLASH_CR_PER      (1u << 1)
#define FLASH_CR_STRT     (1u << 6)
#define FLASH_CR_LOCK     (1u << 7)

#define FLASH_SIZE_KB     (*(volatile uint16_t *)0x1FFFF7E0u)

/* SDIO */
#define SDIO_BASE     0x40018000u
#define SDIO_POWER    REG(SDIO_BASE + 0x00)
#define SDIO_CLKCR    REG(SDIO_BASE + 0x04)
#define SDIO_ARG      REG(SDIO_BASE + 0x08)
#define SDIO_CMD      REG(SDIO_BASE + 0x0C)
#define SDIO_RESPCMD  REG(SDIO_BASE + 0x10)
#define SDIO_RESP1    REG(SDIO_BASE + 0x14)
#define SDIO_RESP2    REG(SDIO_BASE + 0x18)
#define SDIO_RESP3    REG(SDIO_BASE + 0x1C)
#define SDIO_RESP4    REG(SDIO_BASE + 0x20)
#define SDIO_DTIMER   REG(SDIO_BASE + 0x24)
#define SDIO_DLEN     REG(SDIO_BASE + 0x28)
#define SDIO_DCTRL    REG(SDIO_BASE + 0x2C)
#define SDIO_STA      REG(SDIO_BASE + 0x34)
#define SDIO_ICR      REG(SDIO_BASE + 0x38)
#define SDIO_MASK     REG(SDIO_BASE + 0x3C)
#define SDIO_FIFO     REG(SDIO_BASE + 0x80)

#define SDIO_STA_CCRCFAIL (1u << 0)
#define SDIO_STA_DCRCFAIL (1u << 1)
#define SDIO_STA_CTIMEOUT (1u << 2)
#define SDIO_STA_DTIMEOUT (1u << 3)
#define SDIO_STA_TXUNDERR (1u << 4)
#define SDIO_STA_RXOVERR  (1u << 5)
#define SDIO_STA_CMDREND  (1u << 6)
#define SDIO_STA_CMDSENT  (1u << 7)
#define SDIO_STA_DATAEND  (1u << 8)
#define SDIO_STA_STBITERR (1u << 9)
#define SDIO_STA_DBCKEND  (1u << 10)
#define SDIO_STA_CMDACT   (1u << 11)
#define SDIO_STA_RXDAVL   (1u << 21)
#define SDIO_ICR_STATIC   0x000005FFu

#define SDIO_CMD_RESP_SHORT (1u << 6)
#define SDIO_CMD_RESP_LONG  (3u << 6)
#define SDIO_CMD_CPSMEN     (1u << 10)

#define SDIO_CLKCR_CLKEN    (1u << 8)

#define SDIO_DCTRL_DTEN     (1u << 0)
#define SDIO_DCTRL_DTDIR    (1u << 1)
#define SDIO_DCTRL_BS512    (9u << 4)

/* SysTick / SCB / NVIC */
#define SYST_CSR      REG(0xE000E010u)
#define SYST_RVR      REG(0xE000E014u)
#define SYST_CVR      REG(0xE000E018u)
#define SCB_VTOR      REG(0xE000ED08u)
#define NVIC_ICER(n)  REG(0xE000E180u + 4u * (n))
#define NVIC_ICPR(n)  REG(0xE000E280u + 4u * (n))

#endif
