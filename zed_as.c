#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zed_opcodes.h"
#include "lib_tasarim.h"

#define BELLEK_BOYUTU 16384
#define MAX_SEMBOL    1000

typedef struct { char isim[64]; int adres; } Sembol;
Sembol etiketler[MAX_SEMBOL];
Sembol degiskenler[MAX_SEMBOL];
int e_say = 0, d_say = 0;
int sanal_bellek[BELLEK_BOYUTU];
int ram[BELLEK_BOYUTU];
int kod_uzunlugu = 0;

int komut_bul(char *ad) {
    for (int i = 0; i < ZED_KOMUT_SAYISI; i++)
        if (strcmp(ad, ZED_KOMUT_SETI[i].isim) == 0) return i;
    return -1;
}

int etiket_bul(char *ad) {
    for (int i = 0; i < e_say; i++)
        if (strcmp(etiketler[i].isim, ad) == 0) return etiketler[i].adres;
    return -1;
}

int degisken_al(char *ad) {
    for (int i = 0; i < d_say; i++)
        if (strcmp(degiskenler[i].isim, ad) == 0) return i;
    if (d_say >= MAX_SEMBOL) { printf("HATA: Cok fazla degisken!\n"); exit(1); }
    strncpy(degiskenler[d_say].isim, ad, 63);
    return d_say++;
}

void ram_string_yaz(int adres, const char *metin) {
    int i = 0, j = 0;
    while (metin[i] != '\0') {
        if (metin[i] != '"' && metin[i] != ',') {
            if (adres + j < BELLEK_BOYUTU)
                ram[adres + j++] = (int)metin[i];
        }
        i++;
    }
    if (adres + j < BELLEK_BOYUTU) ram[adres + j] = 0;
}

void hata(const char *mesaj, int satir) {
    printf("\033[0;31m[HATA] Satir %d: %s\033[0m\n", satir, mesaj);
    exit(1);
}

void gecis_1(FILE *f) {
    char m[128];
    int pos = 0, satir = 0;
    while (fscanf(f, "%127s", m) != EOF) {
        satir++;
        if (m[0] == ';') { fgets(m, sizeof(m), f); continue; }
        if (strcmp(m, "#include") == 0) {
            char dosya[128];
            fscanf(f, "%s", dosya);
            char temiz[128];
            int len = strlen(dosya);
            int start = (dosya[0] == '"') ? 1 : 0;
            int end = (dosya[len-1] == '"') ? len-1 : len;
            strncpy(temiz, dosya + start, end - start);
            temiz[end - start] = '\0';
            FILE *inc = fopen(temiz, "r");
            if (inc) { gecis_1(inc); fclose(inc); }
            else { printf("HATA: Include dosyasi bulunamadi: %s\n", temiz); }
            continue;
        }
        if (m[0] == '@') {
            if (e_say >= MAX_SEMBOL) hata("Cok fazla etiket!", satir);
            strncpy(etiketler[e_say].isim, m + 1, 63);
            etiketler[e_say++].adres = pos;
            continue;
        }
        if (strcmp(m, "DATA") == 0) {
            int adr; char val[256];
            fscanf(f, "%d", &adr);
            // Stringi tırnak işaretleri ile oku
            char c;
            while ((c = fgetc(f)) != EOF && c != '"'); // İlk tırnağı atla
            int i = 0;
            while ((c = fgetc(f)) != EOF && c != '"' && i < 255) {
                val[i++] = c;
            }
            val[i] = '\0';
            continue;
        }
        int k = komut_bul(m);
        if (k != -1) pos += ZED_KOMUT_SETI[k].boyut;
    }
}

