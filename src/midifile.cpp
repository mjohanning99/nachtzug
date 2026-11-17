// MIDI export: see midifile.h.
//
// What's here: gmDrumNote() and writeMidiFile(), used by Export and the self-test; the engine plays the song with
// its note log on, and the small helpers below write the bytes of a standard MIDI file.
// Changes in the song file: none.

#include "midifile.h"
#include <QFile>
#include <QHash>
#include <algorithm>
#include <cmath>

int gmDrumNote(int drum) {
	static const int map[dsp::D_COUNT] = {36, 38, 39, 42,
	                                      46, 45, 37, 49}; // kick, snare, clap, closed hat, open hat, tom, rim, cymbal
	return map[std::clamp(drum, 0, dsp::D_COUNT - 1)];
}

namespace {
constexpr int MPPQ = 960; // ticks per quarter note in the file: fine enough for swing and quick repeats

struct Ev {
	long tick;
	int order;
	QByteArray bytes;
}; // order: note-offs before note-ons at the same tick

void putVlq(QByteArray& out, quint32 v) {
	quint8 buf[5];
	int n = 0;
	buf[n++] = quint8(v & 0x7f);
	while (v >>= 7)
		buf[n++] = quint8(0x80 | (v & 0x7f));
	while (n)
		out.append(char(buf[--n]));
}
void put32(QByteArray& out, quint32 v) {
	for (int i = 3; i >= 0; i--)
		out.append(char((v >> (8 * i)) & 0xff));
}
void put16(QByteArray& out, quint16 v) {
	out.append(char(v >> 8));
	out.append(char(v & 0xff));
}

QByteArray meta(int type, const QByteArray& data) {
	QByteArray b;
	b.append(char(0xff));
	b.append(char(type));
	putVlq(b, quint32(data.size()));
	b.append(data);
	return b;
}

QByteArray chunk(std::vector<Ev> evs, long endTick) {
	std::stable_sort(evs.begin(), evs.end(),
	                 [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });
	QByteArray body;
	long last = 0;
	for (const Ev& e : evs) {
		putVlq(body, quint32(std::max(0L, e.tick - last)));
		last = std::max(last, e.tick);
		body.append(e.bytes);
	}
	putVlq(body, quint32(std::max(0L, endTick - last)));
	body.append(meta(0x2f, {})); // end of track
	QByteArray out("MTrk");
	put32(out, quint32(body.size()));
	out.append(body);
	return out;
}

QByteArray msg(int status, int a, int b) {
	QByteArray m;
	m.append(char(status));
	m.append(char(a & 0x7f));
	m.append(char(b & 0x7f));
	return m;
}
} // namespace

