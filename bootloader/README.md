# Bootloader genérico por cartão SD (Ender 3 Neo, placa CR-FDM v4.2.2)

Arquivo pronto para gravar: [`binarios/bootloader-ender3neo.bin`](../binarios/bootloader-ender3neo.bin) (3,8 KB, gravar em `0x08000000`).

## Sobre o vídeo

O vídeo do Martin Zeman é para o Ender 3 antigo, com placa Creality V1.1.x e chip **ATmega1284P** (8 bits, AVR). Lá o bootloader é gravado com um USBasp pelo Arduino IDE ("Gravar bootloader", placa Sanguino) e depois o firmware TH3D vai pela USB.

A placa do Neo usa um chip **ARM (GD32F303)**, então esse caminho não se aplica: o Arduino IDE e o USBasp não falam com esse chip. O equivalente para a sua placa é gravar pelo **ST-Link**, que você já usa. Por isso este bootloader foi escrito do zero para a V4.2.2.

## O que ele faz

1. Liga, procura um arquivo `.bin` na raiz do cartão SD.
2. Confere se o arquivo é mesmo um firmware para `0x08007000` (pilha na RAM e endereço de início dentro da área da aplicação). Arquivos compilados para `0x08000000`, grandes demais ou lixo do macOS (`._arquivo.bin`) são ignorados.
3. Compara o arquivo com o que já está gravado. Se for igual, não faz nada. Se for diferente, apaga e grava a partir de `0x08007000` e confere de novo.
4. Inicia o firmware em `0x08007000`.

Diferenças em relação ao bootloader da Creality:

- **Não precisa renomear o arquivo** a cada atualização: qualquer nome serve, e o mesmo firmware não é regravado.
- **O cartão nunca é escrito** (o arquivo não vira `.CUR`). Pode deixar o `.bin` no cartão; deixe só um.
- Os 28 KB iniciais (o próprio bootloader) nunca são apagados por ele.

### Bipes (buzzer do display)

| Bipe | Significado |
|---|---|
| 1 curto | começou a gravar |
| 2 curtos | gravou e conferiu, vai iniciar o firmware |
| 1 longo | erro de leitura ou gravação |
| 1 bipe a cada 3 s | não há firmware válido; coloque o cartão com o `.bin` |

## Como gravar com o STM32 ST-LINK Utility

1. Ligue o ST-Link na placa como da outra vez (SWDIO, SWCLK, GND, 3.3V) e conecte (*Target → Connect*).
2. **Não** faça *Full chip erase*: o firmware que já está em `0x08007000` deve continuar lá.
3. *File → Open* → `bootloader-ender3neo.bin`.
4. *Target → Program & Verify*, endereço inicial **`0x08000000`**, marque *Reset after programming* e clique *Start*. Ele só apaga as 2 primeiras páginas (onde hoje está o "stub" de 1 KB).
5. Desligue o ST-Link e ligue a impressora: deve iniciar como antes (com o cartão contendo o mesmo firmware, demora uns décimos de segundo a mais porque ele compara o arquivo).

### Testando a atualização pelo cartão

- Cartão formatado em **FAT32** (cartões até 32 GB; exFAT não é suportado).
- Copie um firmware para o Neo na raiz (ex.: o `Neo-CFW-06.bin` oficial, ou um Marlin compilado com offset de 28 KB, que é o padrão do ambiente `STM32F103RE_creality`).
- Ligue a impressora: 1 bipe curto, alguns segundos, 2 bipes curtos e o firmware inicia.

### Se algo der errado

Nada fica sem volta: grave de novo pelo ST-Link o que estava antes (firmware em `0x08007000` e o stub de 1 KB em `0x08000000`).

## Situação dos testes

- A leitura de FAT16/FAT32 e a lógica de atualização foram testadas no PC com imagens de cartão (`make test`): nome longo, partição MBR, arquivo fragmentado, raiz grande, arquivo de tamanho ímpar, firmware errado, cartão sem FAT e troca de firmware.
- O driver do leitor de cartão (SDIO, modo 1 bit, polling) e a gravação da flash foram escritos a partir dos manuais do STM32F103/GD32F303, e ainda estão em validação na placa real.

## Detalhes técnicos

- Clock: HSI/2 × 16 = 64 MHz (não depende do cristal); antes de saltar volta para HSI 8 MHz, desliga SysTick, SDIO e GPIOs, e ajusta `VTOR = 0x08007000`.
- Cartão: SDIO em PC8–PC12 e PD2, 400 kHz na inicialização e 8 MHz na leitura. Suporta SDSC e SDHC.
- Flash: páginas de 2 KB; o primeiro setor do firmware (a tabela de vetores) é gravado por último, então uma falta de energia no meio deixa a aplicação inválida e ele tenta de novo no próximo boot.

## Compilar (opcional)

Precisa do `arm-none-eabi-gcc` (GNU Arm Embedded Toolchain) e `make`:

```
make          # gera build/bootloader.bin
make test     # testes no PC (Linux, precisa de mtools e dosfstools)
```

Código em `src/`: `main.c` (clock, flash, bipes, salto), `sdio.c` (cartão), `fat.c` (FAT16/32), `update.c` (lógica de atualização).
