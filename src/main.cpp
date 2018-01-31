#include <stdint.h>
#include "mp4v2/mp4v2.h"
#include <string.h>
#include <stdlib.h>
#include "CMp4Decoder.h"
#include <unistd.h>
unsigned char sps[64], pps[64];
int spslen = 0, ppslen = 0;
int get_264_stream(MP4FileHandle oMp4File, int VTrackId, int totalFrame, const char *out_file) {
    if (!oMp4File)
        return -1;
    char NAL[5] = {0x00, 0x00, 0x00, 0x01};
    unsigned char *pData = NULL;
    unsigned int nSize = 0;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;
    bool pIsSyncSample = 0;

    int nReadIndex = 0;
    FILE *pFile = NULL;
    pFile = fopen(out_file, "wb");

    while (nReadIndex < totalFrame) {
        nReadIndex++;
        //printf("nReadIndex:%d\n",nReadIndex);
        MP4ReadSample(oMp4File, VTrackId, nReadIndex, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset, &pIsSyncSample);

        //IDR? 帧，写入sps pps先
        if (pIsSyncSample) {
            fwrite(NAL, 4, 1, pFile);
            fwrite(sps, spslen, 1, pFile);

            fwrite(NAL, 4, 1, pFile);
            fwrite(pps, ppslen, 1, pFile);

        }
        //264frame
        if (pData && nSize > 4) {
            //标准的264帧，前面几个字节就是frame的长度.
            //需要替换为标准的264 nal 头.
            pData[0] = 0x00;
            pData[1] = 0x00;
            pData[2] = 0x00;
            pData[3] = 0x01;
            fwrite(pData, nSize, 1, pFile);
        }

        //如果传入MP4ReadSample的视频pData是null
        // 它内部就会new 一个内存
        //如果传入的是已知的内存区域，
        //则需要保证空间bigger then max frames size.
        free(pData);
        pData = NULL;
    }
    fflush(pFile);
    fclose(pFile);
    return 0;
}


#define ADTS_HEADER_SIZE 7

int adts_write_frame_header1(uint8_t *buf, int pce_size, uint8_t protection_absent, uint8_t profile, uint32_t timeScale, int32_t ch) {

    int sample_rate_table[13] = {96000,88200,64000,48000,44100,32000,24000,22050
            ,16000,12000,11025,8000,7350};

    int sf_index = 0;
    for (int i = 0; i < 13; ++i) {
        if (sample_rate_table[i] == timeScale){
            sf_index = i;
            break;
        }
    }

    uint32_t aac_frame_length = (uint32_t) (protection_absent == 1 ? 7 : 9) + pce_size;
    uint8_t *packet = buf;
    packet[0] = 0xff;
    packet[1] = (uint8_t) (0xf0 + protection_absent);

    packet[2] = (uint8_t) (((profile - 1) << 6) + (sf_index << 2) + (ch >> 2));
    packet[3] = (uint8_t) (((ch & 3) << 6) + (aac_frame_length >> 11));
    packet[4] = (uint8_t) ((aac_frame_length & 0x7FF) >> 3);
    packet[5] = (uint8_t) (((aac_frame_length & 7) << 5) + 0x1F);
    packet[6] = 0xFC;
    return 0;
}


int get_aac_stream(MP4FileHandle oMp4File, MP4TrackId VTrackId, int totalFrame, const char *out_file) {

    if (!oMp4File)
        return -1;
    unsigned char *pData = NULL;
    unsigned int nSize = 0;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;
    bool pIsSyncSample = 0;

    uint32_t nReadIndex = 0;
    FILE *pFile = NULL;
    pFile = fopen(out_file, "wb");

    uint8_t protection_absent = 1; /* no crc */
    //uint8_t  type = MP4GetTrackEsdsObjectTypeId(oMp4File, VTrackId);
    uint8_t type = MP4GetTrackAudioMpeg4Type(oMp4File, VTrackId);
    uint32_t timeScale = MP4GetTrackTimeScale(oMp4File, VTrackId);
    int32_t ch = MP4GetTrackAudioChannels(oMp4File, VTrackId);

    while (nReadIndex < totalFrame) {
        nReadIndex++;
        MP4ReadSample(oMp4File, VTrackId, nReadIndex, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset, &pIsSyncSample);

        /* adts header */
        if (pIsSyncSample) {
            uint8_t buf[ADTS_HEADER_SIZE];
            adts_write_frame_header1(buf, nSize, protection_absent, type, timeScale, ch);
            fwrite(buf, ADTS_HEADER_SIZE, 1, pFile);
        }
        // aac frame
        if (pData && nSize > 0) {
            fwrite(pData, nSize, 1, pFile);
        }
        //如果传入MP4ReadSample的视频pData是null
        // 它内部就会new 一个内存
        //如果传入的是已知的内存区域，
        //则需要保证空间bigger then max frames size.
        free(pData);
        pData = NULL;
    }
    fflush(pFile);
    fclose(pFile);

    return 0;
}

