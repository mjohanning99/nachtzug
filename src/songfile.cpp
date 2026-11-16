// Saving and opening songs: a Song as JSON in a .nachtzug file, and back.
//
// What's here: Song::toJson() and Song::fromJson(), used by App (saving, opening, the spare copy, the clipboard)
// and the self-test.
// Changes in the song file: all of them. Every song saved by an earlier version must still open: a new field needs
// a default in fromJson, and a saved field is never renamed or given a new meaning (AGENTS.md).
#include "model.h"
#include <QJsonArray>
#include <algorithm>

using namespace dsp;

// ---------------------------------------------------------------- JSON

static QJsonObject clipToJson(const Clip& c) {
	QJsonObject cj;
	cj["id"] = c.id;
	cj["start"] = c.start;
	cj["len"] = c.len;
	cj["loopLen"] = c.loopLen;
	cj["offset"] = c.offset;
	if (c.scene >= 0) cj["column"] = c.scene;
	if (!c.name.isEmpty()) cj["name"] = c.name;
	if (c.inKind) {
		cj["inKind"] = c.inKind;
		cj["inLen"] = c.inLen;
	}
	if (c.outKind) {
		cj["outKind"] = c.outKind;
		cj["outLen"] = c.outLen;
	}
	if (c.ad.on) { // the drummer writes these notes again when the song is opened
		QJsonObject aj;
		aj["style"] = c.ad.style;
		aj["intensity"] = double(c.ad.intensity);
		aj["complexity"] = double(c.ad.complexity);
		aj["fill"] = c.ad.fillEvery;
		aj["seed"] = double(c.ad.seed);
		aj["backbeat"] = c.ad.backbeat;
		aj["hats"] = c.ad.hats;
		aj["shape"] = c.ad.shape;
		aj["shuffle"] = double(c.ad.shuffle);
		aj["crash"] = c.ad.crash;
		cj["auto"] = aj;
		QJsonArray kept; // hits marked Keep: the drummer writes around them, so they are saved with it
		for (const Note& n : c.notes)
			if (n.lock) kept.append(QJsonArray{n.start, n.len, n.deg, (n.acc ? 1 : 0) | 4, n.chance, n.rep});
		if (!kept.isEmpty()) cj["kept"] = kept;
	} else {
		QJsonArray ns;
		for (const Note& n : c.notes) {
			QJsonArray a{n.start, n.len, n.deg,
			             (n.acc ? 1 : 0) | (n.slide ? 2 : 0) | (n.lock ? 4 : 0) |
			                 (n.alt < 0   ? 8
			                  : n.alt > 0 ? 16
			                              : 0)};
			if (n.chance != 100 || n.rep != 1) {
				a.append(n.chance);
				a.append(n.rep);
			} // left out when plain, as older songs have it
			ns.append(a);
		}
		cj["notes"] = ns;
	}
	return cj;
}

