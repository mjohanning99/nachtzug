// Parity: runs everything in the song code that makes or changes notes and sounds, with fixed seeds, and writes
// down what came out as plain text, one line per result. Two runs of the same program give the same file; a
// tidy-up that changes nothing must give the same file too. tools/parity.sh renders the songs and takes the
// screenshots beside it.
//
// What's here: writeParity(), called from main() for "--parity folder".
// Changes in the song file: none (it writes the demo song's JSON for comparison, and reads nothing).
#include "parity.h"
#include "model.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <random>

namespace {

QString num(float v) {
	return QString::number(double(v), 'g', 9);
}

QString notesText(const Clip& c) {
	QString out;
	for (const Note& n : c.notes) {
		out += QStringLiteral("%1,%2,%3,%4%5%6,%7,%8,%9 ")
		           .arg(n.start)
		           .arg(n.len)
		           .arg(n.deg)
		           .arg(n.acc ? 'a' : '-')
		           .arg(n.slide ? 's' : '-')
		           .arg(n.lock ? 'k' : '-')
		           .arg(n.chance)
		           .arg(n.rep)
		           .arg(n.alt);
	}
	return out;
}

QString patchText(const Patch& p) {
	QString out = p.name + QLatin1Char(':');
	for (float v : p.p)
		out += QLatin1Char(' ') + num(v);
	return out;
}

QString kitText(const DrumKit& k) {
	QString out = k.name + QLatin1Char(':');
	for (float v : k.p)
		out += QLatin1Char(' ') + num(v);
	for (const QString& s : k.sample)
		out += QLatin1Char(' ') + (s.isEmpty() ? QStringLiteral("-") : s);
	return out;
}

QString songText(const Song& s) {
	return QString::fromUtf8(QJsonDocument(s.toJson()).toJson(QJsonDocument::Compact));
}

// a clip whose notes were played in by hand: the generated notes, pushed off the grid, some doubled, some lost,
// a few tiny accidental ones beside them
Clip played(const Clip& from, std::mt19937& g) {
	Clip c = from;
	c.notes.clear();
	std::uniform_int_distribution<int> jitter(-9, 9), coin(0, 9);
	for (const Note& n : from.notes) {
		const int roll = coin(g);
		if (roll == 0) continue;
		Note m = n;
		m.start = std::max(0, n.start + jitter(g));
		c.notes.push_back(m);
		if (roll == 1) c.notes.push_back(m);
		if (roll == 2) {
			Note tiny = m;
			tiny.len = 3;
			tiny.start = m.start + 5;
			tiny.deg = m.deg + 1;
			c.notes.push_back(tiny);
		}
	}
	return c;
}

} // namespace

