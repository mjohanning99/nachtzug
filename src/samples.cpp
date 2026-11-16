#include "samples.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <cmath>
#include <cstring>

namespace samples {

static QString tr(const char* s) {
	return QCoreApplication::translate("samples", s);
} // the context lupdate gives this namespace

// ---------------------------------------------------------------- the library

// category (the folder under :/samples), name (the file without .wav), and a line about it
struct Entry {
	const char* folder;
	const char* name;
	const char* about;
};
static const Entry ENTRIES[] = {
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Deep boom"),
	 QT_TRANSLATE_NOOP("samples", "Long and round, with a deep tail")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Punch"),
	 QT_TRANSLATE_NOOP("samples", "Short and firm, cuts through a busy mix")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Analog thump"),
	 QT_TRANSLATE_NOOP("samples", "Dry and woody, like an old drum machine")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Rumble"),
	 QT_TRANSLATE_NOOP("samples", "Hard, with a dark rumbling tail, for techno")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Hard"), QT_TRANSLATE_NOOP("samples", "Distorted and tight, for acid")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Soft felt"),
	 QT_TRANSLATE_NOOP("samples", "Muffled and gentle, for quiet songs")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Sub boom"),
	 QT_TRANSLATE_NOOP("samples", "More felt than heard: a very low swell")},
    {"Kicks", QT_TRANSLATE_NOOP("samples", "Click"), QT_TRANSLATE_NOOP("samples", "Very short, with a sharp click")},
    {"Snares", QT_TRANSLATE_NOOP("samples", "Tight"), QT_TRANSLATE_NOOP("samples", "Crisp and short")},
    {"Snares", QT_TRANSLATE_NOOP("samples", "Fat"), QT_TRANSLATE_NOOP("samples", "Deep and long, with a little room")},
    {"Snares", QT_TRANSLATE_NOOP("samples", "Tape"),
	 QT_TRANSLATE_NOOP("samples", "Warm and worn, as if played from tape")},
    {"Snares", QT_TRANSLATE_NOOP("samples", "Brush"), QT_TRANSLATE_NOOP("samples", "The soft swish of a brush")},
    {"Snares", QT_TRANSLATE_NOOP("samples", "Electro"), QT_TRANSLATE_NOOP("samples", "Noise and a falling zap")},
    {"Snares", QT_TRANSLATE_NOOP("samples", "Rim snare"), QT_TRANSLATE_NOOP("samples", "A snare with a ringing rim")},
    {"Claps", QT_TRANSLATE_NOOP("samples", "Classic"),
	 QT_TRANSLATE_NOOP("samples", "Several hands at once, like a drum machine")},
    {"Claps", QT_TRANSLATE_NOOP("samples", "Big room"), QT_TRANSLATE_NOOP("samples", "A clap in a large hall")},
    {"Claps", QT_TRANSLATE_NOOP("samples", "Snap"), QT_TRANSLATE_NOOP("samples", "A finger snap")},
    {"Claps", QT_TRANSLATE_NOOP("samples", "Crowd"), QT_TRANSLATE_NOOP("samples", "Many people clapping together")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Closed"), QT_TRANSLATE_NOOP("samples", "The everyday hi-hat")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Tight"), QT_TRANSLATE_NOOP("samples", "Very short and bright")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Open"), QT_TRANSLATE_NOOP("samples", "Opens and rings out")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Sizzle"), QT_TRANSLATE_NOOP("samples", "A long, sizzling open hat")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Pedal"), QT_TRANSLATE_NOOP("samples", "A soft chick from the foot pedal")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Shaker"), QT_TRANSLATE_NOOP("samples", "A shaker, back and forth")},
    {"Hats", QT_TRANSLATE_NOOP("samples", "Tambourine"), QT_TRANSLATE_NOOP("samples", "Jingles")},
    {"Cymbals", QT_TRANSLATE_NOOP("samples", "Crash"), QT_TRANSLATE_NOOP("samples", "A big crash")},
    {"Cymbals", QT_TRANSLATE_NOOP("samples", "Ride"), QT_TRANSLATE_NOOP("samples", "A ringing ride")},
    {"Cymbals", QT_TRANSLATE_NOOP("samples", "Ride bell"), QT_TRANSLATE_NOOP("samples", "The bell of a ride")},
    {"Cymbals", QT_TRANSLATE_NOOP("samples", "Reverse crash"),
	 QT_TRANSLATE_NOOP("samples", "A crash backwards: it swells up to the next bar")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Cowbell"), QT_TRANSLATE_NOOP("samples", "Two metallic tones")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Clave"), QT_TRANSLATE_NOOP("samples", "A bright wooden click")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Woodblock"), QT_TRANSLATE_NOOP("samples", "A hollow wooden knock")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Conga high"), QT_TRANSLATE_NOOP("samples", "A hand drum, high")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Conga low"), QT_TRANSLATE_NOOP("samples", "A hand drum, low")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Bongo"), QT_TRANSLATE_NOOP("samples", "A small hand drum")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Tom low"), QT_TRANSLATE_NOOP("samples", "A synth tom, low")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Tom mid"), QT_TRANSLATE_NOOP("samples", "A synth tom, middle")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Tom high"), QT_TRANSLATE_NOOP("samples", "A synth tom, high")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Rimshot"), QT_TRANSLATE_NOOP("samples", "A stick on the rim")},
    {"Percussion", QT_TRANSLATE_NOOP("samples", "Triangle"), QT_TRANSLATE_NOOP("samples", "A long, clear ring")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Rail joint"),
	 QT_TRANSLATE_NOOP("samples", "Wheels over a rail joint: ta-dum")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Rail joint far"), QT_TRANSLATE_NOOP("samples", "The same, far away")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Door chime"),
	 QT_TRANSLATE_NOOP("samples", "Two tones before the doors close")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Station bell"),
	 QT_TRANSLATE_NOOP("samples", "A small bell on the platform")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Brake hiss"), QT_TRANSLATE_NOOP("samples", "Air from the brakes")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Night horn"),
	 QT_TRANSLATE_NOOP("samples", "A soft horn chord in the distance")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Steam puff"), QT_TRANSLATE_NOOP("samples", "A puff of steam")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Coupling clank"), QT_TRANSLATE_NOOP("samples", "Two carriages coupling")},
    {"Train", QT_TRANSLATE_NOOP("samples", "Wheel squeal"),
	 QT_TRANSLATE_NOOP("samples", "Wheels squealing in a curve")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Sub drop"), QT_TRANSLATE_NOOP("samples", "A deep tone falling away")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Zap"), QT_TRANSLATE_NOOP("samples", "A fast falling zap")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Laser"), QT_TRANSLATE_NOOP("samples", "A space laser")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Blip"), QT_TRANSLATE_NOOP("samples", "A short beep")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Acid stab"), QT_TRANSLATE_NOOP("samples", "One squelchy acid note, C")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Chord stab"), QT_TRANSLATE_NOOP("samples", "A short chord, C minor")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "FM bell"), QT_TRANSLATE_NOOP("samples", "A glassy bell, A")},
    {"Tones", QT_TRANSLATE_NOOP("samples", "Pluck"), QT_TRANSLATE_NOOP("samples", "A plucked string, A")},
    {"Effects", QT_TRANSLATE_NOOP("samples", "Riser"),
	 QT_TRANSLATE_NOOP("samples", "Noise rising for three seconds, before a drop")},
    {"Effects", QT_TRANSLATE_NOOP("samples", "Downlifter"), QT_TRANSLATE_NOOP("samples", "Noise falling away")},
    {"Effects", QT_TRANSLATE_NOOP("samples", "Impact"), QT_TRANSLATE_NOOP("samples", "A huge hit with a long tail")},
    {"Effects", QT_TRANSLATE_NOOP("samples", "Power down"),
	 QT_TRANSLATE_NOOP("samples", "Like a tape machine stopping")},
    {"Textures", QT_TRANSLATE_NOOP("samples", "Night rain"),
	 QT_TRANSLATE_NOOP("samples", "Rain on the window, seven seconds")},
    {"Textures", QT_TRANSLATE_NOOP("samples", "Vinyl crackle"),
	 QT_TRANSLATE_NOOP("samples", "An old record, seven seconds")},
    {"Textures", QT_TRANSLATE_NOOP("samples", "Wind in the carriage"),
	 QT_TRANSLATE_NOOP("samples", "Wind through a gap, with a faint whistle")},
    {"Textures", QT_TRANSLATE_NOOP("samples", "Rails at night"),
	 QT_TRANSLATE_NOOP("samples", "A train far away: rail joints every 1.1 seconds")}};