static Clip clipFromJson(const QJsonObject& cj) {
	Clip c;
	c.id = cj["id"].toInt();
	c.start = std::max(0, cj["start"].toInt());
	c.len = std::max(12, cj["len"].toInt(BAR));
	c.loopLen = std::max(24, cj["loopLen"].toInt(BAR));
	c.offset = std::max(0, cj["offset"].toInt()) % c.loopLen;
	c.scene = cj["column"].toInt(-1);
	c.name = cj["name"].toString();
	c.inKind = std::clamp(cj["inKind"].toInt(), 0, 3);
	c.inLen = std::max(24, cj["inLen"].toInt(BAR));
	c.outKind = std::clamp(cj["outKind"].toInt(), 0, 3);
	c.outLen = std::max(24, cj["outLen"].toInt(BAR));
	QJsonArray noteList = cj["notes"].toArray();
	for (const auto& kv : cj["kept"].toArray())
		noteList.append(kv); // a drummer clip's kept hits
	for (const auto& nv : noteList) {
		const QJsonArray a = nv.toArray();
		if (a.size() < 3) continue;
		Note n;
		n.start = std::max(0, a[0].toInt());
		n.len = std::max(1, a[1].toInt());
		n.deg = a[2].toInt();
		const int fl = a.size() > 3 ? a[3].toInt() : 0;
		n.acc = fl & 1;
		n.slide = fl & 2;
		n.lock = fl & 4;
		n.alt = fl & 8 ? -1 : fl & 16 ? 1 : 0;
		if (a.size() > 4) n.chance = std::clamp(a[4].toInt(100), 1, 100);
		if (a.size() > 5) n.rep = std::clamp(a[5].toInt(1), 1, 4);
		c.notes.push_back(n);
	}
	if (cj.contains("auto")) {
		const QJsonObject aj = cj["auto"].toObject();
		c.ad.on = true;
		c.ad.style = std::clamp(aj["style"].toInt(), 0, NUM_DRUM_STYLES - 1);
		c.ad.backbeat = std::clamp(aj["backbeat"].toInt(), 0, 4);
		c.ad.hats = std::clamp(aj["hats"].toInt(), 0, 4);
		c.ad.shape = std::clamp(aj["shape"].toInt(), 0, 2);
		c.ad.shuffle = float(std::clamp(aj["shuffle"].toDouble(0), 0.0, 1.0));
		c.ad.crash = aj["crash"].toBool(true);
		c.ad.intensity = float(std::clamp(aj["intensity"].toDouble(0.6), 0.0, 1.0));
		c.ad.complexity = float(std::clamp(aj["complexity"].toDouble(0.4), 0.0, 1.0));
		c.ad.fillEvery = std::clamp(aj["fill"].toInt(4), 0, 16);
		c.ad.seed = unsigned(aj["seed"].toDouble(1));
	}
	return c;
}

// a sound's knobs, by name, as in a song file and in a saved sound (see mysounds.cpp)
QJsonObject patchToJson(const Patch& patch) {
	QJsonObject pj;
	for (int i = 0; i < P_COUNT; i++)
		pj[PARAMS[i].id] = double(patch.p[i]);
	return pj;
}
// missing knobs get their default; every value is kept within its range
void patchFromJson(const QJsonObject& pj, Patch& patch) {
	for (int i = 0; i < P_COUNT; i++) {
		const double top = PARAMS[i].kind == K_CHOICE ? double(QString::fromLatin1(PARAMS[i].choices).count('|')) : 1.0;
		patch.p[i] = float(std::clamp(pj[PARAMS[i].id].toDouble(double(PARAMS[i].def)), 0.0, top));
	}
}

QJsonObject Song::toJson() const {
	QJsonObject o;
	o["format"] = "nachtzug-1";
	o["bpm"] = double(bpm);
	o["swing"] = double(swing);
	o["master"] = double(master);
	o["root"] = root;
	o["scale"] = scale;
	o["dtime"] = dtime;
	o["dfb"] = double(dfb);
	o["rsize"] = double(rsize);
	o["rdamp"] = double(rdamp);
	o["loopStart"] = loopStart;
	o["loopEnd"] = loopEnd;
	o["loopOn"] = loopOn;
	o["nextId"] = nextId;
	o["columns"] = scenes;
	if (!(input == LiveInput{})) {
		QJsonObject ij;
		ij["gain"] = double(input.gain);
		ij["cutoff"] = double(input.cutoff);
		ij["res"] = double(input.res);
		ij["wah"] = double(input.wah);
		ij["drive"] = double(input.drive);
		ij["echo"] = double(input.echo);
		ij["reverb"] = double(input.reverb);
		ij["pump"] = double(input.duck);
		ij["vol"] = double(input.vol);
		ij["pan"] = double(input.pan);
		ij["mute"] = input.mute;
		o["input"] = ij;
	}
	QJsonArray ch;
	for (int c : chords)
		ch.append(c);
	o["chords"] = ch;
	if (std::any_of(chordAlts.begin(), chordAlts.end(), [](int a) { return a != 0; })) {
		QJsonArray ca;
		for (size_t i = 0; i < chords.size(); i++)
			ca.append(i < chordAlts.size() ? chordAlts[i] : 0);
		o["chordAlts"] = ca;
	}
	QJsonArray ts;
	for (const Track& t : tracks) {
		QJsonObject j;
		j["id"] = t.id;
		j["name"] = t.name;
		j["role"] = t.role;
		j["vol"] = double(t.vol);
		j["pan"] = double(t.pan);
		j["mute"] = t.mute;
		j["solo"] = t.solo;
		j["follow"] = t.follow;
		if (t.duck > 0.f) j["pump"] = double(t.duck);
		j["octave"] = t.octave;
		j["chord"] = t.chord;
		j["arp"] = t.arp;
		j["arpRate"] = t.arpRate;
		j["arpOct"] = t.arpOct;
		j["patchName"] = t.patch.name;
		j["patch"] = patchToJson(t.patch);
		QJsonArray kj;
		for (int i = 0; i < DP_COUNT; i++)
			kj.append(double(t.kit.p[i]));
		j["kitName"] = t.kit.name;
		j["kit"] = kj;
		QJsonObject sj;
		for (int i = 0; i < D_COUNT; i++)
			if (!t.kit.sample[i].isEmpty()) sj[QString::number(i)] = t.kit.sample[i];
		if (!sj.isEmpty()) j["samples"] = sj;
		QJsonArray cs, gs;
		for (const Clip& c : t.clips)
			cs.append(clipToJson(c));
		for (const Clip& c : t.cells)
			gs.append(clipToJson(c));
		j["cells"] = gs;
		j["clips"] = cs;
		QJsonArray ls;
		for (const AutoLane& l : t.lanes) {
			if (l.pts.empty()) continue;
			QJsonObject lj;
			lj["param"] = l.param == AP_VOLUME ? QStringLiteral("volume") : QString::fromLatin1(PARAMS[l.param].id);
			QJsonArray pts;
			for (const AutoPoint& p : l.pts)
				pts.append(QJsonArray{p.tick, double(p.v)});
			lj["points"] = pts;
			lj["repeat"] = l.loop;
			ls.append(lj);
		}
		j["moves"] = ls;
		j["lineOpen"] = t.laneOpen;
		j["lineKnob"] = t.laneParam;
		ts.append(j);
	}
	o["tracks"] = ts;
	return o;
}

