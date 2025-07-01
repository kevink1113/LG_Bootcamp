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

// 볼륨(RMS) 계산 함수
static double calculate_rms(short *buffer, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrt(sum / size);
}

// 피치(Hz)를 계이름(도레미 등)과 옥타브로 변환하는 함수
static const char *note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static void pitch_to_note_and_octave(double pitch, const char **note, int *octave) {
    if (pitch <= 0.0) {
        *note = "-";
        *octave = -1;
        return;
    }
    int midi_num = (int)(round(12.0 * log2(pitch / 440.0) + 69));
    int note_index = (midi_num + 1200) % 12; // +1200 for negative midi_num
    *note = note_names[note_index];
    *octave = (midi_num / 12) - 1; // MIDI 옥타브 규칙
}

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

// 점수 계산 함수 (A2~A5, # 포함, 플랫 제외)
static int get_pitch_score(const char *note, int octave) {
    // 점수는 A2(1) ~ A5(37)
    static const char *score_notes[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
    int base_octave = 2;
    int score = 0;
    for (int o = base_octave; o <= 5; ++o) {
        for (int n = 0; n < 12; ++n) {
            if (strcmp(note, score_notes[n]) == 0 && octave == o) {
                return score + 1;
            }
            score++;
            // A5까지만
            if (o == 5 && n == 0) return 37;
        }
    }
    return 0; // 범위 밖
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

        // 볼륨(RMS) 계산
        double rms = calculate_rms(buffer, FRAME_SIZE);
        // 피치 계산
        double pitch = detect_pitch_int(buffer, FRAME_SIZE, SAMPLE_RATE);
        const char *note;
        int octave;
        pitch_to_note_and_octave(pitch, &note, &octave);
        if (rms >= 300 && pitch < 1000) {
            if (pitch >= MIN_PITCH_HZ && pitch <= MAX_PITCH_HZ) {
                int score = get_pitch_score(note, octave);
                printf("🎵 Pitch: %.2f Hz | Note: %s | Octave: %d | Score: %d | Volume(RMS): %.1f\n", pitch, note, octave, score, rms);
                // 점수와 볼륨을 /tmp/pitch_score.dat에 저장 (int, float 순서)
                FILE *fp = fopen("/tmp/pitch_score", "w");
                if (fp) {
                    fprintf(fp, "%d %.1f\n", score, rms);
                    fclose(fp);
                }
            } else {
                printf("... (No valid pitch) | Volume(RMS): %.1f\n", rms);
            }
        }

        // CPU 사용량 줄이기 위해 약간 쉬기
        usleep(20000); // 20ms
    }

    snd_pcm_close(pcm_handle);
    return 0;
}