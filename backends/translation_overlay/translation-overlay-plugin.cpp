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

#include "backends/translation_overlay/translation-overlay-plugin.h"
#include "backends/translation_overlay/translation-overlay-helpers.h"

#include "common/debug.h"
#include "common/str.h"

// ---------------------------------------------------------------------------
// TranslationOverlayPlugin
// ---------------------------------------------------------------------------

TranslationOverlayPlugin::TranslationOverlayPlugin() {
	readConfig();
}

void TranslationOverlayPlugin::readConfig() {
	_settings.loadSettings();
}

void TranslationOverlayPlugin::loadConfig() {
	readConfig();
}

void TranslationOverlayPlugin::dumpOriginal(const Common::String &id, Audio::AudioStream *original) const {
	return;
	/* if (Audio::SeekableAudioStream *cast = dynamic_cast<Audio::SeekableAudioStream *>(original)) {
		Common::String filename = _originalsPath.appendComponent(id).append(".wav").toString();
		TranslationOverlay::dumpAudioStreamToWAV(cast, filename.c_str());
	}*/
}

int TranslationOverlayPlugin::translateText(const byte *rawMsg, int actorId, byte *dst, int dstSize) const {

	if (!_settings.isEnabled())
		return -1;

	if (!rawMsg || !*rawMsg || !dst || dstSize < 1)
		return -1;

	if (actorId == 0xFF)
		actorId = 0;

	const TranslationOverlay::Settings::LinesByActorMap &linesByActor = _settings.getLinesByActor();

	if (!linesByActor.contains(actorId))
		return -1;

	const TranslationOverlay::Settings::ActorLines &al = linesByActor[actorId];
	Common::String cleaned = TranslationOverlay::cleanForKey(rawMsg);
	if (cleaned.empty())
		return -1;

	auto it = al._lines.find(cleaned);
	if (it == al._lines.end()) {
		debug(1, "TranslationOverlayPlugin: miss key='%s'", cleaned.c_str());
		return -1;
	}

	debug(1, "TranslationOverlayPlugin: hit stem='%s' key='%s', actor='%s', actor_id = '%d' and line='%s'",
		it->_value._entryId.c_str(),
		cleaned.c_str(),
		al._actorName.c_str(),
		actorId,
		it->_value._translatedText.c_str());

	_cachedIndex = it->_value._ptrIndex;

	// Walk the stored translated text, expanding <FF:OP> / <FF:OP:P1:P2> tags
	// into raw byte sequences. All other bytes (including '^' = 0x5E) are copied
	// verbatim. The output buffer is always null-terminated.
	byte *out = dst;
	const byte *end = dst + dstSize - 1;  // reserve 1 byte for null terminator
	const char *p = it->_value._translatedText.c_str();

	while (*p && out < end) {
		if (p[0] == '<' && p[1] == 'F' && p[2] == 'F' && p[3] == ':') {
			p += 4;  // skip "<FF:"

			// Opcode byte (2 hex chars)
			if (!TranslationOverlay::isHexDigit(p[0]) || !TranslationOverlay::isHexDigit(p[1]))
				continue;
			byte op = TranslationOverlay::parseHexByte(p);
			p += 2;

			// Optional first param byte
			byte pb1 = 0, pb2 = 0;
			bool hasP1 = false, hasP2 = false;
			if (p[0] == ':' && TranslationOverlay::isHexDigit(p[1]) && TranslationOverlay::isHexDigit(p[2])) {
				p++;
				pb1 = TranslationOverlay::parseHexByte(p);
				p += 2;
				hasP1 = true;
			}
			// Optional second param byte
			if (p[0] == ':' && TranslationOverlay::isHexDigit(p[1]) && TranslationOverlay::isHexDigit(p[2])) {
				p++;
				pb2 = TranslationOverlay::parseHexByte(p);
				p += 2;
				hasP2 = true;
			}
			if (*p == '>') p++;

			// Write the decoded opcode sequence
			if (out < end) *out++ = 0xFF;
			if (out < end) *out++ = op;
			if (hasP1 && out < end) *out++ = pb1;
			if (hasP2 && out < end) *out++ = pb2;
		} else {
			*out++ = (byte)*p++;
		}
	}

	*out = 0;
	return (int)(out - dst);
}

Common::String TranslationOverlayPlugin::getStemFromId(int soundId) const {
	TranslationOverlay::Settings::TranslationInfo *info = _settings.getAllLines()[soundId];
	return info->_entryId;
}

Audio::SeekableAudioStream *TranslationOverlayPlugin::translateStream(int soundId, Audio::AudioStream *original) const {
	dumpOriginal(getStemFromId(soundId), original);
	return _settings.getVoiceCache().getStream(getStemFromId(soundId));
}

Audio::SeekableAudioStream *TranslationOverlayPlugin::translateStream(const char *tag, Audio::AudioStream *original) const {
	dumpOriginal(tag, original);
	return _settings.getVoiceCache().getStream(tag);
}

int TranslationOverlayPlugin::getLastSoundId() const {
	return _cachedIndex;
}

REGISTER_PLUGIN_STATIC(TRANSLATION_OVERLAY, PLUGIN_TYPE_VOICE_REPLACEMENT, TranslationOverlayPlugin);
