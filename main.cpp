#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// MPU6050 no I2C0
#define MPU_I2C i2c0
#define MPU_SDA 4
#define MPU_SCL 5
#define MPU_ADDR 0x68

// OLED SSD1306 no I2C1
#define OLED_I2C i2c1
#define OLED_SDA 14
#define OLED_SCL 15
#define OLED_ADDR 0x3C

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUF_SIZE 1024

static uint8_t oled_buffer[OLED_BUF_SIZE];
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static char ultima_classe[16] = "---";
static float ultima_confianca = 0.0f;

// ---------- Edge Impulse porting mínimo para Pico SDK ----------

extern "C" void ei_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

extern "C" void ei_printf_float(float f) {
    printf("%f", f);
}

extern "C" void ei_putchar(char c) {
    putchar(c);
}

extern "C" void ei_puts(const char *s) {
    printf("%s", s);
}

// ---------- I2C ----------

void i2c_setup() {
    i2c_init(MPU_I2C, 400 * 1000);
    gpio_set_function(MPU_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPU_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPU_SDA);
    gpio_pull_up(MPU_SCL);

    i2c_init(OLED_I2C, 400 * 1000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);
}

// ---------- MPU6050 ----------

void mpu_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(MPU_I2C, MPU_ADDR, data, 2, false);
}

void mpu_read(uint8_t reg, uint8_t *buffer, uint8_t len) {
    i2c_write_blocking(MPU_I2C, MPU_ADDR, &reg, 1, true);
    i2c_read_blocking(MPU_I2C, MPU_ADDR, buffer, len, false);
}

void mpu_init() {
    mpu_write(0x6B, 0x00);
    sleep_ms(100);
}

int16_t convert_data(uint8_t high, uint8_t low) {
    return (int16_t)((high << 8) | low);
}

// ---------- OLED ----------

void oled_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    i2c_write_blocking(OLED_I2C, OLED_ADDR, data, 2, false);
}

void oled_init() {
    sleep_ms(100);

    uint8_t cmds[] = {
        0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
        0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
        0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
        0x40, 0x8D, 0x14, 0xAF
    };

    for (size_t i = 0; i < sizeof(cmds); i++) {
        oled_cmd(cmds[i]);
    }
}

void oled_clear() {
    memset(oled_buffer, 0, OLED_BUF_SIZE);
}

void oled_update() {
    oled_cmd(0x21);
    oled_cmd(0);
    oled_cmd(127);

    oled_cmd(0x22);
    oled_cmd(0);
    oled_cmd(7);

    uint8_t data[OLED_BUF_SIZE + 1];
    data[0] = 0x40;
    memcpy(&data[1], oled_buffer, OLED_BUF_SIZE);

    i2c_write_blocking(OLED_I2C, OLED_ADDR, data, OLED_BUF_SIZE + 1, false);
}

void oled_pixel(int x, int y) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    oled_buffer[x + (y / 8) * OLED_WIDTH] |= 1 << (y % 8);
}

void font5x7(char c, uint8_t out[5]) {
    memset(out, 0, 5);

    static const uint8_t c0[5] = {0x3E,0x51,0x49,0x45,0x3E};
    static const uint8_t c1[5] = {0x00,0x42,0x7F,0x40,0x00};
    static const uint8_t c2[5] = {0x42,0x61,0x51,0x49,0x46};
    static const uint8_t c3[5] = {0x21,0x41,0x45,0x4B,0x31};
    static const uint8_t c4[5] = {0x18,0x14,0x12,0x7F,0x10};
    static const uint8_t c5[5] = {0x27,0x45,0x45,0x45,0x39};
    static const uint8_t c6[5] = {0x3C,0x4A,0x49,0x49,0x30};
    static const uint8_t c7[5] = {0x01,0x71,0x09,0x05,0x03};
    static const uint8_t c8[5] = {0x36,0x49,0x49,0x49,0x36};
    static const uint8_t c9[5] = {0x06,0x49,0x49,0x29,0x1E};
    static const uint8_t A[5]  = {0x7E,0x11,0x11,0x11,0x7E};
    static const uint8_t F[5]  = {0x7F,0x09,0x09,0x09,0x01};
    static const uint8_t G[5]  = {0x3E,0x41,0x49,0x49,0x7A};
    static const uint8_t H[5]  = {0x7F,0x08,0x08,0x08,0x7F};
    static const uint8_t K[5]  = {0x7F,0x08,0x14,0x22,0x41};
    static const uint8_t L[5]  = {0x7F,0x40,0x40,0x40,0x40};
    static const uint8_t M[5]  = {0x7F,0x02,0x0C,0x02,0x7F};
    static const uint8_t O[5]  = {0x3E,0x41,0x41,0x41,0x3E};
    static const uint8_t X[5]  = {0x63,0x14,0x08,0x14,0x63};
    static const uint8_t Y[5]  = {0x07,0x08,0x70,0x08,0x07};
    static const uint8_t Z[5]  = {0x61,0x51,0x49,0x45,0x43};
    static const uint8_t colon[5] = {0x00,0x36,0x36,0x00,0x00};
    static const uint8_t dash[5]  = {0x08,0x08,0x08,0x08,0x08};
    static const uint8_t dot[5]   = {0x00,0x60,0x60,0x00,0x00};
    static const uint8_t pct[5]   = {0x62,0x64,0x08,0x13,0x23};
    static const uint8_t sp[5]    = {0x00,0x00,0x00,0x00,0x00};

    const uint8_t *src = sp;
    switch (c) {
        case '0': src = c0; break; case '1': src = c1; break; case '2': src = c2; break;
        case '3': src = c3; break; case '4': src = c4; break; case '5': src = c5; break;
        case '6': src = c6; break; case '7': src = c7; break; case '8': src = c8; break;
        case '9': src = c9; break; case 'A': src = A; break; case 'F': src = F; break;
        case 'G': src = G; break; case 'H': src = H; break; case 'K': src = K; break;
        case 'L': src = L; break; case 'M': src = M; break; case 'O': src = O; break;
        case 'X': src = X; break; case 'Y': src = Y; break; case 'Z': src = Z; break;
        case ':': src = colon; break; case '-': src = dash; break; case '.': src = dot; break;
        case '%': src = pct; break; case ' ': src = sp; break;
    }
    memcpy(out, src, 5);
}

