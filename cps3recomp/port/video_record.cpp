// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
//
// The video recording (video_record.h)
#include "video_record.h"

extern "C" {
#include "faac.h"
#include "minih264e.h"
#include "minimp4.h"
}

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace port {

namespace {

// Pictures waiting to be encoded, at most (a second and a half of them); with more, the newest are left out
constexpr size_t kMostWaiting = 90;
// The H.264's quantizer: the same for every picture, low enough that the board's pixels stay sharp blocks; a little
// higher the larger they are made (each of them more of the video's pixels, its edges as sharp), for files of a size
// like the smaller ones
int Quantizer(int height) {
    return height <= 700 ? 20 : height <= 950 ? 22 : 24;
}
// A key frame every two seconds, to go to any point of the video quickly
constexpr int kKeyEvery = 120;
// The AAC's bits a second, for each of the two channels
constexpr uint32_t kSoundBits = 96000;

FILE *OpenForWriting(const std::string &path) {
#ifdef _WIN32
    return _wfopen(std::filesystem::path(std::u8string(path.begin(), path.end())).wstring().c_str(), L"wb");
#else
    return fopen(path.c_str(), "wb");
#endif
}

int WriteAt(int64_t offset, const void *data, size_t size, void *token) {
    FILE *file = static_cast<FILE *>(token);
#ifdef _WIN32
    if (_fseeki64(file, offset, SEEK_SET)) {
        return 1;
    }
#else
    if (fseeko(file, static_cast<off_t>(offset), SEEK_SET)) {
        return 1;
    }
#endif
    return fwrite(data, 1, size, file) != size;
}

// (minih264's threads: its jobs for a picture run side by side, each part of the picture on a thread)
void RunJobs(void *, void (*job)(void *), void *data[], int count) {
    std::vector<std::thread> threads;
    for (int n = 1; n < count; n++) {
        threads.emplace_back(job, data[n]);
    }
    if (count > 0) {
        job(data[0]);
    }
    for (std::thread &thread : threads) {
        thread.join();
    }
}

// Where each of the video's columns (or lines) takes its colour from, made larger: the board's pixel it is in, and
// of the next one as much as it covers of it (0 to 256), so that each pixel is a sharp block with only its edges
// blended, as the Display page's Sharp scaling does
struct Taps {
    std::vector<int> from;
    std::vector<int> next; // (of 256)
    void Make(int pixels, int size) {
        from.resize(static_cast<size_t>(size));
        next.resize(static_cast<size_t>(size));
        const double step = static_cast<double>(pixels) / size;
        for (int n = 0; n < size; n++) {
            const double start = n * step, end = start + step;
            const int first = std::min(static_cast<int>(start), pixels - 1);
            from[static_cast<size_t>(n)] = first;
            const double into = end - (first + 1); // (how far into the next pixel this one reaches)
            next[static_cast<size_t>(n)] = first + 1 < pixels && into > 1e-9 ? static_cast<int>(std::lround(256 * into / step)) : 0;
        }
    }
};

inline uint32_t Blend(uint32_t a, uint32_t b, int weight) {
    if (weight == 0) {
        return a;
    }
    uint32_t out = 0;
    for (int shift = 0; shift < 24; shift += 8) {
        const int x = (a >> shift) & 0xFF, y = (b >> shift) & 0xFF;
        out |= static_cast<uint32_t>(x + (((y - x) * weight + 128) >> 8)) << shift;
    }
    return out;
}

} // namespace

struct VideoRecorder::State {
    FILE *file = nullptr;
    MP4E_mux_t *mux = nullptr;
    mp4_h26x_writer_t writer{};
    std::vector<uint8_t> persist, scratch; // (minih264's memory)
    H264E_persist_t *encoder = nullptr;
    faac_encoder *sound = nullptr;
    int soundTrack = -1;
    uint32_t soundFrame = 1024;      // (samples of each channel in each AAC frame)
    std::vector<uint8_t> soundOut;
    std::vector<int16_t> soundWaiting;
    std::vector<uint32_t> rgb;       // (the picture made larger, the video's size)
    std::vector<uint8_t> y, u, v;    // (... and as the H.264 takes it)
    // (the planes' lines, a multiple of 16 bytes long however wide the picture is: minih264's NEON and SSE2 code takes
    // only lines that are a multiple of 8, the colours' as well, so a picture 598 or 1194 wide failed to encode)
    int stride = 0;
    Taps across, down;
    int tapsColumns = 0, tapsWidth = 0, tapsRows = 0, tapsHeight = 0;
    uint64_t framesOut = 0;          // (the board's frames encoded)
    unsigned timeOut = 0;            // (... in 90 kHz ticks)
};

VideoRecorder::VideoRecorder() = default;

VideoRecorder::~VideoRecorder() {
    Stop();
}

void VideoRecorder::SizeFor(float shape, int lines, int &width, int &height) {
    height = lines & ~1;
    width = static_cast<int>(std::lround(height * shape / 2)) * 2;
}

bool VideoRecorder::Start(const std::string &path, int width, int height, double fps, std::string &error) {
    Stop();
    if (width < 16 || height < 16 || (width & 1) || (height & 1) || fps <= 0) {
        error = "bad video size";
        return false;
    }
    auto state = std::make_unique<State>();
    State &s = *state;
    s.file = OpenForWriting(path);
    if (!s.file) {
        error = "could not write " + path;
        return false;
    }
    auto fail = [&](const std::string &why) {
        if (s.sound) {
            faac_encoder_close(&s.sound);
        }
        if (s.mux) {
            MP4E_close(s.mux);
        }
        fclose(s.file);
        std::error_code ignored;
        std::filesystem::remove(std::filesystem::path(std::u8string(path.begin(), path.end())), ignored);
        error = why;
        return false;
    };

    // The pictures: H.264, its Constrained Baseline profile, each at the same quantizer
    H264E_create_param_t create{};
    create.width = width;
    create.height = height;
    create.gop = kKeyEvery;
    create.const_input_flag = 1; // (minih264 may use the planes it is given as it likes)
    create.enableNEON = 1;
    create.num_layers = 1;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    create.max_threads = static_cast<int>(std::min(4u, cores > 2 ? cores - 1 : 1u));
    if (create.max_threads > 1) {
        create.run_func_in_thread = RunJobs;
    }
    int persistSize = 0, scratchSize = 0;
    if (H264E_sizeof(&create, &persistSize, &scratchSize)) {
        return fail("the video encoder does not take this size");
    }
    s.persist.resize(static_cast<size_t>(persistSize));
    s.scratch.resize(static_cast<size_t>(scratchSize));
    s.encoder = reinterpret_cast<H264E_persist_t *>(s.persist.data());
    if (H264E_init(s.encoder, &create)) {
        return fail("the video encoder could not start");
    }

    // The file: the pictures' track (minimp4's H.264 writer), and the sound's
    s.mux = MP4E_open(0, 0, s.file, WriteAt);
    if (!s.mux || mp4_h26x_write_init(&s.writer, s.mux, width, height, 0) != MP4E_STATUS_OK) {
        return fail("could not start the video file");
    }
    faac_params params;
    faac_params_init(&params, sizeof(params));
    params.sample_rate = kRate;
    params.num_channels = 2;
    params.object_type = FAAC_OBJ_LOW;
    params.mpeg_version = FAAC_MPEG4;
    params.bit_rate = kSoundBits;
    params.input_format = FAAC_INPUT_16BIT;
    params.output_format = FAAC_STREAM_RAW;
    if (faac_encoder_open(&params, &s.sound) != FAAC_OK) {
        return fail("the sound encoder could not start");
    }
    faac_encoder_info info{};
    info.struct_size = sizeof(info);
    faac_encoder_get_info(s.sound, &info);
    s.soundFrame = info.frame_samples;
    // (an AAC decoder plays the encoder's delay, a frame of it, as silence before the sound; and the MP4's sound would be
    // that much behind its pictures, as minimp4 writes no edit list to skip it: the sound's first frame left out instead)
    const size_t trim = info.encoder_delay;
    s.soundOut.resize(info.max_output_bytes);
    const uint8_t *config = nullptr;
    uint32_t configSize = 0;
    faac_encoder_asc(s.sound, &config, &configSize);
    MP4E_track_t track{};
    track.object_type_indication = MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3;
    track.language[0] = 'u';
    track.language[1] = 'n';
    track.language[2] = 'd';
    track.track_media_kind = e_audio;
    track.time_scale = kRate;
    track.u.a.channelcount = 2;
    s.soundTrack = MP4E_add_track(s.mux, &track);
    if (s.soundTrack < 0 || MP4E_set_dsi(s.mux, s.soundTrack, config, static_cast<int>(configSize)) != MP4E_STATUS_OK) {
        return fail("could not start the video file's sound");
    }

    s.rgb.assign(static_cast<size_t>(width) * height, 0);
    s.stride = (width + 15) & ~15;
    s.y.assign(static_cast<size_t>(s.stride) * height, 16); // (what is past the picture's width: black, never shown)
    s.u.assign(static_cast<size_t>(s.stride / 2) * (height / 2), 128);
    s.v.assign(s.u.size(), 128);

    m_state = std::move(state);
    m_path = path;
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_framesIn = 0;
    m_soundOut = 0;
    m_dropped = 0;
    m_trim = trim;
    m_ending = false;
    m_failed = false;
    m_jobs.clear();
    m_running = true;
    m_thread = std::thread([this] { Run(); });
    return true;
}

void VideoRecorder::Sound(const int16_t *samples, size_t frames, int rate) {
    if (!m_running || frames == 0 || rate <= 0) {
        return;
    }
    if (!m_game || rate != m_gameRate) {
        const SDL_AudioSpec from{SDL_AUDIO_S16, 2, rate}, to{SDL_AUDIO_F32, 2, kRate};
        if (m_game) {
            SDL_SetAudioStreamFormat(m_game, &from, &to);
        } else {
            m_game = SDL_CreateAudioStream(&from, &to);
        }
        m_gameRate = rate;
    }
    if (m_game) {
        SDL_PutAudioStreamData(m_game, samples, static_cast<int>(frames * 2 * sizeof(int16_t)));
    }
}

void VideoRecorder::Music(const float *samples, size_t frames, int rate) {
    if (!m_running || frames == 0 || rate <= 0) {
        return;
    }
    if (!m_music || rate != m_musicRate) {
        const SDL_AudioSpec from{SDL_AUDIO_F32, 2, rate}, to{SDL_AUDIO_F32, 2, kRate};
        if (m_music) {
            SDL_SetAudioStreamFormat(m_music, &from, &to);
        } else {
            m_music = SDL_CreateAudioStream(&from, &to);
        }
        m_musicRate = rate;
    }
    if (m_music) {
        SDL_PutAudioStreamData(m_music, samples, static_cast<int>(frames * 2 * sizeof(float)));
    }
}

// The sound's next `frames` (at kRate): the game's and the soundtrack's added, as the sound device adds them; what
// is not there yet (the converters keep a little back) silence
void VideoRecorder::Mix(std::vector<int16_t> &out, size_t frames) {
    out.assign(frames * 2, 0);
    if (frames == 0) {
        return;
    }
    auto take = [&](SDL_AudioStream *stream, std::vector<float> &buffer) {
        buffer.assign(frames * 2, 0.0f);
        if (stream) {
            SDL_GetAudioStreamData(stream, buffer.data(), static_cast<int>(buffer.size() * sizeof(float)));
        }
    };
    take(m_game, m_gameBuffer);
    take(m_music, m_musicBuffer);
    for (size_t n = 0; n < out.size(); n++) {
        const float mixed = std::clamp(m_gameBuffer[n] + m_musicBuffer[n], -1.0f, 1.0f);
        out[n] = static_cast<int16_t>(std::lround(mixed * 32767.0f));
    }
}

void VideoRecorder::Picture(const uint32_t *pixels, int columns, int rows, float shape, int frames) {
    if (!m_running || frames <= 0 || columns <= 0 || rows <= 0) {
        return;
    }
    m_framesIn += static_cast<uint64_t>(frames);
    // (the sound up to the end of this picture's frames, all of it, whether the picture is kept or not)
    const uint64_t due = static_cast<uint64_t>(std::llround(m_framesIn * (kRate / m_fps)));
    std::vector<int16_t> sound;
    Mix(sound, static_cast<size_t>(due - m_soundOut));
    m_soundOut = due;
    if (m_trim) {
        const size_t cut = std::min(m_trim, sound.size() / 2);
        sound.erase(sound.begin(), sound.begin() + static_cast<std::ptrdiff_t>(cut * 2));
        m_trim -= cut;
    }
    {
        std::unique_lock<std::mutex> hold(m_lock);
        if (m_waiting) {
            m_taken.wait(hold, [this] { return m_jobs.size() < kMostWaiting; });
        }
        if (m_jobs.size() >= kMostWaiting) {
            // (behind: this picture left out, the one before shown for its frames too)
            Job &last = m_jobs.back();
            last.frames += frames;
            last.sound.insert(last.sound.end(), sound.begin(), sound.end());
            m_dropped++;
        } else {
            Job job;
            job.pixels.assign(pixels, pixels + static_cast<size_t>(columns) * rows);
            job.columns = columns;
            job.rows = rows;
            job.shape = shape;
            job.frames = frames;
            job.sound = std::move(sound);
            m_jobs.push_back(std::move(job));
        }
    }
    m_wake.notify_one();
}

void VideoRecorder::Run() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> hold(m_lock);
            m_wake.wait(hold, [this] { return m_ending || !m_jobs.empty(); });
            if (m_jobs.empty()) {
                return; // (ending, and all encoded)
            }
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }
        m_taken.notify_one();
        if (!m_failed) {
            Encode(job);
        }
    }
}

