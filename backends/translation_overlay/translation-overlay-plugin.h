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

#ifndef BACKENDS_TRANSLATION_OVERLAY_PLUGIN_H
#define BACKENDS_TRANSLATION_OVERLAY_PLUGIN_H

#include "backends/translation_overlay/translation-overlay-settings.h"

#include "audio/voiceplugin.h"
#include "common/path.h"
#include "common/str.h"

namespace Audio {
class AudioStream;
}

class TranslationOverlayPlugin : public VoiceReplacementPluginObject {
public:
	TranslationOverlayPlugin();

	const char *getName() const override { return "Translation overlay"; }

	void loadConfig() override;

	// -----------------------------------------------------------------------
	// Integer-ID voice interface (general games)
	// -----------------------------------------------------------------------
	Audio::SeekableAudioStream *translateStream(int soundId, Audio::AudioStream *original = nullptr) const override;

	// -----------------------------------------------------------------------
	// Tag-based voice interface (SCUMM v7: The Dig, Curse of Monkey Island)
	// -----------------------------------------------------------------------
	Audio::SeekableAudioStream *translateStream(const char *tag, Audio::AudioStream *original = nullptr) const override;

	int translateText(const byte *rawMsg, int actorId, byte *dst, int dstSize) const override;
	int getLastSoundId() const override;

private:
	TranslationOverlay::Settings _settings;

	mutable int _cachedIndex = -1;

	void readConfig();
	void dumpOriginal(const Common::String &id, Audio::AudioStream *original) const;
	Common::Path buildWavPath(const Common::String &stem) const;
	Common::String getStemFromId(int soundId) const;
};

#endif // BACKENDS_TRANSLATION_OVERLAY_PLUGIN_H
