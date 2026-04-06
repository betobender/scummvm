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

#include "audio/decoders/wave.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/formats/json.h"
#include "common/fs.h"
#include "common/ptr.h"
#include "common/stream.h"
#include "common/str.h"
#include "common/str-enc.h"
#include "common/textconsole.h"

// ---------------------------------------------------------------------------
// Latin-1 diacritic → ASCII base map (0xC0–0xFF)
// ---------------------------------------------------------------------------

static const char _latin1BaseMap[64] = {
	// C0..C7: À Á Â Ã Ä Å Æ Ç
	'A','A','A','A','A','A','A','C',
	// C8..CF: È É Ê Ë Ì Í Î Ï
	'E','E','E','E','I','I','I','I',
	// D0..D7: Ð Ñ Ò Ó Ô Õ Ö ×
	'D','N','O','O','O','O','O','?',
	// D8..DF: Ø Ù Ú Û Ü Ý Þ ß
	'O','U','U','U','U','Y','?','s',
	// E0..E7: à á â ã ä å æ ç
	'a','a','a','a','a','a','a','c',
	// E8..EF: è é ê ë ì í î ï
	'e','e','e','e','i','i','i','i',
	// F0..F7: ð ñ ò ó ô õ ö ÷
	'd','n','o','o','o','o','o','?',
	// F8..FF: ø ù ú û ü ý þ ÿ
	'o','u','u','u','u','y','?','y',
};

