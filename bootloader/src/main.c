/* Bootloader generico por cartao SD para Creality V4.2.2 (Ender 3 Neo).
   Fica em 0x08000000 (28 KB) e inicia a aplicacao em 0x08007000. */
#include "regs.h"
#include "board.h"

static uint32_t hclk = 8000000u;

/* ---------- clock ---------- */

static void clock_init(void)
{
    RCC_CR |= RCC_CR_HSION;
    while (!(RCC_CR & RCC_CR_HSIRDY)) { }

    /* HSI/2 x16 = 64 MHz, sem depender do cristal. APB1 /2. */
    FLASH_ACR = 0x10u | 2u;                       /* prefetch + 2 wait states */
    RCC_CFGR = (14u << 18) | (4u << 8);
    RCC_CR |= RCC_CR_PLLON;
    for (uint32_t i = 0; i < 200000u; i++) {
        if (RCC_CR & RCC_CR_PLLRDY) {
            RCC_CFGR |= 2u;                       /* SYSCLK = PLL */
            while (((RCC_CFGR >> 2) & 3u) != 2u) { }
            hclk = 64000000u;
            return;
        }
    }
    RCC_CR &= ~RCC_CR_PLLON;                      /* fica no HSI de 8 MHz */
}

static void clock_deinit(void)
{
    RCC_CFGR &= ~3u;                              /* SYSCLK = HSI */
    while ((RCC_CFGR >> 2) & 3u) { }
    RCC_CR &= ~RCC_CR_PLLON;
    while (RCC_CR & RCC_CR_PLLRDY) { }
    RCC_CFGR = 0;
    RCC_CIR = 0x009F0000u;                        /* limpa flags, sem interrupcoes */
    FLASH_ACR = 0x30u;                            /* valor de reset */
}

/* ---------- tempo ---------- */

static void delay_us(uint32_t us)
{
    SYST_CSR = 0;
    SYST_RVR = (hclk / 1000000u) * us - 1u;
    SYST_CVR = 0;
    SYST_CSR = 5u;                                /* clock do processador, ligado */
    while (!(SYST_CSR & (1u << 16))) { }
    SYST_CSR = 0;
}

void delay_ms(uint32_t ms)
{
    while (ms--) delay_us(1000);
}

/* ---------- buzzer do display (EXP1, PC6) ---------- */

#define BEEPER_BIT (1u << 6)

static void beeper_init(void)
{
    RCC_APB2ENR |= RCC_APB2_IOPC;
    GPIO_BRR(GPIOC_BASE) = BEEPER_BIT;
    GPIO_CRL(GPIOC_BASE) = (GPIO_CRL(GPIOC_BASE) & ~0x0F000000u) | 0x02000000u; /* saida 2 MHz */
}

static void beeper_deinit(void)
{
    GPIO_BRR(GPIOC_BASE) = BEEPER_BIT;
    GPIO_CRL(GPIOC_BASE) = (GPIO_CRL(GPIOC_BASE) & ~0x0F000000u) | 0x04000000u;
}

/* Onda quadrada de ~2 kHz: funciona com buzzer ativo ou passivo */
void beep(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 4u; i++) {
        GPIO_BSRR(GPIOC_BASE) = BEEPER_BIT;
        delay_us(125);
        GPIO_BRR(GPIOC_BASE) = BEEPER_BIT;
        delay_us(125);
    }
}

/* ---------- flash ---------- */

uint32_t flash_end(void)
{
    uint32_t kb = FLASH_SIZE_KB;
    if (kb < 64 || kb > 512) kb = 256;
    return BOOT_BASE + kb * 1024u;
}

const uint8_t *flash_ptr(uint32_t addr)
{
    return (const uint8_t *)addr;
}

static int flash_wait(void)
{
    while (FLASH_SR & FLASH_SR_BSY) { }
    uint32_t sr = FLASH_SR;
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
    return (sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) ? -1 : 0;
}

int flash_unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = 0x45670123u;
        FLASH_KEYR = 0xCDEF89ABu;
    }
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
    return (FLASH_CR & FLASH_CR_LOCK) ? -1 : 0;
}

void flash_lock(void)
{
    FLASH_CR = FLASH_CR_LOCK;
}

int flash_erase_page(uint32_t addr)
{
    /* nunca apaga o proprio bootloader */
    if (addr < APP_BASE || addr >= flash_end()) return -1;
    flash_wait();
    FLASH_CR |= FLASH_CR_PER;
    FLASH_AR = addr;
    FLASH_CR |= FLASH_CR_STRT;
    int r = flash_wait();
    FLASH_CR &= ~FLASH_CR_PER;
    return r;
}

int flash_program(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (addr < APP_BASE || addr + len > flash_end() || (addr & 1) || (len & 1)) return -1;
    int r = 0;
    flash_wait();
    FLASH_CR |= FLASH_CR_PG;
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t hw = (uint16_t)(data[i] | (data[i + 1] << 8));
        volatile uint16_t *dst = (volatile uint16_t *)(addr + i);
        *dst = hw;
        if (flash_wait() || *dst != hw) { r = -1; break; }
    }
    FLASH_CR &= ~FLASH_CR_PG;
    return r;
}

/* ---------- salto para a aplicacao ---------- */

static void __attribute__((noreturn)) jump_to_app(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_BASE;
    uint32_t pc = *(volatile uint32_t *)(APP_BASE + 4);

    beeper_deinit();
    RCC_APB2ENR = 0;                              /* desliga clocks de GPIO (reset) */
    SYST_CSR = 0;
    SYST_RVR = 0;
    SYST_CVR = 0;
    clock_deinit();
    for (int i = 0; i < 8; i++) {
        NVIC_ICER(i) = 0xFFFFFFFFu;
        NVIC_ICPR(i) = 0xFFFFFFFFu;
    }
    SCB_VTOR = APP_BASE;
    __asm volatile ("dsb\n isb" ::: "memory");
    __asm volatile ("msr msp, %0\n bx %1" :: "r"(sp), "r"(pc));
    for (;;) { }
}

int main(void)
{
    clock_init();
    beeper_init();

    for (;;) {
        int r = update_from_sd();
        if (r == UPD_FLASHED) {
            beep(80); delay_ms(100); beep(80);
        } else if (r == UPD_ERROR) {
            beep(800);
        }
        if (app_is_valid()) jump_to_app();

        /* Sem firmware valido: bipa e tenta o cartao de novo */
        beep(300);
        delay_ms(3000);
    }
}

/* ---------- inicializacao ---------- */

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void)
{
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
    main();
    for (;;) { }
}

void Default_Handler(void)
{
    for (;;) { }
}

__attribute__((section(".isr_vector"), used))
static void (*const vectors[])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    Default_Handler,   /* NMI */
    Default_Handler,   /* HardFault */
    Default_Handler,   /* MemManage */
    Default_Handler,   /* BusFault */
    Default_Handler,   /* UsageFault */
};