void oled_char(int x, int y, char c) {
    uint8_t columns[5];
    font5x7(c, columns);

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (columns[col] & (1 << row)) oled_pixel(x + col, y + row);
        }
    }
}

void oled_text(int x, int y, const char *text) {
    while (*text) {
        oled_char(x, y, *text);
        x += 6;
        text++;
    }
}

// ---------- Edge Impulse: janela sintética para Wokwi ----------
// O modelo foi treinado com accX, accY, accZ em 937 Hz e janela de ~2 s.
// No Wokwi, o MPU6050 não gera vibração mecânica real, então alternamos OK/FALHA
// para demonstrar o modelo incorporado ao firmware e a chamada real a run_classifier().

static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void gerar_janela_ok() {
    const float pi = 3.14159265f;
    for (size_t i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
        float t = (float)i / (float)EI_CLASSIFIER_FREQUENCY;
        features[(i * 3) + 0] = 0.12f + 0.015f * sinf(2.0f * pi * 6.0f * t);
        features[(i * 3) + 1] = 0.00f + 0.015f * cosf(2.0f * pi * 7.0f * t);
        features[(i * 3) + 2] = 1.03f + 0.020f * sinf(2.0f * pi * 5.0f * t);
    }
}

void gerar_janela_falha() {
    const float pi = 3.14159265f;
    for (size_t i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
        float t = (float)i / (float)EI_CLASSIFIER_FREQUENCY;
        features[(i * 3) + 0] = 1.05f + 0.10f * sinf(2.0f * pi * 4.0f * t);
        features[(i * 3) + 1] = 1.20f * sinf(2.0f * pi * 55.0f * t);
        features[(i * 3) + 2] = 1.20f * cosf(2.0f * pi * 55.0f * t);
    }
}

void executar_inferencia(bool simular_falha) {
    if (simular_falha) gerar_janela_falha();
    else gerar_janela_ok();

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &get_signal_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK) {
        printf("Erro na inferencia Edge Impulse: %d\n", res);
        snprintf(ultima_classe, sizeof(ultima_classe), "ERRO");
        ultima_confianca = 0.0f;
        return;
    }

    float melhor_valor = 0.0f;
    const char *melhor_label = "---";

    printf("\n--- Inferencia Edge Impulse ---\n");
    printf("Janela simulada: %s\n", simular_falha ? "falha" : "OK");

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        const char *label = result.classification[ix].label;
        float value = result.classification[ix].value;
        printf("%s: %.4f\n", label, value);
        if (value > melhor_valor) {
            melhor_valor = value;
            melhor_label = label;
        }
    }

    snprintf(ultima_classe, sizeof(ultima_classe), "%s", melhor_label);
    ultima_confianca = melhor_valor;

    printf("Classe prevista: %s (%.2f%%)\n", ultima_classe, ultima_confianca * 100.0f);
}

// ---------- Programa principal ----------

int main() {
    stdio_init_all();
    sleep_ms(1000);

    i2c_setup();
    mpu_init();
    oled_init();

    uint8_t data[14];
    char linha[24];
    uint32_t contador = 0;

    while (true) {
        mpu_read(0x3B, data, 14);

        int16_t ax = convert_data(data[0], data[1]);
        int16_t ay = convert_data(data[2], data[3]);
        int16_t az = convert_data(data[4], data[5]);

        // Alterna o cenário a cada execução para demonstrar OK e FALHA no Wokwi.
        bool simular_falha = (contador % 2) == 1;
        executar_inferencia(simular_falha);
        contador++;

        printf("Acelerometro MPU6050: X=%d Y=%d Z=%d\n", ax, ay, az);

        oled_clear();

        snprintf(linha, sizeof(linha), "AX:%6d", ax);
        oled_text(0, 0, linha);

        snprintf(linha, sizeof(linha), "AY:%6d", ay);
        oled_text(0, 10, linha);

        snprintf(linha, sizeof(linha), "AZ:%6d", az);
        oled_text(0, 20, linha);

        oled_text(0, 36, "ML:");
        oled_text(24, 36, ultima_classe);

        int conf = (int)(ultima_confianca * 100.0f + 0.5f);
        snprintf(linha, sizeof(linha), "CONF:%3d%%", conf);
        oled_text(0, 50, linha);

        oled_update();

        sleep_ms(2000);
    }
}
