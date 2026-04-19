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

#ifndef BACKENDS_TRANSLATION_OVERLAY_SETTINGS_H
#define BACKENDS_TRANSLATION_OVERLAY_SETTINGS_H

#include "backends/translation_overlay/translation-overlay-stream.h"

#include "common/array.h"
#include "common/hash-str.h"
#include "common/path.h"

namespace TranslationOverlay {

class Settings {
public:
	enum TextEncoding {
		kAsciiStrip,  ///< Strip diacritics to plain ASCII (safe default)
		kUtf8,        ///< Pass through as-is
		kCp850,       ///< DOS CP850 — SCUMM v6/v7 charset byte indices
		kWindows1252, ///< Windows-1252 — SCUMM v8 (CMI)
		kLatin1,      ///< ISO-8859-1
	};

	struct TranslationInfo {
		Common::String _translatedText;
		Common::String _entryId;
		Common::String _part;
		unsigned int _ptrIndex;
	};

	typedef Common::HashMap<uint, TranslationInfo> LineMap;

	typedef Common::HashMap<Common::String, TextEncoding,
			Common::CaseSensitiveString_Hash,
			Common::CaseSensitiveString_EqualTo> EncodingMap;

	struct ActorLines {
		int _actorId;
		Common::String _actorName;
		LineMap _lines;
	};

	typedef Common::HashMap<int, ActorLines> LinesByActorMap;
	typedef Common::Array<TranslationInfo *> LinesArray;

	bool isEnabled() const;
	void loadSettings();
	const LinesByActorMap &getLinesByActor() const;
	const VoiceCache &getVoiceCache() const;
	const LinesArray &getAllLines() const;
	const Common::Path& getReportPath() const;

private:
	Common::Path _overlayPath;
	Common::Path _reportPath;
	Common::String _targetLang;
	Common::String _keyLang;
	TextEncoding _encoding{kAsciiStrip};
	EncodingMap _encodingMap;
	VoiceCache _voiceCache;
	bool _cp850Mask[256] = {};
	bool _hasEncodingFilter{false};
	LinesByActorMap _linesByActor;
	LinesArray _allLines;

	void resetDefault();
	void loadEncodingMap();
	void loadConfMan();
	void loadDialogue();
};

}

#endif // BACKENDS_TRANSLATION_OVERLAY_SETTINGS_H
