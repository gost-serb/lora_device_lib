/* Copyright (c) 2017 Cameron Harper
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * */

#ifndef LORA_FRAME_H
#define LORA_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* types **************************************************************/

/** recongised message types */
enum message_type {
    FRAME_TYPE_JOIN_REQ = 0,
    FRAME_TYPE_JOIN_ACCEPT,
    FRAME_TYPE_DATA_UNCONFIRMED_UP,
    FRAME_TYPE_DATA_UNCONFIRMED_DOWN,
    FRAME_TYPE_DATA_CONFIRMED_UP,
    FRAME_TYPE_DATA_CONFIRMED_DOWN,    
};

/** frame parameters */
struct lora_frame {

    enum message_type type;
    
    union {
        
        struct {
    
            uint32_t devAddr;
            uint32_t counter;
            bool ack;
            bool adr;
            bool adrAckReq;
            bool pending;

            const uint8_t *opts;
            uint8_t optsLen;

            uint8_t port;
            const uint8_t *data;
            uint8_t dataLen;
        
        } data;
        
        struct {

            uint8_t appNonce[3];
            uint8_t netID[3];
            uint32_t devAddr;
            uint8_t rx1DataRateOffset;
            uint8_t rx2DataRate;
            uint8_t rxDelay;
            uint8_t cfList[16];
            bool cfListPresent;
            
        } joinAccept; 
        
        struct {
            
            uint8_t appEUI[8];
            uint8_t devEUI[8];
            uint16_t devNonce;
            
        } joinRequest;
        
    } fields;
    
};

/** the result of a decode operation */
enum lora_frame_result {
    LORA_FRAME_OK,      /**< frame is OK */
    LORA_FRAME_BAD,     /**< frame format is bad */
    LORA_FRAME_MIC      /**< frame format is OK but MIC check failed */
};

/* globals ************************************************************/

extern const size_t LORA_PHYPAYLOAD_OVERHEAD;

/* function prototypes ************************************************/

/** encode a frame
 *
 * @param[in] key   key to use for MIC and encrypt (16 byte field)
 * @param[in] f     frame parameter structure
 * @param[out] out  frame buffer
 * @param[in] max   maximum byte length of `out`
 *
 * @return bytes encoded
 *
 * @retval 0 frame could not be encoded
 *
 * */
uint8_t LoraFrame_encode(const uint8_t *key, const struct lora_frame *f, uint8_t *out, uint8_t max);

/** decode a frame
 *
 * @param[in] appKey    application key (16 byte field)
 * @param[in] netSKey   network session key (16 byte field)
 * @param[in] appSKey   application session key (16 byte field)
 * @param[in] in        frame buffer (decrypt will be done in-place)
 * @param[in] len       byte length of `in`
 * @param[out] f        decoded frame structure
 *
 * @return lora_frame_result
 *
 * @retval LORA_FRAME_OK    frame decoded without any problems
 * @retval LORA_FRAME_BAD   frame format is bad
 * @retval LORA_FRAME_MIC   frame format OK but MIC check failed
 *
 * */
enum lora_frame_result LoraFrame_decode(const uint8_t *appKey, const uint8_t *netSKey, const uint8_t *appSKey, uint8_t *in, uint8_t len, struct lora_frame *f);

/** calculate size of the PhyPayload
 *
 * @param[in] dataLen length of data field in bytes
 * @param[in] optsLen length of options field in bytes
 *
 * @return PhyPayload size in bytes
 *
 * */
uint16_t LoraFrame_getPhyPayloadSize(uint8_t dataLen, uint8_t optsLen);

bool LoraFrame_isUpstream(enum message_type type);

#ifdef __cplusplus
}
#endif

#endif