static const char* const CATEGORIES[] = {
    QT_TRANSLATE_NOOP("samples", "Kicks"),   QT_TRANSLATE_NOOP("samples", "Snares"),
    QT_TRANSLATE_NOOP("samples", "Claps"),   QT_TRANSLATE_NOOP("samples", "Hats"),
    QT_TRANSLATE_NOOP("samples", "Cymbals"), QT_TRANSLATE_NOOP("samples", "Percussion"),
    QT_TRANSLATE_NOOP("samples", "Train"),   QT_TRANSLATE_NOOP("samples", "Tones"),
    QT_TRANSLATE_NOOP("samples", "Effects"), QT_TRANSLATE_NOOP("samples", "Textures")};

const QList<LibrarySound>& library() {
	static QList<LibrarySound> list; // built once, after the translation is loaded
	if (list.isEmpty())
		for (const Entry& e : ENTRIES)
			list.append({QStringLiteral("nz:%1/%2").arg(QLatin1String(e.folder), QString::fromUtf8(e.name)),
			             tr(e.folder), tr(e.name), tr(e.about)});
	return list;
}

QStringList libraryCategories() {
	QStringList out;
	for (const char* c : CATEGORIES)
		out << tr(c);
	return out;
}

bool isLibrary(const QString& name) {
	return name.startsWith(QLatin1String("nz:"));
}

