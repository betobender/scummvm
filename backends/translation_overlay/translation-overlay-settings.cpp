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

#include "backends/translation_overlay/translation-overlay-helpers.h"
#include "backends/translation_overlay/translation-overlay-settings.h"

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/formats/json.h"
#include "common/fs.h"

namespace TranslationOverlay {

bool Settings::isEnabled() const {
	return !_overlayPath.empty();
}

void Settings::loadSettings() {
	resetDefault();
	loadConfMan();
	loadDialogue();
}

void Settings::resetDefault() {
	loadEncodingMap();
	_overlayPath = Common::Path();
	_keyLang = Common::String("en-US");
	_targetLang = Common::String("pt-BR");
	_voiceCache.reset();
	_encoding = kAsciiStrip;
	_hasEncodingFilter = false;
	memset(_cp850Mask, 0, sizeof(_cp850Mask));
	_linesByActor.clear();
	_allLines.clear();
}

void Settings::loadConfMan() {
	if (ConfMan.hasKey("translation_overlay_path"))
		_overlayPath = ConfMan.getPath("translation_overlay_path");

	if (isEnabled()) {
		if (ConfMan.hasKey("translation_target_lang"))
			_targetLang = ConfMan.get("translation_target_lang");
		if (ConfMan.hasKey("translation_key_lang"))
			_keyLang = ConfMan.get("translation_key_lang");
		if (ConfMan.hasKey("translation_report"))
			_reportPath = ConfMan.get("translation_report");
	}

	// WAV directory defaults to <dialogue.json directory>/wavs/.
	// May be overridden by "voice_path" inside dialogue.json (set in _loadDialogueJson).
	if (!_overlayPath.empty()) {
		_voiceCache.setWavPath(Common::FSNode(_overlayPath).getParent().getPath().appendComponent("wavs").appendComponent(_targetLang));
		//_originalsPath = Common::FSNode(_overlayPath).getParent().getPath().appendComponent("originals").appendComponent(_keyLang);
	}
}

void Settings::loadDialogue() {

	if (!isEnabled())
		return;

		Common::FSNode node(_overlayPath);
		Common::SeekableReadStream *f = node.createReadStream();
		if (!f) {
			warning("TranslationOverlayPlugin: cannot open '%s'", _overlayPath.toString().c_str());
			return;
		}

		Common::String contents;
		while (!f->eos()) {
			contents += f->readLine();
			contents += '\n';
		}
		delete f;

		Common::ScopedPtr<Common::JSONValue> rootPtr(Common::JSON::parse(contents));
		JSONHelper root("root", rootPtr.get());

		if (!root.isValid()) {
			warning("TranslationOverlayPlugin: failed to parse JSON from '%s'", _overlayPath.toString().c_str());
			return;
		}

		// Parse voice_path (optional): overrides the default <json dir>/waves/ directory.
		// Always resolved relative to the dialogue.json's parent directory.
		// rbender NEED TO CHECK THIS
		/* Common::String voicePath;
		if (root.getValue("voice_path", voicePath)) {
			_wavDir = Common::FSNode(_overlayPath).getParent().getPath().appendComponent(voicePath);
			debug(1, "TranslationOverlayPlugin: voice_path set to '%s'", _wavDir.toString().c_str());
		}*/

		// Parse text_encoding (optional; defaults to kAsciiStrip)
		Common::String encoding;
		if (root.getValue("text_encoding", encoding)) {
			EncodingMap::const_iterator it = _encodingMap.find(encoding);
			if (it != _encodingMap.end()) {
				_encoding = it->_value;
			}
		}

		// Parse charset_extended (optional array of CP850 byte values with glyphs)
		Common::JSONValue *extVal = root.getValue()->child("charset_extended");
		if (extVal && extVal->isArray()) {
			for (uint i = 0; i < extVal->asArray().size(); ++i) {
				Common::JSONValue *item = extVal->asArray()[i];
				if (item && item->isNumber()) {
					int b = (int)item->asIntegerNumber();
					if (b >= 0x80 && b <= 0xFF) {
						_cp850Mask[b] = true;
						_hasEncodingFilter = true;
					}
				}
			}
		}

		Common::JSONValue *entriesVal = root.getValue()->child("entries");
		if (!entriesVal || !entriesVal->isObject()) {
			warning("TranslationOverlayPlugin: no 'entries' object in '%s'",
					_overlayPath.toString().c_str());
			return;
		}

		int loaded = 0;
		const Common::JSONObject &entries = entriesVal->asObject();
		for (Common::JSONObject::const_iterator it = entries.begin(); it != entries.end(); ++it) {

			JSONHelper entry(it->_key, it->_value);

			if (!entry.isValid())
				continue;

			int actorId;
			Common::String actorName;
			Common::String part;

			// Mandatory data
			if (!entry.getValue("actor_id", actorId) ||
				!entry.getValue("actor", actorName))
				continue;

			// Optinal data
			entry.getValue("part", part);

			JSONHelper langs = entry.getChild("languages");
			if (!langs.isValid())
				continue;

			JSONHelper keyLang = langs.getChild(_keyLang.c_str());
			if (!keyLang.isValid())
				continue;

			Common::JSONValue *enVars = keyLang.getValue()->child("variations");
			if (!enVars || !enVars->isArray() || enVars->asArray().empty())
				continue;
			Common::JSONValue *enText = enVars->asArray()[0]->child("text");
			if (!enText || !enText->isString() || enText->asString().empty())
				continue;

			// Target language text
			Common::JSONValue *tgtLang = langs.getValue()->child(_targetLang.c_str());
			if (!tgtLang || !tgtLang->isObject())
				continue;
			Common::JSONValue *tgtVars = tgtLang->child("variations");
			if (!tgtVars || !tgtVars->isArray() || tgtVars->asArray().empty())
				continue;
			Common::JSONValue *tgtText = tgtVars->asArray()[0]->child("text");
			if (!tgtText || !tgtText->isString() || tgtText->asString().empty())
				continue;

			// Build lookup key from en-US text (strip inline tags, then clean bytecode).
			Common::String forKey = stripCueTags(enText->asString());
			Common::String key = cleanForKey(reinterpret_cast<const byte *>(forKey.c_str()));

			if (key.empty())
				continue;

			uint keyHash = key.hash();

			// Get or create the actor bucket.
			if (!_linesByActor.contains(actorId)) {
				_linesByActor[actorId] = ActorLines{actorId, actorName};
			}

			ActorLines *actorLines = &_linesByActor[actorId];

			if (actorLines->_lines.contains(keyHash)) {
				warning("TranslationOverlayPlugin: duplicate key '%s', keeping first", key.c_str());
				continue;
			}

			_voiceCache.registerVoice(entry.getKey());

			// Encode translated text according to target charset.
			Common::U32String u32text = Common::convertUtf8ToUtf32(tgtText->asString());
			Common::String stored;
			switch (_encoding) {
			case kUtf8:
				stored = tgtText->asString();
				break;
			case kCp850: {
				if (!_hasEncodingFilter) {
					stored = u32text.encode(Common::kDos850);
				} else {
					for (uint ci = 0; ci < u32text.size(); ++ci) {
						uint32 cp = (uint32)u32text[ci];
						if (cp < 0x80) {
							stored += (char)cp;
							continue;
						}
						Common::U32String singleChar(&u32text[ci], 1);
						Common::String cp850out;
						singleChar.encode(cp850out, Common::kDos850, '\x01');
						byte b = cp850out.empty() ? 0 : (byte)cp850out[0];
						if (b >= 0x80 && _cp850Mask[b]) {
							stored += (char)b;
						} else if (cp >= 0xC0 && cp <= 0xFF) {
							stored += TranslationOverlay::_latin1BaseMap[cp - 0xC0];
						}
					}
				}
				break;
			}
			case kWindows1252:
				stored = u32text.encode(Common::kWindows1252);
				break;
			case kLatin1:
				stored = u32text.encode(Common::kISO8859_1);
				break;
			case kAsciiStrip:
			default:
				stored = TranslationOverlay::stripDiacritics(u32text);
				break;
			}

			actorLines->_lines[keyHash] = TranslationInfo{stored, entry.getKey(), part, _allLines.size()};
			_allLines.push_back(&actorLines->_lines[keyHash]);

			++loaded;
		}

		if (loaded == 0) {
			warning("TranslationOverlayPlugin: no entries loaded from '%s' (lang '%s' present?)",
					_overlayPath.toString().c_str(), _targetLang.c_str());
			return;
		}

		debug(1, "TranslationOverlayPlugin: loaded %d entries from '%s' (lang=%s)",
			  loaded, _overlayPath.toString().c_str(), _targetLang.c_str());

		_voiceCache.dumpStats();
}

void Settings::loadEncodingMap() {
	if (_encodingMap.empty()) {
		_encodingMap["utf8"] = _encodingMap["utf-8"] = kUtf8;
		_encodingMap["cp850"] = _encodingMap["dos850-8"] = kCp850;
		_encodingMap["windows1252"] = _encodingMap["windows-1252"] = kWindows1252;
		_encodingMap["latin1"] = _encodingMap["iso-8859-1"] = kLatin1;
		_encodingMap["ascii_strip"] = _encodingMap["ascii"] = kAsciiStrip;
	}
}

const Settings::LinesByActorMap &Settings::getLinesByActor() const {
	return _linesByActor;
}

const VoiceCache &Settings::getVoiceCache() const {
	return _voiceCache;
}

const Settings::LinesArray &Settings::getAllLines() const {
	return _allLines;
}

const Common::Path& Settings::getReportPath() const {
	return _reportPath;
}

} // namespace TranslationOverlay
