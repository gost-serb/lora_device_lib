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

#include "lora_channel_list.h"
#include "lora_region.h"
#include "lora_debug.h"

#include <string.h>

/* static prototypes **************************************************/

static uint32_t calculateOnAirTime(const struct lora_channel_list *self, uint8_t payloadLen);
static void cycleChannel(struct lora_channel_list *self);
static void addDefaultChannel(void *receiver, uint8_t chIndex, uint32_t freq);

/* functions **********************************************************/

void ChannelList_init(struct lora_channel_list *self, const struct lora_region *region)
{
    LORA_ASSERT(self != NULL)
    LORA_ASSERT(region != NULL)
    
    size_t i;
    const struct lora_region_default *settings;
    
    (void)memset(self, 0, sizeof(*self));

    for(i=0; i < sizeof(self->bands)/sizeof(*self->bands); i++){

        self->bands[i].channel = -1;
    }

    Region_getDefaultChannels(region, self, addDefaultChannel);
    settings = Region_getDefaultSettings(region);

    self->region = region;
    
    self->nextBand = -1;
    self->nextChannel = -1;
    self->numUnmasked = 0;

    self->rx2_rate = settings->rx2_rate;
    self->rx2_freq = settings->rx2_freq;
    self->power = settings->init_tx_power;
    self->rate = settings->init_tx_rate;    
}

bool ChannelList_add(struct lora_channel_list *self, uint8_t chIndex, uint32_t freq)
{
    LORA_ASSERT(self != NULL)
    
    bool retval = false;
    struct lora_channel *channel = NULL;
    uint8_t band;

    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        if(freq == 0){
            
            ChannelList_remove(self, chIndex);
            retval = true;
        }
        else{
            
            channel = &self->channels[chIndex];

            if(Region_validateFrequency(self->region, freq, &band)){

                if(channel->freq != 0U){
                    
                    ChannelList_remove(self, chIndex);
                }

                channel->freq = freq;
                channel->band = (int)band;
                
                self->bands[channel->band].numUnmasked++;
                self->numUnmasked++;

                if(self->numUnmasked == 1){

                    cycleChannel(self);
                }

                retval = true;
            }
        }
    }
    
    return retval;
}

void ChannelList_remove(struct lora_channel_list *self, uint8_t chIndex)
{
    LORA_ASSERT(self != NULL)
    
    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        if(self->channels[chIndex].freq > 0){
    
            self->channels[chIndex].freq = 0U;        
            
            if(!self->channels[chIndex].masked){
                self->bands[self->channels[chIndex].band].numUnmasked--;
                self->numUnmasked--;
            }
            if(self->nextChannel == chIndex){ 
                cycleChannel(self); 
            }
        }
    }
}

bool ChannelList_mask(struct lora_channel_list *self, uint8_t chIndex)
{
    LORA_ASSERT(self != NULL)
    
    bool retval = false;
    
    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        if(self->channels[chIndex].freq > 0U){
        
            if(!self->channels[chIndex].masked){

                self->channels[chIndex].masked = true;
                self->bands[self->channels[chIndex].band].numUnmasked--;
                self->numUnmasked--;

                if(self->nextChannel == (int)chIndex){ 
                    
                    cycleChannel(self); 
                }
            }
        
            retval = true;
        }       
    }
        
    return retval;
}

void ChannelList_unmask(struct lora_channel_list *self, uint8_t chIndex)
{
    LORA_ASSERT(self != NULL)
    
    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        self->channels[chIndex].masked = false;
        self->bands[self->channels[chIndex].band].numUnmasked++;
        self->numUnmasked++;

        if(self->numUnmasked == 1){ 
            
            cycleChannel(self); 
        }
    }
}

bool ChannelList_setRateAndPower(struct lora_channel_list *self, uint8_t rate, uint8_t power)    
{
    LORA_ASSERT(self != NULL)
    
    bool retval = true;
    
    self->rate = rate;
    self->power = power;

    return retval;
}

