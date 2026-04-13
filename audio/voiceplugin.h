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

#ifndef AUDIO_VOICEPLUGIN_H
#define AUDIO_VOICEPLUGIN_H

#include "base/plugins.h"
#include "common/singleton.h"
#include "common/str.h"

namespace Audio {
class AudioStream;
class SeekableAudioStream;
}

class VoiceReplacementPluginObject : public PluginObject {
public:
	virtual ~VoiceReplacementPluginObject() {}

	virtual Audio::SeekableAudioStream *translateStream(int soundId, Audio::AudioStream *original = nullptr) const = 0;
	virtual Audio::SeekableAudioStream *translateStream(const char *tag, Audio::AudioStream *original = nullptr) const = 0;
	virtual int translateText(const byte *rawMsg, int actorId, byte *dst, int dstSize) const = 0;
	virtual int getLastSoundId() const = 0;
	virtual void loadConfig() = 0;
};

class VoiceReplacementManager : public Common::Singleton<VoiceReplacementManager> {
private:
	friend class Common::Singleton<SingletonBaseType>;
	const Plugin *_activePlugin;
	VoiceReplacementManager();
public:
	const PluginList &getPlugins() const;
	void setActivePlugin(int index);
	bool isActive() const { return _activePlugin != nullptr; }
	Audio::SeekableAudioStream *translateStream(int soundId, Audio::AudioStream *original = nullptr) const;
	Audio::SeekableAudioStream *translateStream(const char *tag, Audio::AudioStream *original = nullptr) const;
	int translateText(const byte *rawMsg, int actorId, byte *dst, int dstSize) const;
	int getLastSoundId() const;
	void reloadConfig();
};

#define TranslationMan VoiceReplacementManager::instance()

#endif // AUDIO_VOICEPLUGIN_H
