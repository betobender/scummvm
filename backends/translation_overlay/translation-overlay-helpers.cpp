/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "backends/translation_overlay/translation-overlay-helpers.h"

#include "common/fs.h"
#include "common/formats/json.h"
#include "common/ptr.h"

#include "audio/audiostream.h"

JSONHelper::JSONHelper(const Common::String &key, Common::JSONValue *json)
	: _key(key), _json(json) {
}

const Common::String &JSONHelper::getKey() const {
	return _key;
}

const Common::JSONValue *JSONHelper::getValue() const {
	return _json;
}

Common::JSONValue *JSONHelper::getValue() {
	return _json;
}

bool JSONHelper::isValid() const {
	return _json && _json->isObject();
}

bool JSONHelper::getValue(const char *name, int &val) const {
	Common::JSONValue *value = _json->child(name);
	if (!value || !value->isNumber())
		return false;
	val = (int)value->asIntegerNumber();
	return true;
}

bool JSONHelper::getValue(const char *name, Common::String &val) const {
	Common::JSONValue *value = _json->child(name);
	if (!value || !value->isString())
		return false;
	val = value->asString();
	return true;
}

JSONHelper JSONHelper::getChild(const char *name) const {
	Common::JSONValue *child = _json->child(name);
	return JSONHelper(name, child);
}

namespace TranslationOverlay {

bool isHexDigit(char c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

byte parseHexByte(const char *p) {
	auto hexNibble = [](char c) -> int {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return c - 'a' + 10;
	};
	return (byte)((hexNibble(p[0]) << 4) | hexNibble(p[1]));
}

Common::String stripDiacritics(const Common::U32String &u32) {
	Common::String result;
	for (uint i = 0; i < u32.size(); ++i) {
		uint32 cp = (uint32)u32[i];
		if (cp < 0x80) {
			result += (char)cp;
		} else if (cp >= 0xC0 && cp <= 0xFF) {
			result += _latin1BaseMap[cp - 0xC0];
		}
	}
	return result;
}

Common::String cleanForKey(const byte *rawMsg) {
	Common::String result;
	const byte *p = rawMsg;

	while (*p) {
		byte b = *p++;
		if (b == 0xFF) {
			if (*p) {
				byte esc = *p++;
				// Opcodes 4,5,6,7,9,10,12,13,14 carry a 2-byte parameter.
				if (esc == 4 || esc == 5 || esc == 6 || esc == 7 ||
					esc == 9 || esc == 10 || esc == 12 || esc == 13 || esc == 14) {
					p += 2;
				}
			}
		} else if (b == 0x5E) {
			result += '^';
		} else if (b >= 0x20 && b <= 0x7E) {
			result += (char)b;
		}
	}

	// Collapse runs of spaces (but not '^').
	Common::String collapsed;
	bool prevSpace = false;
	for (uint i = 0; i < result.size(); ++i) {
		if (result[i] == ' ') {
			if (!prevSpace && !collapsed.empty())
				collapsed += ' ';
			prevSpace = true;
		} else {
			collapsed += result[i];
			prevSpace = false;
		}
	}
	while (!collapsed.empty() && collapsed.lastChar() == ' ')
		collapsed.deleteLastChar();

	return collapsed;
}

Common::String stripCueTags(const Common::String &s) {
	Common::String out;
	const char *p = s.c_str();
	while (*p) {
		if (p[0] == '<' && p[1] == 'F' && p[2] == 'F' && p[3] == ':') {
			while (*p && *p != '>')
				p++;
			if (*p == '>')
				p++;
		} else {
			out += *p++;
		}
	}
	return out;
}


void dumpAudioStreamToWAV(Audio::SeekableAudioStream *stream, const char *filename) {
	const int rate = stream->getRate();
	const bool stereo = stream->isStereo();
	const int channels = stereo ? 2 : 1;
	const int bitsPerSample = 16;
	const int blockAlign = channels * (bitsPerSample / 8);

	Common::FSNode fsNodePath(filename);
	Common::ScopedPtr<Common::SeekableWriteStream> out(fsNodePath.createWriteStream());

	if (out == nullptr)
		return;

	stream->rewind();

	// Collect PCM data first (need size for header)
	Common::Array<int16> samples;
	int16 buf[4096];
	int read;
	while ((read = stream->readBuffer(buf, ARRAYSIZE(buf))) > 0)
		for (int i = 0; i < read; i++)
			samples.push_back(buf[i]);

	uint32 dataSize = samples.size() * sizeof(int16);

	// WAV header
	out->writeUint32BE(MKTAG('R', 'I', 'F', 'F'));
	out->writeUint32LE(36 + dataSize);
	out->writeUint32BE(MKTAG('W', 'A', 'V', 'E'));
	out->writeUint32BE(MKTAG('f', 'm', 't', ' '));
	out->writeUint32LE(16); // chunk size
	out->writeUint16LE(1);  // PCM
	out->writeUint16LE(channels);
	out->writeUint32LE(rate);
	out->writeUint32LE(rate * blockAlign);
	out->writeUint16LE(blockAlign);
	out->writeUint16LE(bitsPerSample);
	out->writeUint32BE(MKTAG('d', 'a', 't', 'a'));
	out->writeUint32LE(dataSize);
	out->write(samples.data(), dataSize);
}

} // namespace TranslationOverlay