static const LibrarySound* findLibrary(const QString& id) {
	for (const LibrarySound& s : library())
		if (s.id == id) return &s;
	return nullptr;
}

QString displayName(const QString& name) {
	if (isLibrary(name)) {
		const LibrarySound* s = findLibrary(name);
		return s ? s->name : name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
	}
	return QFileInfo(name).completeBaseName();
}

QString about(const QString& name) {
	const LibrarySound* s = findLibrary(name);
	return s ? s->about : QString();
}

static QString resourcePath(const QString& id) {
	return QStringLiteral(":/samples/") + id.mid(3) + QStringLiteral(".wav");
}

QString folder() {
	QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.nachtzug");
	const QString dir = base + QStringLiteral("/samples");
	QDir().mkpath(dir);
	return dir;
}

static bool soundFile(const QString& name) {
	const QString s = name.toLower();
	return s.endsWith(QLatin1String(".wav")) || s.endsWith(QLatin1String(".aif")) ||
	       s.endsWith(QLatin1String(".aiff")) || s.endsWith(QLatin1String(".aifc"));
}

QStringList list() {
	QStringList out;
	for (const QFileInfo& fi : QDir(folder()).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase))
		if (soundFile(fi.fileName())) out << fi.fileName();
	return out;
}

// loaded samples, by name, with the file's time so a replaced file is read again
struct Cached {
	std::shared_ptr<const dsp::SampleData> data;
	qint64 stamp;
};
static QHash<QString, Cached>& cache() {
	static QHash<QString, Cached> c;
	return c;
}

std::shared_ptr<const dsp::SampleData> get(const QString& name) {
	if (name.isEmpty()) return nullptr;
	if (isLibrary(name)) { // built in: read once, never changes
		auto it = cache().find(name);
		if (it != cache().end()) return it->data;
		auto d = std::make_shared<dsp::SampleData>();
		QString err;
		if (!readFile(resourcePath(name), *d, err)) return nullptr;
		cache().insert(name, {d, 0});
		return d;
	}
	const QFileInfo fi(QDir(folder()).filePath(name));
	if (!fi.exists()) return nullptr;
	const qint64 stamp = fi.lastModified().toMSecsSinceEpoch() ^ fi.size();
	auto it = cache().find(name);
	if (it != cache().end() && it->stamp == stamp) return it->data;
	auto d = std::make_shared<dsp::SampleData>();
	QString err;
	if (!readFile(fi.filePath(), *d, err)) return nullptr;
	cache().insert(name, {d, stamp});
	return d;
}

bool exists(const QString& name) {
	if (isLibrary(name)) return QFileInfo::exists(resourcePath(name));
	return !name.isEmpty() && QFileInfo::exists(QDir(folder()).filePath(name));
}

double seconds(const QString& name) {
	const auto d = get(name);
	return d ? double(d->frames()) / double(d->rate) : 0.0;
}

