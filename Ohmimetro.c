/*
 * Por: Wilton Lacerda Silva
 * Ohmímetro utilizando o ADC da BitDogLab
 *
 * 
 * Neste exemplo, utilizamos o ADC do RP2040 para medir a resistência de um resistor
 * desconhecido, utilizando um divisor de tensão com dois resistores.
 * O resistor conhecido é de 10k ohm e o desconhecido é o que queremos medir.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "pico/bootrom.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28
#define Botao_A 5
#define Botao_B 6

int R_conhecido = 10000;   // Resistor de 10k ohm
double R_x = 0.0;          // Resistor desconhecido
double T_x = 0.0;          // Resistor desconhecido
float ADC_VREF = 3.3;      // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)

const char* resistor_colors[] = {
  "Preto", "Marrom", "Vermelho", "Laranja",
  "Amarelo", "Verde", "Azul", "Violeta",
  "Cinza", "Branco"
};

const double E24[] = {
  10, 11, 12, 13, 15, 16, 18, 20,
  22, 24, 27, 30, 33, 36, 39, 43,
  47, 51, 56, 62, 68, 75, 82, 91
};
const int E24_SIZE = 24;

void gpio_irq_handler(uint gpio, uint32_t events) {
  reset_usb_boot(0, 0);
}

double find_closest_e24_down(double resistance) {
  double closest = 0;
  double max_candidate = 0;

  // Testa cada valor da série E24
  for (int decade = -1; decade <= 6; decade++) {
    double factor = pow(10, decade);
    for (int i = 0; i < E24_SIZE; i++) {
      double candidate = E24[i] * factor;

      if (candidate <= resistance && candidate > max_candidate) {
        max_candidate = candidate;
      }
    }
  }

  return max_candidate;
}

int main()
{
  stdio_init_all();

  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(Botao_B);
  gpio_set_dir(Botao_B, GPIO_IN);
  gpio_pull_up(Botao_B);
  gpio_set_irq_enabled_with_callback(
    Botao_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler
  );

  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // Inicializa o display
  ssd1306_t ssd;
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
  ssd1306_config(&ssd);
  ssd1306_send_data(&ssd);

  // Limpa o display. O display inicia com todos os pixels apagados
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  adc_init();
  // GPIO 28 como entrada analógica
  adc_gpio_init(ADC_PIN);

  float tensao;
  char str_x[5]; // Buffer para armazenar a string
  char str_y[5]; // Buffer para armazenar a string

  bool cor = true;
  while (true) {
    // Seleciona o ADC para eixo X
    adc_select_input(2);

    double soma = 0.0f;
    for (int i = 0; i < 100; i++) {
      soma += adc_read();
      sleep_ms(1);
    }
    double media = soma / 100;
    
    // Conversão
    // T_x = media * ADC_VREF / ADC_RESOLUTION;
    // R_x = (R_conhecido * T_x) / (ADC_VREF - T_x);
    R_x = (R_conhecido * media) / (ADC_RESOLUTION - media);

    int real_resistence = find_closest_e24_down(R_x);
    printf("Resistência: %lf\n", R_x);
    printf("Resistência real: %d\n", real_resistence);

    // Verificar índices do código de cores
    int multiplier = 0;
    int first_color_index = 0;
    int second_color_index = 0;
    int i;
    int resistence_reference = real_resistence;
    for (i = 0; resistence_reference >= 100; i++) {
      resistence_reference /= 10;
    }
    first_color_index = resistence_reference / 10;
    second_color_index = resistence_reference % 10;
    multiplier = i;

    printf("%d, %d, %d\n", first_color_index, second_color_index, multiplier);
    
    sprintf(str_x, "%1.0f", media);
    sprintf(str_y, "%d", real_resistence);

    // Atualiza o conteúdo do display com valor de resistência
    ssd1306_fill(&ssd, !cor);                           // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
    ssd1306_line(&ssd, 3, 25, 123, 25, cor);            // Desenha uma linha
    ssd1306_line(&ssd, 3, 37, 123, 37, cor);            // Desenha uma linha
    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);  // Desenha uma string
    ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);   // Desenha uma string
    ssd1306_draw_string(&ssd, "  Ohmimetro  ", 10, 28); // Desenha uma string
    ssd1306_draw_string(&ssd, "ADC", 13, 41);           // Desenha uma string
    ssd1306_draw_string(&ssd, "Resistor", 50, 41);      // Desenha uma string
    ssd1306_line(&ssd, 44, 37, 44, 60, cor);            // Desenha uma linha vertical
    ssd1306_draw_string(&ssd, str_x, 8, 52);            // Desenha uma string
    ssd1306_draw_string(&ssd, str_y, 59, 52);           // Desenha uma string
    ssd1306_send_data(&ssd);                            // Atualiza o display

    sleep_ms(700);

    // Atualiza o conteúdo do display com cores do resistor lido
    ssd1306_fill(&ssd, !cor);
    ssd1306_draw_string(&ssd, "Resistor:", 8, 6);
    ssd1306_draw_string(&ssd, resistor_colors[first_color_index], 8, 16);
    ssd1306_draw_string(&ssd, resistor_colors[second_color_index], 8, 26);
    ssd1306_draw_string(&ssd, resistor_colors[multiplier], 8, 36);
    ssd1306_send_data(&ssd);

    sleep_ms(700);
  }
}