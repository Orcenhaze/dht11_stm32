# dht11_stm32
Minimal single-file DHT11 HAL driver for STM32.  
Tested on: Nucleo-F334R8.

---

### Example Usage
```C
/* USER CODE BEGIN Includes */
#include ...
#include ...
#define DHT11_STM32_IMPLEMENTATION
#include "dht11_stm32.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
DHT11_State dht = dht11_state_create(GPIOC, GPIO_PIN_0, &htim16);
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
  if (dht11_read(&dht)) {
    uart_length = sprintf((char*)uart_data, "H: %u%%, T: %uC\r\n", dht.humidity, dht.temperature);
    HAL_UART_Transmit(&huart2, uart_data, uart_length, 10);
    HAL_Delay(5000);
  }
  HAL_Delay(100);
  /* USER CODE END WHILE */
}
```

### Notes
- This code is not production ready because it lacks things like returning error codes and ability to communicate with DHT11 in a non-blocking way which would require an interrupt driven implementation of sorts. It's mainly a bookmark.
- The timer you pass to `dht11_state_create()` must increment every 1us. To workout the correct prescaler value for the timer: `prescaler = (timer_clock_hz / 1_000_000) - 1`
- DATA line between MCU and DHT11 needs a pull-up resistor (≈4.7k). Some modules have it onboard; bare sensor needs an external resistor.
