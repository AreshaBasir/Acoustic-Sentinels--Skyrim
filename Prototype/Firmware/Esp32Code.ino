#include <Arduino.h>
#include <driver/spi_slave.h>
#include <driver/i2s.h>

#define SPI_MOSI 11
#define SPI_MISO 13
#define SPI_SCLK 12
#define SPI_CS   10

#define HANDSHAKE_PIN 9 

#define I2S_SCK  4
#define I2S_WS   5
#define I2S_SD   6

#define SNAPSHOT_SAMPLES 5000 
#define SPI_BUFFER_BYTES 1024 

int32_t* mic_buffer;
uint16_t* piezo_normal_buffer;
uint16_t* piezo_mist_buffer;

WORD_ALIGNED_ATTR uint8_t spi_recv_buffer[SPI_BUFFER_BYTES];

void setup() {
  Serial.begin(921600);
  while(!Serial) delay(10);

  pinMode(HANDSHAKE_PIN, OUTPUT);
  digitalWrite(HANDSHAKE_PIN, LOW); 

  mic_buffer = (int32_t*)ps_malloc(SNAPSHOT_SAMPLES * sizeof(int32_t));
  piezo_normal_buffer = (uint16_t*)ps_malloc(SNAPSHOT_SAMPLES * sizeof(uint16_t));
  piezo_mist_buffer = (uint16_t*)ps_malloc(SNAPSHOT_SAMPLES * sizeof(uint16_t));

  if (!mic_buffer || !piezo_normal_buffer || !piezo_mist_buffer) {
    Serial.println("PSRAM Allocation Failed!");
    while(1);
  }

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  spi_bus_config_t buscfg = {
      .mosi_io_num = SPI_MOSI, .miso_io_num = SPI_MISO,
      .sclk_io_num = SPI_SCLK, .quadwp_io_num = -1, .quadhd_io_num = -1
  };
  spi_slave_interface_config_t slvcfg = {
      .spics_io_num = SPI_CS, .flags = 0, .queue_size = 3, .mode = 0
  };
  spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
}

void loop() {
  int samples_collected = 0;
  
  spi_slave_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = SPI_BUFFER_BYTES * 8; 
  t.rx_buffer = spi_recv_buffer;   

  //STM32 is paused before start
  digitalWrite(HANDSHAKE_PIN, LOW);
  delay(5); 

  while (samples_collected < SNAPSHOT_SAMPLES) {
    //ARM THE RECEIVER FIRST
    spi_slave_queue_trans(SPI2_HOST, &t, portMAX_DELAY);
    
    //TELL STM32 TO FIRE
    digitalWrite(HANDSHAKE_PIN, HIGH);
    
    //WAIT FOR THE PACKET
    spi_slave_transaction_t* out;
    spi_slave_get_trans_result(SPI2_HOST, &out, portMAX_DELAY);
    
    //PAUSE STM32 WHILE WE PROCESS
    digitalWrite(HANDSHAKE_PIN, LOW);

    uint16_t* raw_piezos = (uint16_t*)spi_recv_buffer;
    int packet_samples = SPI_BUFFER_BYTES / 2; 

    for (int i = 0; i < packet_samples; i += 2) {
      if (samples_collected >= SNAPSHOT_SAMPLES) break;

      int32_t mic_sample = 0;
      size_t bytes_read;
      i2s_read(I2S_NUM_0, &mic_sample, sizeof(int32_t), &bytes_read, 0);
      
      mic_buffer[samples_collected] = (mic_sample >> 8); 
      piezo_normal_buffer[samples_collected] = raw_piezos[i];     
      piezo_mist_buffer[samples_collected] = raw_piezos[i+1];     
      
      samples_collected++;
    }
  }

  //dump to PC
  for (int i = 0; i < SNAPSHOT_SAMPLES; i++) {
    Serial.print(mic_buffer[i]);
    Serial.print(",");
    Serial.print(piezo_normal_buffer[i]);
    Serial.print(",");
    Serial.println(piezo_mist_buffer[i]);
  }
}