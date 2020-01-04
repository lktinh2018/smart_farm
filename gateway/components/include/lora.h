#ifndef LORA_H
#define LORA_H

#define BAUD_RATE 115200

#define ECHO_TEST_TXD  (GPIO_NUM_4)
#define ECHO_TEST_RXD  (GPIO_NUM_5)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)

#define BUFF_SIZE (200)

#define NODE_NUM 2

uint8_t completed_sending_a[NODE_NUM + 1];

void init_lora(void);
void loop_lora(void);
void publish_data(uint8_t *message, int len);
void collision_solving_task(void);

#endif