int writeParity(const QString& folder) {
	QDir().mkpath(folder);
	QFile file(folder + QStringLiteral("/model.txt"));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return 1;
	QTextStream out(&file);

	// the tables and helpers the rest is built on
	for (int sc = 0; sc < NUM_SCALES; sc++) {
		for (int root = 0; root < 12; root += 5) {
			Song s;
			s.scale = sc;
			s.root = root;
			Track t = makeTrack(s, R_LEAD);
			out << "scale " << sc << " root " << root << ':';
			for (int deg = DEG_MIN; deg <= DEG_MAX; deg++) {
				out << ' ' << s.degToSemis(deg) << '/' << s.approachAlt(deg) << '/' << s.noteName(t, deg) << '/'
				    << s.chordName(deg) << '/' << s.midiOf(t, deg);
			}
			for (int semis = -12; semis <= 24; semis++)
				out << ' ' << s.degreeNear(semis);
			for (int chord = 0; chord < 7; chord++) {
				int offs[4], count = 0;
				s.chordOffsets(chord, offs, count);
				out << " c" << chord;
				for (int i = 0; i < count; i++)
					out << ',' << offs[i];
			}
			out << '\n';
		}
	}
	for (int p = 0; p < dsp::P_COUNT; p++) {
		out << "param " << p << ' ' << paramLabel(p);
		for (float v : {0.f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.f})
			out << " | " << formatParam(p, v);
		out << '\n';
	}
	for (double tick : {0.0, 23.0, 24.0, 48.0, 71.5, 96.0, 383.0}) {
		for (float swing : {0.f, 0.3f, 0.66f}) {
			out << "swing " << tick << ' ' << num(swing) << ' ' << unswing(tick, swing) << ' ' << swingPos(tick, swing)
			    << '\n';
		}
	}
	for (const Patch& p : presets())
		out << "preset " << presetGroup(p.name) << ' ' << patchText(p) << '\n';
	for (const DrumKit& k : drumKits())
		out << "kit " << kitText(k) << '\n';

	// sounds: rolled and changed
	for (int role = 0; role < R_COUNT; role++) {
		for (unsigned seed = 1; seed <= 12; seed++) {
			std::mt19937 g(seed);
			const Patch rolled = rollPatch(role, g);
			out << "roll " << role << ' ' << seed << ' ' << patchText(rolled) << '\n';
			out << "mutate " << role << ' ' << seed << ' ' << patchText(mutatePatch(rolled, g)) << '\n';
		}
	}

	// new songs and tracks
	const Song demo = demoSong();
	out << "demo " << songText(demo) << '\n';
	{
		Song back;
		Song::fromJson(demo.toJson(), back);
		out << "demo again " << songText(back) << '\n';
	}
	for (int role = 0; role < R_COUNT; role++) {
		Song s = demo;
		out << "track " << role << ' ' << songText([&] {
			Song t = s;
			t.tracks.push_back(makeTrack(t, role));
			return t;
		}()) << '\n';
		std::mt19937 g(100 + unsigned(role));
		addTrackWithClip(s, role, g);
		out << "track with clip " << role << ' ' << songText(s) << '\n';
	}
	for (unsigned seed = 1; seed <= 12; seed++) {
		for (int sc : {0, 1, NUM_SCALES - 1}) {
			Song s = demo;
			s.scale = sc;
			std::mt19937 g(seed);
			out << "progression " << seed << ' ' << sc << ':';
			for (int c : randomProgression(s, g))
				out << ' ' << c;
			out << '\n';
		}
	}

	// every idea, at three densities, on two lengths and two scales; then varied; then played in and cleaned up
	for (int role = 0; role < R_COUNT; role++) {
		for (int sc : {0, 3 % NUM_SCALES}) {
			Song s = demo;
			s.scale = sc;
			const Track t = makeTrack(s, role);
			for (const GenDef& gen : generatorsFor(role)) {
				for (int bars : {1, 4}) {
					for (float density : {0.2f, 0.55f, 0.9f}) {
						for (unsigned seed = 1; seed <= 4; seed++) {
							Clip c;
							c.len = c.loopLen = bars * BAR;
							std::mt19937 g(seed * 31u + unsigned(gen.id));
							generate(s, t, c, gen.id, density, g);
							const QString key = QStringLiteral("%1 %2 %3 %4 %5 %6")
							                        .arg(role)
							                        .arg(sc)
							                        .arg(gen.id)
							                        .arg(bars)
							                        .arg(num(density))
							                        .arg(seed);
							out << "idea " << key << ' ' << c.name << ": " << notesText(c) << '\n';
							Clip v = c;
							varyClip(s, t, v, g);
							out << "vary " << key << ": " << notesText(v) << '\n';
							Clip p = played(c, g);
							for (int grid : {12, 24}) {
								Clip cleaned = p;
								const CleanReport r = cleanUpClip(cleaned, t.isDrums(), grid);
								out << "clean " << key << ' ' << grid << ' ' << r.snapped << ',' << r.merged << ','
								    << r.removed << ',' << r.filled << ',' << r.fixed << ": " << notesText(cleaned)
								    << '\n';
							}
						}
					}
				}
			}
		}
	}

	// the auto drummer, through its settings
	for (int style = 0; style < NUM_DRUM_STYLES; style++) {
		for (unsigned seed = 1; seed <= 3; seed++) {
			for (int variant = 0; variant < 6; variant++) {
				Clip c;
				c.len = c.loopLen = 8 * BAR;
				c.ad.on = true;
				c.ad.style = style;
				c.ad.seed = seed;
				c.ad.intensity = 0.2f + 0.15f * float(variant);
				c.ad.complexity = 0.9f - 0.15f * float(variant);
				c.ad.fillEvery = variant % 3 == 0 ? 0 : (variant % 3 == 1 ? 2 : 4);
				c.ad.backbeat = variant % 5;
				c.ad.hats = (variant + 2) % 5;
				c.ad.shape = variant % 3;
				c.ad.shuffle = variant % 2 ? 0.4f : 0.f;
				c.ad.crash = variant % 2 == 0;
				autoDrumFill(c);
				out << "drummer " << style << ' ' << seed << ' ' << variant << ": " << notesText(c) << '\n';
			}
		}
	}
	{
		Song s = demo;
		const bool changed = refreshAutoDrums(s);
		out << "refresh " << changed << ' ' << songText(s) << '\n';
	}

	// Arrange for me, three lengths, with and without a melody written for the lead
	for (int length = 0; length < 3; length++) {
		out << "plan " << length << ':';
		for (const ArrangePart& part : arrangePlan(length))
			out << ' ' << part.name << '/' << part.type << '/' << part.bars;
		out << '\n';
		for (bool lead : {false, true}) {
			Song s = demo;
			std::mt19937 g(7);
			const std::vector<ArrangePart> parts = arrangeSong(s, length, lead, g);
			out << "arranged " << length << ' ' << lead << ' ' << parts.size() << ' ' << songText(s) << '\n';
		}
	}

	out.flush();
	return file.error() == QFileDevice::NoError ? 0 : 1;
}
