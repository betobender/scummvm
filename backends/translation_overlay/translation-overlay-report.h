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

#ifndef BACKENDS_TRANSLATION_OVERLAY_REPORT_H
#define BACKENDS_TRANSLATION_OVERLAY_REPORT_H

#include "backends/translation_overlay/translation-overlay-settings.h"

#include "common/str.h"
#include "common/hashmap.h"
#include "common/fs.h"
#include "common/path.h"
#include "common/ptr.h"

namespace Audio {
class SeekableAudioStream;
class AudioStream;
}

namespace TranslationOverlay {

class Report {
public:
	void reset(const Common::Path& reportPath);

	void missingActor(const Common::String &text, int actorId) const;
	void missingLine(const Common::String &text, const Settings::ActorLines& actorLines) const;
	void reportLine(const Common::String &text, const Settings::ActorLines &actorLines, const Settings::TranslationInfo &tInfo) const;

	Audio::SeekableAudioStream *translateStream(int soundId, Audio::SeekableAudioStream *voice, Audio::AudioStream *original) const;
	Audio::SeekableAudioStream *translateStream(const char *tag, Audio::SeekableAudioStream *voice, Audio::AudioStream *original) const;

private:
	const byte _separtor{';'};

	typedef Common::HashMap<uint, uint> ReportedMap;

	struct ReportEntry
	{
		enum Type {
			missingActor,
			missingLine,
			line,
		};

		Type _type;
		const Common::String *_text{nullptr};
		int _actorId{0};
		const Settings::ActorLines *_actorLines{nullptr};
		const Settings::TranslationInfo *_translationInfo{nullptr};
	};

	Common::Path _reportPath;
	Common::ScopedPtr<Common::SeekableWriteStream> _reportFilename;

	mutable bool _writeHeader{true};
	mutable ReportedMap _missingActorsReported;
	mutable ReportedMap _missingLinesReported;
	mutable ReportedMap _linesReported;

	bool isEnabled() const;
	void write(const ReportEntry &entry) const;
	void write(const Common::String *str) const;
	void write(const Common::String &str) const;
	void write(const char *str) const;
	void write(uint nbr) const;
	void write(int nbr) const;
};

}

#endif // BACKENDS_TRANSLATION_OVERLAY_REPORT_H
