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

#ifndef BACKENDS_TRANSLATION_OVERLAY_STREAM_H
#define BACKENDS_TRANSLATION_OVERLAY_STREAM_H

#include "common/hash-str.h"
#include "common/path.h"
#include "common/ptr.h"
#include "common/str.h"
#include "common/stream.h"

#include "audio/audiostream.h"

namespace TranslationOverlay {

class VoiceCache {
public:
	struct Voice {
		Common::String _id;
		Common::Path _path;
		Common::ScopedPtr<Common::SeekableReadStream> _sstream;
		Common::ScopedPtr<Audio::SeekableAudioStream> _astream;
		bool _exists{false};
		bool _cached{false};

		void load(VoiceCache* owner);
	};

	typedef Common::HashMap<Common::String, Voice,
			Common::CaseSensitiveString_Hash,
			Common::CaseSensitiveString_EqualTo> VoiceMap;

	void reset();
	void setWavPath(const Common::Path &wavPath);
	void registerVoice(const Common::String &id);
	void dumpStats() const;
	const Voice *getVoice(const Common::String &id) const;
	Audio::SeekableAudioStream *getStream(const Common::String &id) const;
	bool hasVoice(const Common::String &id) const;

private:
	const Common::String _ext{".wav"};

	Common::Path _wavPath;
	VoiceMap _voiceMap;
	uint64 _streamsSizeInBytes{0};

	Common::Path VoiceCache::buildWavPath(const Common::String &id) const;
};

}

#endif // BACKENDS_TRANSLATION_OVERLAY_STREAM_H