static Common::String stripDiacritics(const Common::U32String &u32) {
	Common::String result;
	for (uint i = 0; i < u32.size(); ++i) {
		uint32 cp = (uint32)u32[i];
		if (cp < 0x80) {
			result += (char)cp;
		} else if (cp >= 0xC0 && cp <= 0xFF) {
			result += _latin1BaseMap[cp - 0xC0];
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// buildOutput hex helpers
// ---------------------------------------------------------------------------

static inline bool isHexDigit(char c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static inline byte parseHexByte(const char *p) {
	auto hexNibble = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return c - 'a' + 10;
	};
	return (byte)((hexNibble(p[0]) << 4) | hexNibble(p[1]));
}

// ---------------------------------------------------------------------------
// TranslationOverlayPlugin
// ---------------------------------------------------------------------------

TranslationOverlayPlugin::TranslationOverlayPlugin() {
	_readConfig();
}

void TranslationOverlayPlugin::_readConfig() {
	// translation_overlay_path — path to dialogue.json (required)
	if (ConfMan.hasKey("translation_overlay_path"))
		_overlayPath = ConfMan.getPath("translation_overlay_path");
	else
		_overlayPath = Common::Path();

	// translation_lang (default pt-BR)
	_lang = ConfMan.hasKey("translation_lang")
	      ? ConfMan.get("translation_lang")
	      : Common::String("pt-BR");

	// WAV directory defaults to <dialogue.json directory>/waves/.
	// May be overridden by "voice_path" inside dialogue.json (set in _loadDialogueJson).
	if (!_overlayPath.empty())
		_wavDir = Common::FSNode(_overlayPath).getParent().getPath().appendComponent("waves");
	else
		_wavDir = Common::Path();
}

void TranslationOverlayPlugin::loadConfig() {
	_readConfig();
	_linesByActor.clear();
	memset(_cp850Mask, 0, sizeof(_cp850Mask));
	_encoding = kAsciiStrip;

	if (!_overlayPath.empty())
		_loadDialogueJson();
}

// ---------------------------------------------------------------------------
// _cleanForKey — mirrors extract_dialogue.py::_decode_scumm_msg() key logic
// ---------------------------------------------------------------------------

// static
Common::String TranslationOverlayPlugin::_cleanForKey(const byte *rawMsg) {
	Common::String result;
	const byte *p = rawMsg;

	while (*p) {
		byte b = *p++;
		if (b == 0xFF) {
			if (*p) {
				byte esc = *p++;
				// Opcodes 4,5,6,7,9,10,12,13,14 carry a 2-byte parameter.
				if (esc == 4 || esc == 5 || esc == 6 || esc == 7 ||
				    esc == 9 || esc == 10 || esc == 12 || esc == 13 || esc == 14) {
					p += 2;
				}
			}
		} else if (b == 0x5E) {
			result += '^';
		} else if (b >= 0x20 && b <= 0x7E) {
			result += (char)b;
		}
	}

	// Collapse runs of spaces (but not '^').
	Common::String collapsed;
	bool prevSpace = false;
	for (uint i = 0; i < result.size(); ++i) {
		if (result[i] == ' ') {
			if (!prevSpace && !collapsed.empty())
				collapsed += ' ';
			prevSpace = true;
		} else {
			collapsed += result[i];
			prevSpace = false;
		}
	}
	while (!collapsed.empty() && collapsed.lastChar() == ' ')
		collapsed.deleteLastChar();

	return collapsed;
}

// ---------------------------------------------------------------------------
// _stripCueTags — remove <FF:XX> / <FF:XX:YY:ZZ> tags from stored text
// ---------------------------------------------------------------------------

// static
Common::String TranslationOverlayPlugin::_stripCueTags(const Common::String &s) {
	Common::String out;
	const char *p = s.c_str();
	while (*p) {
		if (p[0] == '<' && p[1] == 'F' && p[2] == 'F' && p[3] == ':') {
			while (*p && *p != '>') p++;
			if (*p == '>') p++;
		} else {
			out += *p++;
		}
	}
	return out;
}

// ---------------------------------------------------------------------------
// _loadDialogueJson — parse dialogue.json and build lookup tables
// ---------------------------------------------------------------------------

void TranslationOverlayPlugin::_loadDialogueJson() {
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

	Common::ScopedPtr<Common::JSONValue> root(Common::JSON::parse(contents));
	if (!root || !root->isObject()) {
		warning("TranslationOverlayPlugin: failed to parse JSON from '%s'",
		        _overlayPath.toString().c_str());
		return;
	}

	// Parse voice_path (optional): overrides the default <json dir>/waves/ directory.
	// Always resolved relative to the dialogue.json's parent directory.
	Common::JSONValue *vpVal = root->child("voice_path");
	if (vpVal && vpVal->isString() && !vpVal->asString().empty()) {
		_wavDir = Common::FSNode(_overlayPath).getParent().getPath().appendComponent(vpVal->asString());
		debug(1, "TranslationOverlayPlugin: voice_path set to '%s'", _wavDir.toString().c_str());
	}

	// Parse text_encoding (optional; defaults to kAsciiStrip)
	Common::JSONValue *encVal = root->child("text_encoding");
	if (encVal && encVal->isString()) {
		const Common::String &enc = encVal->asString();
		if (enc == "utf8" || enc == "utf-8")
			_encoding = kUtf8;
		else if (enc == "cp850" || enc == "dos850")
			_encoding = kCp850;
		else if (enc == "windows1252" || enc == "windows-1252")
			_encoding = kWindows1252;
		else if (enc == "latin1" || enc == "iso-8859-1")
			_encoding = kLatin1;
		else if (enc == "ascii_strip" || enc == "ascii")
			_encoding = kAsciiStrip;
		else
			warning("TranslationOverlayPlugin: unknown text_encoding '%s', defaulting to ascii_strip",
			        enc.c_str());
	}

	// Parse charset_extended (optional array of CP850 byte values with glyphs)
	Common::JSONValue *extVal = root->child("charset_extended");
	if (extVal && extVal->isArray()) {
		for (uint i = 0; i < extVal->asArray().size(); ++i) {
			Common::JSONValue *item = extVal->asArray()[i];
			if (item && item->isNumber()) {
				int b = (int)item->asIntegerNumber();
				if (b >= 0x80 && b <= 0xFF)
					_cp850Mask[b] = true;
			}
		}
	}

	Common::JSONValue *entriesVal = root->child("entries");
	if (!entriesVal || !entriesVal->isObject()) {
		warning("TranslationOverlayPlugin: no 'entries' object in '%s'",
		        _overlayPath.toString().c_str());
		return;
	}

	int loaded = 0;
	const Common::JSONObject &entries = entriesVal->asObject();
	for (Common::JSONObject::const_iterator it = entries.begin();
	     it != entries.end(); ++it) {
		const Common::String &entryId = it->_key;  // e.g. "10042" — audio filename stem
		Common::JSONValue *entry = it->_value;
		if (!entry || !entry->isObject()) continue;

		Common::JSONValue *actorVal = entry->child("actor_id");
		if (!actorVal || !actorVal->isNumber()) continue;
		int actorId = (int)actorVal->asIntegerNumber();

		Common::JSONValue *langs = entry->child("languages");
		if (!langs || !langs->isObject()) continue;

		// English source text (key language)
		Common::JSONValue *enUS = langs->child("en-US");
		if (!enUS || !enUS->isObject()) continue;
		Common::JSONValue *enVars = enUS->child("variations");
		if (!enVars || !enVars->isArray() || enVars->asArray().empty()) continue;
		Common::JSONValue *enText = enVars->asArray()[0]->child("text");
		if (!enText || !enText->isString() || enText->asString().empty()) continue;

		// Target language text
		Common::JSONValue *tgtLang = langs->child(_lang.c_str());
		if (!tgtLang || !tgtLang->isObject()) continue;
		Common::JSONValue *tgtVars = tgtLang->child("variations");
		if (!tgtVars || !tgtVars->isArray() || tgtVars->asArray().empty()) continue;
		Common::JSONValue *tgtText = tgtVars->asArray()[0]->child("text");
		if (!tgtText || !tgtText->isString() || tgtText->asString().empty()) continue;

		// Build lookup key from en-US text (strip inline tags, then clean bytecode).
		Common::String forKey = _stripCueTags(enText->asString());
		Common::String key = _cleanForKey(
		    reinterpret_cast<const byte *>(forKey.c_str()));
		if (key.empty()) continue;

		// Get or create the actor bucket.
		if (!_linesByActor.contains(actorId))
			_linesByActor[actorId];
		ActorLines *actorLines = &_linesByActor[actorId];

		if (actorLines->_lines.contains(key)) {
			warning("TranslationOverlayPlugin: duplicate key '%s', keeping first", key.c_str());
			continue;
		}

		// Encode translated text according to target charset.
		Common::U32String u32text = Common::convertUtf8ToUtf32(tgtText->asString());
		Common::String stored;
		switch (_encoding) {
		case kUtf8:
			stored = tgtText->asString();
			break;
		case kCp850: {
			bool hasFilter = false;
			for (int mi = 0x80; mi <= 0xFF; ++mi)
				if (_cp850Mask[mi]) { hasFilter = true; break; }

			if (!hasFilter) {
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
						stored += _latin1BaseMap[cp - 0xC0];
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
			stored = stripDiacritics(u32text);
			break;
		}

		actorLines->_lines[key] = stored;
		actorLines->_entryIds[key] = entryId;  // store entry ID for V6 voice lookup
		++loaded;
	}

	if (loaded == 0) {
		warning("TranslationOverlayPlugin: no entries loaded from '%s' (lang '%s' present?)",
		        _overlayPath.toString().c_str(), _lang.c_str());
		return;
	}

	debug(1, "TranslationOverlayPlugin: loaded %d entries from '%s' (lang=%s)",
	      loaded, _overlayPath.toString().c_str(), _lang.c_str());
}

// ---------------------------------------------------------------------------
// buildOutput — decode stored translated text into the output buffer
// ---------------------------------------------------------------------------

int TranslationOverlayPlugin::buildOutput(const byte *rawMsg, int actorId,
                                           byte *dst, int dstSize) const {
	if (!rawMsg || !*rawMsg || !dst || dstSize < 1)
		return -1;

	if (actorId == 0xFF)
		actorId = 0;

	if (!_linesByActor.contains(actorId))
		return -1;

	const ActorLines &al = _linesByActor[actorId];
	Common::String cleaned = _cleanForKey(rawMsg);
	if (cleaned.empty())
		return -1;

	auto it = al._lines.find(cleaned);
	if (it == al._lines.end()) {
		debug(1, "TranslationOverlayPlugin: miss key='%s'", cleaned.c_str());
		return -1;
	}

	debug(1, "TranslationOverlayPlugin: hit key='%s'", cleaned.c_str());

	// Walk the stored translated text, expanding <FF:OP> / <FF:OP:P1:P2> tags
	// into raw byte sequences. All other bytes (including '^' = 0x5E) are copied
	// verbatim. The output buffer is always null-terminated.
	byte *out = dst;
	const byte *end = dst + dstSize - 1;  // reserve 1 byte for null terminator
	const char *p = it->_value.c_str();

	while (*p && out < end) {
		if (p[0] == '<' && p[1] == 'F' && p[2] == 'F' && p[3] == ':') {
			p += 4;  // skip "<FF:"

			// Opcode byte (2 hex chars)
			if (!isHexDigit(p[0]) || !isHexDigit(p[1]))
				continue;
			byte op = parseHexByte(p);
			p += 2;

			// Optional first param byte
			byte pb1 = 0, pb2 = 0;
			bool hasP1 = false, hasP2 = false;
			if (p[0] == ':' && isHexDigit(p[1]) && isHexDigit(p[2])) {
				p++;
				pb1 = parseHexByte(p); p += 2;
				hasP1 = true;
			}
			// Optional second param byte
			if (p[0] == ':' && isHexDigit(p[1]) && isHexDigit(p[2])) {
				p++;
				pb2 = parseHexByte(p); p += 2;
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

// ---------------------------------------------------------------------------
// resolveV6Voice — text-keyed voice lookup for SCUMM v6 games
// ---------------------------------------------------------------------------

Common::String TranslationOverlayPlugin::resolveV6Voice(const byte *rawMsg,
                                                         int actorId) const {
	if (!rawMsg || !*rawMsg || _wavDir.empty())
		return Common::String();

	if (actorId == 0xFF)
		actorId = 0;

	if (!_linesByActor.contains(actorId))
		return Common::String();

	const ActorLines &al = _linesByActor[actorId];
	Common::String cleaned = _cleanForKey(rawMsg);
	if (cleaned.empty())
		return Common::String();

	auto it = al._entryIds.find(cleaned);
	if (it == al._entryIds.end())
		return Common::String();

	return _buildWavPath(it->_value).toString();
}

// ---------------------------------------------------------------------------
// Integer-ID voice
// ---------------------------------------------------------------------------

Common::Path TranslationOverlayPlugin::_buildWavPath(const Common::String &stem) const {
	return _wavDir.appendComponent(stem + ".wav");
}

bool TranslationOverlayPlugin::hasReplacement(int soundId) const {
	if (_wavDir.empty())
		return false;
	return Common::FSNode(_buildWavPath(Common::String::format("%d", soundId))).exists();
}

Audio::SeekableAudioStream *TranslationOverlayPlugin::createReplacementStream(int soundId) const {
	Common::Path path = _buildWavPath(Common::String::format("%d", soundId));
	Audio::SeekableAudioStream *stream = _openWav(path);
	if (stream)
		debug(2, "TranslationOverlayPlugin: voice replacement for sound ID %d ('%s')",
		      soundId, path.toString().c_str());
	return stream;
}

// ---------------------------------------------------------------------------
// Tag-based voice (SCUMM v7)
// ---------------------------------------------------------------------------

bool TranslationOverlayPlugin::hasTagReplacement(const char *tag) const {
	if (!tag || !tag[0] || _wavDir.empty())
		return false;
	return Common::FSNode(_buildWavPath(tag)).exists();
}

Audio::SeekableAudioStream *TranslationOverlayPlugin::createTagReplacementStream(const char *tag) const {
	if (!tag || !tag[0])
		return nullptr;

	Common::Path path = _buildWavPath(tag);
	Audio::SeekableAudioStream *stream = _openWav(path);
	if (stream)
		debug(2, "TranslationOverlayPlugin: voice replacement for tag '%s'", tag);
	return stream;
}

// ---------------------------------------------------------------------------
// WAV helper
// ---------------------------------------------------------------------------

// static
Audio::SeekableAudioStream *TranslationOverlayPlugin::_openWav(const Common::Path &path) {
	Common::SeekableReadStream *f = Common::FSNode(path).createReadStream();
	if (!f) {
		debug(2, "TranslationOverlayPlugin: could not open '%s'", path.toString().c_str());
		return nullptr;
	}
	Audio::SeekableAudioStream *stream = Audio::makeWAVStream(f, DisposeAfterUse::YES);
	if (!stream)
		warning("TranslationOverlayPlugin: failed to decode WAV '%s'", path.toString().c_str());
	return stream;
}

REGISTER_PLUGIN_STATIC(TRANSLATION_OVERLAY, PLUGIN_TYPE_VOICE_REPLACEMENT, TranslationOverlayPlugin);
