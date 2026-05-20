#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "zed_opcodes.h"
#include "lib_tasarim.h"

#define BELLEK_BOYUTU 32768  // Bellek boyutu Mega seviyesine çıkarıldı (128 KB)
#define STACK_BOYUTU  2048   // Stack boyutu artırıldı
#define MAX_SEMBOL    2000
#define ZED_MAGIC     0x5A454400

typedef struct { char isim[64]; int adres; } Sembol;
Sembol degiskenler[MAX_SEMBOL], etiketler[MAX_SEMBOL];
int d_say = 0, e_say = 0, ip = 0, sp = 0;
int sanal_bellek[BELLEK_BOYUTU] = {0};
int ram[BELLEK_BOYUTU]          = {0};
int stack[STACK_BOYUTU];

FILE *dosya_okuma = NULL;
FILE *dosya_yazma = NULL;

int heap_baslangic = 16000, heap_isaretci = 16000;

const char* op_isim_bul(int op);

void hata(const char *msg) {
    printf("\033[0;31m\n========================================\033[0m\n");
    printf("\033[1;31m[ZEDIN MEGA AKILLI HATA RAPORU]\033[0m\n");
    printf("\033[0;31mHata Mesaji: %s\033[0m\n", msg);
    printf("\033[0;33mKomut Adresi (IP): %d\033[0m\n", ip);
    printf("\033[0;33mStack Durumu (SP): %d\033[0m\n", sp);
    printf("\033[0;36mSon Islem: %s\033[0m\n", op_isim_bul(sanal_bellek[ip]));
    
    printf("\n\033[1;32m[ONERI]:\033[0m ");
    if (strstr(msg, "Bellek erisim")) printf("Lutfen DATA adreslerini kontrol edin. Gecersiz bir RAM hucresine yazmaya calisiyorsunuz.\n");
    else if (strstr(msg, "Stack Overflow")) printf("Cok fazla PUSH veya ic ice CALL yaptiniz. Stack limitini astiniz.\n");
    else if (strstr(msg, "Sifira bolme")) printf("Matematiksel bir hata: Bir sayiyi 0'a bolemezsiniz.\n");
    else printf("Kodunuzdaki mantik akisini kontrol edin.\n");
    
    printf("\033[0;31m========================================\033[0m\n");

    if (dosya_okuma) fclose(dosya_okuma);
    if (dosya_yazma) fclose(dosya_yazma);
    exit(1);
}

void bellek_kontrol(int adres) {
    if (adres < 0 || adres >= BELLEK_BOYUTU) {
        hata("Bellek erisim hatasi (Segmentation Fault)");
    }
}

int komut_bul(char *ad) {
    for (int i = 0; i < ZED_KOMUT_SAYISI; i++)
        if (strcmp(ad, ZED_KOMUT_SETI[i].isim) == 0) return i;
    return -1;
}

const char* op_isim_bul(int op) {
    for (int i = 0; i < ZED_KOMUT_SAYISI; i++)
        if (ZED_KOMUT_SETI[i].op == op) return ZED_KOMUT_SETI[i].isim;
    return "BILINMEYEN";
}

int etiket_bul(char *ad) {
    for (int i = 0; i < e_say; i++)
        if (strcmp(etiketler[i].isim, ad) == 0) return etiketler[i].adres;
    return -1;
}

int degisken_al(char *ad) {
    for (int i = 0; i < d_say; i++)
        if (strcmp(degiskenler[i].isim, ad) == 0) return i;
    if (d_say >= MAX_SEMBOL) hata("Maksimum degisken sayisina ulasildi");
    strncpy(degiskenler[d_say].isim, ad, 63);
    return d_say++;
}

void ram_string_al(int adres, char *hedef, int max) {
    int i = 0;
    while (i < max - 1) {
        bellek_kontrol(adres + i);
        if (ram[adres + i] == 0) break;
        hedef[i] = (char)ram[adres + i];
        i++;
    }
    hedef[i] = '\0';
}