bool Song::fromJson(const QJsonObject& o, Song& s) {
	if (!o.contains("tracks")) return false;
	s = Song();
	s.bpm = float(std::clamp(o["bpm"].toDouble(118), 40.0, 240.0));
	s.swing = float(std::clamp(o["swing"].toDouble(0), 0.0, 1.0));
	s.master = float(std::clamp(o["master"].toDouble(0.8), 0.0, 1.0));
	s.root = std::clamp(o["root"].toInt(9), 0, 11);
	s.scale = std::clamp(o["scale"].toInt(0), 0, NUM_SCALES - 1);
	s.dtime = std::clamp(o["dtime"].toInt(2), 0, 4);
	s.dfb = float(std::clamp(o["dfb"].toDouble(0.45), 0.0, 1.0));
	s.rsize = float(std::clamp(o["rsize"].toDouble(0.55), 0.0, 1.0));
	s.rdamp = float(std::clamp(o["rdamp"].toDouble(0.45), 0.0, 1.0));
	s.loopStart = std::max(0, o["loopStart"].toInt(0));
	s.loopEnd = std::max(s.loopStart + BAR, o["loopEnd"].toInt(8 * BAR));
	s.loopOn = o["loopOn"].toBool(true);
	s.scenes = std::clamp(o["columns"].toInt(8), 4, 32);
	if (o.contains("input")) {
		const QJsonObject ij = o["input"].toObject();
		auto f = [&](const char* k, float def, double lo = 0.0) {
			return float(std::clamp(ij[k].toDouble(def), lo, 1.0));
		};
		s.input.gain = f("gain", 0.5f);
		s.input.cutoff = f("cutoff", 1.f);
		s.input.res = f("res", 0.f);
		s.input.wah = f("wah", 0.f);
		s.input.drive = f("drive", 0.f);
		s.input.echo = f("echo", 0.f);
		s.input.reverb = f("reverb", 0.1f);
		s.input.duck = f("pump", 0.f);
		s.input.vol = f("vol", 0.75f);
		s.input.pan = f("pan", 0.f, -1.0);
		s.input.mute = ij["mute"].toBool();
	}
	s.chords.clear();
	for (const auto& v : o["chords"].toArray())
		s.chords.push_back(std::clamp(v.toInt(), -42, 42)); // Chords mode and MIDI keys reach far up and down
	if (s.chords.empty()) s.chords.push_back(0);
	s.chordAlts.clear();
	for (const auto& v : o["chordAlts"].toArray())
		s.chordAlts.push_back(std::clamp(v.toInt(), -1, 1));
	int maxId = 0;
	for (const auto& tv : o["tracks"].toArray()) {
		if (int(s.tracks.size()) >= MAX_TRACKS) break;
		const QJsonObject j = tv.toObject();
		Track t;
		t.id = j["id"].toInt();
		t.name = j["name"].toString(QStringLiteral("Track"));
		t.role = std::clamp(j["role"].toInt(), 0, R_COUNT - 1);
		t.vol = float(std::clamp(j["vol"].toDouble(0.75), 0.0, 1.0));
		t.pan = float(std::clamp(j["pan"].toDouble(0), -1.0, 1.0));
		t.mute = j["mute"].toBool();
		t.solo = j["solo"].toBool();
		t.follow = j["follow"].toBool(true);
		t.duck = float(std::clamp(j["pump"].toDouble(0), 0.0, 1.0));
		t.octave = std::clamp(j["octave"].toInt(), -2, 4);
		t.chord = std::clamp(j["chord"].toInt(), 0, 6);
		t.arp = std::clamp(j["arp"].toInt(), 0, 5);
		t.arpRate = std::clamp(j["arpRate"].toInt(2), 0, 3);
		t.arpOct = std::clamp(j["arpOct"].toInt(), 0, 2);
		t.patch.name = j["patchName"].toString();
		t.laneOpen = j["lineOpen"].toBool(false);
		t.laneParam = j["lineKnob"].toInt(P_CUTOFF);
		if (t.laneParam != AP_VOLUME &&
		    (t.laneParam < 0 || t.laneParam >= P_COUNT || PARAMS[t.laneParam].kind == K_CHOICE))
			t.laneParam = P_CUTOFF;
		patchFromJson(j["patch"].toObject(), t.patch);
		t.kit = drumKits()[0];
		t.kit.name = j["kitName"].toString(t.kit.name);
		const QJsonArray kj = j["kit"].toArray();
		for (int i = 0; i < DP_COUNT && i < kj.size(); i++)
			t.kit.p[i] = float(std::clamp(kj[i].toDouble(), 0.0, 1.0));
		const QJsonObject sj = j["samples"].toObject();
		for (int i = 0; i < D_COUNT; i++)
			t.kit.sample[i] = sj[QString::number(i)].toString();
		for (const auto& cv : j["clips"].toArray()) {
			Clip c = clipFromJson(cv.toObject());
			c.scene = -1;
			maxId = std::max(maxId, c.id);
			t.clips.push_back(std::move(c));
		}
		for (const auto& cv : j["cells"].toArray()) {
			Clip c = clipFromJson(cv.toObject());
			if (c.scene < 0 || c.scene >= 64 || t.cell(c.scene)) continue;
			c.start = 0;
			c.offset = 0;
			c.len = c.loopLen;
			maxId = std::max(maxId, c.id);
			t.cells.push_back(std::move(c));
		}
		for (const auto& lv : j["moves"].toArray()) {
			const QJsonObject lj = lv.toObject();
			const QString pid = lj["param"].toString();
			AutoLane l;
			l.param = -1;
			l.loop = std::max(0, lj["repeat"].toInt());
			if (pid == "volume")
				l.param = AP_VOLUME;
			else
				for (int i = 0; i < P_COUNT; i++)
					if (pid == QLatin1String(PARAMS[i].id)) l.param = i;
			if (l.param < 0) continue;
			for (const auto& pv : lj["points"].toArray()) {
				const QJsonArray a = pv.toArray();
				if (a.size() == 2)
					l.pts.push_back({std::max(0, a[0].toInt()), float(std::clamp(a[1].toDouble(), 0.0, 1.0))});
			}
			std::stable_sort(l.pts.begin(), l.pts.end(),
			                 [](const AutoPoint& a, const AutoPoint& b) { return a.tick < b.tick; });
			if (!l.pts.empty()) t.lanes.push_back(std::move(l));
		}
		maxId = std::max(maxId, t.id);
		s.tracks.push_back(std::move(t));
	}
	s.nextId = std::max(o["nextId"].toInt(1), maxId + 1);
	refreshAutoDrums(s);
	return true;
}
