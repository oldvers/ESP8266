#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

#include "esp_attr.h"

#include "esp8266/uart_struct.h"
#include "esp8266/uart_register.h"
#include "esp8266/pin_mux_register.h"
#include "esp8266/eagle_soc.h"
#include "esp8266/rom_functions.h"

#include "rom/ets_sys.h"

#include "driver/uart.h"
#include "driver/uart_select.h"

#include "east_uart.h"

#define UART_ENTER_CRITICAL()      portENTER_CRITICAL()
#define UART_EXIT_CRITICAL()       portEXIT_CRITICAL()
#define UART_EMPTY_THRESH_DEFAULT  (10)
#define UART_FULL_THRESH_DEFAULT   (80)
#define UART_TOUT_THRESH_DEFAULT   (10)

/* DRAM_ATTR is required to avoid UART array placed in flash,
   due to accessed from ISR */
static DRAM_ATTR uart_dev_t * const EAST_UART = &uart0;
static EAST_UART_CbByte gRxByteCb  = NULL;
static EAST_UART_CbByte gRxCmpltCb = NULL;
static EAST_UART_CbByte gTxByteCb  = NULL;
static EAST_UART_CbByte gTxCmpltCb = NULL;

//-----------------------------------------------------------------------------
/** @brief The internal IRQ handler for the UART peripheral
 *  @param param - Pointer to the parameter passed to IRQ
 *  @return None
 */

static void EAST_UART_IrqHandler(void * param)
{
    FW_BOOLEAN result = FW_FALSE;
    uint8_t data = 0;
    int rx_fifo_len = EAST_UART->status.rxfifo_cnt;
    uint8_t buf_idx = 0;
    uint32_t uart_intr_status = EAST_UART->int_st.val;

    while (0 != uart_intr_status)
    {
        buf_idx = 0;

        if ( 0 != (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST_M) )
        {
            /* Clear the interrupt flag */
            EAST_UART->int_clr.txfifo_empty = 1;

            /* Get the size of space in FIFO */
            int tx_fifo_rem = UART_FIFO_LEN - EAST_UART->status.txfifo_cnt;

            /* Write all the available bytes to FIFO */
            while (0 < tx_fifo_rem)
            {
                if (NULL != gTxByteCb)
                {
                    result = gTxByteCb(&data);
                }

                /* If there is no more bytes - break */
                if (FW_TRUE == result)
                {
                    EAST_UART->fifo.rw_byte = data;
                }
                else
                {
                    break;
                }
                tx_fifo_rem--;
            }

            /* If all the bytes were sent - complete and disable the IRQ */
            if (UART_FIFO_LEN == tx_fifo_rem)
            {
                EAST_UART->int_ena.txfifo_empty = 0;
                if (NULL != gTxCmpltCb)
                {
                    (void)gTxCmpltCb(NULL);
                }
            }
        }
        else if ( (0 != (uart_intr_status & UART_RXFIFO_TOUT_INT_ST_M)) ||
                  (0 != (uart_intr_status & UART_RXFIFO_FULL_INT_ST_M)) )
        {
            /* Get the count of received bytes in FIFO */
            rx_fifo_len = EAST_UART->status.rxfifo_cnt;

            /* Read out all the bytes from FIFO to clear the interrupt flag */
            while (buf_idx < rx_fifo_len)
            {
                data = EAST_UART->fifo.rw_byte;
                if (NULL != gRxByteCb)
                {
                    (void)gRxByteCb(&data);
                }
                buf_idx++;
            }
            
            /* Reading is complete */
            if (NULL != gRxCmpltCb)
            {
                (void)gRxCmpltCb(NULL);
            }

            /* After copying the bytes from FIFO, clear IRQ flags */
            EAST_UART->int_clr.rxfifo_tout = 1;
            EAST_UART->int_clr.rxfifo_full = 1;
        }
        else if (uart_intr_status & UART_RXFIFO_OVF_INT_ST_M)
        {
            /* When FIFO overflows - reset the FIFO */
            EAST_UART->conf0.rxfifo_rst = 0x1;
            EAST_UART->conf0.rxfifo_rst = 0x0;
            EAST_UART->int_clr.rxfifo_ovf = 1;
        }
        else if (uart_intr_status & UART_FRM_ERR_INT_ST_M)
        {
            EAST_UART->int_clr.frm_err = 1;
        }
        else if (uart_intr_status & UART_PARITY_ERR_INT_ST_M)
        {
            EAST_UART->int_clr.parity_err = 1;
        }
        else
        {
            /* Simply clear all the other IRQ flags */
            EAST_UART->int_clr.val = uart_intr_status;
        }

        /* Get the IRQ flags */
        uart_intr_status = EAST_UART->int_st.val;
    }
}

