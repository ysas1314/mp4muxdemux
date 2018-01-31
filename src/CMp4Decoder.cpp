//
// Created by root on 8/14/17.
//

#include <string.h>
#include <stdlib.h>
#include "CMp4Decoder.h"

CMp4Decoder::CMp4Decoder() {
    //test_aac_file = fopen("/home/foscam/svn/yyy.aac", "wb");
    m_current_track = 0;
    m_track_cnt = 0;

}

CMp4Decoder::~CMp4Decoder(){
    if (m_file_handle != NULL) {
        MP4Close(m_file_handle);
        m_file_handle = NULL;
    }
}

/*  */
int CMp4Decoder::OpenMp4File(const char *fileName) {
    m_file_handle = MP4Read(fileName);
    if (m_file_handle == MP4_INVALID_FILE_HANDLE) {
        fprintf(stderr, "open mp4 file:%s error\n", fileName);
        return MP4DECODER_ERR_OPEN_FILE;
    }

    m_track_cnt = MP4GetNumberOfTracks(m_file_handle, NULL, 0);
    printf("track cnt=%d\n", m_track_cnt);
    for (int i = 0; i < m_track_cnt && i< MAX_TRACK_CNT; ++i) {
        trackInfo[i].track_id = MP4FindTrackId(m_file_handle, i, NULL, 0);
        sprintf( (char *)trackInfo[i].track_type, "%s", MP4GetTrackType(m_file_handle, trackInfo[i].track_id));
        trackInfo[i].samples_cnt = MP4GetTrackNumberOfSamples(m_file_handle, trackInfo[i].track_id);
        trackInfo[i].track_bps =  MP4GetTrackBitRate(m_file_handle, trackInfo[i].track_id);
        trackInfo[i].samples_index = 0;
         if (MP4_IS_VIDEO_TRACK_TYPE((char *)trackInfo[i].track_type)) {
             unsigned char **seqheader = NULL;
             unsigned char **pictheader;
             unsigned int *pictheadersize;
             unsigned int *seqheadersize;
             uint32_t ix;
             MP4GetTrackH264SeqPictHeaders(m_file_handle, trackInfo[i].track_id, &seqheader, &seqheadersize, &pictheader, &pictheadersize);
             trackInfo[i].video.fps = MP4GetTrackVideoFrameRate(m_file_handle, trackInfo[i].track_id);
             trackInfo[i].video.width = MP4GetTrackVideoWidth(m_file_handle, trackInfo[i].track_id);
             trackInfo[i].video.height = MP4GetTrackVideoHeight(m_file_handle, trackInfo[i].track_id);

             for (ix = 0; seqheadersize[ix] != 0; ix++) {
                 memcpy(trackInfo[i].video.sps, seqheader[ix], seqheadersize[ix]);
                 trackInfo[i].video.spslen = seqheadersize[ix];
                 free(seqheader[ix]);
             }
             free(seqheader);
             free(seqheadersize);
             for (ix = 0; pictheadersize[ix] != 0; ix++) {
                 memcpy(trackInfo[i].video.pps, pictheader[ix], pictheadersize[ix]);
                 trackInfo[i].video.ppslen = pictheadersize[ix];
                 free(pictheader[ix]);
             }
             free(pictheader);
             free(pictheadersize);
             printf("[Video] track id:%d sample_cnt:%d heith:%d width:%d fps:%f bps:%d\n",
                    trackInfo[i].track_id, trackInfo[i].samples_cnt,trackInfo[i].video.height, trackInfo[i].video.width, trackInfo[i].video.fps,trackInfo[i].track_bps);
        } else if (MP4_IS_AUDIO_TRACK_TYPE((char *)trackInfo[i].track_type)) {
            trackInfo[i].audio.type = MP4GetTrackAudioMpeg4Type(m_file_handle, trackInfo[i].track_id);
            trackInfo[i].audio.timeScale = MP4GetTrackTimeScale(m_file_handle, trackInfo[i].track_id);
            trackInfo[i].audio.ch = MP4GetTrackAudioChannels(m_file_handle, trackInfo[i].track_id);
             printf("[Audio] track id:%d sample_cnt:%d type:%d timeScale:%d ch:%d bps:%d\n",
                    trackInfo[i].track_id, trackInfo[i].samples_cnt,trackInfo[i].audio.type, trackInfo[i].audio.timeScale, trackInfo[i].audio.ch, trackInfo[i].track_bps);
        }
    }
    return MP4DECODER_SUCESS;
}