// ---------------------------------------------------------------- reading

static quint32 le32(const uchar* p) {
	return quint32(p[0]) | quint32(p[1]) << 8 | quint32(p[2]) << 16 | quint32(p[3]) << 24;
}
static quint16 le16(const uchar* p) {
	return quint16(p[0] | p[1] << 8);
}
static quint32 be32(const uchar* p) {
	return quint32(p[3]) | quint32(p[2]) << 8 | quint32(p[1]) << 16 | quint32(p[0]) << 24;
}
static quint16 be16(const uchar* p) {
	return quint16(p[1] | p[0] << 8);
}

// one sample of the given kind, as -1..1
static float pcm(const uchar* p, int bits, bool isFloat, bool big) {
	if (isFloat) {
		if (bits == 32) {
			quint32 u = big ? be32(p) : le32(p);
			float f;
			std::memcpy(&f, &u, 4);
			return std::isfinite(f) ? f : 0.f;
		}
		quint64 u = 0;
		for (int i = 0; i < 8; i++)
			u |= quint64(p[big ? i : 7 - i]) << (8 * (7 - i));
		double d;
		std::memcpy(&d, &u, 8);
		return std::isfinite(d) ? float(d) : 0.f;
	}
	switch (bits) {
	case 8:
		return big ? float(qint8(p[0])) / 128.f : (float(p[0]) - 128.f) / 128.f; // WAV 8-bit is unsigned, AIFF signed
	case 16:
		return float(qint16(big ? be16(p) : le16(p))) / 32768.f;
	case 24: {
		const qint32 v = big ? (qint32(p[0]) << 24 | qint32(p[1]) << 16 | qint32(p[2]) << 8)
		                     : (qint32(p[2]) << 24 | qint32(p[1]) << 16 | qint32(p[0]) << 8);
		return float(v) / 2147483648.f;
	}
	case 32:
		return float(qint32(big ? be32(p) : le32(p))) / 2147483648.f;
	default:
		return 0.f;
	}
}

static bool decode(const uchar* data, qint64 bytes, int channels, int bits, bool isFloat, bool big, double rate,
                   dsp::SampleData& out, double maxSeconds, QString& error) {
	if (channels < 1 || channels > 32 ||
	    !(bits == 8 || bits == 16 || bits == 24 || bits == 32 || (isFloat && bits == 64)) || rate < 1000 ||
	    rate > 768000) {
		error = tr("This kind of sound file is not supported (%1 channels, %2-bit). Save it as a 16- or 24-bit WAV "
		           "file and try again.")
		            .arg(channels)
		            .arg(bits);
		return false;
	}
	const int frameBytes = channels * bits / 8;
	qint64 frames = bytes / frameBytes;
	frames = std::min<qint64>(frames, qint64(maxSeconds * rate));
	if (frames <= 0) {
		error = tr("The sound file is empty.");
		return false;
	}
	out.rate = float(rate);
	out.l.resize(size_t(frames));
	out.r.resize(size_t(frames));
	for (qint64 i = 0; i < frames; i++) {
		const uchar* f = data + i * frameBytes;
		const float a = pcm(f, bits, isFloat, big);
		out.l[size_t(i)] = a;
		out.r[size_t(i)] = channels > 1 ? pcm(f + bits / 8, bits, isFloat, big) : a;
	}
	return true;
}

// the 80-bit extended number AIFF keeps its sample rate in
static double extended(const uchar* p) {
	const int expon = ((p[0] & 0x7f) << 8) | p[1];
	quint64 mant = 0;
	for (int i = 0; i < 8; i++)
		mant = (mant << 8) | p[2 + i];
	if (expon == 0 && mant == 0) return 0.0;
	const double v = std::ldexp(double(mant), expon - 16383 - 63);
	return (p[0] & 0x80) ? -v : v;
}

