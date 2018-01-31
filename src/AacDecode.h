//
// Created by root on 8/14/17.
//

#ifndef MP4DEMUX_AUDIOCODEC_H
#define MP4DEMUX_AUDIOCODEC_H

#include <stdint.h>
#include "faad.h"
class AacDecode {
public:
    AacDecode();
    ~AacDecode();
    int Init(uint8_t *frame,  uint32_t size);
    int Decode(uint8_t *frame,  uint32_t size, uint8_t** pcm_data);

private:
    int decoder_inited = 0;
    unsigned long samplerate;
    unsigned char channels;
    NeAACDecHandle decoder;
    NeAACDecFrameInfo frame_info;
    FILE *test_aac_file;
};


#endif //MP4DEMUX_AUDIOCODEC_H
