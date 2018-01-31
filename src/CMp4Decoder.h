//
// Created by root on 8/14/17.
//

#ifndef MP4DEMUX_CMP4DECODER_H
#define MP4DEMUX_CMP4DECODER_H

#include <stdint.h>
#include <mp4v2/mp4v2.h>
#include <memory.h>
#include <string.h>
#include "AacDecode.h"
#define ATTRIBUTE_PACKED __attribute__((packed))
#define ADTS_HEADER_SIZE                    7
#define AAC_FRAME_MAX_LEN               102400
#define MP4DECODER_SUCESS                           0

#define MP4DECODER_ERR_OPEN_FILE          -10001

#define MP4DECODER_MP4_EOF                      -20001
#define MAX_BUF_SIZE    4194304
#define FSFrameTag  *(uint32_t*)"FSFM"
#define IFrame		'I'
#define PFrame      'P'
#define AFrame      'A'
#define MFrame      'M'
#define CFrame      'C' //switch file ctrl Frame
#define SFrame      'S' //Seek frame, used to indicate drop all exist frame
#define EFrame      'E' //End frame, used to indicate all file played out
#define ZFrame      'Z' //ch zero control frame
#define XFrame      'X' //Smart record, video frame ignored
#define EncH264		'F'
#define EncMJ		'J'
#define EncPCM		'P'
#define EncADPCM	'p'
#define EncG726     'G'
#define EncG711     'g'
typedef struct _VideoHead_ {
    uint16_t VWidth;
    uint16_t VHeight;
    uint32_t VFrameRate;
    uint32_t VBitRate;
} ATTRIBUTE_PACKED VideoHead_t;

typedef struct AudioHead_t {
    uint32_t sampleRate;
    uint32_t bitsPerSample;
} ATTRIBUTE_PACKED AudioHead_t;

typedef struct _FsFrame_ {
    char mac[16]; //for bpi when adpterfs rcv data wirte mac here
    uint32_t frameTag;
    uint8_t encrypted;
    uint8_t encodeType;
    uint8_t frameType;
    uint8_t channel;
    uint32_t frameNo;
    int32_t dataLen;
    int64_t pts;
    int64_t time;
    union {
        VideoHead_t video;
        AudioHead_t audio;
    };
    uint8_t fillLen;
    uint32_t frameIdx;
    uint8_t reserved[3];
    int8_t data[0];
} ATTRIBUTE_PACKED FsFrame_t;




#define MAX_TRACK_CNT 8



typedef struct AudioTrack_s {
    int8_t adts_header[ADTS_HEADER_SIZE];
    int8_t adts_header_got;
    uint8_t type;
    uint32_t timeScale;
    int32_t ch;
//    AudioTrack_s() {
//        adts_header_got = 0;
//        type = 0;
//        timeScale = 0;
//        ch = 0;
//    }
}AudioTrack_t;


typedef struct VideoTrack_s {
    uint8_t sps[128];
    uint8_t pps[128];
    uint32_t  spslen;
    uint32_t ppslen;
    double fps;
    uint16_t height;
    uint16_t width;
//    VideoTrack_s() {
//        spslen = 0;
//        ppslen = 0;
//        height = 0;
//        width = 0;
//    }
}VideoTrack_t;

typedef struct TrackInfo_s {
    int8_t track_type[32];
    uint32_t track_id;
    uint32_t samples_index;
    uint32_t samples_cnt;
    uint8_t samples_eof;
    uint32_t track_bps;
    union {
        AudioTrack_t audio;
        VideoTrack_t video;
    };
//    TrackInfo_s() {
//        memset(track_type, 0, 32);
//        track_id = 0;
//        samples_index = 0;
//        samples_cnt = 0;
//        samples_eof = 0;
//    }
}ATTRIBUTE_PACKED TrackInfo_t;

class CMp4Decoder {
public:
    CMp4Decoder();

    ~CMp4Decoder();

    /*  */
    int OpenMp4File(const char *fileName);

    /*  */
    int CloseMp4File();

    /*  */
    int RequestFrame(uint8_t *frame, uint32_t &len);

private:
    MP4FileHandle m_file_handle;
    uint32_t m_track_cnt;
    TrackInfo_t trackInfo[MAX_TRACK_CNT];
    AacDecode aacDecoder;
    uint32_t m_current_track;
    uint32_t m_i_slice_num;
};


#endif //MP4DEMUX_CMP4DECODER_H