void VideoRecorder::Encode(Job &job) {
    State &s = *m_state;
    // The picture fitted in the video's size, in its shape, black round it
    int fitHeight = m_height, fitWidth = static_cast<int>(std::lround(m_height * job.shape));
    if (fitWidth > m_width) {
        fitWidth = m_width;
        fitHeight = std::min(m_height, static_cast<int>(std::lround(m_width / job.shape)));
    }
    fitWidth = std::max(fitWidth, 1);
    fitHeight = std::max(fitHeight, 1);
    const int left = (m_width - fitWidth) / 2, top = (m_height - fitHeight) / 2;
    if (job.columns != s.tapsColumns || fitWidth != s.tapsWidth) {
        s.across.Make(job.columns, fitWidth);
        s.tapsColumns = job.columns;
        s.tapsWidth = fitWidth;
        std::fill(s.rgb.begin(), s.rgb.end(), 0); // (the black round it, which may have changed)
    }
    if (job.rows != s.tapsRows || fitHeight != s.tapsHeight) {
        s.down.Make(job.rows, fitHeight);
        s.tapsRows = job.rows;
        s.tapsHeight = fitHeight;
        std::fill(s.rgb.begin(), s.rgb.end(), 0);
    }
    std::vector<uint32_t> line(static_cast<size_t>(fitWidth)), lineNext(static_cast<size_t>(fitWidth));
    auto across = [&](const uint32_t *from, std::vector<uint32_t> &to) {
        for (int x = 0; x < fitWidth; x++) {
            const int at = s.across.from[static_cast<size_t>(x)], weight = s.across.next[static_cast<size_t>(x)];
            to[static_cast<size_t>(x)] = Blend(from[at], weight ? from[at + 1] : 0, weight);
        }
    };
    int madeRow = -1;
    for (int y = 0; y < fitHeight; y++) {
        const int at = s.down.from[static_cast<size_t>(y)], weight = s.down.next[static_cast<size_t>(y)];
        if (at != madeRow) {
            across(&job.pixels[static_cast<size_t>(at) * job.columns], line);
            if (at + 1 < job.rows) {
                across(&job.pixels[static_cast<size_t>(at + 1) * job.columns], lineNext);
            }
            madeRow = at;
        }
        uint32_t *to = &s.rgb[static_cast<size_t>(top + y) * m_width + left];
        for (int x = 0; x < fitWidth; x++) {
            to[x] = Blend(line[static_cast<size_t>(x)], lineNext[static_cast<size_t>(x)], weight);
        }
    }
    // As the H.264 takes it: BT.709 in the limited range (as its colour description says, minih264e.h), the colours'
    // planes half as large each way
    const int W = m_width, H = m_height;
    for (int y = 0; y < H; y++) {
        const uint32_t *from = &s.rgb[static_cast<size_t>(y) * W];
        uint8_t *to = &s.y[static_cast<size_t>(y) * s.stride];
        for (int x = 0; x < W; x++) {
            const int r = from[x] & 0xFF, g = (from[x] >> 8) & 0xFF, b = (from[x] >> 16) & 0xFF;
            to[x] = static_cast<uint8_t>(16 + ((47 * r + 157 * g + 16 * b + 128) >> 8));
        }
    }
    for (int y = 0; y < H / 2; y++) {
        const uint32_t *one = &s.rgb[static_cast<size_t>(2 * y) * W], *two = one + W;
        for (int x = 0; x < W / 2; x++) {
            const uint32_t p[4] = {one[2 * x], one[2 * x + 1], two[2 * x], two[2 * x + 1]};
            int r = 0, g = 0, b = 0;
            for (uint32_t c : p) {
                r += c & 0xFF;
                g += (c >> 8) & 0xFF;
                b += (c >> 16) & 0xFF;
            }
            const size_t at = static_cast<size_t>(y) * (s.stride / 2) + x;
            s.u[at] = static_cast<uint8_t>(std::clamp(128 + ((-26 * r - 87 * g + 113 * b + 512) >> 10), 16, 240));
            s.v[at] = static_cast<uint8_t>(std::clamp(128 + ((113 * r - 102 * g - 11 * b + 512) >> 10), 16, 240));
        }
    }
    H264E_io_yuv_t planes{};
    planes.yuv[0] = s.y.data();
    planes.yuv[1] = s.u.data();
    planes.yuv[2] = s.v.data();
    planes.stride[0] = s.stride;
    planes.stride[1] = planes.stride[2] = s.stride / 2;
    H264E_run_param_t run{};
    run.encode_speed = 2;
    run.qp_min = run.qp_max = Quantizer(m_height);
    uint8_t *coded = nullptr;
    int codedSize = 0;
    if (H264E_encode(s.encoder, reinterpret_cast<H264E_scratch_t *>(s.scratch.data()), &run, &planes, &coded, &codedSize)) {
        m_failed = true;
        return;
    }
    // (each picture's time in the file: its frames of the board's, in the 90 kHz ticks minimp4 writes)
    s.framesOut += static_cast<uint64_t>(job.frames);
    const unsigned time = static_cast<unsigned>(std::llround(s.framesOut * (90000.0 / m_fps)));
    if (mp4_h26x_write_nal(&s.writer, coded, codedSize, time - s.timeOut) != MP4E_STATUS_OK) {
        m_failed = true;
        return;
    }
    s.timeOut = time;

    // The sound, in AAC's frames as they fill
    s.soundWaiting.insert(s.soundWaiting.end(), job.sound.begin(), job.sound.end());
    const size_t each = static_cast<size_t>(s.soundFrame) * 2;
    size_t used = 0;
    while (s.soundWaiting.size() - used >= each) {
        uint32_t written = 0;
        if (faac_encoder_encode(s.sound, s.soundWaiting.data() + used, static_cast<uint32_t>(each), s.soundOut.data(),
                                static_cast<uint32_t>(s.soundOut.size()), &written) != FAAC_OK) {
            m_failed = true;
            return;
        }
        used += each;
        if (written && MP4E_put_sample(s.mux, s.soundTrack, s.soundOut.data(), static_cast<int>(written),
                                       static_cast<int>(s.soundFrame), MP4E_SAMPLE_RANDOM_ACCESS) != MP4E_STATUS_OK) {
            m_failed = true;
            return;
        }
    }
    s.soundWaiting.erase(s.soundWaiting.begin(), s.soundWaiting.begin() + static_cast<std::ptrdiff_t>(used));
}

