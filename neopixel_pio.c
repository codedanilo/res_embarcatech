#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7
#define BTN_A_PIN 5
#define BTN_B_PIN 6
#define LED_R_PIN 13
#define LED_G_PIN 11
#define LED_B_PIN 12

// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Variáveis de estado
volatile int numero = 0;
volatile bool btn_a_pressed = false;
volatile bool btn_b_pressed = false;
volatile uint64_t last_btn_a_time = 0;
volatile uint64_t last_btn_b_time = 0;

// Função de debouncing
#define DEBOUNCE_TIME 200000 // 200ms de debounce (em microssegundos)
void debounce_button(uint gpio, volatile bool *button_flag, volatile uint64_t *last_time) {
    uint64_t current_time = time_us_32();
    if (current_time - *last_time > DEBOUNCE_TIME) {
        *button_flag = true;
        *last_time = current_time;
    }
}

// Função do LED RGB piscando
void led_rgb_blink() {
    static bool red_on = false;
    if (red_on) {
        gpio_put(LED_R_PIN, 1);
    } else {
        gpio_put(LED_R_PIN, 0);
    }
    red_on = !red_on;
}

void npInit(uint pin) {
    printf("Inicializando a matriz de LEDs\n"); // Print de inicialização dos LEDs
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }

    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
    printf("Matriz de LEDs inicializada\n"); // Print de confirmação
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

int getIndex(int x, int y) {
    if (y % 2 == 0) {
        return 24 - (y * 5 + x);
    } else {
        return 24 - (y * 5 + (4 - x));
    }
}

// Função para exibir um número na matriz de LEDs
void display_number(int num) {
    int matriz [2][5][5][3] = {
        // Número 0
        {
            {{0, 0, 0}, {243, 7, 7}, {243, 7, 7}, {243, 7, 7}, {0, 0, 0}},
            {{243, 7, 7}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {243, 7, 7}},
            {{243, 7, 7}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {243, 7, 7}},
            {{243, 7, 7}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {243, 7, 7}},
            {{0, 0, 0}, {243, 7, 7}, {243, 7, 7}, {243, 7, 7}, {0, 0, 0}}
        },
        // Número 1
        {
            {{0, 0, 0}, {0, 0, 0}, {243, 7, 7}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {243, 7, 7}, {243, 7, 7}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {243, 7, 7}, {243, 7, 7}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {243, 7, 7}, {243, 7, 7}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {243, 7, 7}, {243, 7, 7}, {0, 0, 0}, {0, 0, 0}}
        },
    };

    printf("Exibindo o número: %d\n", num); // Print para indicar qual número está sendo exibido.
    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            int posicao = getIndex(linha, coluna);
            printf("Posição %d (linha %d, coluna %d) - RGB(%d, %d, %d)\n", 
                posicao, linha, coluna, 
                matriz[num][coluna][linha][0], matriz[num][coluna][linha][1], matriz[num][coluna][linha][2]); // Print de cada LED
            npSetLED(posicao, matriz[num][coluna][linha][0], matriz[num][coluna][linha][1], matriz[num][coluna][linha][2]);
        }
    }
    npWrite();
}

void button_a_isr(uint gpio, uint32_t events) {
    printf("Interrupt A acionado!\n"); // Print para verificar se a interrupção está funcionando
    debounce_button(gpio, &btn_a_pressed, &last_btn_a_time);
}

void button_b_isr(uint gpio, uint32_t events) {
    printf("Interrupt B acionado!\n"); // Print para verificar se a interrupção está funcionando
    debounce_button(gpio, &btn_b_pressed, &last_btn_b_time);
}

int main() {
    stdio_init_all();
    printf("Sistema inicializado. Teste de impressão.\n");
    npInit(LED_PIN);
    npClear();

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_a_isr);
    printf("Botão A configurado para interrupção\n");

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
    gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true, &button_b_isr);
    printf("Botão B configurado para interrupção\n");

    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);

    // Main loop
    while (true) {
        // Pisca o LED RGB vermelho
        led_rgb_blink();
        sleep_ms(200); // Intervalo para piscar 5 vezes por segundo

        // Verifica se os botões foram pressionados
        if (btn_a_pressed) {
            btn_a_pressed = false;
            if (numero < 9) numero++;
            printf("Botão A pressionado. Número incrementado: %d\n", numero); // Print para verificar o número após o botão A ser pressionado.
            display_number(numero);
        }

        if (btn_b_pressed) {
            btn_b_pressed = false;
            if (numero > 0) numero--;
            printf("Botão B pressionado. Número decrementado: %d\n", numero); // Print para verificar o número após o botão B ser pressionado.
            display_number(numero);
        }

        // Delay para estabilizar o código
        sleep_ms(100);
    }
}
