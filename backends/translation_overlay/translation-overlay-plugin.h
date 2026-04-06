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

#include "audio/voiceplugin.h"
#include "common/fs.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/path.h"
#include "common/str.h"

/**
 * Unified translation overlay plugin.
 *
 * Combines three previously separate components into one plugin registered
 * with the ScummVM plugin system as PLUGIN_TYPE_VOICE_REPLACEMENT:
 *
 *  1. Text substitution — loads a dialogue.json (schema v2.0) and replaces
 *     English SCUMM v6 dialogue with translated text at runtime.
 *
 *  2. V6 voice replacement — maps translated dialogue entries to WAV files
 *     keyed by the dialogue.json entry ID (e.g. "10042.wav"), enabling
 *     AI-dubbed voice-over for Sam & Max and other SCUMM v6 titles.
 *
 *  3. Integer-ID voice replacement — serves WAV files by numeric sound ID,
 *     used by games that pass resource IDs through Audio::Mixer::playStream().
 *
 *  4. Tag-based voice replacement — serves WAV files keyed by SCUMM v7
 *     string tags (e.g. "PIG.018"), used by The Dig and Curse of Monkey Island.
 *
 * Configuration keys (scummvm.ini, game section):
 *   translation_overlay_path   — path to dialogue.json
 *   translation_lang           — BCP-47 language tag (default: "pt-BR")
 *   translation_voice_path     — directory for integer-ID and entry-ID WAV files
 *   translation_tag_voice_path — directory for SCUMM v7 tag-based WAV files
 */
class TranslationOverlayPlugin : public VoiceReplacementPluginObject {
public:
	TranslationOverlayPlugin();

	const char *getName() const override { return "Translation overlay"; }

	/**
	 * (Re-)read all config keys and load the dialogue.json if configured.
	 * Safe to call multiple times; clears and rebuilds all lookup tables.
	 */
	void loadConfig() override;

	// -----------------------------------------------------------------------
	// Integer-ID voice interface (general games)
	// -----------------------------------------------------------------------
	bool hasReplacement(int soundId) const override;
	Audio::SeekableAudioStream *createReplacementStream(int soundId) const override;

	// -----------------------------------------------------------------------
	// Tag-based voice interface (SCUMM v7: The Dig, Curse of Monkey Island)
	// -----------------------------------------------------------------------
	bool hasTagReplacement(const char *tag) const override;
	Audio::SeekableAudioStream *createTagReplacementStream(const char *tag) const override;

	// -----------------------------------------------------------------------
	// Text substitution interface (SCUMM v6: Sam & Max)
	// -----------------------------------------------------------------------

	/**
	 * Look up the translation for @p rawMsg (raw SCUMM bytecode) spoken by
	 * @p actorId and write the decoded output into @p dst.
	 * Returns the number of bytes written, or -1 if no translation found.
	 */
	int buildOutput(const byte *rawMsg, int actorId, byte *dst, int dstSize) const override;

	// -----------------------------------------------------------------------
	// V6 voice interface (SCUMM v6: Sam & Max)
	// -----------------------------------------------------------------------

	/**
	 * Resolve @p rawMsg (raw SCUMM bytecode) for @p actorId to a WAV file
	 * path via the dialogue.json entry ID.  Returns an empty string if no
	 * voice replacement is available.
	 */
	Common::String resolveV6Voice(const byte *rawMsg, int actorId) const override;

private:
	/** Controls how translated UTF-8 text is re-encoded for the SCUMM charset. */
	enum TextEncoding {
		kAsciiStrip,   ///< Strip diacritics to plain ASCII (safe default)
		kUtf8,         ///< Pass through as-is
		kCp850,        ///< DOS CP850 — SCUMM v6/v7 charset byte indices
		kWindows1252,  ///< Windows-1252 — SCUMM v8 (CMI)
		kLatin1,       ///< ISO-8859-1
	};

	// Config (read in constructor and refreshed by loadConfig())
	Common::Path   _overlayPath;      ///< translation_overlay_path → dialogue.json
	Common::String _lang;             ///< translation_lang (default "pt-BR")

	// WAV directory: defaults to <dialogue.json parent>/waves/, overridable via
	// "voice_path" property inside dialogue.json (relative paths resolved against
	// the dialogue.json directory).
	Common::Path   _wavDir;

	// Dialogue.json state
	TextEncoding _encoding = kAsciiStrip;
	bool _cp850Mask[256] = {};

	typedef Common::HashMap<Common::String, Common::String,
	                        Common::CaseSensitiveString_Hash,
	                        Common::CaseSensitiveString_EqualTo> LineMap;

	struct ActorLines {
		LineMap _lines;     ///< cleanedKey → translated text (with inline tags)
		LineMap _entryIds;  ///< cleanedKey → dialogue.json entry ID (for V6 audio)
	};

	Common::HashMap<int, ActorLines> _linesByActor;

	void _readConfig();
	void _loadDialogueJson();

	/** Strip 0xFF opcodes and collapse whitespace — mirrors extract_dialogue.py key logic. */
	static Common::String _cleanForKey(const byte *rawMsg);

	/** Remove <FF:XX> / <FF:XX:YY:ZZ> inline tags from a stored text string. */
	static Common::String _stripCueTags(const Common::String &s);

	/** Build a WAV path: <_wavDir>/<stem>.wav */
	/** Build a WAV path: <_wavDir>/<stem>.wav */
	Common::Path _buildWavPath(const Common::String &stem) const;

	/** Open a WAV file by path and return a decoded audio stream, or nullptr on failure. */
	static Audio::SeekableAudioStream *_openWav(const Common::Path &path);
};

#endif // BACKENDS_TRANSLATION_OVERLAY_PLUGIN_H
