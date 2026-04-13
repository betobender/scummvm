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

#ifndef BACKENDS_TRANSLATION_OVERLAY_HELPERS_H
#define BACKENDS_TRANSLATION_OVERLAY_HELPERS_H

#include "common/str.h"

namespace Common {
class JSONValue;
}

namespace Audio {
class SeekableAudioStream;
}

class JSONHelper {
public:
	JSONHelper(const Common::String &key, Common::JSONValue *json);

	const Common::String &getKey() const;
	const Common::JSONValue *getValue() const;
	Common::JSONValue *getValue();
	bool isValid() const;
	bool getValue(const char *name, int &val) const;
	bool getValue(const char *name, Common::String &val) const;
	JSONHelper getChild(const char *name) const;

private:
	const Common::String &_key;
	Common::JSONValue *_json;
};

class ConfManHelper {
public:
};

namespace TranslationOverlay {
	// ---------------------------------------------------------------------------
	// Latin-1 diacritic → ASCII base map (0xC0–0xFF)
	// ---------------------------------------------------------------------------
	const char _latin1BaseMap[64] = {
		// C0..C7: À Á Â Ã Ä Å Æ Ç
		'A','A','A','A','A','A','A','C',
		// C8..CF: È É Ê Ë Ì Í Î Ï
		'E','E','E','E','I','I','I','I',
		// D0..D7: Ð Ñ Ò Ó Ô Õ Ö ×
		'D','N','O','O','O','O','O','?',
		// D8..DF: Ø Ù Ú Û Ü Ý Þ ß
		'O','U','U','U','U','Y','?','s',
		// E0..E7: à á â ã ä å æ ç
		'a','a','a','a','a','a','a','c',
		// E8..EF: è é ê ë ì í î ï
		'e','e','e','e','i','i','i','i',
		// F0..F7: ð ñ ò ó ô õ ö ÷
		'd','n','o','o','o','o','o','?',
		// F8..FF: ø ù ú û ü ý þ ÿ
		'o','u','u','u','u','y','?','y',
	};

	bool isHexDigit(char c);
	byte parseHexByte(const char *p);
	Common::String stripDiacritics(const Common::U32String &u32);
	Common::String cleanForKey(const byte *rawMsg);
	Common::String stripCueTags(const Common::String &s);
	void dumpAudioStreamToWAV(Audio::SeekableAudioStream *stream, const char *filename);
}

#endif
