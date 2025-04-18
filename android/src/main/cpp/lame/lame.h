#ifndef LAME_LAME_H
#define LAME_LAME_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lame_global_flags_tag {
    int dummy; // Placeholder for the actual structure
} lame_global_flags;

// Function declarations
lame_global_flags* lame_init(void);
int lame_set_num_channels(lame_global_flags* gfp, int channels);
int lame_set_in_samplerate(lame_global_flags* gfp, int samplerate);
int lame_set_brate(lame_global_flags* gfp, int brate);
int lame_set_quality(lame_global_flags* gfp, int quality);
int lame_set_VBR(lame_global_flags* gfp, int vbr);
int lame_init_params(lame_global_flags* gfp);
int lame_encode_buffer(lame_global_flags* gfp, short* buffer_l, short* buffer_r, int nsamples, unsigned char* mp3buf, int mp3buf_size);
int lame_encode_buffer_interleaved(lame_global_flags* gfp, short* pcm, int num_samples, unsigned char* mp3buf, int mp3buf_size);
int lame_encode_flush(lame_global_flags* gfp, unsigned char* mp3buf, int mp3buf_size);
int lame_close(lame_global_flags* gfp);

// VBR modes
#define vbr_off 0

#ifdef __cplusplus
}
#endif

#endif // LAME_LAME_H 