bool readFile(const QString& path, dsp::SampleData& out, QString& error, double maxSeconds) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		error = tr("The file could not be opened.");
		return false;
	}
	if (f.size() > 512LL * 1024 * 1024) {
		error = tr("The file is too large.");
		return false;
	}
	const QByteArray all = f.readAll();
	const uchar* d = reinterpret_cast<const uchar*>(all.constData());
	const qint64 n = all.size();
	if (n < 12) {
		error = tr("This is not a sound file Nachtzug can read. It reads WAV and AIFF files.");
		return false;
	}
	if (!std::memcmp(d, "RIFF", 4) && !std::memcmp(d + 8, "WAVE", 4)) {
		int channels = 0, bits = 0, format = 0;
		quint32 rate = 0;
		for (qint64 at = 12; at + 8 <= n;) {
			const quint32 size = le32(d + at + 4);
			const uchar* body = d + at + 8;
			const qint64 avail = std::min<qint64>(size, n - at - 8);
			if (!std::memcmp(d + at, "fmt ", 4) && avail >= 16) {
				format = le16(body);
				channels = le16(body + 2);
				rate = le32(body + 4);
				bits = le16(body + 14);
				if (format == 0xFFFE && avail >= 26)
					format = le16(body + 24); // WAVE_FORMAT_EXTENSIBLE: the real format is in the sub-format
			} else if (!std::memcmp(d + at, "data", 4)) {
				if (!channels) break;
				if (format != 1 && format != 3) {
					error = tr("This WAV file is compressed. Save it as an uncompressed (PCM) WAV file and try again.");
					return false;
				}
				return decode(body, avail, channels, bits, format == 3, false, rate, out, maxSeconds, error);
			}
			at += 8 + qint64(size) + (size & 1);
		}
		error = tr("This WAV file has no sound in it, or it is damaged.");
		return false;
	}
	if (!std::memcmp(d, "FORM", 4) && (!std::memcmp(d + 8, "AIFF", 4) || !std::memcmp(d + 8, "AIFC", 4))) {
		const bool aifc = !std::memcmp(d + 8, "AIFC", 4);
		int channels = 0, bits = 0;
		double rate = 0;
		bool isFloat = false, big = true, haveComm = false;
		const uchar* sound = nullptr;
		qint64 soundBytes = 0;
		for (qint64 at = 12; at + 8 <= n;) {
			const quint32 size = be32(d + at + 4);
			const uchar* body = d + at + 8;
			const qint64 avail = std::min<qint64>(size, n - at - 8);
			if (!std::memcmp(d + at, "COMM", 4) && avail >= 18) {
				channels = be16(body);
				bits = be16(body + 6);
				rate = extended(body + 8);
				haveComm = true;
				if (aifc && avail >= 22) {
					if (!std::memcmp(body + 18, "sowt", 4))
						big = false;
					else if (!std::memcmp(body + 18, "fl32", 4) || !std::memcmp(body + 18, "FL32", 4)) {
						isFloat = true;
						bits = 32;
					} else if (!std::memcmp(body + 18, "fl64", 4) || !std::memcmp(body + 18, "FL64", 4)) {
						isFloat = true;
						bits = 64;
					} else if (std::memcmp(body + 18, "NONE", 4) && std::memcmp(body + 18, "twos", 4)) {
						error = tr(
						    "This AIFF file is compressed. Save it as an uncompressed AIFF or WAV file and try again.");
						return false;
					}
				}
			} else if (!std::memcmp(d + at, "SSND", 4) && avail >= 8) {
				const quint32 offset = be32(body);
				sound = body + 8 + offset;
				soundBytes = avail - 8 - qint64(offset);
			}
			at += 8 + qint64(size) + (size & 1);
		}
		if (!haveComm || !sound || soundBytes <= 0) {
			error = tr("This AIFF file has no sound in it, or it is damaged.");
			return false;
		}
		bits = (bits + 7) / 8 * 8;
		return decode(sound, soundBytes, channels, bits, isFloat, big, rate, out, maxSeconds, error);
	}
	error = tr("This is not a sound file Nachtzug can read. It reads WAV and AIFF files; MP3, FLAC and OGG files can "
	           "be turned into WAV with a free program such as Audacity.");
	return false;
}

// ---------------------------------------------------------------- library

static QString uniqueName(const QString& wanted) {
	const QFileInfo fi(wanted);
	QString base = fi.completeBaseName().trimmed(), ext = fi.suffix().toLower();
	base.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
	if (base.isEmpty()) base = tr("Sample");
	if (ext.isEmpty()) ext = QStringLiteral("wav");
	QString name = base + QLatin1Char('.') + ext;
	for (int k = 2; QFileInfo::exists(QDir(folder()).filePath(name)); k++)
		name = QStringLiteral("%1 %2.%3").arg(base).arg(k).arg(ext);
	return name;
}