void gecis_2(FILE *f) {
    char m[128];
    int yaz = 0, satir = 0;
    while (fscanf(f, "%127s", m) != EOF) {
        satir++;
        if (m[0] == ';') { fgets(m, sizeof(m), f); continue; }
        if (strcmp(m, "#include") == 0) {
            char dosya[128];
            fscanf(f, "%s", dosya);
            char temiz[128];
            int len = strlen(dosya);
            int start = (dosya[0] == '"') ? 1 : 0;
            int end = (dosya[len-1] == '"') ? len-1 : len;
            strncpy(temiz, dosya + start, end - start);
            temiz[end - start] = '\0';
            FILE *inc = fopen(temiz, "r");
            if (inc) { gecis_2(inc); fclose(inc); }
            continue;
        }
        if (m[0] == '@') continue;
        if (strcmp(m, "DATA") == 0) {
            int hedef; char metin[256];
            fscanf(f, "%d", &hedef);
            // Stringi tırnak işaretleri ile oku
            char c;
            while ((c = fgetc(f)) != EOF && c != '"'); // İlk tırnağı atla
            int i = 0;
            while ((c = fgetc(f)) != EOF && c != '"' && i < 255) {
                metin[i++] = c;
            }
            metin[i] = '\0';
            ram_string_yaz(hedef, metin);
            continue;
        }
        int k = komut_bul(m);
        if (k == -1) {
            printf("\033[0;33m[UYARI] Satir %d: Bilinmeyen komut '%s'\033[0m\n", satir, m);
            continue;
        }
        if (yaz >= BELLEK_BOYUTU) hata("Kod bellek boyutunu asti!", satir);
        sanal_bellek[yaz++] = ZED_KOMUT_SETI[k].op;
        for (int i = 0; i < ZED_KOMUT_SETI[k].boyut - 1; i++) {
            if (fscanf(f, "%127s", m) == EOF) break;
            if (yaz >= BELLEK_BOYUTU) hata("Kod bellek boyutunu asti!", satir);
            if (m[0] == '#')
                sanal_bellek[yaz++] = atoi(m + 1);
            else if (m[0] == '$') {
                int adres = etiket_bul(m + 1);
                if (adres == -1) hata("Bilinmeyen etiket!", satir);
                sanal_bellek[yaz++] = adres;
            }
            else if ((m[0] >= '0' && m[0] <= '9') || m[0] == '-')
                sanal_bellek[yaz++] = atoi(m);
            else
                sanal_bellek[yaz++] = degisken_al(m);
        }
    }
    kod_uzunlugu = yaz;
}

void bin_yaz(const char *hedef) {
    int ram_son = 8000;
    for (int i = BELLEK_BOYUTU - 1; i >= 0; i--) {
        if (ram[i] != 0) { ram_son = i + 1; break; }
    }
    if (ram_son < 8000) ram_son = 8000;
    FILE *bin = fopen(hedef, "wb");
    if (!bin) { printf("%s[!] HATA:%s %s yazılamadı!\n", RENK_KIRMIZI, RENK_SIFIR, hedef); exit(1); }
    int magic = ZED_MAGIC, ram_bas = 0;
    fwrite(&magic,        sizeof(int), 1,            bin);
    fwrite(&kod_uzunlugu, sizeof(int), 1,            bin);
    fwrite(&ram_bas,      sizeof(int), 1,            bin);
    fwrite(&ram_son,      sizeof(int), 1,            bin);
    fwrite(sanal_bellek,  sizeof(int), kod_uzunlugu, bin);
    fwrite(ram,           sizeof(int), ram_son,      bin);
    fclose(bin);
    printf("%s[✓] Derleme Başarılı:%s %s%s%s\n", RENK_YESIL, RENK_SIFIR, RENK_TURKUAZ, hedef, RENK_SIFIR);
}

int main(int argc, char *argv[]) {
    zedin_banner();
    if (argc < 2) { printf("%sKullanım:%s zed_as <kaynak.zed> [cikti.bin]\n", RENK_SARI, RENK_SIFIR); return 1; }
    const char *kaynak = argv[1];
    const char *hedef  = (argc >= 3) ? argv[2] : "program.bin";
    printf("%s[*]%s Kaynak dosya okunuyor: %s%s%s\n", RENK_MAVI, RENK_SIFIR, RENK_TURKUAZ, kaynak, RENK_SIFIR);
    FILE *f = fopen(kaynak, "r");
    if (!f) { printf("%s[!] HATA:%s %s bulunamadı!\n", RENK_KIRMIZI, RENK_SIFIR, kaynak); return 1; }
    
    durum_cubugu("Analiz", 30);
    gecis_1(f); rewind(f);
    durum_cubugu("Derleme", 70);
    gecis_2(f); fclose(f);
    durum_cubugu("Tamamlandı", 100);
    printf("\n");
    
    bin_yaz(hedef);
    return 0;
}