int bin_yukle(const char *dosya_adi) {
    FILE *bin = fopen(dosya_adi, "rb");
    if (!bin) return 0;
    int magic, kod_uzunlugu, ram_bas, ram_uzunlugu;
    fread(&magic, sizeof(int), 1, bin);
    fread(&kod_uzunlugu, sizeof(int), 1, bin);
    fread(&ram_bas, sizeof(int), 1, bin);
    fread(&ram_uzunlugu, sizeof(int), 1, bin);
    if (magic != ZED_MAGIC) { fclose(bin); return 0; }
    fread(sanal_bellek, sizeof(int), kod_uzunlugu, bin);
    fread(ram,          sizeof(int), ram_uzunlugu, bin);
    fclose(bin);
    return 1;
}

void calistir(int debug_mod, int baslangic_ip) {
    int ceb = 0, aktif = 1;
    ip = baslangic_ip; sp = 0;

    while (aktif) {
        if (ip < 0 || ip >= BELLEK_BOYUTU) hata("IP bellek disina cikti!");
        int op = sanal_bellek[ip];

        switch (op) {
            case DUR: aktif = 0; break;
            case YUKLE:      ceb = sanal_bellek[ip+1]; ip += 2; break;
            case SAKLA:      bellek_kontrol(sanal_bellek[ip+2]); ram[sanal_bellek[ip+2]] = sanal_bellek[ip+1]; ip += 3; break;
            case GETIR:      bellek_kontrol(sanal_bellek[ip+1]); ceb = ram[sanal_bellek[ip+1]]; ip += 2; break;
            case SAKLA_CEBI: bellek_kontrol(sanal_bellek[ip+1]); ram[sanal_bellek[ip+1]] = ceb; ip += 2; break;
            case TOPLA: ceb += sanal_bellek[ip+1]; ip += 2; break;
            case CIKAR: ceb -= sanal_bellek[ip+1]; ip += 2; break;
            case CARP:  ceb *= sanal_bellek[ip+1]; ip += 2; break;
            case BOL:
                if (sanal_bellek[ip+1] == 0) hata("Sifira bolme!");
                ceb /= sanal_bellek[ip+1]; ip += 2; break;
            case MOD:
                if (sanal_bellek[ip+1] == 0) hata("Sifira mod!");
                ceb %= sanal_bellek[ip+1]; ip += 2; break;
            case VE:    ceb &= sanal_bellek[ip+1]; ip += 2; break;
            case VEYA:  ceb |= sanal_bellek[ip+1]; ip += 2; break;
            case XOR:   ceb ^= sanal_bellek[ip+1]; ip += 2; break;
            case SOLA_KAYDIR: ceb <<= sanal_bellek[ip+1]; ip += 2; break;
            case SAGA_KAYDIR: ceb >>= sanal_bellek[ip+1]; ip += 2; break;
            case DEGIL: ceb = ~ceb; ip++; break;
            case GIT:          ip = sanal_bellek[ip+1]; break;
            case EGER_ESITSE:  bellek_kontrol(sanal_bellek[ip+1]); ip = (ceb == ram[sanal_bellek[ip+1]]) ? sanal_bellek[ip+2] : ip+3; break;
            case EGER_DEGILSE: bellek_kontrol(sanal_bellek[ip+1]); ip = (ceb != ram[sanal_bellek[ip+1]]) ? sanal_bellek[ip+2] : ip+3; break;
            case BUYUKSE:      bellek_kontrol(sanal_bellek[ip+1]); ip = (ceb >  ram[sanal_bellek[ip+1]]) ? sanal_bellek[ip+2] : ip+3; break;
            case KUCUKSE:      bellek_kontrol(sanal_bellek[ip+1]); ip = (ceb <  ram[sanal_bellek[ip+1]]) ? sanal_bellek[ip+2] : ip+3; break;
            case PUSH:
                if (sp >= STACK_BOYUTU) hata("Stack Overflow");
                stack[sp++] = sanal_bellek[ip+1]; ip += 2; break;
            case POP:
                if (sp <= 0) hata("Stack Underflow");
                ceb = stack[--sp]; ip++; break;
            case CALL:
                if (sp >= STACK_BOYUTU) hata("Stack Overflow (CALL)");
                stack[sp++] = ip + 2;
                ip = sanal_bellek[ip+1]; break;
            case RET:
                if (sp <= 0) hata("Stack Underflow (RET)");
                ip = stack[--sp]; break;
            case SAKLA_IND: {
                int adr = ram[sanal_bellek[ip+1]] + ram[sanal_bellek[ip+2]];
                bellek_kontrol(adr); ram[adr] = ceb; ip += 3; break;
            }
            case GETIR_IND: {
                int adr = ram[sanal_bellek[ip+1]] + ram[sanal_bellek[ip+2]];
                bellek_kontrol(adr); ceb = ram[adr]; ip += 3; break;
            }
            case PAKET_GUNCELLE: system("sudo apt update -y"); ip++; break;
            case PAKET_YUKLE: {
                char p[128]; ram_string_al(sanal_bellek[ip+1], p, 128);
                char k[256]; snprintf(k, 256, "sudo apt install -y %s", p);
                system(k); ip += 2; break;
            }
            case AG_ISTEK: { // Yeni: HTTP İstek (curl ile)
                char url[256]; ram_string_al(ceb, url, 256);
                char k[512]; snprintf(k, 512, "curl -s %s > zedin_ag_yanit.txt", url);
                system(k); ip += 2; break;
            }
            case GRAFIK_CIZ: { // Yeni: Grafik Çizme (Simülasyon)
                int x = sanal_bellek[ip+1];
                int y = sanal_bellek[ip+2];
                printf("\033[1;34m[GRAFIK] (%d, %d) konumuna nokta cizildi.\033[0m\n", x, y);
                ip += 3; break;
            }
            case LISTE_EKLE: { // Yeni: Dinamik Liste (Heap üzerinde)
                int liste_basi = sanal_bellek[ip+1];
                int deger = sanal_bellek[ip+2];
                int i = 0;
                while (ram[liste_basi + i] != 0) i++;
                ram[liste_basi + i] = deger;
                ram[liste_basi + i + 1] = 0;
                ip += 3; break;
            }
            case KESME: {
                int c = sanal_bellek[ip+1];
                char ds[256];
                switch (c) {
                    case 20: printf("%c", (char)ceb); fflush(stdout); break;
                    case 22: { int ptr = ceb; while (ptr >= 0 && ptr < BELLEK_BOYUTU && ram[ptr]) { printf("%c",(char)ram[ptr]); ptr++; } fflush(stdout); } break;
                    case 23: printf("%d", ceb); fflush(stdout); break;
                    case 70: ram_string_al(ceb, ds, 256); system(ds); break;
                }
                ip += 2; break;
            }
            default: ip++; break;
        }
    }
}

