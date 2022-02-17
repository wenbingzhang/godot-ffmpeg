#include "FFmpegRect.h"

#include <cstring>

#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

bool FFmpegRect::load(String path) {
	if (nativeGetDecoderState(id) > UNINITIALIZED) {
		return false;
	}

	CharString utf8 = path.utf8();
	const char *cstr = utf8.get_data();

	nativeCreateDecoder(cstr, id);

	bool is_loaded = nativeGetDecoderState(id) == INITIALIZED;
	if (is_loaded) {
		nativeGetVideoFormat(id, width, height, length);
		data_size = width * height * 3;

// 		bool audio_enabled = nativeIsAudioEnabled(id);
// 		if (audio_enabled) {
// 			nativeSetAudioAllChDataEnable(id, true);
// 		}
	}

	return is_loaded;
}

void FFmpegRect::load_async(String path) {
	if (nativeGetDecoderState(id) > UNINITIALIZED) {
		emit_signal("async_loaded", false);
		return;
	}

	CharString utf8 = path.utf8();
	const char *cstr = utf8.get_data();

	nativeCreateDecoderAsync(cstr, id);

	set_texture(texture);

	async_loading = true;
	processing = true;
}

void FFmpegRect::play() {
	if (nativeGetDecoderState(id) == INITIALIZED) {
		nativeStartDecoding(id);
	} else if (paused) {
		paused = false;
	}

	set_texture(texture);

	global_start_time = Time::get_singleton()->get_unix_time_from_system();

	processing = true;
}

void FFmpegRect::stop() {
	if (nativeGetDecoderState(id) != DECODING) {
		return;
	}

	nativeDestroyDecoder(id);

	current_time = 0.0f;

	paused = false;
	processing = false;
	buffering = false;
	seeking = false;
	async_loading = false;
}

bool FFmpegRect::is_playing() const {
	return !paused && nativeGetDecoderState(id) == DECODING;
}

void FFmpegRect::set_paused(bool p_paused) {
	paused = p_paused;

	if (paused) {
		hang_time = Time::get_singleton()->get_unix_time_from_system();
	} else {
		float time = Time::get_singleton()->get_unix_time_from_system();
		global_start_time = time - (time - hang_time);
	}
}

bool FFmpegRect::is_paused() const {
	return paused;
}

float FFmpegRect::get_length() const {
	return length;
}

void FFmpegRect::set_loop(bool p_enable) {
	looping = p_enable;
}

bool FFmpegRect::has_loop() const {
	return looping;
}

float FFmpegRect::get_playback_position() const {
	return current_time;
}

void FFmpegRect::seek(float p_time) {
	if (nativeGetDecoderState(id) != DECODING) {
		return;
	}

	if (p_time < 0.0f) {
		p_time = 0.0f;
	} else if (p_time > length) {
		p_time = length;
	}

	nativeSetSeekTime(id, p_time);
	nativeSetVideoTime(id, p_time);

	hang_time = Time::get_singleton()->get_unix_time_from_system();

	seeking = true;
}

void FFmpegRect::_process(float delta) {
	if (async_loading) {
		if (nativeGetDecoderState(id) == INITIALIZED) {
			nativeGetVideoFormat(id, width, height, length);
			data_size = width * height * 3;

			async_loading = false;

			emit_signal("async_loaded", true);
		} else if (nativeGetDecoderState(id) == INIT_FAIL) {
			processing = false;
			async_loading = false;

			emit_signal("async_loaded", false);
		}

		return;
	}

	if (!processing || paused) {
		return;
	}

	if (!seeking) {
		// TODO: Implement audio.
		unsigned char *audio_data = nullptr;
		int audio_size = 0;
		double audio_time = nativeGetAudioData(id, &audio_data, audio_size);
		if (audio_time != -1.0f) {
			nativeFreeAudioData(id);
		}
	}

	if (buffering) {
		if (nativeIsVideoBufferFull(id) || nativeIsEOF(id)) {
			double time = Time::get_singleton()->get_unix_time_from_system();
			global_start_time = time - (time - hang_time);

			buffering = false;
		}

		return;
	}

	if (seeking) {
		if (nativeIsSeekOver(id)) {
			double time = Time::get_singleton()->get_unix_time_from_system();
			global_start_time = time - (time - hang_time);

			seeking = false;
		}

		return;
	}

	void* frame_data = nullptr;
	bool frame_ready = false;

	nativeGrabVideoFrame(id, &frame_data, frame_ready);

	if (frame_ready) {
		PackedByteArray image_data;
		image_data.resize(data_size);
		memcpy(image_data.ptrw(), frame_data, data_size);

		image->create_from_data(width, height, false, Image::FORMAT_RGB8, image_data);
 		texture->create_from_image(image);

 		nativeReleaseVideoFrame(id);
	}

	current_time = Time::get_singleton()->get_unix_time_from_system() - global_start_time;

	if (current_time < length || length == -1.0f || !nativeIsVideoBufferEmpty(id)) {
		nativeSetVideoTime(id, current_time);
	}

	if (nativeIsVideoBufferEmpty(id) && !nativeIsEOF(id)) {
		UtilityFunctions::print("buffered");
		hang_time = Time::get_singleton()->get_unix_time_from_system();

		buffering = true;
	}
}

FFmpegRect::FFmpegRect() {
	texture = Ref<ImageTexture>(memnew(ImageTexture));
	image = Ref<Image>(memnew(Image()));
}

FFmpegRect::~FFmpegRect() {
	nativeScheduleDestroyDecoder(id);
}

void FFmpegRect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load", "path"), &FFmpegRect::load);
	ClassDB::bind_method(D_METHOD("load_async", "path"), &FFmpegRect::load_async);
	ClassDB::bind_method(D_METHOD("play"), &FFmpegRect::play);
	ClassDB::bind_method(D_METHOD("stop"), &FFmpegRect::stop);
	ClassDB::bind_method(D_METHOD("is_playing"), &FFmpegRect::is_playing);
	ClassDB::bind_method(D_METHOD("set_paused", "paused"), &FFmpegRect::set_paused);
	ClassDB::bind_method(D_METHOD("get_length"), &FFmpegRect::get_length);
	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &FFmpegRect::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &FFmpegRect::has_loop);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &FFmpegRect::get_playback_position);
	ClassDB::bind_method(D_METHOD("seek", "time"), &FFmpegRect::seek);
	ClassDB::bind_method(D_METHOD("is_paused"), &FFmpegRect::is_paused);

	ADD_SIGNAL(MethodInfo("async_loaded", PropertyInfo(Variant::BOOL, "successful")));
}
