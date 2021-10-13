#ifndef __BLOCKQUEUE_H__
#define __BLOCKQUEUE_H__

#include "types.h"

#define IRQ_IsInExceptionMode() (FW_BOOLEAN)xPortInIsrContext()

/* Block Queue Description Structure */
typedef struct BlockQueue_s * BlockQueue_p;

BlockQueue_p BlockQueue_Init   (U8 * pBuffer, U32 aBufferSize, U32 aBlockSize);
void       BlockQueue_Reset               (BlockQueue_p pQueue);
U32        BlockQueue_GetCapacity         (BlockQueue_p pQueue);
U32        BlockQueue_GetCountOfAllocated (BlockQueue_p pQueue);
U32        BlockQueue_GetCountOfFree      (BlockQueue_p pQueue);
FW_RESULT  BlockQueue_Allocate(BlockQueue_p pQueue, U8** ppBlock, U32 * pSize);
FW_RESULT  BlockQueue_Enqueue             (BlockQueue_p pQueue, U32 aSize);
FW_RESULT  BlockQueue_Dequeue (BlockQueue_p pQueue, U8** ppBlock, U32 * pSize);
FW_RESULT  BlockQueue_Release             (BlockQueue_p pQueue);

#endif /* __BLOCKQUEUE_H__ */