#include <string.h>
#include "east_packet.h"
#include "debug.h"

/* EAST data packet format is as follows:
   Size:   1            2           N     1           2
   Descr: [Start Token][Length = N][Data][Stop Token][Control Sum]

   Control Sum = XOR of N Data bytes. Initial value = 0.

  Test Packets
  #24 #05#00 #01#02#03#04#05 #42 #01#00
  #24 #40#00 #01#02#03#04#05#06#07#08#09#10#11#12#13#14#15#16
             #01#02#03#04#05#06#07#08#09#10#11#12#13#14#15#16
             #01#02#03#04#05#06#07#08#09#10#11#12#13#14#15#16
             #01#02#03#04#05#06#07#08#09#10#11#12#13#14#15#16 #42 #00#00 */

//-----------------------------------------------------------------------------

//#define EAST_DEBUG

#ifdef EAST_DEBUG
#  define EAST_LOG    DBG
#else
#  define EAST_LOG(...)
#endif

#define EAST_PACKET_START_TOKEN           ( 0x24 )
#define EAST_PACKET_STOP_TOKEN            ( 0x42 )
#define EAST_PACKET_WRAPPER_SIZE          ( 6 )
#define EAST_PACKET_MODE_INPUT            ( 0 )
#define EAST_PACKET_MODE_OUTPUT           ( 1 )

#define EAST_PACKET_CHECK_LENGTH(a,m)     ((FW_BOOLEAN)((0 < a) && (a <= m)))
#define EAST_PACKET_POSITION(b,i)         (b[(i) - 3])
#define EAST_PACKET_IS_COMPLETE(i,a)      ((FW_BOOLEAN)((0 == i) && (0 < a)))

#define EAST_PACKET_STAGE_START(i)        (0 == i)
#define EAST_PACKET_STAGE_LENGTHL(i)      (1 == i)
#define EAST_PACKET_STAGE_LENGTHH(i)      (2 == i)
#define EAST_PACKET_STAGE_DATA(i,a)       ((2 < i) && (i < (a + 3)))
#define EAST_PACKET_STAGE_STOP(i,a)       (i == (a + 3))
#define EAST_PACKET_STAGE_CSL(i,a)        (i == (a + 4))
#define EAST_PACKET_STAGE_CSH(i,a)        (i == (a + 5))
#define EAST_PACKET_STAGE_COMPLETE(i,a)   ((5 < i) && (i == (a + 6)))

//-----------------------------------------------------------------------------

typedef struct EAST_s
{
    U16             MaxSize;      /* Maximum size of the data */
    U16             ActSize;      /* Size of the currently collected data */
    U16             Index;        /* Current position in the packet */
    U16             FCS;          /* Factual control sum */
    U16             RCS;          /* Running control sum */
    struct
    {
      U16           OK : 1;       /* Packet correctness */
      U16           Mode : 1;     /* Packet mode: in/out */
      U16           Complete : 1; /* Packet completeness */
    };
    U8            * Buffer;       /* Pointer to the data buffer */
} EAST_t;

//-----------------------------------------------------------------------------
/** @brief Initializes the EAST component
 *  @param[in] pContainer - Pointer to the container for EAST placement
 *  @param[in] aSize - Size of the container
 *  @param[in] pBuffer - Pointer to the EAST data buffer
 *  @param[in] aBufferSize - Size of the buffer
 *  @return Pointer to the created EAST component
 */

EAST_p EAST_Init(U8 * pContainer, U32 aSize, U8 * pBuffer, U32 aBufferSize)
{
    EAST_p result = NULL;

    EAST_LOG("-*- EAST_Init() -*-\r\n");
    EAST_LOG("--- Inputs\r\n");
    EAST_LOG("  Container Addr = %08X\r\n", pContainer);
    EAST_LOG("  Container Size = %d\r\n", aSize);
    EAST_LOG("  Buffer Addr    = %08X\r\n", pBuffer);
    EAST_LOG("  Buffer Size    = %d\r\n", aBufferSize);
    EAST_LOG("--- Internals\r\n");
    EAST_LOG("  EAST Size      = %d\r\n", sizeof(EAST_t));

    /* Check the size of container */
    if ((NULL == pContainer) || (sizeof(EAST_t) > aSize))
    {
        EAST_LOG("  Input parameters error!\r\n");
        return NULL;
    }

    /* Initialization */
    result = (EAST_p)pContainer;
    memset(result, 0, sizeof(EAST_t));

    if ((NULL != pBuffer) && (0 != aBufferSize))
    {
        result->Buffer = pBuffer;
        result->MaxSize = aBufferSize;
        result->Mode = EAST_PACKET_MODE_OUTPUT;
    }

    EAST_LOG("  EAST Address   = %08X\r\n", result);

    return result;
}

