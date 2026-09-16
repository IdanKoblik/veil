#pragma once

enum CodecType { CODEC_LSB_MATCHING, CODEC_LSB_REPLACEMENT, CODEC_DCT, CODEC_UNKNOWN };

enum CodecType str_to_codec(const char *str);
