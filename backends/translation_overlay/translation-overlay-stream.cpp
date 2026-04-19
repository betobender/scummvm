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

#include "backends/translation_overlay/translation-overlay-stream.h"

#include "common/debug.h"
#include "common/fs.h"
#include "common/util.h"

#include "audio/decoders/wave.h"

namespace TranslationOverlay {

void VoiceCache::reset() {
	_voiceMap.clear();
	_streamsSizeInBytes = 0;
	_wavPath = "";
}

void VoiceCache::setWavPath(const Common::Path &wavPath) {
	_wavPath = wavPath;
}

void VoiceCache::registerVoice(const Common::String &id) {
	Voice &voice = _voiceMap[id.hash()];
	voice._id = id;
	voice.load(this);
}

const VoiceCache::Voice *VoiceCache::getVoice(const Common::String &id) const {
	VoiceMap::const_iterator it = _voiceMap.find(id.hash());
	if (it != _voiceMap.end()) {
		return &(it->_value);
	}
	return nullptr;
}

Common::Path VoiceCache::buildWavPath(const Common::String &id) const {
	return _wavPath.appendComponent(id + _ext);
}

Audio::SeekableAudioStream *VoiceCache::getStream(const Common::String &id) const {
	const Voice *voice = getVoice(id);
	if (voice != nullptr) {
		if (voice->_astream != nullptr)
			voice->_astream->rewind();
		return voice->_astream.get();
	}
	return nullptr;
}

bool VoiceCache::hasVoice(const Common::String &id) const {
	const TranslationOverlay::VoiceCache::Voice *voice = getVoice(id);
	return voice != nullptr && voice->_exists;
}

void VoiceCache::dumpStats() const {
#ifndef DISABLE_TEXT_CONSOLE
	const char *units;
	Common::String val = Common::getHumanReadableBytes(_streamsSizeInBytes, units);
	debug(1, "VoiceCache:: Total voices:         %u voices", _voiceMap.size());
	debug(1, "VoiceCache:: Total cached streams: %s %s", val.c_str(), units);
#endif
}

void VoiceCache::Voice::load(VoiceCache *owner) {
	_path = owner->buildWavPath(_id);
	Common::FSNode fsNodePathe(_path);
	_exists = fsNodePathe.exists();

	if (_exists) {
		_sstream.reset(fsNodePathe.createReadStream());
		if (_sstream != nullptr) {
			_astream.reset(Audio::makeWAVStream(_sstream.get(), DisposeAfterUse::NO));
			_cached = _astream != nullptr;

			if (!_cached) {
				_sstream.release();
			} else {
				owner->_streamsSizeInBytes += _sstream->size();
			}
		}
	}
}

} // namespace TranslationOverlay
