#!/bin/sh
# Gera imagens de cartao com mtools e roda o teste em varias situacoes.
set -e
B=build; T=$B/t; rm -rf $T; mkdir -p $T
NO_CARD=1; NO_FILE=2; SAME=3; FLASHED=4
run() { printf '%-48s ' "$1"; shift; $B/host_test "$@"; }

# firmware sintetico: SP=0x20010000, reset=0x08009261, resto aleatorio
mkfw() { python3 - "$1" "$2" "$3" <<'PY'
import sys, os, struct
path, size, pc = sys.argv[1], int(sys.argv[2]), int(sys.argv[3], 16)
open(path, "wb").write(struct.pack("<II", 0x20010000, pc) + os.urandom(size - 8))
PY
}
mkfw $T/fw.bin 245761 0x08009261          # tamanho impar
mkfw $T/fw2.bin 200000 0x08009261
mkfw $T/offset0.bin 100000 0x08000261     # compilado para 0x08000000
mkfw $T/huge.bin 500000 0x08009261        # maior que 484 KB
head -c 4096 /dev/urandom > $T/junk.bin

# 1) FAT32 sem particao, nome longo, outros arquivos antes
dd if=/dev/zero of=$T/a.img bs=1M count=64 status=none
mkfs.fat -F 32 $T/a.img >/dev/null
mcopy -i $T/a.img $T/junk.bin ::/._Ender3Neo.bin
mcopy -i $T/a.img $T/offset0.bin ::/marlin-offset0.bin
mmd -i $T/a.img ::/pasta.bin
mcopy -i $T/a.img $T/fw.bin "::/Ender-3 Neo firmware v1.bin"
run "FAT32, nome longo, lixo antes" $T/a.img $FLASHED $T/fw.bin
run "FAT32, mesmo firmware de novo" $T/a.img $SAME $T/fw.bin $B/flash_after.bin

# 2) FAT16 dentro de uma particao MBR, arquivo fragmentado
dd if=/dev/zero of=$T/b.img bs=1M count=64 status=none
python3 - $T/b.img <<'PY2'
import sys, struct
f = open(sys.argv[1], "r+b"); f.seek(446)
f.write(bytes([0, 0, 0, 0, 6, 0, 0, 0]) + struct.pack("<II", 2048, 129024)); f.seek(510); f.write(b"\x55\xaa")
PY2
mkfs.fat --offset 2048 -F 16 -s 4 $T/b.img 64512 >/dev/null
head -c 30000 /dev/urandom > $T/s1; head -c 60000 /dev/urandom > $T/s2; head -c 30000 /dev/urandom > $T/s3
mcopy -i $T/b.img@@1M $T/s1 ::/A.TXT; mcopy -i $T/b.img@@1M $T/s2 ::/B.TXT; mcopy -i $T/b.img@@1M $T/s3 ::/C.TXT
mdel -i $T/b.img@@1M ::/B.TXT
mcopy -i $T/b.img@@1M $T/fw2.bin ::/FIRMWARE.BIN
run "FAT16 em particao MBR, fragmentado" $T/b.img $FLASHED $T/fw2.bin
# troca de firmware: flash ja tem fw.bin de outro teste
$B/host_test $T/a.img $FLASHED $T/fw.bin >/dev/null
run "FAT16, substitui firmware maior por menor" $T/b.img $FLASHED $T/fw2.bin $B/flash_after.bin

# 3) so arquivos invalidos
dd if=/dev/zero of=$T/c.img bs=1M count=64 status=none
mkfs.fat -F 32 $T/c.img >/dev/null
mcopy -i $T/c.img $T/offset0.bin ::/firmware.bin
mcopy -i $T/c.img $T/huge.bin ::/huge.bin
run "so firmware para 0x08000000 ou grande demais" $T/c.img $NO_FILE

# 4) sem cartao / cartao sem FAT
run "sem cartao" - $NO_CARD
dd if=/dev/urandom of=$T/d.img bs=1M count=1 status=none
run "cartao sem sistema de arquivos" $T/d.img $NO_CARD

# 5) FAT32 com raiz ocupando mais de um cluster (muitos arquivos)
dd if=/dev/zero of=$T/e.img bs=1M count=64 status=none
mkfs.fat -F 32 -s 1 $T/e.img >/dev/null
for i in $(seq 1 60); do mcopy -i $T/e.img $T/s1 "::/arquivo com nome comprido numero $i.txt"; done
mcopy -i $T/e.img $T/fw.bin ::/z.bin
run "FAT32, raiz em varios clusters" $T/e.img $FLASHED $T/fw.bin
echo "todos os testes passaram"
