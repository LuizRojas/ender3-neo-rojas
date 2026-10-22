/* Leitor FAT16/FAT32 minimo, somente leitura, so o diretorio raiz. */
#include "fat.h"
#include <string.h>

static uint32_t vol_lba;        /* inicio da particao */
static uint32_t fat_lba;        /* inicio da 1a FAT */
static uint32_t root_lba;       /* FAT16: inicio da raiz fixa */
static uint32_t root_sectors;   /* FAT16: tamanho da raiz fixa */
static uint32_t root_cluster;   /* FAT32: cluster da raiz */
static uint32_t data_lba;       /* inicio da area de dados (cluster 2) */
static uint32_t spc;            /* setores por cluster */
static uint32_t max_cluster;
static int      is_fat32;

static uint32_t sbuf[128];      /* setor de diretorio / boot */
static uint32_t fbuf[128];      /* cache de um setor da FAT */
static uint32_t fbuf_lba = 0xFFFFFFFFu;

static uint16_t ld16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t ld32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 0 = VBR FAT, 1 = outro setor valido (MBR?), 2 = invalido */
static int check_vbr(const uint8_t *b)
{
    if (ld16(b + 510) != 0xAA55) return 2;
    if ((b[0] == 0xEB || b[0] == 0xE9 || b[0] == 0xE8) && ld16(b + 11) == 512) {
        if (!memcmp(b + 0x36, "FAT", 3) || !memcmp(b + 0x52, "FAT32", 5)) return 0;
    }
    return 1;
}

int fat_mount(void)
{
    uint8_t *b = (uint8_t *)sbuf;
    fbuf_lba = 0xFFFFFFFFu;
    vol_lba = 0;
    if (disk_read(0, b)) return -1;
    int r = check_vbr(b);
    if (r == 2) return -2;
    if (r == 1) {
        /* MBR: usa a primeira particao com tipo e inicio validos */
        int i;
        for (i = 0; i < 4; i++) {
            const uint8_t *pe = b + 446 + 16 * i;
            if (pe[4] != 0 && ld32(pe + 8) != 0) { vol_lba = ld32(pe + 8); break; }
        }
        if (i == 4) return -3;
        if (disk_read(vol_lba, b)) return -1;
        if (check_vbr(b) != 0) return -4;
    }

    spc = b[13];
    uint32_t rsvd   = ld16(b + 14);
    uint32_t nfats  = b[16];
    uint32_t nroot  = ld16(b + 17);
    uint32_t totsec = ld16(b + 19) ? ld16(b + 19) : ld32(b + 32);
    uint32_t fatsz  = ld16(b + 22) ? ld16(b + 22) : ld32(b + 36);
    if (spc == 0 || (spc & (spc - 1)) || rsvd == 0 || nfats == 0 || fatsz == 0) return -5;

    root_sectors = (nroot * 32 + 511) / 512;
    fat_lba  = vol_lba + rsvd;
    root_lba = fat_lba + nfats * fatsz;
    data_lba = root_lba + root_sectors;
    uint32_t meta = rsvd + nfats * fatsz + root_sectors;
    if (totsec <= meta) return -5;
    uint32_t nclus = (totsec - meta) / spc;
    if (nclus < 4085) return -6;            /* FAT12 nao suportado */
    is_fat32 = nclus >= 65525;
    if (is_fat32) {
        if (nroot != 0) return -5;
        root_cluster = ld32(b + 44);
    }
    max_cluster = nclus + 1;
    return 0;
}

static uint32_t clus2lba(uint32_t c) { return data_lba + (c - 2) * spc; }

/* Retorna o proximo cluster, 0xFFFFFFFF no fim da cadeia ou 0 em erro. */
static uint32_t fat_next(uint32_t c)
{
    uint32_t off = is_fat32 ? c * 4 : c * 2;
    uint32_t lba = fat_lba + off / 512;
    if (lba != fbuf_lba) {
        if (disk_read(lba, (uint8_t *)fbuf)) { fbuf_lba = 0xFFFFFFFFu; return 0; }
        fbuf_lba = lba;
    }
    const uint8_t *p = (const uint8_t *)fbuf + (off % 512);
    uint32_t n;
    if (is_fat32) {
        n = ld32(p) & 0x0FFFFFFFu;
        if (n >= 0x0FFFFFF8u) return 0xFFFFFFFFu;
    } else {
        n = ld16(p);
        if (n >= 0xFFF8u) return 0xFFFFFFFFu;
    }
    if (n < 2 || n > max_cluster) return 0;
    return n;
}

int fat_find_bin(fat_file_t *f, fat_filter_t filter)
{
    uint8_t *b = (uint8_t *)sbuf;
    uint32_t clus = root_cluster, sec = 0, idx = 0;
    /* Limite de seguranca contra cadeias em loop */
    for (uint32_t guard = 0; guard < 65536; guard++) {
        uint32_t lba;
        if (is_fat32) {
            if (sec == spc) {
                clus = fat_next(clus);
                if (clus == 0xFFFFFFFFu) return -1;
                if (clus == 0) return -2;
                sec = 0;
            }
            lba = clus2lba(clus) + sec;
        } else {
            if (idx >= root_sectors) return -1;
            lba = root_lba + idx;
        }
        if (disk_read(lba, b)) return -2;
        for (int e = 0; e < 16; e++) {
            const uint8_t *d = b + 32 * e;
            if (d[0] == 0x00) return -1;            /* fim do diretorio */
            if (d[0] == 0xE5) continue;             /* apagado */
            uint8_t attr = d[11];
            if ((attr & 0x0F) == 0x0F) continue;    /* entrada LFN */
            if (attr & (0x02 | 0x04 | 0x08 | 0x10)) continue; /* oculto, sistema, volume, pasta */
            if (memcmp(d + 8, "BIN", 3)) continue;
            if (d[0] == '.' || d[0] == '_') continue; /* lixo do macOS (._xxx) */

            memcpy(f->name, d, 11);
            f->name[11] = 0;
            f->first_cluster = ((uint32_t)ld16(d + 20) << 16) | ld16(d + 26);
            if (!is_fat32) f->first_cluster &= 0xFFFFu;
            f->size = ld32(d + 28);
            if (f->size == 0 || f->first_cluster < 2 || f->first_cluster > max_cluster) continue;
            fat_rewind(f);
            int ok = filter ? filter(f) : 1;
            /* o filtro pode ter lido o arquivo; recarrega o setor do diretorio */
            if (ok) { fat_rewind(f); return 0; }
            if (disk_read(lba, b)) return -2;
        }
        if (is_fat32) sec++; else idx++;
    }
    return -2;
}

void fat_rewind(fat_file_t *f)
{
    f->cluster = f->first_cluster;
    f->sector_in_cluster = 0;
    f->pos = 0;
}

int fat_read_sector(fat_file_t *f, uint8_t *buf)
{
    if (f->pos >= f->size) return 0;
    if (f->sector_in_cluster == spc) {
        uint32_t n = fat_next(f->cluster);
        if (n == 0 || n == 0xFFFFFFFFu) return -1;  /* cadeia menor que o tamanho */
        f->cluster = n;
        f->sector_in_cluster = 0;
    }
    if (disk_read(clus2lba(f->cluster) + f->sector_in_cluster, buf)) return -1;
    f->sector_in_cluster++;
    uint32_t left = f->size - f->pos;
    uint32_t n = left < 512 ? left : 512;
    f->pos += n;
    return (int)n;
}