/*  */
int CMp4Decoder::CloseMp4File(){
    if (m_file_handle != NULL) {
        MP4Close(m_file_handle);
        m_file_handle = NULL;
    }
}

int SetNal(uint8_t * buf) {
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x01;
    return 4;
}

#define ADTS_HEADER_SIZE 7

int adts_write_frame_header(uint8_t *buf, int pce_size, uint8_t protection_absent, uint8_t profile, uint32_t timeScale, int32_t ch) {
    int sample_rate_table[13] = {96000,88200,64000,48000,44100,32000,24000,22050
            ,16000,12000,11025,8000,7350};

    int sf_index = 0;
    for (int i = 0; i < 13; ++i) {
        if (sample_rate_table[i] == timeScale){
            sf_index = i;
            break;
        }
    }
    printf("sf_index=%d\n", sf_index);
    uint32_t aac_head_len = protection_absent == 1 ? 7 : 9;
    uint32_t aac_frame_length = (uint32_t) aac_head_len + pce_size;
    uint8_t *packet = buf;
    packet[0] = 0xff;
    packet[1] = (uint8_t) (0xf0 + protection_absent);

    packet[2] = (uint8_t) (((profile - 1) << 6) + (sf_index << 2) + (ch >> 2));
    packet[3] = (uint8_t) (((ch & 3) << 6) + (aac_frame_length >> 11));
    packet[4] = (uint8_t) ((aac_frame_length & 0x7FF) >> 3);
    packet[5] = (uint8_t) (((aac_frame_length & 7) << 5) + 0x1F);
    packet[6] = 0xFC;
    return aac_head_len;
}