uint32_t ChannelList_waitTime(const struct lora_channel_list *self, uint64_t timeNow)
{
    LORA_ASSERT(self != NULL)
    
    uint32_t retval;
    
    if((self->nextBand == -1) || (timeNow >= self->bands[self->nextBand].timeReady)){

        retval = 0U;
    }
    else{

        retval = self->bands[self->nextBand].timeReady - timeNow;
    }

    return retval;
}

struct lora_adr_ans ChannelList_adrRequest(struct lora_channel_list *self, uint8_t rate, uint8_t power, uint16_t mask, uint8_t maskControl)
{
    LORA_ASSERT(self != NULL)
    
    struct lora_adr_ans retval = {
        .channelOK = false,
        .rateOK = false,
        .powerOK = false    
    };

    return retval;
}

void ChannelList_registerTransmission(struct lora_channel_list *self, uint64_t time, uint8_t payloadLen)
{
    LORA_ASSERT(self != NULL)
    
    if(self->nextBand != -1){

        uint16_t offTimeFactor;
        Region_getOffTimeFactor(self->region, self->nextBand, &offTimeFactor);

        uint32_t onTime_us = calculateOnAirTime(self, payloadLen);
    
        uint64_t offTime_us = (uint64_t)onTime_us * (uint64_t)offTimeFactor;

        self->bands[self->nextBand].timeReady = time + offTime_us;
        cycleChannel(self);
    }
}

size_t ChannelList_capacity(const struct lora_channel_list *self)
{
    LORA_ASSERT(self != NULL)
    
    return sizeof(self->channels)/sizeof(*self->channels);
}

bool ChannelList_txSetting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    LORA_ASSERT(self != NULL)
    
    const struct lora_data_rate *rate;
    bool retval = false;
    
    if(self->nextChannel != -1){
    
        rate = Region_getDataRateParameters(self->region, self->rate);
        
        LORA_ASSERT(rate != NULL)
        
        setting->freq = self->channels[self->nextChannel].freq;
        setting->sf = rate->sf;
        setting->bw = rate->bw;
        setting->cr = CR_5;
        //setting->erp = self->erp;
        
        retval = true;
    }
        
    return retval;
}

bool ChannelList_rx1Setting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    LORA_ASSERT(self != NULL)
    
    const struct lora_data_rate *rate;
    uint8_t rx1_rate;    
    bool retval = false;
    
    if(ChannelList_txSetting(self, setting)){
        
        setting->freq = self->channels[self->nextChannel].freq;
        
        (void)Region_getRX1DataRate(self->region, self->rate, self->rx1_offset, &rx1_rate);
        
        rate = Region_getDataRateParameters(self->region, rx1_rate);
        
        LORA_ASSERT(rate != NULL)
        
        setting->sf = rate->sf;
        setting->bw = rate->bw;        
        setting->cr = CR_5;
        //setting->erp = self->erp;
        
        retval = true;
    }
    
    return retval;
}

bool ChannelList_rx2Setting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    LORA_ASSERT(self != NULL)
        
    const struct lora_data_rate *rate;
    const struct lora_region_default *defaults;
    bool retval = false;
    
    if(ChannelList_upstreamSetting(self, setting)){
        
        defaults = Region_getDefaultSettings(self->region);
        
        LORA_ASSERT(defaults != NULL)
        
        rate = Region_getDataRateParameters(self->region, defaults->rx2_rate);
        
        LORA_ASSERT(rate != NULL)
        
        setting->freq = defaults->rx2_freq;
        setting->sf = rate->sf;
        setting->bw = rate->bw;
        setting->cr = CR_5;
        //setting->erp = self->erp;
        
        retval = true;        
    }
    
    return retval;
}

const struct lora_region *ChannelList_region(const struct lora_channel_list *self)
{
    LORA_ASSERT(self != NULL)
    
    return self->region;
}

/* static functions ***************************************************/

