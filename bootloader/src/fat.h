/* Leitor FAT16/FAT32 minimo, somente leitura, so o diretorio raiz. */
#ifndef FAT_H
#define FAT_H
#include <stdint.h>

typedef struct {
    char     name[12];      /* nome 8.3 sem ponto, ex.: "FIRMWAREBIN" */
    uint32_t first_cluster;
    uint32_t size;
    /* estado de leitura sequencial */
    uint32_t cluster;
    uint32_t sector_in_cluster;
    uint32_t pos;
} fat_file_t;

/* Le um setor de 512 bytes do cartao. Retorna 0 em sucesso. */
int disk_read(uint32_t lba, uint8_t *buf);

int fat_mount(void);

/* Chamado para cada arquivo .BIN da raiz. Retorna 1 para aceitar. */
typedef int (*fat_filter_t)(fat_file_t *f);

/* Procura o primeiro .BIN da raiz aceito pelo filtro. 0 = achou. */
int fat_find_bin(fat_file_t *f, fat_filter_t filter);

/* Volta o arquivo para o inicio. */
void fat_rewind(fat_file_t *f);

/* Le o proximo setor do arquivo para buf (512 bytes).
   Retorna o numero de bytes validos (0 no fim), ou -1 em erro. */
int fat_read_sector(fat_file_t *f, uint8_t *buf);

#endif