int demuxMp4() {
    const char src_mp4[] = "/root/work/help1.mp4";
    char dst_h264[] = "/root/work/help1.h264";
    char dst_aac[] = "/root/work/help1.aac";

    MP4FileHandle mp4_file_handle = MP4Read(src_mp4);
    if (mp4_file_handle == MP4_INVALID_FILE_HANDLE) {
        printf("open error!!!!!");
        exit(0);
    }

    uint32_t frame_cnt = 0;
    uint32_t samples_cnt = 0;
    int32_t video_index = -1;
    MP4TrackId trackId = MP4_INVALID_TRACK_ID;
    uint32_t track_cnt = MP4GetNumberOfTracks(mp4_file_handle, NULL, 0);
    printf("track cnt=%d\n", track_cnt);

    for (int i = 0; i < track_cnt; ++i) {
        trackId = MP4FindTrackId(mp4_file_handle, i, NULL, 0);
        const char *track_type = MP4GetTrackType(mp4_file_handle, trackId);
        if (MP4_IS_VIDEO_TRACK_TYPE(track_type)) {
            unsigned char **seqheader = NULL;
            unsigned char **pictheader;
            unsigned int *pictheadersize;
            unsigned int *seqheadersize;
            uint32_t ix;
            printf("video_index=%d\n", trackId);
            samples_cnt = MP4GetTrackNumberOfSamples(mp4_file_handle, trackId);

            frame_cnt = samples_cnt;
            MP4GetTrackH264SeqPictHeaders(mp4_file_handle, trackId, &seqheader, &seqheadersize, &pictheader, &pictheadersize);
            for (ix = 0; seqheadersize[ix] != 0; ix++) {
                memcpy(sps, seqheader[ix], seqheadersize[ix]);
                spslen = seqheadersize[ix];
                free(seqheader[ix]);
            }
            free(seqheader);
            free(seqheadersize);

            for (ix = 0; pictheadersize[ix] != 0; ix++) {
                memcpy(pps, pictheader[ix], pictheadersize[ix]);
                ppslen = pictheadersize[ix];
                free(pictheader[ix]);
            }
            free(pictheader);
            free(pictheadersize);

            if (trackId >= 0) {
                get_264_stream(mp4_file_handle, trackId, frame_cnt, dst_h264);
            }

        } else if (MP4_IS_AUDIO_TRACK_TYPE(track_type)) {
            printf("audio_index=%d\n", trackId);
            samples_cnt = MP4GetTrackNumberOfSamples(mp4_file_handle, trackId);
            frame_cnt = samples_cnt;
            if (trackId >= 0) {
                get_aac_stream(mp4_file_handle, trackId, frame_cnt, dst_aac);
            }
        }
    }
    MP4Close(mp4_file_handle, 0);
}

#include "MP4Encoder.h"
int muxMp4() {
    MP4Encoder mp4Encoder;
    // convert H264 file to mp4 file
    mp4Encoder.WriteH264File("/root/work/t1.h264","/root/work/t1.mp4");
}
#include <time.h>
int main() {
#if 0
    const char src_mp4[] = "/home/foscam/svn/test/MDalarm_19700101_000211.mp4";
    char dst_h264[] = "/home/foscam/svn/test/help1.h264";
    char dst_aac[] = "/home/foscam/svn/test/help1.pcm";


    uint8_t frame[MAX_BUF_SIZE];
    uint32_t len = MAX_BUF_SIZE;
    CMp4Decoder mp4_dec;
    mp4_dec.OpenMp4File(src_mp4);

    FILE *fp264 = fopen(dst_h264, "wb");

    if (fp264 == NULL) {
        return 0;
    }

    FILE *fppcm = fopen(dst_aac, "wb");

    if (fppcm == NULL) {
        return 0;
    }

    for (int i = 0; 1; ++i) {
       int ret = mp4_dec.RequestFrame(frame, len);
        if (ret != 0) {
            printf("RequestFrame frame error\n");
            return 0;
        }
        FsFrame_t * frame_head = (FsFrame_t*) frame;
        if (frame_head->encodeType == EncH264) {
            fwrite(frame+sizeof(FsFrame_t), len, 1, fp264);
            //printf("video len=%d. header len:%d\n", len, sizeof(FsFrame_t) );
        } else if (frame_head->encodeType == EncPCM) {
            fwrite(frame+sizeof(FsFrame_t), len, 1, fppcm);
           // printf("pcm len=%d. header len:%d\n", len, sizeof(FsFrame_t) );
        }
        //printf("i = %d\n", i);
        //usleep(100000);
    }
    //demuxMp4();
    return 0;
#endif
#if 0
    char ssid[32] = {'\0'};
    char pwd[32] = {'\0'};
    int randNum;
    char pwdChar;
    srand(time(0));
    for(int i = 0; i < 12; i++)
    {
        randNum = rand();
        pwdChar = (randNum >> 8) % 62;
        if (pwdChar < 10)
        {
            pwd[i] = '0' + pwdChar;
        }
        else if(pwdChar < 36)
        {
            pwd[i] = 'a' + pwdChar - 10;
        }
        else
        {
            pwd[i] = 'A' + pwdChar - 36;
        }
    }

    for(int i = 0; i < 12; i++)
    {
        randNum = rand();
        pwdChar = (randNum >> 8) % 62;
        if (pwdChar < 10)
        {
            ssid[i] = '0' + pwdChar;
        }
        else if(pwdChar < 36)
        {
            ssid[i] = 'a' + pwdChar - 10;
        }
        else
        {
            ssid[i] = 'A' + pwdChar - 36;
        }
    }

    unsigned char ssidDec[256] = {'\0'};
    unsigned char pwdDec[256] = {'\0'};
    memcpy(ssidDec,ssid,sizeof(ssid));
    memcpy(pwdDec,pwd,sizeof(pwd));

    for (int j = 0; j < 32; ++j) {
        printf("%c",ssidDec[j]);
    }
    printf("\n");
    for (int k = 0; k < 32; ++k) {
        printf("%c",pwdDec[k]);
    }
    //demuxMp4();
#endif
    //demuxMp4();
    muxMp4();
}
