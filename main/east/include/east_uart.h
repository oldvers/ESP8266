#ifndef __EAST_UART_H__
#define __EAST_UART_H__

#include "types.h"

/* Callback Function Declarations */
typedef FW_BOOLEAN (* EAST_UART_CbByte)(U8 * pByte);

/* Function Declarations */

//-----------------------------------------------------------------------------
/** @brief Initializes the UART peripheral
 *  @param aBaudRate - Baud Rate.
 *  @param pRxByteCb - Callback, called when the byte is received.
 *                     Result is ignored. Called in IRQ context.
 *  @param pRxCmpltCb - Callback, called when the Rx timeout happened.
 *                      Called in IRQ context.
 *  @param pTxByteCb - Callback, called to get byte that need to be transmitted
 *                     Called in IRQ context
 *                     If returns FW_TRUE - continue transmiting
 *                     If returns FW_FALSE - transmiting stops
 *  @param pTxCmpltCb - Callback, called when all the bytes were sent.
 *                      Called in IRQ context.
 * 
 *  @return None
 */
void EAST_UART_Init
(
  U32              aBaudRate,
  EAST_UART_CbByte pRxByteCb,
  EAST_UART_CbByte pRxCmpltCb,
  EAST_UART_CbByte pTxByteCb,
  EAST_UART_CbByte pTxCmpltCb
);
//-----------------------------------------------------------------------------
/** @brief DeInitializes the UART peripheral
 *  @param None
 *  @return None
 */
void EAST_UART_DeInit(void);
//-----------------------------------------------------------------------------
/** @brief Enables transmiting via UART using Interrupts
 *  @param None
 *  @return None
 *  @note When Tx Data Register empty - callback function is called to get
 *        the next byte to transmit
 */
void EAST_UART_TxStart(void);
//-----------------------------------------------------------------------------
/** @brief Enables receiving via UART using Interrupts
 *  @param None
 *  @return None
 *  @note When Rx Data Register is not empty - callback function is called
 *        to put the received byte to higher layer
 */
void EAST_UART_RxStart(void);
//-----------------------------------------------------------------------------
/** @brief Changes UART baud rate
 *  @param aValue - Baud rate
 *  @return None
 */
void EAST_UART_SetBaudrate(U32 aValue);

#endif /* __EAST_UART_H__ */
