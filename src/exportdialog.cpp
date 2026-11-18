// Export: see exportdialog.h.
//
// What's here: ExportDialog and ExportDialog::run(), called from File > Export and the Export key in the top bar;
// run() renders with renderSong(), renderStems() or writeMidiFile() and shows the progress and the last window.
// Changes in the song file: none (the choices are kept as settings).

#include "exportdialog.h"
#include "app.h"
#include "widgets.h"
#include "midifile.h"
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

static QString minutes(double seconds) {
	const int s = int(std::lround(seconds));
	return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

static QString megabytes(double bytes) {
	return QLocale().toString(bytes / 1e6, 'f', bytes < 1e7 ? 1 : 0) + QStringLiteral(" MB");
}

struct Quality {
	float rate;
	int bits;
	const char* name;
};
static const Quality QUALITIES[] = {
    {44100.f, 16, QT_TRANSLATE_NOOP("ExportDialog", "CD quality: 44.1 kHz, 16-bit (plays everywhere)")},
    {48000.f, 16, QT_TRANSLATE_NOOP("ExportDialog", "48 kHz, 16-bit (for video)")},
    {48000.f, 24, QT_TRANSLATE_NOOP("ExportDialog", "Studio: 48 kHz, 24-bit (for mastering)")},
    {96000.f, 24, QT_TRANSLATE_NOOP("ExportDialog", "High resolution: 96 kHz, 24-bit")},
};

ExportDialog::ExportDialog(App* a, QWidget* parent) : QDialog(parent), app(a) {
	setWindowTitle(tr("Export the song"));
	QSettings st;
	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(20, 18, 20, 14);
	v->setSpacing(14);
	head = new QLabel;
	head->setTextFormat(Qt::RichText);
	head->setWordWrap(true);
	head->setMinimumHeight(head->fontMetrics().height() * 3);
	v->addWidget(head);

	auto* grid = new QGridLayout;
	grid->setHorizontalSpacing(14);
	grid->setVerticalSpacing(8);
	auto caption = [](const QString& t) {
		auto* l = new QLabel(t);
		l->setObjectName("cap");
		return l;
	};
	int row = 0;
	// the kind of file
	// the captions flush left, as in every other dialog; beside a row of keys, level with the keys rather than the line
	// beneath them
	QList<QPair<QLabel*, QWidget*>> besideKeys;
	auto* makeCap = caption(tr("Make"));
	grid->addWidget(makeCap, row, 0, Qt::AlignTop | Qt::AlignLeft);
	format = new QButtonGroup(this);
	{
		// joined keys, as in the welcome window; what the chosen kind does is said in a soft line beneath
		const char* names[3] = {QT_TR_NOOP("One sound file"), QT_TR_NOOP("A file per track"), QT_TR_NOOP("MIDI notes")};
		static const char* tips[3] = {
		    QT_TR_NOOP("The whole song mixed into one file, ready to play anywhere"),
		    QT_TR_NOOP("Every track that is heard in its own WAV file, in a new folder. They all start at the same "
			           "moment and add up to the song, for mixing it in another program."),
		    QT_TR_NOOP("The notes of every track, without the sound, for other music programs. Arpeggios, chords and "
			           "quick repeats are written out as you hear them.")};
		auto* col = new QVBoxLayout;
		col->setSpacing(4);
		auto* keys = new QHBoxLayout;
		keys->setSpacing(0);
		for (int i = 0; i < 3; i++) {
			auto* k = new QPushButton(tr(names[i]));
			k->setObjectName(i == 0 ? "segL" : i == 2 ? "segR" : "segM");
			k->setCheckable(true);
			k->setToolTip(tr(tips[i]));
			format->addButton(k, i);
			keys->addWidget(k);
			if (i == 0) besideKeys.append({makeCap, k});
		}
		keys->addStretch(1);
		col->addLayout(keys);
		auto* note = new QLabel;
		note->setObjectName("dim");
		note->setWordWrap(true);
		col->addWidget(note);
		grid->addLayout(col, row++, 1);
		format->button(std::clamp(st.value(QStringLiteral("export/kind"), 0).toInt(), 0, 2))->setChecked(true);
		auto say = [this, note] { note->setText(tr(tips[std::clamp(format->checkedId(), 0, 2)])); };
		say();
		connect(format, &QButtonGroup::idClicked, this, say);
	}
	// what to write: the whole song or the loop, with how many times; the bars and the length beneath
	auto* whatCap = caption(tr("What"));
	grid->addWidget(whatCap, row, 0, Qt::AlignTop | Qt::AlignLeft);
	what = new QButtonGroup(this);
	auto* whole = new QPushButton(tr("The whole song"));
	besideKeys.append({whatCap, whole});
	auto* loopOnly = new QPushButton(tr("The loop"));
	whole->setObjectName("segL");
	loopOnly->setObjectName("segR");
	for (QPushButton* k : {whole, loopOnly})
		k->setCheckable(true);
	wholeText = new QLabel;
	loopText = new QLabel;
	for (QLabel* l : {wholeText, loopText})
		l->setObjectName("dim");
	what->addButton(whole, 0);
	what->addButton(loopOnly, 1);
	repeats = new QSpinBox;
	repeats->setRange(1, 32);
	repeats->setFixedWidth(64);
	repeats->setValue(st.value(QStringLiteral("export/repeats"), 4).toInt());
	repeats->setToolTip(tr("How many times the loop plays in the file"));
	auto* times = new QLabel(tr("times"));
	{
		auto* col = new QVBoxLayout;
		col->setSpacing(4);
		auto* keys = new QHBoxLayout;
		keys->setSpacing(0);
		keys->addWidget(whole);
		keys->addWidget(loopOnly);
		keys->addSpacing(10);
		keys->addWidget(repeats);
		keys->addSpacing(6);
		keys->addWidget(times);
		keys->addStretch(1);
		col->addLayout(keys);
		col->addWidget(wholeText);
		col->addWidget(loopText);
		grid->addLayout(col, row++, 1);
	}
	const bool hasLoop = app->song.loopEnd > app->song.loopStart;
	loopOnly->setEnabled(hasLoop);
	(st.value(QStringLiteral("export/loop"), false).toBool() && hasLoop ? loopOnly : whole)->setChecked(true);
	// how it ends
	endingCap = caption(tr("Ending"));
	grid->addWidget(endingCap, row, 0, Qt::AlignTop | Qt::AlignLeft);
	ending = new QButtonGroup(this);
	auto* ring = new QPushButton(tr("Let it ring out"));
	besideKeys.append({endingCap, ring});
	auto* fade = new QPushButton(tr("Fade out"));
	ring->setToolTip(tr("Let the echoes and reverb ring out after the last note"));
	fade->setToolTip(tr("Fade out over the last bars"));
	ring->setObjectName("segL");
	fade->setObjectName("segR");
	for (QPushButton* k : {ring, fade})
		k->setCheckable(true);
	ending->addButton(ring, 0);
	ending->addButton(fade, 1);
	(st.value(QStringLiteral("export/fade"), false).toBool() ? fade : ring)->setChecked(true);
	endingBox = new QWidget;
	{
		auto* eb = new QHBoxLayout(endingBox);
		eb->setContentsMargins(0, 0, 0, 0);
		eb->setSpacing(0);
		eb->addWidget(ring);
		eb->addWidget(fade);
		eb->addStretch(1);
	}
	grid->addWidget(endingBox, row++, 1);
	// quality
	qualityCap = caption(tr("Quality"));
	grid->addWidget(qualityCap, row, 0, Qt::AlignLeft);
	quality = new QComboBox;
	for (const Quality& q : QUALITIES)
		quality->addItem(tr(q.name));
	quality->setCurrentIndex(
	    std::clamp(st.value(QStringLiteral("export/quality"), 0).toInt(), 0, int(std::size(QUALITIES)) - 1));
	quality->setToolTip(tr("If you are not sure, keep CD quality: every player and phone can play it."));
	qualityBox = quality;
	grid->addWidget(quality, row++, 1);
	// where
	grid->addWidget(caption(tr("Save as")), row, 0, Qt::AlignLeft);
	auto* where = new QHBoxLayout;
	where->setSpacing(6);
	path = new QLineEdit;
	QString dir = st.value(QStringLiteral("export/dir")).toString();
	if (dir.isEmpty() || !QDir(dir).exists()) dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
	if (dir.isEmpty() || !QDir(dir).exists()) dir = QDir::homePath();
	lastDir = dir;
	path->setMinimumWidth(380);
	auto* browse = new QPushButton(tr("Choose…"));
	browse->setToolTip(tr("Pick the folder and file name"));
	where->addWidget(path, 1);
	where->addWidget(browse);
	grid->addLayout(where, row++, 1);
	grid->setColumnStretch(1, 1);
	v->addLayout(grid);
	for (const auto& [cap, key] : besideKeys) {
		key->ensurePolished();
		cap->setMinimumHeight(key->sizeHint().height());
		cap->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}

	info = new QLabel;
	info->setObjectName("dim");
	v->addWidget(info);
	auto* buttons = new QDialogButtonBox;
	auto* go = buttons->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
	go->setObjectName("primary");
	go->setDefault(true);
	buttons->addButton(QDialogButtonBox::Cancel);
	v->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, this, [this] {
		if (path->text().trimmed().isEmpty()) {
			choosePath();
			if (path->text().trimmed().isEmpty()) return;
		}
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(browse, &QPushButton::clicked, this, &ExportDialog::choosePath);
	connect(what, &QButtonGroup::idClicked, this, &ExportDialog::updateInfo);
	connect(ending, &QButtonGroup::idClicked, this, &ExportDialog::updateInfo);
	connect(repeats, &QSpinBox::valueChanged, this, [this] {
		what->button(1)->setChecked(true);
		updateInfo();
	});
	connect(quality, &QComboBox::activated, this, &ExportDialog::updateInfo);
	connect(format, &QButtonGroup::idClicked, this, &ExportDialog::kindChanged);
	kindChanged();
}

ExportDialog::Kind ExportDialog::kind() const {
	return Kind(std::clamp(format->checkedId(), 0, 2));
}

QString ExportDialog::baseName() const {
	return app->filePath.isEmpty() ? tr("Nachtzug song %1").arg(QDate::currentDate().toString(Qt::ISODate))
	                               : QFileInfo(app->filePath).completeBaseName();
}

// a different kind of file: other settings apply, and the name to save as changes with it
void ExportDialog::kindChanged() {
	const Kind k = kind();
	static const char* heads[3] = {
	    QT_TR_NOOP("<b style=\"font-size:16px\">Export the song as a sound file</b><br>A WAV file you can play "
		           "anywhere, put in a video or send to a friend."),
	    QT_TR_NOOP("<b style=\"font-size:16px\">Export each track as a sound file</b><br>One WAV file per track, in a "
		           "new folder, for mixing the song in another music program."),
	    QT_TR_NOOP("<b style=\"font-size:16px\">Export the notes as a MIDI file</b><br>The notes of every track, for "
		           "other music programs and synthesisers. It holds no sound.")};
	head->setText(tr(heads[k]));
	const bool sound = k != Midi;
	for (QWidget* w : {endingBox, qualityBox, static_cast<QWidget*>(endingCap), static_cast<QWidget*>(qualityCap)})
		w->setEnabled(sound);
	// keep the folder of what was typed, put the right ending on the name
	QString dir = lastDir;
	QString name = baseName();
	const QString now = path->text().trimmed();
	if (!now.isEmpty()) {
		const QFileInfo fi(now);
		dir = fi.absolutePath();
		name = fi.fileName();
		for (const QString& end : {QStringLiteral(".wav"), QStringLiteral(".mid"), QStringLiteral(".midi")})
			if (name.endsWith(end, Qt::CaseInsensitive)) name.chop(end.size());
		const QString stems = tr(" (tracks)");
		if (name.endsWith(stems)) name.chop(stems.size());
	}
	const QString file = k == Wav    ? name + QStringLiteral(".wav")
	                     : k == Midi ? name + QStringLiteral(".mid")
	                                 : name + tr(" (tracks)");
	path->setText(QDir(dir).filePath(file));
	path->setToolTip(k == Stems ? tr("The folder the track files go into; it is made if it does not exist")
	                            : QString());
	updateInfo();
}

RenderOptions ExportDialog::options() const {
	RenderOptions o;
	const Quality& q = QUALITIES[std::clamp(quality->currentIndex(), 0, int(std::size(QUALITIES)) - 1)];
	o.sampleRate = q.rate;
	o.bits = q.bits;
	o.loopOnly = what->checkedId() == 1;
	o.repeats = repeats->value();
	o.fadeOut = ending->checkedId() == 1;
	return o;
}

void ExportDialog::updateInfo() {
	const Song& s = app->song;
	RenderOptions whole = options();
	whole.loopOnly = false;
	const int lastBar = std::max(BAR, s.endTick()) / BAR + (std::max(BAR, s.endTick()) % BAR ? 1 : 0);
	wholeText->setText(tr("Bars 1 to %1 (%2)").arg(lastBar).arg(minutes(renderLength(s, whole))));
	if (s.loopEnd > s.loopStart)
		loopText->setText((repeats->value() == 1 ? tr("Bars %1 to %2, played once")
		                                          : tr("Bars %1 to %2, played %3 times").arg(repeats->value()))
		                      .arg(s.loopStart / BAR + 1)
		                      .arg((s.loopEnd + BAR - 1) / BAR));
	else
		loopText->setText(tr("There is no loop yet"));
	wholeText->setVisible(what->checkedId() != 1);
	loopText->setVisible(what->checkedId() == 1);
	const RenderOptions o = options();
	const double secs = renderLength(s, o) + (o.fadeOut ? 0.0 : 3.0);
	const double bytes = secs * o.sampleRate * 2 * (o.bits / 8);
	const int heard = int(audibleTracks(s).size());
	int muted = 0;
	for (const Track& t : s.tracks)
		muted += t.mute || (s.anySolo() && !t.solo);
	QString left = muted > 0 ? QStringLiteral(" ") + tr("Muted tracks left out: %1.").arg(muted) : QString();
	if (app->liveInput()) left += QStringLiteral(" ") + tr("The live input is not in the file.");
	switch (kind()) {
	case Wav:
		info->setText(tr("About %1 long, about %2.").arg(minutes(secs), megabytes(bytes)) + left);
		break;
	case Stems:
		info->setText(tr("Files: %1, each about %2 long, about %3 in all.")
		                  .arg(heard)
		                  .arg(minutes(secs), megabytes(bytes * heard)) +
		              left);
		break;
	case Midi:
		info->setText(tr("Tracks of notes: %1, about %2 long.").arg(heard).arg(minutes(renderLength(s, o))) + left);
		break;
	}
}

void ExportDialog::choosePath() {
	const Kind k = kind();
	if (k == Stems) {
		const QString d = QFileDialog::getExistingDirectory(this, tr("Folder for the track files"),
		                                                    QFileInfo(path->text()).absolutePath());
		if (!d.isEmpty()) path->setText(QDir(d).filePath(baseName() + tr(" (tracks)")));
		return;
	}
	const QString ext = k == Midi ? QStringLiteral(".mid") : QStringLiteral(".wav");
	const QString p =
	    QFileDialog::getSaveFileName(this, k == Midi ? tr("Export the notes as MIDI") : tr("Export song as WAV"),
		                             path->text(), k == Midi ? tr("MIDI files (*.mid)") : tr("WAV audio (*.wav)"));
	if (!p.isEmpty()) path->setText(p.endsWith(ext, Qt::CaseInsensitive) ? p : p + ext);
}

void ExportDialog::run(App* app, QWidget* parent) {
	ExportDialog dlg(app, parent);
	if (dlg.exec() != QDialog::Accepted) return;
	const Kind kind = dlg.kind();
	QString file = dlg.path->text().trimmed();
	if (kind == Wav && !file.endsWith(QLatin1String(".wav"), Qt::CaseInsensitive)) file += QStringLiteral(".wav");
	if (kind == Midi && !file.endsWith(QLatin1String(".mid"), Qt::CaseInsensitive) &&
	    !file.endsWith(QLatin1String(".midi"), Qt::CaseInsensitive))
		file += QStringLiteral(".mid");
	const RenderOptions opt = dlg.options();
	QSettings st;
	st.setValue(QStringLiteral("export/kind"), int(kind));
	st.setValue(QStringLiteral("export/dir"), QFileInfo(file).absolutePath());
	st.setValue(QStringLiteral("export/loop"), opt.loopOnly);
	st.setValue(QStringLiteral("export/repeats"), opt.repeats);
	st.setValue(QStringLiteral("export/fade"), opt.fadeOut);
	st.setValue(QStringLiteral("export/quality"), dlg.quality->currentIndex());
	if (audibleTracks(app->song).empty()) {
		QMessageBox::information(
		    parent, tr("Nothing to export"),
		    tr("Every track is muted, so there is nothing to hear. Switch a track back on in the mixer."));
		return;
	}

	QStringList stemFiles;
	if (kind == Stems) {
		// the files that would be replaced
		QStringList existing;
		const std::vector<int> ids = audibleTracks(app->song);
		for (size_t i = 0; i < ids.size(); i++)
			if (QFileInfo::exists(QDir(file).filePath(stemFileName(app->song, ids[i], int(i) + 1))))
				existing << stemFileName(app->song, ids[i], int(i) + 1);
		if (!existing.isEmpty() &&
		    QMessageBox::question(parent, tr("Replace the files?"),
		                          tr("The folder %1 already holds track files with these names: %2. Replace them?")
		                              .arg(QFileInfo(file).fileName(), existing.join(QStringLiteral(", "))),
		                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
	} else if (QFileInfo::exists(file) &&
	           QMessageBox::question(parent, tr("Replace the file?"),
	                                 tr("%1 already exists. Replace it?").arg(QFileInfo(file).fileName()),
	                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;
	QDir().mkpath(kind == Stems ? file : QFileInfo(file).absolutePath());

	RenderResult res;
	QProgressDialog progress(kind == Stems  ? tr("Writing each track to its own sound file…")
	                         : kind == Midi ? tr("Writing the notes…")
	                                        : tr("Writing the song to a sound file…"),
	                         tr("Cancel"), 0, 100, parent);
	progress.setWindowTitle(tr("Export"));
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(kind == Midi ? 400 : 0);
	auto step = [&](float done) {
		progress.setValue(int(done * 100.f));
		QApplication::processEvents();
		return !progress.wasCanceled();
	};
	if (kind == Wav)
		res = renderSong(app->song, file, opt, step);
	else if (kind == Stems)
		res = renderStems(app->song, file, opt, step, &stemFiles);
	else {
		const double secs = writeMidiFile(app->song, file, opt);
		res.ok = secs >= 0;
		res.seconds = secs;
		res.bytes = QFileInfo(file).size();
	}
	progress.setValue(100);
	if (!res.ok) {
		if (!progress.wasCanceled())
			QMessageBox::warning(parent, tr("Export failed"),
			                     kind == Stems ? tr("The track files could not be written to %1.").arg(file)
			                                   : tr("The file could not be written to %1.").arg(file));
		return;
	}
	emit app->status(tr("Exported to %1").arg(file));

	// the payoff: say it is done, and offer to listen
	QDialog done(parent);
	done.setWindowTitle(kind == Stems ? tr("Your tracks are ready") : tr("Your song is ready"));
	auto* v = new QVBoxLayout(&done);
	v->setContentsMargins(22, 20, 22, 14);
	v->setSpacing(14);
	auto* h = new QHBoxLayout;
	h->setSpacing(16);
	auto* mark = new QLabel;
	mark->setPixmap(theme::logo(128));
	mark->setScaledContents(true);
	mark->setFixedSize(64, 64);
	h->addWidget(mark, 0, Qt::AlignTop);
	QString body;
	if (kind == Stems) {
		QStringList names;
		for (const QString& f : stemFiles)
			names << QFileInfo(f).fileName().toHtmlEscaped();
		body = tr("<b style=\"font-size:16px\">Your tracks are ready.</b><br><br>In the folder %1, each up to %2 long:")
		           .arg(QFileInfo(file).fileName().toHtmlEscaped(), minutes(res.seconds)) +
		       QStringLiteral("<br>") + names.join(QStringLiteral("<br>"));
	} else if (kind == Midi) {
		body = tr("<b style=\"font-size:16px\">Your notes are ready.</b><br><br>%1<br>%2 long. Open it in another "
		          "music program, or drop it onto its tracks.")
		           .arg(QFileInfo(file).fileName().toHtmlEscaped(), minutes(res.seconds));
	} else {
		body = tr("<b style=\"font-size:16px\">Your song is ready.</b><br><br>%1<br>%2 long, %3")
		           .arg(QFileInfo(file).fileName().toHtmlEscaped(), minutes(res.seconds), megabytes(double(res.bytes)));
	}
	auto* text = new QLabel(body);
	text->setTextFormat(Qt::RichText);
	text->setWordWrap(true);
	text->setTextInteractionFlags(Qt::TextSelectableByMouse);
	h->addWidget(text, 1);
	v->addLayout(h);
	auto* buttons = new QDialogButtonBox;
	QPushButton* playIt = nullptr;
	if (kind == Wav) {
		playIt = buttons->addButton(tr("Play it"), QDialogButtonBox::ActionRole);
		playIt->setToolTip(tr("Open the file in your usual music player"));
		playIt->setObjectName("primary");
		QObject::connect(playIt, &QPushButton::clicked, &done,
		                 [file] { QDesktopServices::openUrl(QUrl::fromLocalFile(file)); });
	}
	auto* folder = buttons->addButton(tr("Show in folder"), QDialogButtonBox::ActionRole);
	folder->setToolTip(tr("Open the folder the file was saved in"));
	if (!playIt) folder->setObjectName("primary");
	const QString where = kind == Stems ? file : QFileInfo(file).absolutePath();
	QObject::connect(folder, &QPushButton::clicked, &done,
	                 [where] { QDesktopServices::openUrl(QUrl::fromLocalFile(where)); });
	buttons->addButton(QDialogButtonBox::Close);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &done, &QDialog::accept);
	v->addWidget(buttons);
	done.exec();
}
