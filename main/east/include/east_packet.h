#ifndef __EAST_H__
#define __EAST_H__

#include "types.h"

typedef struct EAST_s * EAST_p;

EAST_p    EAST_Init(U8 * pContainer, U32 aSize, U8 * pBuffer, U32 aBufferSize);
FW_RESULT   EAST_SetBuffer      (EAST_p pEAST, U8 * pBuffer, U32 aSize);
FW_RESULT   EAST_PutByte        (EAST_p pEAST, U8 aValue);
FW_RESULT   EAST_GetByte        (EAST_p pEAST, U8 * pValue);
U16         EAST_GetDataSize    (EAST_p pEAST);
U16         EAST_GetPacketSize  (EAST_p pEAST);

#endif /* __EAST_H__ */
