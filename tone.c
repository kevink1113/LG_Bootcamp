#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>

#define SAMPLE_RATE 16000
#define FRAME_SIZE 1024
#define MIN_PITCH_HZ 80
#define MAX_PITCH_HZ 1000

double detect_pitch_int(short *buffer, int size, int sample_rate) {
    int min_lag = sample_rate / MAX_PITCH_HZ;
    int max_lag = sample_rate / MIN_PITCH_HZ;

    int best_lag = -1;
    int64_t max_corr = 0;

    for (int lag = min_lag; lag <= max_lag; lag++) {
        int64_t corr = 0;
        for (int i = 0; i < size - lag; i++) {
            corr += (int32_t)buffer[i] * (int32_t)buffer[i + lag];
        }
        if (corr > max_corr) {
            max_corr = corr;
            best_lag = lag;
        }
    }

    if (best_lag > 0) {
        return (double)sample_rate / best_lag;
    } else {
        return 0.0;
    }
}

int main() {
    const char *device = "plughw:4,0"; // 마이크 장치
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    int pcm;

    short buffer[FRAME_SIZE];

    // ALSA PCM 캡처 장치 열기
    if ((pcm = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "ERROR: Cannot open PCM device %s: %s\n", device, snd_strerror(pcm));
        return 1;
    }

    // 하드웨어 파라미터 구조체 초기화
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1); // 모노
    snd_pcm_hw_params_set_rate(pcm_handle, params, SAMPLE_RATE, 0);
    snd_pcm_hw_params_set_period_size(pcm_handle, params, FRAME_SIZE, 0);

    // 파라미터 설정 적용
    if ((pcm = snd_pcm_hw_params(pcm_handle, params)) < 0) {
        fprintf(stderr, "ERROR: Can't set hardware parameters: %s\n", snd_strerror(pcm));
        return 1;
    }

    printf("🎙️ Listening for pitch (press Ctrl+C to stop)...\n");

    while (1) {
        pcm = snd_pcm_readi(pcm_handle, buffer, FRAME_SIZE);
        if (pcm == -EPIPE) {
            // 버퍼 오버런
            fprintf(stderr, "XRUN (overrun)\n");
            snd_pcm_prepare(pcm_handle);
            continue;
        } else if (pcm < 0) {
            fprintf(stderr, "ERROR reading from PCM device: %s\n", snd_strerror(pcm));
            continue;
        } else if (pcm != FRAME_SIZE) {
            fprintf(stderr, "Short read: got %d frames\n", pcm);
            continue;
        }

        // 피치 계산
        double pitch = detect_pitch_int(buffer, FRAME_SIZE, SAMPLE_RATE);
        if (pitch >= MIN_PITCH_HZ && pitch <= MAX_PITCH_HZ) {
            printf("🎵 Pitch: %.2f Hz\n", pitch);
        } else {
            printf("... (No valid pitch)\n");
        }

        // CPU 사용량 줄이기 위해 약간 쉬기
        usleep(20000); // 20ms
    }

    snd_pcm_close(pcm_handle);
    return 0;
}