//-----------------------------------------------------------------------------

void EAST_UART_Init
(
  U32              aBaudRate,
  EAST_UART_CbByte pRxByteCb,
  EAST_UART_CbByte pRxCmpltCb,
  EAST_UART_CbByte pTxByteCb,
  EAST_UART_CbByte pTxCmpltCb
)
{
    /* Init the GPIO pins of UART */
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

    UART_ENTER_CRITICAL();

    /* Set the callbacks */
    gRxByteCb  = pRxByteCb;
    gRxCmpltCb = pRxCmpltCb;
    gTxByteCb  = pTxByteCb;
    gTxCmpltCb = pTxCmpltCb;

    /* Disable the hardware flow control */
    EAST_UART->conf1.rx_flow_en = 0;
    EAST_UART->conf0.tx_flow_en = 0;

    /* Set the baud rate */
    EAST_UART->clk_div.val = (uint32_t)(UART_CLK_FREQ / aBaudRate) & 0xFFFFF;

    /* Set the word length */        
    EAST_UART->conf0.bit_num = UART_DATA_8_BITS;

    /* Set the stop bits */
    EAST_UART->conf0.stop_bit_num = UART_STOP_BITS_1;

    /* Set the parity */
    EAST_UART->conf0.parity = (UART_PARITY_DISABLE & 0x1);
    EAST_UART->conf0.parity_en = ((UART_PARITY_DISABLE >> 1) & 0x1);

    /* Reset the Rx FIFO */
    EAST_UART->conf0.rxfifo_rst = 0x1;
    EAST_UART->conf0.rxfifo_rst = 0x0;

    /* Set the Rx/Tx/Timeout treshholds */
    EAST_UART->conf1.rx_tout_thrhd = ((UART_TOUT_THRESH_DEFAULT) & 0x7F);
    EAST_UART->conf1.rx_tout_en = 1;
    EAST_UART->conf1.rxfifo_full_thrhd = UART_FULL_THRESH_DEFAULT;
    EAST_UART->conf1.txfifo_empty_thrhd = UART_EMPTY_THRESH_DEFAULT;

    UART_EXIT_CRITICAL();

    /* Register the IRQ handler */
    (void)uart_isr_register(UART_NUM_0, EAST_UART_IrqHandler, NULL);

    UART_ENTER_CRITICAL();

    /* Clear the interrupts' flags */
    EAST_UART->int_clr.val = UART_INTR_MASK;

    /* Enable the Frame/Overflow error interrupts  */
    EAST_UART->int_ena.frm_err = 1;
    EAST_UART->int_ena.rxfifo_ovf = 1;

    UART_EXIT_CRITICAL();
}

//-----------------------------------------------------------------------------

void EAST_UART_DeInit(void)
{
    //
}

//-----------------------------------------------------------------------------

void EAST_UART_TxStart(void)
{
    UART_ENTER_CRITICAL();

    /* Clear the interrupt flag */
    EAST_UART->int_clr.txfifo_empty = 1;
    /* Enable the interrupt */
    EAST_UART->int_ena.txfifo_empty = 1;

    UART_EXIT_CRITICAL();
}

//-----------------------------------------------------------------------------

void EAST_UART_RxStart(void)
{
    UART_ENTER_CRITICAL();

    /* Clear the interrupts' flags */
    EAST_UART->int_clr.rxfifo_full = 1;
    EAST_UART->int_clr.rxfifo_tout = 1;
    /* Enable the Rx buffer empty and Rx timeout interrupts */
    EAST_UART->int_ena.rxfifo_full = 1;
    EAST_UART->int_ena.rxfifo_tout = 1;

    UART_EXIT_CRITICAL();
}

//-----------------------------------------------------------------------------

void EAST_UART_SetBaudrate(U32 aValue)
{
    //
}

//-----------------------------------------------------------------------------