QString import(const QString& path, QString& error) {
	dsp::SampleData probe;
	if (!readFile(path, probe, error)) return QString();
	const QString name = uniqueName(QFileInfo(path).fileName());
	if (!QFile::copy(path, QDir(folder()).filePath(name))) {
		error = tr("The file could not be copied into the sample folder.");
		return QString();
	}
	return name;
}

QString saveRecording(const QString& wantedName, const std::vector<float>& mono, int rate, QString& error) {
	if (mono.empty()) {
		error = tr("Nothing was recorded.");
		return QString();
	}
	return saveSound(wantedName.trimmed().isEmpty() ? tr("Recording") : wantedName, mono, {}, rate, error);
}

QString saveSound(const QString& wantedName, const std::vector<float>& left, const std::vector<float>& right, int rate,
                  QString& error) {
	if (left.empty()) {
		error = tr("There is no sound to save.");
		return QString();
	}
	const bool stereo = right.size() == left.size() && right != left;
	const QString name =
	    uniqueName((wantedName.trimmed().isEmpty() ? tr("Sound") : wantedName.trimmed()) + QStringLiteral(".wav"));
	QFile f(QDir(folder()).filePath(name));
	if (!f.open(QIODevice::WriteOnly)) {
		error = tr("The sound could not be saved in the sample folder.");
		return QString();
	}
	QByteArray b;
	const quint16 ch = stereo ? 2 : 1;
	const quint32 dataBytes = quint32(left.size() * 3 * ch);
	auto put32 = [&](quint32 v) {
		for (int i = 0; i < 4; i++)
			b.append(char((v >> (8 * i)) & 0xff));
	};
	auto put16 = [&](quint16 v) {
		b.append(char(v & 0xff));
		b.append(char(v >> 8));
	};
	b.append("RIFF");
	put32(36 + dataBytes);
	b.append("WAVEfmt ");
	put32(16);
	put16(1);
	put16(ch);
	put32(quint32(rate));
	put32(quint32(rate) * 3 * ch);
	put16(quint16(3 * ch));
	put16(24);
	b.append("data");
	put32(dataBytes);
	b.reserve(int(44 + dataBytes));
	auto put24 = [&](float x) {
		const int v = std::clamp(int(std::lround(double(std::clamp(x, -1.f, 1.f)) * 8388607.0)), -8388608, 8388607);
		b.append(char(v & 0xff));
		b.append(char((v >> 8) & 0xff));
		b.append(char((v >> 16) & 0xff));
	};
	for (size_t i = 0; i < left.size(); i++) {
		put24(left[i]);
		if (stereo) put24(right[i]);
	}
	if (f.write(b) != b.size()) {
		error = tr("The sound could not be saved in the sample folder.");
		f.remove();
		return QString();
	}
	return name;
}

bool remove(const QString& name) {
	cache().remove(name);
	return !name.isEmpty() && QFile::remove(QDir(folder()).filePath(name));
}

bool rename(const QString& from, const QString& to, QString& error) {
	QString target = to.trimmed();
	if (QFileInfo(target).suffix().isEmpty()) target += QLatin1Char('.') + QFileInfo(from).suffix();
	if (target.contains(QLatin1Char('/')) || target.contains(QLatin1Char('\\'))) {
		error = tr("A name cannot contain / or \\.");
		return false;
	}
	if (QFileInfo::exists(QDir(folder()).filePath(target))) {
		error = tr("There is already a sample called %1.").arg(target);
		return false;
	}
	if (!QFile::rename(QDir(folder()).filePath(from), QDir(folder()).filePath(target))) {
		error = tr("The sample could not be renamed.");
		return false;
	}
	cache().remove(from);
	return true;
}

std::vector<size_t> equalCuts(size_t frames, int parts) {
	std::vector<size_t> out;
	parts = std::max(1, parts);
	for (int i = 0; i < parts; i++)
		out.push_back(frames * size_t(i) / size_t(parts));
	return out;
}

