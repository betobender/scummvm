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

#include "backends/translation_overlay/translation-overlay-report.h"

#include "common/debug.h"


namespace TranslationOverlay {

		/* if (Audio::SeekableAudioStream *cast = dynamic_cast<Audio::SeekableAudioStream *>(original)) {
		Common::String filename = _originalsPath.appendComponent(id).append(".wav").toString();
		TranslationOverlay::dumpAudioStreamToWAV(cast, filename.c_str());
	}*/


void Report::reset(const Common::Path &reportPath) {
	_reportPath = reportPath;
	
	_missingActorsReported.clear();
	_missingLinesReported.clear();
	_linesReported.clear();
	_writeHeader = true;

	Common::FSNode fsNodePath(reportPath.appendComponent("report.csv"));
	_reportFilename.reset(fsNodePath.createWriteStream(false));
}

bool Report::isEnabled() const {
	return !_reportPath.empty();
}

void Report::missingActor(const Common::String& text, int actorId) const {
	if (!isEnabled())
		return;

	uint &count = _missingActorsReported[actorId];
	if (++count == 1) {
		write(ReportEntry{ReportEntry::missingActor, &text, actorId});
		debug(1, "TranslationOverlay::Report: miss actor for = '%s', actor_id = '%d'", text.c_str(), actorId);
	}
}

void Report::missingLine(const Common::String &text, const Settings::ActorLines &actorLines) const {
	if (!isEnabled())
		return;

	const uint key = text.hash();
	uint &count = _missingLinesReported[key];

	if (++count == 1) {
		write(ReportEntry{ReportEntry::missingLine, &text, actorLines._actorId, &actorLines});
		debug(1, "TranslationOverlay::Report: miss line for = '%s', actor_id = '%d' (%s)", text.c_str(), actorLines._actorId, actorLines._actorName.c_str());
	}
}

void Report::reportLine(const Common::String &text, const Settings::ActorLines &actorLines, const Settings::TranslationInfo &tInfo) const {
	if (!isEnabled())
		return;

	const uint key = text.hash();
	uint &count = _linesReported[key];

	if (++count == 1) {
		write(ReportEntry{ReportEntry::line, &text, actorLines._actorId, &actorLines, &tInfo});
		debug(1, "TranslationOverlayPlugin: hit stem='%s' key='%s', actor='%s', actor_id = '%d' and line='%s'",
			  tInfo._entryId.c_str(),
			  text.c_str(),
			  actorLines._actorName.c_str(),
			  actorLines._actorId,
			  tInfo._translatedText.c_str());
	}
}


Audio::SeekableAudioStream* Report::translateStream(int soundId, Audio::SeekableAudioStream* voice, Audio::AudioStream* original) const {
	if (isEnabled()) {
	}

	return voice;
}
Audio::SeekableAudioStream* Report::translateStream(const char* tag, Audio::SeekableAudioStream* voice, Audio::AudioStream* original) const {
	if (isEnabled()) {

	}

	return voice;
}

void Report::write(const ReportEntry &entry) const {
	if (isEnabled()) {

		if (_writeHeader) {
			_writeHeader = false;

			write("TYPE");
			write("ACTOR_ID");
			write("LINE_HASH");
			write("LINE_ORIGINAL");
			write("ACTOR_NAME");
			write("TOTAL_LINES");
			write("STEM");
			write("PART");
			write("ALL_IDX");
			write("TRANSLATION");

			_reportFilename->writeString("\n");
			_reportFilename->flush();
		}

		switch (entry._type) {
		case ReportEntry::missingActor: write("M ACTOR"); break;
		case ReportEntry::missingLine: write("M LINE"); break;
		case ReportEntry::line: write("HIT"); break;
		}

		write(entry._actorId);
		write(entry._text != nullptr ? entry._text->hash() : 0u);
		write(entry._text);
		write(entry._actorLines != nullptr ? entry._actorLines->_actorName.c_str() : nullptr);
		write(entry._actorLines != nullptr ? entry._actorLines->_lines.size() : 0u);
		write(entry._translationInfo != nullptr ? entry._translationInfo->_entryId.c_str() : nullptr);
		write(entry._translationInfo != nullptr ? entry._translationInfo->_part.c_str() : nullptr);
		write(entry._translationInfo != nullptr ? entry._translationInfo->_ptrIndex : 0u);
		write(entry._translationInfo != nullptr ? entry._translationInfo->_translatedText.c_str() : nullptr);

		_reportFilename->writeString("\n");
		_reportFilename->flush();
	}
}

void Report::write(const Common::String *str) const {
	if (str != nullptr) {
		_reportFilename->writeString(*str);
	}
	_reportFilename->writeByte(_separtor);
}

void Report::write(const Common::String &str) const {
	_reportFilename->writeString(str);
	_reportFilename->writeByte(_separtor);
}


void Report::write(const char *str) const {
	if (str != nullptr) {
		_reportFilename->writeString(str);
	}
	_reportFilename->writeByte(_separtor);
}

void Report::write(uint nbr) const {
	write(Common::String::format("%u", nbr));
}

void Report::write(int nbr) const {
	write(Common::String::format("%d", nbr));
}

}
