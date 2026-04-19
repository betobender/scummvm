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

// ---------------------------------------------------------------------------
// TranslationOverlayPlugin
// ---------------------------------------------------------------------------

TranslationOverlayPlugin::TranslationOverlayPlugin() {
	loadConfig();
}

void TranslationOverlayPlugin::loadConfig() {
	_settings.loadSettings();
	_processor.reset(_settings.getReportPath());
}

int TranslationOverlayPlugin::translateText(const byte *rawMsg, int actorId, byte *dst, int dstSize) const {
	_cachedIndex = -1;

	if (!_settings.isEnabled())
		return -1;

	if (!rawMsg || !*rawMsg || !dst || dstSize < 1)
		return -1;

	if (actorId == 0xFF)
		actorId = 0;

	const TranslationOverlay::Settings::LinesByActorMap &linesByActor = _settings.getLinesByActor();

	Common::String cleaned = TranslationOverlay::cleanForKey(rawMsg);
	if (cleaned.empty())
		return -1;

	if (!linesByActor.contains(actorId)) {
		_processor.missingActor(cleaned, actorId);
		return -1;
	}

	const TranslationOverlay::Settings::ActorLines &al = linesByActor[actorId];

	uint keyHash = cleaned.hash();
	auto it = al._lines.find(keyHash);
	if (it == al._lines.end()) {
		_processor.missingLine(cleaned, al);
		return -1;
	}

	_processor.reportLine(cleaned, al, it->_value);

	_cachedIndex = it->_value._ptrIndex;

	return TranslationOverlay::literalCuesToBin(dst, dstSize, it->_value._translatedText);
}

Common::String TranslationOverlayPlugin::getStemFromId(int soundId) const {
	TranslationOverlay::ScopedTimer scopedTimer(Common::String::format("TranslationOverlayPlugin::getStemFromId(%d)", soundId));
	TranslationOverlay::Settings::TranslationInfo *info = _settings.getAllLines()[soundId];
	return info->_entryId;
}

Audio::SeekableAudioStream *TranslationOverlayPlugin::translateStream(int soundId, Audio::AudioStream *original) const {
	TranslationOverlay::ScopedTimer scopedTimer(Common::String::format("TranslationOverlayPlugin::translateStream(%d)", soundId));
	return _processor.translateStream(soundId, _settings.getVoiceCache().getStream(getStemFromId(soundId)), original);
}

Audio::SeekableAudioStream *TranslationOverlayPlugin::translateStream(const char *tag, Audio::AudioStream *original) const {
	TranslationOverlay::ScopedTimer scopedTimer(Common::String::format("TranslationOverlayPlugin::translateStream('%s')", tag));
	return _processor.translateStream(tag, _settings.getVoiceCache().getStream(tag), original);
}

int TranslationOverlayPlugin::getLastSoundId() const {
	return _cachedIndex;
}

REGISTER_PLUGIN_STATIC(TRANSLATION_OVERLAY, PLUGIN_TYPE_VOICE_REPLACEMENT, TranslationOverlayPlugin);