bool VideoRecorder::Finish() {
    State &s = *m_state;
    bool ok = !m_failed;
    if (ok && s.sound) {
        // (what is left of the sound, and what the encoder still holds)
        if (!s.soundWaiting.empty()) {
            uint32_t written = 0;
            if (faac_encoder_encode(s.sound, s.soundWaiting.data(), static_cast<uint32_t>(s.soundWaiting.size()), s.soundOut.data(),
                                    static_cast<uint32_t>(s.soundOut.size()), &written) == FAAC_OK && written) {
                MP4E_put_sample(s.mux, s.soundTrack, s.soundOut.data(), static_cast<int>(written), static_cast<int>(s.soundFrame),
                                MP4E_SAMPLE_RANDOM_ACCESS);
            }
        }
        for (int n = 0; n < 8; n++) {
            uint32_t written = 0;
            if (faac_encoder_encode(s.sound, nullptr, 0, s.soundOut.data(), static_cast<uint32_t>(s.soundOut.size()), &written) != FAAC_OK ||
                !written) {
                break;
            }
            MP4E_put_sample(s.mux, s.soundTrack, s.soundOut.data(), static_cast<int>(written), static_cast<int>(s.soundFrame),
                            MP4E_SAMPLE_RANDOM_ACCESS);
        }
    }
    if (s.sound) {
        faac_encoder_close(&s.sound);
    }
    if (s.mux) {
        ok = MP4E_close(s.mux) == MP4E_STATUS_OK && ok;
        mp4_h26x_write_close(&s.writer);
        s.mux = nullptr;
    }
    if (s.file) {
        ok = fclose(s.file) == 0 && ok;
        s.file = nullptr;
    }
    // (nothing was encoded: no file left behind)
    if (s.framesOut == 0) {
        std::error_code ignored;
        std::filesystem::remove(std::filesystem::path(std::u8string(m_path.begin(), m_path.end())), ignored);
        ok = false;
    }
    return ok;
}

bool VideoRecorder::Stop() {
    if (!m_running) {
        return false;
    }
    {
        std::lock_guard<std::mutex> hold(m_lock);
        m_ending = true;
    }
    m_wake.notify_one();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    const bool ok = Finish();
    m_state.reset();
    m_running = false;
    for (SDL_AudioStream **stream : {&m_game, &m_music}) {
        if (*stream) {
            SDL_DestroyAudioStream(*stream);
            *stream = nullptr;
        }
    }
    m_gameRate = m_musicRate = 0;
    return ok;
}

} // namespace port