std::vector<size_t> findHits(const dsp::SampleData& d, float sensitivity, int maxSlices) {
	const size_t n = d.frames();
	std::vector<size_t> cuts{0};
	if (n < 512 || maxSlices < 2) return cuts;
	// loudness in short steps, in decibels: of the whole sound, and of its highs alone (the change from one sample
	// to the next), where a hi-hat or a clave stands out even over the tail of a kick
	const size_t hop = std::max<size_t>(64, size_t(d.rate / 375.f)), win = hop * 2; // about 2.7 ms steps
	std::vector<float> db, hiDb;
	for (size_t at = 0; at + win <= n; at += hop) {
		double e = 0, h = 0;
		float prev = at > 0 ? 0.5f * (d.l[at - 1] + d.r[at - 1]) : 0.f;
		for (size_t i = at; i < at + win; i++) {
			const float x = 0.5f * (d.l[i] + d.r[i]);
			e += double(x) * x;
			h += double(x - prev) * (x - prev);
			prev = x;
		}
		db.push_back(float(10.0 * std::log10(e / double(win) + 1e-12)));
		hiDb.push_back(float(10.0 * std::log10(h / double(win) + 1e-12)));
	}
	const float loudest = db.empty() ? -120.f : *std::max_element(db.begin(), db.end());
	const float loudestHi = hiDb.empty() ? -120.f : *std::max_element(hiDb.begin(), hiDb.end());
	const float rise = 5.f + (1.f - std::clamp(sensitivity, 0.f, 1.f)) * 12.f; // how sudden a rise counts as a hit
	const size_t gap = size_t(0.07 * d.rate / double(hop)) + 1;                // hits at least 70 ms apart
	struct Hit {
		size_t frame;
		float strength;
	};
	std::vector<Hit> hits;
	auto jumpAt = [&](const std::vector<float>& v, float top, size_t k) {
		const float before = std::min({v[k - 1], v[k - 2], v[k - 3], v[k - 4]});
		if (v[k] < top - 42.f) return 0.f;
		if (v[k + 1] - before > v[k] - before + 0.5f) return 0.f; // still rising: the next step is the hit
		return v[k] - before;
	};
	for (size_t k = 4; k + 1 < db.size(); k++) {
		const float jump = std::max(jumpAt(db, loudest, k), jumpAt(hiDb, loudestHi, k));
		if (jump < rise) continue;
		if (!hits.empty() && k - hits.back().frame / hop < gap) {
			if (jump > hits.back().strength) hits.back() = {k * hop, jump};
			continue;
		}
		hits.push_back({k * hop, jump});
	}
	// the strongest ones, back in order; each starts a moment before its rise, so the attack is whole
	std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.strength > b.strength; });
	const size_t preroll = size_t(0.003 * d.rate);
	for (const Hit& h : hits) {
		if (int(cuts.size()) >= maxSlices) break;
		const size_t at = h.frame > preroll ? h.frame - preroll : 0;
		if (at < size_t(0.03 * d.rate)) continue; // the very start is a slice already
		cuts.push_back(at);
	}
	std::sort(cuts.begin(), cuts.end());
	return cuts;
}

dsp::SampleData slice(const dsp::SampleData& d, size_t from, size_t to) {
	dsp::SampleData out;
	out.rate = d.rate;
	to = std::min(to, d.frames());
	if (from >= to) return out;
	out.l.assign(d.l.begin() + long(from), d.l.begin() + long(to));
	out.r.assign(d.r.begin() + long(from), d.r.begin() + long(to));
	const size_t len = out.l.size();
	const size_t fadeIn = from > 0 ? std::min<size_t>(len / 4, size_t(0.0005 * d.rate)) : 0;
	const size_t fadeOut = std::min<size_t>(len / 4, size_t(0.004 * d.rate));
	for (size_t i = 0; i < fadeIn; i++) {
		const float g = float(i) / float(fadeIn);
		out.l[i] *= g;
		out.r[i] *= g;
	}
	for (size_t i = 0; i < fadeOut; i++) {
		const float g = float(i) / float(fadeOut);
		out.l[len - 1 - i] *= g;
		out.r[len - 1 - i] *= g;
	}
	return out;
}

int guessBars(double seconds, double bpm) {
	const double bars = seconds * bpm / 240.0;
	int best = 1;
	double err = 1e9;
	for (int b : {1, 2, 4, 8}) {
		const double e = std::fabs(std::log2(bars / b));
		if (e < err) {
			err = e;
			best = b;
		}
	}
	return best;
}

} // namespace samples
