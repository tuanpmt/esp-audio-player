#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "i2s.h"
#include "i2s_dma/i2s_dma.h"


// When samples are not sent fast enough underrun condition occurs
volatile uint32_t underrun_counter = 0, write_pos = 0;
int *curr_dma_buf = NULL;

#define DMA_BUFFER_SIZE         256
#define DMA_QUEUE_SIZE          8

// Circular list of descriptors
static dma_descriptor_t dma_block_list[DMA_QUEUE_SIZE];

// Array of buffers for circular list of descriptors
static uint8_t dma_buffer[DMA_QUEUE_SIZE][DMA_BUFFER_SIZE];

// Queue of empty DMA blocks
static QueueHandle_t dma_queue;


/**
 * Create a circular list of DMA descriptors
 */
static inline void init_descriptors_list()
{
    int i;
    memset(dma_buffer, 0, DMA_QUEUE_SIZE * DMA_BUFFER_SIZE);

    for(i = 0; i < DMA_QUEUE_SIZE; i++) {
        dma_block_list[i].owner = 1;
        dma_block_list[i].eof = 1;
        dma_block_list[i].sub_sof = 0;
        dma_block_list[i].unused = 0;
        dma_block_list[i].buf_ptr = dma_buffer[i];
        dma_block_list[i].datalen = DMA_BUFFER_SIZE;
        dma_block_list[i].blocksize = DMA_BUFFER_SIZE;
        if(i == (DMA_QUEUE_SIZE - 1)) {
            dma_block_list[i].next_link_ptr = &dma_block_list[0];
        } else {
            dma_block_list[i].next_link_ptr = &dma_block_list[i + 1];
        }
    }

    // The queue depth is one smaller than the amount of buffers we have,
    // because there's always a buffer that is being used by the DMA subsystem
    // *right now* and we don't want to be able to write to that simultaneously
    dma_queue = xQueueCreate(DMA_QUEUE_SIZE - 1, sizeof(uint8_t*));
}

// DMA interrupt handler. It is called each time a DMA block is finished processing.
static void dma_isr_handler(void)
{
    portBASE_TYPE task_awoken = pdFALSE;

    if(i2s_dma_is_eof_interrupt()) {
        dma_descriptor_t *descr = i2s_dma_get_eof_descriptor();

        if(xQueueIsQueueFullFromISR(dma_queue)) {
            // List of empty blocks is full. Sender don't send data fast enough.
            int dummy;
            underrun_counter++;
            // Discard top of the queue
            xQueueReceiveFromISR(dma_queue, &dummy, &task_awoken);
        }
        // Push the processed buffer to the queue so sender can refill it.
        memset(descr->buf_ptr, 0, DMA_BUFFER_SIZE);
        xQueueSendFromISR(dma_queue, (void*)(&descr->buf_ptr), &task_awoken);
    }
    i2s_dma_clear_interrupt();

    portEND_SWITCHING_ISR(task_awoken);
}

void i2s_init(int sample_rate)
{
    i2s_clock_div_t clock_div = i2s_get_clock_div(sample_rate * 2 * 16);

    printf("i2s clock dividers, bclk=%d, clkm=%d\n", clock_div.bclk_div, clock_div.clkm_div);
    i2s_pins_t i2s_pins = {.data = true, .clock = true, .ws = true};

    memset(dma_buffer, 0, DMA_QUEUE_SIZE * DMA_BUFFER_SIZE);

    i2s_dma_init(dma_isr_handler, clock_div, i2s_pins);
    init_descriptors_list();
    i2s_dma_start(dma_block_list);
}

void i2s_slient()
{
    memset(dma_buffer, 0, DMA_QUEUE_SIZE * DMA_BUFFER_SIZE);
}

void i2s_stop()
{
    i2s_dma_stop();
    vQueueDelete(dma_queue);
}
void i2s_push_sample(int sample)
{
    if(write_pos == (DMA_BUFFER_SIZE/4) || curr_dma_buf == NULL) {
        if(xQueueReceive(dma_queue, &curr_dma_buf, portMAX_DELAY) == pdFALSE) {
            // Or timeout occurs
            printf("Cound't get free blocks to push data\n");
        }
        write_pos = 0;
    }
    curr_dma_buf[write_pos++]  = sample;
}
