#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "player.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "mad.h"
#include "stream.h"
#include "frame.h"
#include "synth.h"

#include "esp_spiffs.h"
#include "i2s.h"

#include "fcntl.h"
#include "unistd.h"


int fd = -1;

#define READBUFSZ (2106)
static char readBuf[READBUFSZ];
track_t *playlist;
int player_run = 0;
TaskHandle_t xTaskPlayerNotify;

OUT_CB out_callback = NULL;
//Reformat the 16-bit mono sample to a format we can send to I2S.
static int to_i2s(short s) {
    //We can send a 32-bit sample to the I2S subsystem and the DAC will neatly split it up in 2
    //16-bit analog values, one for left and one for right.

    //Duplicate 16-bit sample to both the L and R channel
    int samp = s;
    samp = (samp) & 0xffff;
    samp = (samp << 16) | samp;
    return samp;
}
//Routine to print out an error
static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
    printf("dec err 0x%04x (%s)\n", stream->error, mad_stream_errorstr(stream));
    return MAD_FLOW_CONTINUE;
}


void set_dac_sample_rate(int rate)
{
    // printf("\n");
}

static track_t *track_last(void)
{
    track_t *found = playlist;
    while(found->next != NULL) {
        found = found->next;
    }
    return found;
}

static track_t *track_first(void)
{
    return playlist->next;
}


static track_t *track_add(const char *song)
{
    track_t *found = track_last();
    if(found == NULL) {
        printf("Not found last track\n");
        return NULL;
    }
    track_t *new_track = malloc(sizeof(track_t));
    if(new_track == NULL) {
        printf("Not enough memory\n");
        return NULL;
    }
    int song_len = strlen(song);
    memset(new_track, 0, sizeof(track_t));
    new_track->filename = malloc(song_len + 1);
    if(new_track->filename == NULL) {
        free(new_track);
        return NULL;
    }
    strcpy(new_track->filename, song);
    new_track->filename[song_len + 1] = 0;

    found->next = new_track;
    new_track->prev = found;
    printf("added: %s, found: %x, new_track: %x\n", song, (int)found, (int)new_track);
    return new_track;
}

static void track_remove(track_t *song)
{
    if(song == NULL)
        return;
    track_t *prev = song->prev;
    track_t *next = song->next;
    if(prev != NULL) {
        prev->next = next;
    }
    if(next != NULL) {
        next->prev = prev;
    }
    free(song->filename);
    free(song);
}

static void track_clear(void)
{
    track_t *found;
    while((found = track_last()) != NULL) {
        if(found->prev == NULL) {
            break;
        }
        track_remove(found);
    }

}
void render_sample_block(short *short_sample_buff, int no_samples)
{
    int i;
    int samp;
    if(out_callback)
        out_callback(short_sample_buff, no_samples*2);
    for(i = 0; i < no_samples; i++) {

        samp = to_i2s(short_sample_buff[i]);
        // printf("%x ", samp);
        i2s_push_sample(samp);
    }
}

static enum mad_flow input(struct mad_stream *stream) {
    int n;
    int rem, read_bytes;
    //Shift remaining contents of buf to the front
    rem = stream->bufend - stream->next_frame;
    memmove(readBuf, stream->next_frame, rem);

    n = (sizeof(readBuf) - rem);

    read_bytes = read(fd, readBuf + rem, n);
    if(read_bytes < 0) {
        return MAD_FLOW_STOP;
    }
    //Okay, let MAD decode the buffer.
    mad_stream_buffer(stream, (unsigned char*)readBuf, read_bytes + rem);
    // printf(".");
    return MAD_FLOW_CONTINUE;
}

static void play(const char *song)
{
    int r;
    struct mad_stream *stream;
    struct mad_frame *frame;
    struct mad_synth *synth;

    //Allocate structs needed for mp3 decoding
    stream = malloc(sizeof(struct mad_stream));
    frame = malloc(sizeof(struct mad_frame));
    synth = malloc(sizeof(struct mad_synth));

    if(stream == NULL) { printf("MAD: malloc(stream) failed, %d\n", xPortGetFreeHeapSize()); return; }
    if(synth == NULL) { printf("MAD: malloc(synth) failed, %d\n", xPortGetFreeHeapSize()); return; }
    if(frame == NULL) { printf("MAD: malloc(frame) failed, %d\n", xPortGetFreeHeapSize()); return; }

    //Initialize I2S

    printf("MAD: Decoder start., %d\n", xPortGetFreeHeapSize());
    //Initialize mp3 parts
    mad_stream_init(stream);
    mad_frame_init(frame);
    mad_synth_init(synth);
    //openfile
    fd = open(song, O_RDONLY);

    if(fd < 0) {
        printf("Error opening file\n");
        return;
    }
    enum mad_flow m = MAD_FLOW_CONTINUE;
    while(m == MAD_FLOW_CONTINUE) {
        m = input(stream); //calls mad_stream_buffer internally
        while(1) {
            r = mad_frame_decode(frame, stream);
            if(r == -1) {
                if(!MAD_RECOVERABLE(stream->error)) {
                    //We're most likely out of buffer and need to call input() again
                    break;
                }
                error(NULL, stream, frame);
                continue;
            }
            mad_synth_frame(synth, frame);
        }
    }
    free(stream);
    free(frame);
    free(synth);
    close(fd);
    fd = -1;

}

void player_task(void *pv)
{
    track_t *song;
    printf("esp_spiffs_mount,%d\n", xPortGetFreeHeapSize());
    esp_spiffs_init();
    if(esp_spiffs_mount() != SPIFFS_OK) {
        printf("Error mount SPIFFS\n");
        return;
    }
    printf("Begin player task\n");

    while(player_run) {
        printf("wait decode...%d\n", xPortGetFreeHeapSize());
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while((song = track_first()) != NULL) {
            printf("playing: %s\n", song->filename);
            play(song->filename);
            track_remove(song);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    track_clear();
    i2s_stop();
    vTaskDelete(NULL);
}

int player_init(int sample_rate, OUT_CB cb)
{
    out_callback = cb;
    playlist = malloc(sizeof(track_t));
    if(playlist == NULL)
        return -1;
    memset(playlist, 0, sizeof(track_t));
    player_run = 1;

    i2s_init(sample_rate);
    if(xTaskCreate(&player_task, "player_task", 2300, NULL, 5, &xTaskPlayerNotify) == pdFALSE) {
        printf("Error init player_task\n");
    }
    return 0;
}

void player_stop()
{
    player_run = 0;
    i2s_stop();
}
void player_play(const char *file)
{
    track_add(file);
    xTaskNotifyGive(xTaskPlayerNotify);
}
