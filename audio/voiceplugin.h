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
class SeekableAudioStream;
}

/**
 * @defgroup audio_voiceplugin Voice replacement plugin
 * @ingroup audio
 *
 * @brief API for replacing in-game speech with AI-generated audio.
 *
 * Plugins of this type are queried by the mixer whenever a speech stream
 * (kSpeechSoundType) is about to be played. If the active plugin has a
 * replacement for the given sound ID, the replacement stream is used instead
 * of the original game audio, enabling dubbed/translated voice tracks.
 * @{
 */

/**
 * Abstract base class for voice-replacement plugin objects.
 *
 * Concrete subclasses load replacement audio (e.g. WAV files, OGG files, or
 * streams produced by an AI synthesis backend) keyed by the game's sound
 * resource ID. The resource ID is the same integer passed to
 * Audio::Mixer::playStream() as the @c id parameter.
 */
class VoiceReplacementPluginObject : public PluginObject {
public:
	virtual ~VoiceReplacementPluginObject() {}

	/**
	 * Returns true if a replacement stream is available for @p soundId.
	 *
	 * This is a cheap existence-check called while holding the mixer mutex,
	 * so it must not block.
	 */
	virtual bool hasReplacement(int soundId) const = 0;

	/**
	 * Creates and returns a new SeekableAudioStream for @p soundId, or
	 * nullptr if the replacement cannot be opened.
	 *
	 * Ownership of the returned stream is transferred to the caller (the
	 * mixer will delete it when playback finishes).
	 */
	virtual Audio::SeekableAudioStream *createReplacementStream(int soundId) const = 0;

	// -----------------------------------------------------------------------
	// Tag-based interface (SCUMM v7 games: The Dig, CMI)
	//
	// SCUMM v7 identifies speech by a string tag (e.g. "PIG.018") rather than
	// an integer resource ID. Plugins that serve SCUMM v7 replacements should
	// override these two methods instead of (or in addition to) the integer
	// versions above.
	// -----------------------------------------------------------------------

	/**
	 * Returns true if a replacement stream is available for the given string
	 * @p tag (e.g. "PIG.018"). Default implementation returns false.
	 */
	virtual bool hasTagReplacement(const char *tag) const { return false; }

	/**
	 * Creates and returns a replacement stream for @p tag, or nullptr.
	 * Ownership transferred to caller. Default implementation returns nullptr.
	 */
	virtual Audio::SeekableAudioStream *createTagReplacementStream(const char *tag) const { return nullptr; }

	// -----------------------------------------------------------------------
	// Text substitution interface (SCUMM v6 games)
	// -----------------------------------------------------------------------

	/**
	 * Look up the translation for @p rawMsg (raw SCUMM bytecode) spoken by
	 * @p actorId and write the decoded output into @p dst.
	 * Returns bytes written, or -1 if no translation found.
	 * Default implementation returns -1 (no-op).
	 */
	virtual int buildOutput(const byte *rawMsg, int actorId,
	                        byte *dst, int dstSize) const { return -1; }

	/**
	 * Resolve @p rawMsg (raw SCUMM bytecode) for @p actorId to a WAV file
	 * path via the dialogue.json entry ID.  Used for V6 voice replacement
	 * (Sam & Max). Returns an empty string if no voice is available.
	 * Default implementation returns an empty string.
	 */
	virtual Common::String resolveV6Voice(const byte *rawMsg, int actorId) const {
		return Common::String();
	}

	/**
	 * (Re-)read all configuration keys and reload any data files.
	 * Called from the engine after the game config domain is active.
	 * Default implementation is a no-op.
	 */
	virtual void loadConfig() {}
};

/**
 * Singleton manager for the active voice-replacement plugin.
 *
 * Only one plugin may be active at a time. The mixer calls VoiceMan
 * on every speech playback request.
 */
class VoiceReplacementManager : public Common::Singleton<VoiceReplacementManager> {
private:
	friend class Common::Singleton<SingletonBaseType>;
	const Plugin *_activePlugin;

	VoiceReplacementManager();

public:
	/** Returns all loaded voice-replacement plugins. */
	const PluginList &getPlugins() const;

	/**
	 * Activates a plugin by index into getPlugins(). Pass -1 to disable
	 * voice replacement entirely.
	 */
	void setActivePlugin(int index);

	/** Returns true if a replacement plugin is currently active. */
	bool isActive() const { return _activePlugin != nullptr; }

	/**
	 * If the active plugin has a replacement for @p soundId, creates and
	 * returns the replacement stream. Returns nullptr otherwise.
	 */
	Audio::SeekableAudioStream *createReplacementStream(int soundId) const;

	/** Tag-based variant used by SCUMM v7. Returns nullptr if no replacement. */
	bool hasTagReplacement(const char *tag) const;
	Audio::SeekableAudioStream *createTagReplacementStream(const char *tag) const;

	/**
	 * Text substitution for SCUMM v6 games: look up the translation for
	 * @p rawMsg spoken by @p actorId and write decoded output into @p dst.
	 * Returns bytes written, or -1 if no translation found.
	 */
	int buildOutput(const byte *rawMsg, int actorId, byte *dst, int dstSize) const;

	/**
	 * V6 voice replacement: resolve @p rawMsg (raw SCUMM bytecode) to a WAV
	 * path via the active plugin's dialogue.json entry ID mapping.
	 * Returns an empty string if no voice replacement is available.
	 */
	Common::String resolveV6Voice(const byte *rawMsg, int actorId) const;

	/**
	 * Instruct the active plugin to (re-)read config keys and reload data.
	 * Call after the game config domain becomes active.
	 */
	void reloadConfig();
};

/** Convenience shortcut for accessing the translation overlay / voice replacement manager. */
#define TranslationMan VoiceReplacementManager::instance()
/** Keep the old alias so existing callers (mixer.cpp etc.) continue to compile. */
#define VoiceMan VoiceReplacementManager::instance()
/** @} */

#endif // AUDIO_VOICEPLUGIN_H
