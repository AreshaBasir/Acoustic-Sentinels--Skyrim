#include <SPI.h>

#define BUFFER_SIZE 1024
volatile uint16_t adc_buffer[BUFFER_SIZE];

volatile bool send_first_half = false;
volatile bool send_second_half = false;

// The wire listening to the ESP32
const int handshakePin = PB0; 

void setup() {
  // Boosted to 18 MHz to outrun the ADC
  SPI.begin();
  SPI.beginTransaction(SPISettings(18000000, MSBFIRST, SPI_MODE0));

  pinMode(PA4, OUTPUT);
  digitalWrite(PA4, HIGH);
  pinMode(handshakePin, INPUT_PULLDOWN);

  pinMode(PA0, INPUT_ANALOG); 
  pinMode(PA1, INPUT_ANALOG); 

  HAL_Init();
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  DMA1_Channel1->CCR = 0; 
  DMA1_Channel1->CPAR = (uint32_t)&(ADC1->DR);       
  DMA1_Channel1->CMAR = (uint32_t)adc_buffer;       
  DMA1_Channel1->CNDTR = BUFFER_SIZE;               
  
  DMA1_Channel1->CCR |= DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_HTIE | DMA_CCR_TCIE;
  DMA1_Channel1->CCR |= DMA_CCR_EN; 

  ADC1->CR1 = 0;
  ADC1->CR1 |= ADC_CR1_SCAN; 
  
  ADC1->CR2 = 0; 
  ADC1->CR2 |= ADC_CR2_DMA;  
  ADC1->CR2 |= ADC_CR2_CONT; 

  // Set Channel 0 and 1 to take 7.5 clock cycles per reading
  // This physically limits the ADC to around 300,000 samples per second per channel.
  ADC1->SMPR2 = (1 << 0) | (1 << 3); 
  
  ADC1->SQR1 = (1 << 20); 
  ADC1->SQR3 = (0 << 0) | (1 << 5); 
  
  ADC1->CR2 |= ADC_CR2_ADON; 
  delay(1);
  ADC1->CR2 |= ADC_CR2_ADON; 

  NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

void loop() {
  // ONLY transmit if the ESP32 asks for data
  if (digitalRead(handshakePin) == HIGH) {
    if (send_first_half) {
      digitalWrite(PA4, LOW); 
      delayMicroseconds(1); // Give ESP32 1 microsecond to wake up
      SPI.transfer((uint8_t*)adc_buffer, BUFFER_SIZE); 
      digitalWrite(PA4, HIGH); 
      send_first_half = false;
    }
    else if (send_second_half) {
      digitalWrite(PA4, LOW);
      delayMicroseconds(1);
      SPI.transfer((uint8_t*)&adc_buffer[BUFFER_SIZE/2], BUFFER_SIZE); 
      digitalWrite(PA4, HIGH);
      send_second_half = false;
    }
  } else {
    // If ESP32 busy printing in PC, drop the frames to prevent tearing
    send_first_half = false;
    send_second_half = false;
  }
}

extern "C" void DMA1_Channel1_IRQHandler(void) {
  if (DMA1->ISR & DMA_ISR_HTIF1) {
    DMA1->IFCR |= DMA_IFCR_CHTIF1; 
    send_first_half = true;        
  }
  if (DMA1->ISR & DMA_ISR_TCIF1) {
    DMA1->IFCR |= DMA_IFCR_CTCIF1; 
    send_second_half = true;       
  }
}