//-----------------------------------------------------------------------------
/** @brief Initializes the EAST component
 *  @param[in] pEAST - Pointer to the EAST
 *  @param[in] pBuffer - Pointer to the EAST data buffer
 *  @param[in] aSize - Size of the data buffer
 *  @return FW_SUCCESS / FW_ERROR
 *  @note Can be used for reset the EAST component
 */

FW_RESULT EAST_SetBuffer(EAST_p pEAST, U8 * pBuffer, U32 aSize)
{
    EAST_LOG("-*- EAST_SetBuffer() -*-\r\n");
    EAST_LOG("--- Inputs\r\n");
    EAST_LOG("  Buffer Address = %08X\r\n", pBuffer);
    EAST_LOG("  Buffer Size    = %d\r\n", aSize);
    EAST_LOG("--- Internals\r\n");

    if ((NULL == pEAST) || (NULL == pBuffer) || (0 == aSize))
    {
        EAST_LOG("  Input parameters error!\r\n");
        return FW_ERROR;
    }

    /* Reset the state */
    memset(pEAST, 0, sizeof(EAST_t));

    /* Set the buffer */
    pEAST->Buffer = pBuffer;
    pEAST->MaxSize = aSize;
    pEAST->Mode = EAST_PACKET_MODE_OUTPUT;

    EAST_LOG("  EAST Buffer    = %08X\r\n", pBuffer);
    EAST_LOG("  EAST Buff Size = %d\r\n", aSize);

    return FW_SUCCESS;
}

//-----------------------------------------------------------------------------
/** @brief Puts Byte into the EAST data packet
 *  @param[in] pEAST - Pointer to the EAST
 *  @param[in] aValue - Value that should be placed into the EAST packet
 *  @return FW_INPROGRESS / FW_COMPLETE / FW_ERROR
 *  @note On the next call of this function after packet completion,
 *        collectiong of the new packet is started from the beginning. The
 *        previously collected packet is lost.
 */

FW_RESULT EAST_PutByte(EAST_p pEAST, U8 aValue)
{
    FW_RESULT result = FW_INPROGRESS;
    pEAST->OK = FW_TRUE;

    EAST_LOG("-*- EAST_PutByte -*-\r\n");
    EAST_LOG("--- Inputs\r\n");
    EAST_LOG("  EAST Address   = %08X\r\n", pEAST);
    EAST_LOG("  Value          = %02X\r\n", aValue);
    EAST_LOG("--- Internals\r\n");

    if ((NULL == pEAST) || (NULL == pEAST->Buffer))
    {
        EAST_LOG("  Input parameters error!\r\n");
        return FW_ERROR;
    }

    pEAST->Mode = EAST_PACKET_MODE_INPUT;
    pEAST->Complete = FW_FALSE;

    /* Data Stage */
    if EAST_PACKET_STAGE_DATA(pEAST->Index, pEAST->ActSize)
    {
        EAST_PACKET_POSITION(pEAST->Buffer, pEAST->Index) = aValue;
        pEAST->RCS ^= aValue;
    }
    /* Packet Start Token Stage */
    else if EAST_PACKET_STAGE_START(pEAST->Index)
    {
        pEAST->OK = (FW_BOOLEAN)(aValue == EAST_PACKET_START_TOKEN);
    }
    /* Packet Size Stage */
    else if EAST_PACKET_STAGE_LENGTHL(pEAST->Index)
    {
        pEAST->ActSize = aValue;
    }
    else if EAST_PACKET_STAGE_LENGTHH(pEAST->Index)
    {
        pEAST->ActSize = (pEAST->ActSize + (aValue << 8));
        pEAST->OK = EAST_PACKET_CHECK_LENGTH(pEAST->ActSize, pEAST->MaxSize);
        pEAST->RCS = 0;

        EAST_LOG("  Length         = %d\r\n", pEAST->ActSize);
    }
    /* Packet Stop Token Stage */
    else if EAST_PACKET_STAGE_STOP(pEAST->Index, pEAST->ActSize)
    {
        pEAST->OK = (FW_BOOLEAN)(aValue == EAST_PACKET_STOP_TOKEN);
    }
    /* Packet Control Sum Stage */
    else if EAST_PACKET_STAGE_CSL(pEAST->Index, pEAST->ActSize)
    {
        pEAST->FCS = aValue;
    }
    else if EAST_PACKET_STAGE_CSH(pEAST->Index, pEAST->ActSize)
    {
        pEAST->FCS = (pEAST->FCS + (aValue << 8));
        pEAST->OK = (FW_BOOLEAN)(pEAST->FCS == pEAST->RCS);

        EAST_LOG("  Factual CS     = %04X\r\n", pEAST->FCS);
        EAST_LOG("  Running CS     = %04X\r\n", pEAST->RCS);
    }

    /* Packet Index Incrementing Stage */
    if (FW_TRUE == pEAST->OK)
    {
        pEAST->Index++;
    }
    else
    {
        pEAST->Index = 0;
        pEAST->ActSize = 0;

        EAST_LOG("  Wrong packet format!\r\n");
    }

    /* Packet Completed Stage */
    if EAST_PACKET_STAGE_COMPLETE(pEAST->Index, pEAST->ActSize)
    {
        /* Indicate packet completion */
        pEAST->Complete = FW_TRUE;
        result = FW_COMPLETE;
        /* Reset the packet position */
        pEAST->Index = 0;

        EAST_LOG("  Packet complete!\r\n");
    }

    return result;
}