double writeMidiFile(const Song& song, const QString& path, const RenderOptions& opt) {
	// play the song without sound and collect the notes
	const float sr = 48000.f;
	Song s = song;
	s.liveMode = false;
	int from = 0, to = 0;
	if (opt.loopOnly && s.loopEnd > s.loopStart) {
		from = s.loopStart;
		to = s.loopEnd;
		s.loopOn = true;
		s.loopStart = from;
		s.loopEnd = to;
	} else {
		from = 0;
		to = std::max(BAR, s.endTick());
	}
	std::vector<NoteEvent> log;
	Engine e(sr);
	e.eventsOnly = true;
	e.log = &log;
	if (!opt.loopOnly) e.ignoreLoop.store(true);
	e.setSong(std::make_shared<const Song>(s));
	EngineEvent play;
	play.type = EngineEvent::Play;
	play.tick = from;
	e.push(play);
	const double seconds = renderLength(s, opt);
	const long frames = long(seconds * sr);
	std::vector<float> buf(2 * 512);
	long done = 0;
	for (; done < frames; done += 512)
		e.process(buf.data(), 512);
	std::erase_if(log, [&](const NoteEvent& ev) {
		return ev.on && ev.frame >= double(frames) - dsp::CHUNK;
	}); // the next round starting at the very end
	EngineEvent stop;
	stop.type = EngineEvent::Stop;
	e.push(stop);
	e.process(buf.data(), 512); // the notes still sounding are let go here
	const long endTick = long(std::lround(seconds * s.bpm / 60.0 * MPPQ));
	auto tickOf = [&](double frame) { return std::min(endTick, long(std::lround(frame / sr * s.bpm / 60.0 * MPPQ))); };

	// the tempo track
	std::vector<QByteArray> chunks;
	{
		std::vector<Ev> evs;
		const quint32 usPerBeat = quint32(std::lround(60000000.0 / s.bpm));
		QByteArray tempo;
		tempo.append(char(usPerBeat >> 16));
		tempo.append(char((usPerBeat >> 8) & 0xff));
		tempo.append(char(usPerBeat & 0xff));
		evs.push_back({0, 0, meta(0x03, QByteArrayLiteral("Nachtzug"))});
		evs.push_back({0, 0, meta(0x51, tempo)});
		evs.push_back({0, 0, meta(0x58, QByteArray("\x04\x02\x18\x08", 4))}); // 4/4
		chunks.push_back(chunk(evs, endTick));
	}
	// a General MIDI sound to start from, for each kind of track
	static const int program[R_COUNT] = {38, 81, 89, 80, 0}; // synth bass, sawtooth lead, warm pad, square lead
	int nextChannel = 0;
	for (int id : audibleTracks(s)) {
		const Track* t = s.track(id);
		if (!t) continue;
		const bool drums = t->isDrums();
		int ch = 9;
		if (!drums) {
			if (nextChannel == 9) nextChannel++;
			ch = std::min(nextChannel++, 15);
		}
		std::vector<Ev> evs;
		evs.push_back({0, 0, meta(0x03, ui(t->name).toUtf8())});
		if (!drums) {
			QByteArray pc;
			pc.append(char(0xc0 | ch));
			pc.append(char(program[t->role]));
			evs.push_back({0, 0, pc});
		}
		struct Hit {
			long tick;
			int note, vel;
		};
		std::vector<Hit> hits;   // drum hits, in time order
		QHash<int, int> keyNote; // engine key -> MIDI note sounding for it
		int sounding[128];
		std::fill(std::begin(sounding), std::end(sounding), -1); // note -> key holding it
		for (const NoteEvent& ev : log) {
			if (ev.track != id) continue;
			const long tick = tickOf(ev.frame);
			if (drums) {
				if (ev.on) hits.push_back({tick, gmDrumNote(ev.note), ev.acc ? 127 : 100});
				continue;
			}
			if (ev.on) {
				const int note = std::clamp(ev.note, 0, 127);
				if (sounding[note] >= 0) {
					evs.push_back({tick, 0, msg(0x80 | ch, note, 0)});
					keyNote.remove(sounding[note]);
				} // the same note again: end the first
				evs.push_back({tick, 1, msg(0x90 | ch, note, ev.acc ? 120 : 90)});
				sounding[note] = ev.key;
				keyNote.insert(ev.key, note);
			} else if (auto it = keyNote.find(ev.key); it != keyNote.end()) {
				const int note = it.value();
				keyNote.erase(it);
				if (sounding[note] == ev.key) {
					evs.push_back({tick, 0, msg(0x80 | ch, note, 0)});
					sounding[note] = -1;
				}
			}
		}
		for (int note = 0; note < 128; note++)
			if (sounding[note] >= 0) evs.push_back({endTick, 0, msg(0x80 | ch, note, 0)});
		// drum hits last a thirty-second, or until the same drum hits again, so no two of them overlap
		for (size_t i = 0; i < hits.size(); i++) {
			long off = std::min(endTick, hits[i].tick + MPPQ / 8);
			for (size_t j = i + 1; j < hits.size(); j++)
				if (hits[j].note == hits[i].note) {
					off = std::min(off, hits[j].tick);
					break;
				}
			if (off <= hits[i].tick) continue; // two hits of one drum at the same moment: one is enough
			evs.push_back({hits[i].tick, 1, msg(0x90 | ch, hits[i].note, hits[i].vel)});
			evs.push_back({off, 0, msg(0x80 | ch, hits[i].note, 0)});
		}
		chunks.push_back(chunk(evs, endTick));
	}

	QByteArray file("MThd");
	put32(file, 6);
	put16(file, 1);
	put16(file, quint16(chunks.size()));
	put16(file, MPPQ);
	for (const QByteArray& c : chunks)
		file.append(c);
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly) || f.write(file) != file.size()) {
		f.remove();
		return -1.0;
	}
	return seconds;
}
