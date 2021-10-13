#ifndef __EAST_UART_H__
#define __EAST_UART_H__

#include "types.h"

/* Callback Function Declarations */
typedef FW_BOOLEAN (* EAST_UART_CbByte)(U8 * pByte);

/* Function Declarations */
void EAST_UART_Init
(
  U32              aBaudRate,
  EAST_UART_CbByte pRxByteCb,
  EAST_UART_CbByte pRxCmpltCb,
  EAST_UART_CbByte pTxByteCb,
  EAST_UART_CbByte pTxCmpltCb
);
void EAST_UART_DeInit      (void);
void EAST_UART_TxStart     (void);
void EAST_UART_RxStart     (void);
void EAST_UART_SetBaudrate (U32 aValue);

#endif /* __EAST_UART_H__ */
