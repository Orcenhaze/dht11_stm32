/*

You can place this header file in Core/Inc/, next to "main.h".

In "main.c", #define DHT11_STM32_IMPLEMENTATION before including this header to create the implementation. 
Like this:

 // USER CODE BEGIN Includes
#include ...
#include ...
#define DHT11_STM32_IMPLEMENTATION
#include "dht11_stm32.h"
 // USER CODE END Includes

*/

#ifndef DHT11_STM32_H
#define DHT11_STM32_H

#include <stdint.h>
#include "main.h"

typedef struct {
    GPIO_TypeDef      *data_port;
    uint16_t           data_pin;
    TIM_HandleTypeDef *timer_handle;
    
    uint8_t humidity;
    uint8_t temperature;
} DHT11_State;


////////////////////////////////
//~ API
//

// Pass the port and pin of the DATA line between MCU and DHT11 device. Example: GPIOC, GPIO_PIN_0.
// Pass a handle to a timer that increments every 1 micro-second.
// Returns DHT11_State that you should pass to dht11_read().
DHT11_State dht11_state_create(GPIO_TypeDef *data_port, uint16_t data_pin, TIM_HandleTypeDef *timer_handle);

// Fills the humidity and temperature values of DHT11_State.
// Returns 1 if we read successfully, 0 otherwise.
int8_t      dht11_read(DHT11_State *dht11);


////////////////////////////////
//~ Internals
//
inline static void     dht11_delay(const DHT11_State *dht11, uint16_t microseconds);
inline static uint16_t dht11_wait_until_signal_changes(const DHT11_State *dht11, GPIO_PinState signal, uint16_t max_wait_microseconds);

#endif //DHT11_STM32_H



#if defined(DHT11_STM32_IMPLEMENTATION) && !defined(DHT11_STM32_IMPLEMENTATION_LOCK)
#define DHT11_STM32_IMPLEMENTATION_LOCK

inline static void dht11_delay(const DHT11_State *dht11, uint16_t microseconds)
{
    __HAL_TIM_SET_COUNTER(dht11->timer_handle, 0);
    while ((uint16_t)__HAL_TIM_GET_COUNTER(dht11->timer_handle) < microseconds);
}

inline static uint16_t dht11_wait_until_signal_changes(const DHT11_State *dht11, GPIO_PinState signal, uint16_t max_wait_microseconds)
{
    __HAL_TIM_SET_COUNTER(dht11->timer_handle, 0);
    while (HAL_GPIO_ReadPin(dht11->data_port, dht11->data_pin) == signal) {
        if ((uint16_t)__HAL_TIM_GET_COUNTER(dht11->timer_handle) > max_wait_microseconds) {
            break;
        }
    }
    return (uint16_t)__HAL_TIM_GET_COUNTER(dht11->timer_handle);
}

DHT11_State dht11_state_create(GPIO_TypeDef *data_port, uint16_t data_pin, TIM_HandleTypeDef *timer_handle)
{
    DHT11_State result  = {};
    result.data_port    = data_port;
    result.data_pin     = data_pin;
    result.timer_handle = timer_handle;
    HAL_TIM_Base_Start(timer_handle);
    HAL_Delay(1200);
    return result;
}

int8_t dht11_read(DHT11_State *dht11)
{
    // We will just follow the communication protocol described in datasheets you can find online.
    
    const uint16_t TIMEOUT_US = 1000;
    
    //~ MCU Sends Start Signal.
    //
    GPIO_InitTypeDef gpio = {};
    gpio.Pin              = dht11->data_pin;
    gpio.Mode             = GPIO_MODE_OUTPUT_PP;
    gpio.Pull             = GPIO_NOPULL;
    gpio.Speed            = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(dht11->data_port, &gpio);
    
    HAL_GPIO_WritePin(dht11->data_port, dht11->data_pin, GPIO_PIN_RESET);
    HAL_Delay(18);
    
    // @Note: DATA BUS STATE NOW: LOW
    
    __disable_irq();
    
    HAL_GPIO_WritePin(dht11->data_port, dht11->data_pin, GPIO_PIN_SET);
    dht11_delay(dht11, 30);
    
    // @Note: DATA BUS STATE NOW: HIGH
    
    //~ DHT Response.
    //
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(dht11->data_port, &gpio);
    
    if (dht11_wait_until_signal_changes(dht11, GPIO_PIN_SET, TIMEOUT_US) >= TIMEOUT_US) {
        // DHT11 not responding.
        __enable_irq();
        return 0;
    }
    
    // @Note: DATA BUS STATE NOW: LOW
    
    if (dht11_wait_until_signal_changes(dht11, GPIO_PIN_RESET, TIMEOUT_US) >= TIMEOUT_US) {
        __enable_irq();
        return 0;
    }
    
    // @Note: DATA BUS STATE NOW: HIGH
    
    //~ DHT Data Transmission.
    //
    if (dht11_wait_until_signal_changes(dht11, GPIO_PIN_SET, TIMEOUT_US) >= TIMEOUT_US) {
        __enable_irq();
        return 0;
    }
    
    // @Note: DATA BUS STATE NOW: LOW
    
    uint8_t data[5] = {}; // iRH, dRH, iT, dT, check_sum;
    for (int32_t index_data = 0; index_data < 5; index_data++) {
        for (int32_t index_bit = 0; index_bit < 8; index_bit++) {
            
            if (dht11_wait_until_signal_changes(dht11, GPIO_PIN_RESET, TIMEOUT_US) >= TIMEOUT_US) {
                __enable_irq();
                return 0;
            }
            
            // @Note: DATA BUS STATE NOW: HIGH
            
            uint16_t high_signal_duration = dht11_wait_until_signal_changes(dht11, GPIO_PIN_SET, TIMEOUT_US);
            if (high_signal_duration >= TIMEOUT_US) {
                __enable_irq();
                return 0;
            }
            
            // @Note: DATA BUS STATE NOW: LOW
            
            // DHT11 sends MSB first.
            // High signal duration was: short? 0, long? 1;
            //
            data[index_data] <<= 1;
            if (high_signal_duration > 50) {
                data[index_data] |= 1;
            }
        }
    }
    
    // @Note:
    // When the last bit data is transmitted, DHT11 pulls down the voltage level and keeps it for 50us. 
    // Then the Single-Bus voltage will be pulled up by the resistor to set it back to the free status. 
    dht11_delay(dht11, 50);
    
    if ((data[0] + data[1] + data[2] + data[3]) != data[4]) {
        __enable_irq();
        return 0;
    }
    
    // @Note: 
    // In DHT11, data[1] (dRH) and data[3] (dT) seem to be always zero.
    dht11->humidity    = data[0]; // iRH
    dht11->temperature = data[2]; // iT
    
    __enable_irq();
    return 1;
}

#endif // DHT11_STM32_IMPLEMENTATION