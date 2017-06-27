#include "lora_channel_list.h"
#include "lora_debug.h"

#include <string.h>

/* static prototypes **************************************************/

#if 0
static int findChannelByFrequency(const struct lora_channel_list *self, uint32_t freq);
#endif

static uint32_t calculateOnAirTime(const struct lora_channel_list *self, uint8_t payloadLen);
static void cycleChannel(struct lora_channel_list *self);

/* functions **********************************************************/

void ChannelList_init(struct lora_channel_list *self, enum lora_region_id region)
{
    (void)memset(self, 0, sizeof(*self));

    size_t i;

    for(i=0; i < sizeof(self->bands)/sizeof(*self->bands); i++){

        self->bands[i].channel = -1;
    }

    self->region = region;
    self->nextBand = -1;
    self->nextChannel = -1;
    self->numUnmasked = 0;
    self->rateParameters = NULL;

    switch(region){
    case EU_863_870:

        /* add the mandatory channels */
        (void)ChannelList_add(self, 0, 868100000U);
        (void)ChannelList_add(self, 1, 868300000U);
        (void)ChannelList_add(self, 2, 868500000U);

        /* choose a default rate and power (DR3, 14dBm) */
        (void)ChannelList_setRateAndPower(self, 3U, 1U);
        break;
    
    case US_902_928:        
    case CN_779_787:
    case EU_433:
    case AUSTRALIA_915_928:
    case CN_470_510:
    case AS_923:
    case KR_920_923:
    default:
        break;
    }
}

bool ChannelList_add(struct lora_channel_list *self, uint8_t chIndex, uint32_t freq)
{
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

            if(LoraRegion_validateFrequency(self->region, freq, &band)){

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
    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        if(self->channels[chIndex].freq > 0){
    
            self->channels[chIndex].freq = 0U;        
            
            if(!self->channels[chIndex].masked){
                self->bands[self->channels[chIndex].band].numUnmasked--;
                self->numUnmasked--;
            }
            if(self->nextChannel == chIndex){ cycleChannel(self); }
        }
    }
}

bool ChannelList_mask(struct lora_channel_list *self, uint8_t chIndex)
{
    bool retval = false;
    
    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        if(self->channels[chIndex].freq > 0U){
        
            if(!self->channels[chIndex].masked){

                self->channels[chIndex].masked = true;
                self->bands[self->channels[chIndex].band].numUnmasked--;
                self->numUnmasked--;

                if(self->nextChannel == (int)chIndex){ cycleChannel(self); }
            }
        
            retval = true;
        }       
    }
        
    return retval;
}

void ChannelList_unmask(struct lora_channel_list *self, uint8_t chIndex)
{
    if(chIndex < sizeof(self->channels)/sizeof(*self->channels)){
        
        self->channels[chIndex].masked = false;
        self->bands[self->channels[chIndex].band].numUnmasked++;
        self->numUnmasked++;

        if(self->numUnmasked == 1){ cycleChannel(self); }
    }
}

bool ChannelList_setRateAndPower(struct lora_channel_list *self, uint8_t rate, uint8_t power)    
{
    bool retval = false;
    const struct data_rate *param;

    param = LoraRegion_getDataRateParameters(self->region, rate);

    if(param != NULL){

        self->rateParameters = param;
        retval = true;
    }        

    return retval;
}

#if 0
uint32_t ChannelList_frequency(const struct lora_channel_list *self)
{
    return (self->nextChannel == -1) ? 0U : self->channels[self->nextChannel].freq;
}
#endif

uint32_t ChannelList_waitTime(const struct lora_channel_list *self, uint32_t timeNow)
{
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
    struct lora_adr_ans retval = {
        .channelOK = false,
        .rateOK = false,
        .powerOK = false    
    };

    return retval;
}

#if 0
uint8_t ChannelList_maxPayload(const struct lora_channel_list *self)
{
    return (self->rateParameters != NULL) ? self->rateParameters->payload : 0U;
}
#endif

void ChannelList_registerTransmission(struct lora_channel_list *self, uint32_t timeNow, uint8_t payloadLen)
{
    if(self->nextBand != -1){

        uint16_t offTimeFactor;
        LoraRegion_getOffTimeFactor(self->region, self->nextBand, &offTimeFactor);

        uint32_t onTime_us = calculateOnAirTime(self, payloadLen);
    
        uint64_t offTime_us = (uint64_t)onTime_us * (uint64_t)offTimeFactor;

        self->bands[self->nextBand].timeReady = timeNow + (offTime_us / 1000U) + ((offTime_us % 1000U) ? 1U : 0U);
        cycleChannel(self);
    }
}

#if 0
enum spreading_factor ChannelList_sf(const struct lora_channel_list *self)
{
    return (self->rateParameters != NULL) ? self->rateParameters->sf : SF_10;    
}
#endif

#if 0
enum signal_bandwidth ChannelList_bw(const struct lora_channel_list *self)
{
    return (self->rateParameters != NULL) ? self->rateParameters->bw : BW_125;    
}
#endif

#if 0
enum erp_setting ChannelList_erp(const struct lora_channel_list *self)
{
    return self->erp;
}
#endif

#if 0
enum coding_rate ChannelList_cr(const struct lora_channel_list *self)
{
    return CR_5;
}
#endif

size_t ChannelList_capacity(const struct lora_channel_list *self)
{
    return sizeof(self->channels)/sizeof(*self->channels);
}

bool ChannelList_upstreamSetting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    bool retval = false;
    
    if((self->nextChannel != -1) && (self->rateParameters != NULL)){
    
        setting->freq = self->channels[self->nextChannel].freq;
        setting->sf = self->rateParameters->bw;
        setting->bw = self->rateParameters->bw;
        setting->erp = self->erp;
        setting->cr = CR_5;
    
        retval = true;
    }
        
    return retval;
}

bool ChannelList_rx1Setting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    bool retval = false;
    
    if(ChannelList_upstreamSetting(self, setting)){
        
        // work out based on region
        retval = true;
    }
    
    return retval;
}

bool ChannelList_rx2Setting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    bool retval = false;
    
    if(ChannelList_upstreamSetting(self, setting)){
        
        // work out based on region
        retval = true;
    }
    
    return retval;
}

bool ChannelList_beaconSetting(const struct lora_channel_list *self, struct lora_channel_setting *setting)
{
    return false;
}

enum lora_region_id ChannelList_region(const struct lora_channel_list *self)
{
    return self->region;
}

/* static functions ***************************************************/

#if 0
static int findChannelByFrequency(const struct lora_channel_list *self, uint32_t freq)
{
    int retval = -1;
    size_t i;

    if(freq > 0U){

        for(i=0U; i < sizeof(self->channels)/sizeof(*self->channels); i++){

            if(self->channels[i].freq == freq){

                retval = (int)i;
                break;
            }
        }   
    }

    return retval;
}
#endif

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

    (void)ChannelList_upstreamSetting(self, &settings);

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
