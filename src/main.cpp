#include <Arduino.h>
#include <driver/i2s.h>

// --- ALTAVOZ (MAX98357A) - I2S Port 0 ---
#define I2S_SPK_NUM     I2S_NUM_0
#define I2S_SPK_BCLK    1
#define I2S_SPK_LRC     2
#define I2S_SPK_DOUT    4

// --- MICRÓFONO (INMP441) - I2S Port 1 ---
#define I2S_MIC_NUM     I2S_NUM_1
#define I2S_MIC_SCK     6
#define I2S_MIC_WS      7
#define I2S_MIC_SD      8

// --- CONFIGURACIÓN DE AUDIO ---
#define SAMPLE_RATE     16000
#define BUFFER_SIZE     512

void setupMic() {
  i2s_config_t mic_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 envía 32 bits
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,  // L/R a GND = canal izquierdo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = BUFFER_SIZE,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t mic_pins = {
    .bck_io_num   = I2S_MIC_SCK,
    .ws_io_num    = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_MIC_SD
  };

  i2s_driver_install(I2S_MIC_NUM, &mic_config, 0, NULL);
  i2s_set_pin(I2S_MIC_NUM, &mic_pins);
  i2s_zero_dma_buffer(I2S_MIC_NUM);
}

void setupSpeaker() {
  i2s_config_t spk_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = BUFFER_SIZE,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t spk_pins = {
    .bck_io_num   = I2S_SPK_BCLK,
    .ws_io_num    = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_SPK_NUM, &spk_config, 0, NULL);
  i2s_set_pin(I2S_SPK_NUM, &spk_pins);
  i2s_zero_dma_buffer(I2S_SPK_NUM);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Iniciando pass-through micro → altavoz...");

  setupMic();
  setupSpeaker();

  Serial.println("Listo. Habla por el micrófono.");
}

// Buffer de 32 bits para leer del INMP441
int32_t raw32[BUFFER_SIZE];
// Buffer de 16 bits para enviar al altavoz
int16_t out16[BUFFER_SIZE];

void loop() {
  size_t bytes_read = 0;

  // Leer del micrófono (32 bits por muestra)
  i2s_read(I2S_MIC_NUM, raw32, sizeof(raw32), &bytes_read, portMAX_DELAY);

  int samples = bytes_read / 4; // 4 bytes por muestra (32 bit)

  // Convertir 32 bits → 16 bits y aplicar ganancia
  for (int i = 0; i < samples; i++) {
    // El INMP441 mete los datos en los bits 18-bit superiores
    // Desplazamos y ajustamos la ganancia aquí (x4 = 2 bits a la izquierda)
    int32_t val = raw32[i] >> 11;  
    
    // Clamp para evitar overflow
    if (val > 32767)  val = 32767;
    if (val < -32768) val = -32768;
    
    out16[i] = (int16_t)val;
  }

  // Enviar al altavoz
  size_t bytes_written = 0;
  i2s_write(I2S_SPK_NUM, out16, samples * 2, &bytes_written, portMAX_DELAY);
}