//-----------------------------------------------------------------------------
/** @brief Gets Byte from the EAST packet
 *  @param[in] pEAST - Pointer to the EAST
 *  @param[out] pValue - Pointer to the placeholder for Value
 *  @return FW_INPROGRESS / FW_COMPLETE
 *  @note On the next call of this function after packet completion,
 *        producing of the new packet is started from the beginning. The
 *        previously produced packet is left as is.
 */

FW_RESULT EAST_GetByte(EAST_p pEAST, U8 * pValue)
{
    FW_RESULT result = FW_INPROGRESS;
    pEAST->OK = FW_TRUE;

    pEAST->Mode = EAST_PACKET_MODE_OUTPUT;
    pEAST->Complete = FW_FALSE;

    /* Data Stage */
    if EAST_PACKET_STAGE_DATA(pEAST->Index, pEAST->ActSize)
    {
        *pValue = EAST_PACKET_POSITION(pEAST->Buffer, pEAST->Index);
        pEAST->RCS ^= *pValue;
    }
    /* Packet Start Token Stage */
    else if EAST_PACKET_STAGE_START(pEAST->Index)
    {
        *pValue = EAST_PACKET_START_TOKEN;
        pEAST->ActSize = pEAST->MaxSize;
    }
    /* Packet Size Stage */
    else if EAST_PACKET_STAGE_LENGTHL(pEAST->Index)
    {
        *pValue = (pEAST->ActSize & 0xFF);
    }
    else if EAST_PACKET_STAGE_LENGTHH(pEAST->Index)
    {
        *pValue = (pEAST->ActSize >> 8) & 0xFF;
        pEAST->RCS = 0;
    }
    /* Packet Stop Byte Stage */
    else if EAST_PACKET_STAGE_STOP(pEAST->Index, pEAST->ActSize)
    {
        *pValue = EAST_PACKET_STOP_TOKEN;
    }
    /* Packet Control Sum Stage */
    else if EAST_PACKET_STAGE_CSL(pEAST->Index, pEAST->ActSize)
    {
        *pValue = (pEAST->RCS & 0xFF);
    }
    else if EAST_PACKET_STAGE_CSH(pEAST->Index, pEAST->ActSize)
    {
        *pValue = (pEAST->RCS >> 8) & 0xFF;
    }

    /* Packet Index Incrementing Stage */
    if (FW_TRUE == pEAST->OK)
    {
        pEAST->Index++;
    }

    /* Packet Completed Stage */
    if EAST_PACKET_STAGE_COMPLETE(pEAST->Index, pEAST->ActSize)
    {
        /* Indicate packet completion */
        pEAST->Complete = FW_TRUE;
        result = FW_COMPLETE;
        /* Reset the packet position */
        pEAST->Index = 0;

        EAST_LOG("  Packet complete!\r\n");
    }

    return result;
}

//-----------------------------------------------------------------------------
/** @brief Gets the size of the currently collected data
 *  @param[in] pEAST - Pointer to the EAST
 *  @return Packet size
 */

U16 EAST_GetDataSize(EAST_p pEAST)
{
    U16 result = 0;

    if (EAST_PACKET_MODE_INPUT == pEAST->Mode)
    {
        if (FW_TRUE == pEAST->Complete)
        {
            result = pEAST->ActSize;
        }
    }
    else
    {
        result = pEAST->MaxSize;
    }

    return result;
}

//-----------------------------------------------------------------------------
/** @brief Gets the size of the currently collected data
 *  @param[in] pEAST - Pointer to the EAST
 *  @return Packet size
 */

U16 EAST_GetPacketSize(EAST_p pEAST)
{
    U16 result = 0;

    if (EAST_PACKET_MODE_INPUT == pEAST->Mode)
    {
        if (FW_TRUE == pEAST->Complete)
        {
            result = (EAST_PACKET_WRAPPER_SIZE + pEAST->ActSize);
        }
    }
    else
    {
        if (FW_FALSE == pEAST->Complete)
        {
            result = (EAST_PACKET_WRAPPER_SIZE + pEAST->MaxSize - pEAST->Index);
        }
    }

    return result;
}
