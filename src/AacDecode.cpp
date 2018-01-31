//
// Created by root on 8/14/17.
//

#include <stdio.h>
#include "AacDecode.h"
AacDecode::AacDecode() {
    decoder_inited = 0;
    test_aac_file = fopen("/home/foscam/svn/dec.pcm", "wb");
}
AacDecode::~AacDecode() {
    decoder_inited = 0;
    if(decoder  != NULL) {
        NeAACDecClose(decoder);
        decoder  = NULL;
    }
    if (test_aac_file != NULL) {
        fclose(test_aac_file);
        test_aac_file = NULL;
    }

};

int AacDecode::Init(uint8_t *frame,  uint32_t size) {
    //open decoder
    decoder = NeAACDecOpen();
    if (!decoder ) {
        fprintf(stderr, "open decoder error\n");
        return -1;
    }

    //initialize decoder
    NeAACDecInit(decoder, frame,  size,  &samplerate, &channels);
    printf("samplerate %d, channels %d\n", samplerate, channels);
    decoder_inited = 1;
}


int AacDecode::Decode(uint8_t *frame,  uint32_t size, uint8_t** pcm_data) {
    if (decoder_inited == 0) {
        Init(frame, size);
    }
    if (decoder) {
        *pcm_data = (unsigned char *) NeAACDecDecode(decoder, &frame_info, frame, size);
        if (frame_info.error > 0) {
           fprintf(stderr, "%s[%s:%d]\n", NeAACDecGetErrorMessage(frame_info.error), __FILE__,__LINE__);
            fprintf(stderr, "pcm_data = %p, frame_info.samples=%d, decoder=%p, size=%d, frame: %02x %02x %02x %02x %02x %02x %02x\n ",
            *pcm_data, frame_info.samples, decoder, size, frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);
            return 0;
        } else if (pcm_data && frame_info.samples > 0) {
           // printf("frame info: pcm_data = %p bytesconsumed %d, channels %d, header_type %d object_type %d, samples %d, samplerate %d\n",*pcm_data,
            //           frame_info.bytesconsumed, frame_info.channels, frame_info.header_type, frame_info.object_type, frame_info.samples, frame_info.samplerate);
            fwrite(*pcm_data, frame_info.samples * frame_info.channels, 1, test_aac_file);
            return  frame_info.samples * frame_info.channels;
        } else {
            printf("other error pcm_data = %p\n",*pcm_data);
            //DEBUG_INFO(STORAGE, "pcm_data = %p, frame_info.samples=%d, decoder=%p, size=%d, frame: %02x %02x %02x %02x %02x %02x %02x ",
            //pcm_data, frame_info.samples, decoder, size, frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);
        }
        return 0;
    } else {
        return -1;
    }

}