static uint32_t calculateOnAirTime(const struct lora_channel_list *self, uint8_t payloadLen)
{
    /* from 4.1.1.7 of sx1272 datasheet
     *
     * Ts (symbol period)
     * Rs (symbol rate)
     * PL (payload length)
     * SF (spreading factor
     * CRC (presence of trailing CRC)
     * IH (presence of implicit header)
     * DE (presence of data rate optimize)
     * CR (coding rate 1..4)
     * 
     *
     * Ts = 1 / Rs
     * Tpreamble = ( Npreamble x 4.25 ) x Tsym
     *
     * Npayload = 8 + max( ceil[( 8PL - 4SF + 28 + 16CRC + 20IH ) / ( 4(SF - 2DE) )] x (CR + 4), 0 )
     *
     * Tpayload = Npayload x Ts
     *
     * Tpacket = Tpreamble + Tpayload
     *
     * Implementation details:
     *
     * - period will be in microseconds so we can use integer operations rather than float
     * 
     * */

    uint32_t Tpacket = 0U;

    struct lora_channel_setting settings; 

    (void)ChannelList_txSetting(self, &settings);

    if((settings.bw != BW_FSK) && (settings.sf != SF_FSK)){

        // for now hardcode this according to this recommendation
        bool lowDataRateOptimize = ((settings.bw == BW_125) && ((settings.sf == SF_11) || (settings.sf == SF_12))) ? true : false;
        bool crc = true;    // true for uplink, false for downlink
        bool header = (settings.sf == SF_6) ? false : true; 

        uint32_t Ts = ((1U << settings.sf) * 1000000U) / settings.bw;     //symbol rate (us)
        uint32_t Tpreamble = (Ts * 12U) +  (Ts / 4U);       //preamble (us)

        uint32_t numerator = (8U * (uint32_t)payloadLen) - (4U * (uint32_t)settings.sf) + 28U + ( crc ? 16U : 0U ) - ( (header) ? 20U : 0U );
        uint32_t denom = 4U * ((uint32_t)settings.sf - ( lowDataRateOptimize ? 2U : 0U ));

        uint32_t Npayload = 8U + ((numerator / denom) + (((numerator % denom) != 0) ? 1U : 0U)) * ((uint32_t)settings.cr + 4U);

        uint32_t Tpayload = Npayload * Ts;

        Tpacket = Tpreamble + Tpayload;
    }

    return Tpacket;
}

static void cycleChannel(struct lora_channel_list *self)
{
    uint32_t nextTime = UINT32_MAX;
    bool atLeastOneChannel = false;
    int channelCount;
    int bandCount;
    const int numChannels = (int)(sizeof(self->channels)/sizeof(*self->channels));

    self->nextBand = -1;

    // find the next available band with an unmasked channel
    for(bandCount=0; bandCount < (int)(sizeof(self->bands)/sizeof(*self->bands)); bandCount++){

        if(self->bands[bandCount].numUnmasked > 0){

            if(self->bands[bandCount].timeReady < nextTime){

                nextTime = self->bands[bandCount].timeReady;
                atLeastOneChannel = true;
                self->nextBand = bandCount;
            }
        }
    }

    // choose the next channel on the next band
    if(atLeastOneChannel){

        self->nextChannel = (self->bands[self->nextBand].channel == -1) ? 0 : self->bands[self->nextBand].channel + 1;
        
        for(channelCount=0; channelCount < numChannels; channelCount++, self->nextChannel++){

            if(self->nextChannel >= numChannels){

                self->nextChannel = 0;
            }

            if((self->channels[self->nextChannel].freq != 0U) && (self->channels[self->nextChannel].band == self->nextBand) && !self->channels[self->nextChannel].masked){

                self->bands[self->nextBand].channel = self->nextChannel;
                break;
            }
        }

        // if there is a band, there must be a channel
        LORA_ASSERT(channelCount != numChannels)

        // failsafe if asserts are disabled
        if(channelCount == numChannels){

            self->bands[self->nextBand].channel = -1;
            self->nextChannel = -1;
            self->nextBand = -1;
        }
    }
    else{

        self->nextBand = -1;
        self->nextChannel = -1;
    }
}

static void addDefaultChannel(void *receiver, uint8_t chIndex, uint32_t freq)
{
    struct lora_channel_list *self = (struct lora_channel_list *)receiver;    
    if(!ChannelList_add(self, chIndex, freq)){
        
        LORA_ERROR("could not add default channel")
        LORA_ASSERT(false)
    }
}