int main(int argc, char *argv[]) {
    zedin_banner();
    if (argc < 3) {
        printf("%sKullanım:%s %s -calistir <program.bin>\n\n", RENK_SARI, RENK_SIFIR, argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-calistir") == 0) {
        printf("%s[+]%s Program yükleniyor: %s%s%s\n", RENK_YESIL, RENK_SIFIR, RENK_TURKUAZ, argv[2], RENK_SIFIR);
        for(int i=0; i<=100; i+=20) { durum_cubugu("Yükleniyor", i); usleep(50000); }
        printf("\n");
        if (bin_yukle(argv[2])) {
            printf("%s[+]%s Çalıştırma başlatıldı...\n", RENK_YESIL, RENK_SIFIR);
            printf("%s──────────────────────────────────────────────────────────────────────────────%s\n", RENK_GRI, RENK_SIFIR);
            calistir(0, 0);
            printf("\n%s──────────────────────────────────────────────────────────────────────────────%s\n", RENK_GRI, RENK_SIFIR);
            printf("%s[✓]%s Program başarıyla tamamlandı.\n", RENK_YESIL, RENK_SIFIR);
        } else {
            printf("%s[!] HATA:%s Program dosyası yüklenemedi!\n", RENK_KIRMIZI, RENK_SIFIR);
        }
    }
    return 0;
}