/*  */
int CMp4Decoder::RequestFrame(uint8_t *buf, uint32_t &len) {
    uint8_t * pos = buf + sizeof(FsFrame_t);
    FsFrame_t * frame_head = (FsFrame_t*) buf;
    int32_t  ret = 0;
    int i = 0;
    int32_t frame_len = 0;
    unsigned char *pData = NULL;
    unsigned int nSize = 0;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;
    bool a = 0;
    bool *pIsSyncSample = &a;
    /* 1.Check all track is end */
    for (; i < m_track_cnt && i< MAX_TRACK_CNT; ++i) {
        if (trackInfo[i].samples_eof != 1) {
            break;
        }
    }

    if(i == m_track_cnt || i == MAX_TRACK_CNT) {
        return MP4DECODER_MP4_EOF;
    }
    m_current_track %= m_track_cnt;

    i = m_current_track;
    /* 2. */
    if (trackInfo[i].samples_index >= trackInfo[i].samples_cnt) {
        trackInfo[i].samples_eof = 1;
        m_current_track++;
        return ret;
    }
    frame_len = 0;
    pData = NULL;
    nSize = 0;

    if (MP4_IS_VIDEO_TRACK_TYPE( (const char*) trackInfo[i].track_type) ) {
        int rr = MP4ReadSample(m_file_handle, trackInfo[i].track_id, ++trackInfo[i].samples_index, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset, pIsSyncSample);
        if (rr == 0) {
            fprintf(stderr, "MP4ReadSample error\n");
        }
        //IDR 帧，写入sps pps先
        if (*pIsSyncSample) {
            SetNal(pos);
            pos += 4;
            memcpy(pos, trackInfo[i].video.sps, trackInfo[i].video.spslen );
            pos += trackInfo[i].video.spslen;

            SetNal(pos);
            pos += 4;
            memcpy(pos, trackInfo[i].video.pps, trackInfo[i].video.ppslen );
            pos += trackInfo[i].video.ppslen;
            //printf("video idr frame idx:%d\n", trackInfo[i].samples_index);
            frame_head->frameType = IFrame;
        } else {
            frame_head->frameType = PFrame;
        }
        m_i_slice_num++;
        //264frame
        if (pData && nSize > 4) {
            //标准的264帧，前面几个字节就是frame的长度.
            //需要替换为标准的264 nal 头.
            memcpy(pos, pData, nSize);
            SetNal(pos);
            pos += nSize;
        }
        frame_len = pos-buf-sizeof(FsFrame_t);
        // TODO: 帧头需要在这里进行填充
        frame_head->frameTag = FSFrameTag;
        frame_head->encrypted = 0;
        frame_head->encodeType = EncH264;
        frame_head-> frameNo = trackInfo[i].samples_index - 1;
        frame_head->dataLen = frame_len;
        frame_head->time = 0; //CAppTimer::getmSecond();
        frame_head->video.VFrameRate =  trackInfo[i].video.fps;
        frame_head->pts = (uint64_t)m_i_slice_num * trackInfo[i].video.fps *1000000;
        //printf("video pts:%lld", frame_head->pts);
        //pts=1000*(i_frame_counter*2+pic_order_cnt_lsb)* (time_scale/num_units_in_tick)
        //pts=1000*(i_frame_counter+ pic_order_cnt_lsb)*(time_scale/num_units_in_tick)
        frame_head->video.VBitRate =  trackInfo[i].track_bps;
        frame_head->video.VHeight = trackInfo[i].video.height;
        frame_head->video.VWidth = trackInfo[i].video.width;
        len = (uint32_t)frame_len;
    } else if (MP4_IS_AUDIO_TRACK_TYPE((const char*) trackInfo[i].track_type) ) {
        uint8_t aac_data[AAC_FRAME_MAX_LEN ];
        uint8_t *aac_pos = (uint8_t*)aac_data;
        uint8_t protection_absent = 1; /* no crc */
        uint8_t* pcm_data = NULL;
        aac_pos += ADTS_HEADER_SIZE;
        int rr = MP4ReadSample(m_file_handle, trackInfo[i].track_id, ++trackInfo[i].samples_index, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset, pIsSyncSample);
        if (rr == 0) {
            fprintf(stderr, "MP4ReadSample error\n");
        }
        if (*pIsSyncSample) {
            fprintf(stderr, "head.......................\n");
        } else {
            fprintf(stderr, "not head......................\n");
        }

        adts_write_frame_header((uint8_t*)aac_data, nSize, protection_absent, trackInfo[i].audio.type, trackInfo[i].audio.timeScale, trackInfo[i].audio.ch);
        if (pData && nSize > 0) {
            memcpy(aac_pos, pData, nSize);
            aac_pos += nSize;
        }
        frame_len = aacDecoder.Decode(aac_data, nSize + 7, &pcm_data);
        memcpy(pos, pcm_data, frame_len);
        frame_head->frameTag = FSFrameTag;
        frame_head->encrypted = 0;
        frame_head->encodeType = EncPCM;
        frame_head-> frameNo = trackInfo[i].samples_index - 1;
        frame_head->dataLen = frame_len;
        frame_head->time = 0; //CAppTimer::getmSecond();
        frame_head->frameType = AFrame;
        // 这里的base_clock（基本时钟频率），我取的1000000（纳秒）。
        //printf("frame len:%d, index=%d\n", frame_len,trackInfo[i].samples_index);
        frame_head->pts =(uint64_t)  1000000 * (trackInfo[i].samples_index) * frame_len / trackInfo[i].audio.timeScale;
        fprintf(stderr, "audio pts:%lld\n",  frame_head->pts);
        frame_head->audio.bitsPerSample = 0;
        frame_head->audio.sampleRate = trackInfo[i].audio.timeScale;
        len = (uint32_t)frame_len;
    }
    m_current_track++;
    return ret;
}