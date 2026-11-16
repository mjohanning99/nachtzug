#include "chopdialog.h"
#include "app.h"
#include "samples.h"
#include "widgets.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

// ---------------------------------------------------------------- the wave with its cuts

ChopView::ChopView(QWidget* parent) : QWidget(parent) {
	setMouseTracking(true);
	setToolTip(tr("Click a piece to hear it. Drag a line to move a cut, double-click to add one, right-click a line to "
	              "take it away."));
}

void ChopView::setSound(std::shared_ptr<const dsp::SampleData> d) {
	data_ = std::move(d);
	buildPeaks();
	update();
}
void ChopView::setCuts(const std::vector<size_t>& cuts) {
	cuts_ = cuts;
	if (cuts_.empty() || cuts_[0] != 0) cuts_.insert(cuts_.begin(), 0);
	update();
}
void ChopView::resizeEvent(QResizeEvent*) {
	buildPeaks();
}

void ChopView::buildPeaks() {
	peaks_.clear();
	if (!data_ || data_->frames() == 0 || width() < 2) return;
	const size_t n = data_->frames();
	for (int x = 0; x < width(); x++) {
		const size_t a = n * size_t(x) / size_t(width()), b = std::max(a + 1, n * size_t(x + 1) / size_t(width()));
		float lo = 0.f, hi = 0.f;
		for (size_t i = a; i < b && i < n; i++) {
			const float v = 0.5f * (data_->l[i] + data_->r[i]);
			lo = std::min(lo, v);
			hi = std::max(hi, v);
		}
		peaks_.push_back({lo, hi});
	}
}

int ChopView::xOf(size_t frame) const {
	return data_ && data_->frames() ? int(double(frame) / double(data_->frames()) * width()) : 0;
}
size_t ChopView::frameAt(int x) const {
	return data_ ? size_t(std::clamp(double(x) / std::max(1, width()), 0.0, 1.0) * double(data_->frames())) : 0;
}
int ChopView::cutNear(int x) const {
	for (size_t i = 1; i < cuts_.size(); i++)
		if (std::abs(xOf(cuts_[i]) - x) <= 5) return int(i);
	return -1;
}
int ChopView::pieceAt(int x) const {
	const size_t f = frameAt(x);
	for (size_t i = cuts_.size(); i-- > 0;)
		if (cuts_[i] <= f) return int(i);
	return 0;
}

void ChopView::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
	p.setPen(QPen(theme::rule, 1));
	p.setBrush(theme::lcdBg);
	p.drawRoundedRect(r, 6, 6);
	if (!data_) return;
	// the pieces, every second one tinted, the one playing brighter
	for (size_t i = 0; i < cuts_.size(); i++) {
		const int x0 = xOf(cuts_[i]), x1 = i + 1 < cuts_.size() ? xOf(cuts_[i + 1]) : width();
		QColor tint = theme::lcd;
		tint.setAlpha(int(i) == playing_ ? 70 : i % 2 ? 22 : 0);
		if (tint.alpha()) p.fillRect(QRect(x0, 1, x1 - x0, height() - 2), tint);
	}
	const double mid = height() / 2.0, amp = height() * 0.42;
	p.setPen(QPen(theme::lcd, 1));
	for (int x = 0; x < int(peaks_.size()); x++)
		p.drawLine(QPointF(x + 0.5, mid - peaks_[size_t(x)].second * amp),
		           QPointF(x + 0.5, mid - peaks_[size_t(x)].first * amp));
	// the cuts, numbered by the pad each piece goes to
	QFont f = theme::lcdFont(11, true);
	p.setFont(f);
	for (size_t i = 0; i < cuts_.size(); i++) {
		const int x = xOf(cuts_[i]);
		if (i > 0) {
			p.setPen(QPen(theme::amber, 2));
			p.drawLine(x, 4, x, height() - 4);
		}
		p.setPen(theme::lcd);
		p.drawText(QRect(x + 5, 4, 30, 16), Qt::AlignLeft | Qt::AlignVCenter, QString::number(i + 1));
	}
}

void ChopView::mousePressEvent(QMouseEvent* e) {
	const int x = int(e->position().x());
	const int near = cutNear(x);
	if (e->button() == Qt::RightButton) {
		if (near > 0) {
			cuts_.erase(cuts_.begin() + near);
			update();
			emit cutsEdited();
		}
		return;
	}
	if (e->button() != Qt::LeftButton) return;
	if (near > 0) {
		drag_ = near;
		return;
	}
	emit pieceClicked(pieceAt(x));
}

void ChopView::mouseMoveEvent(QMouseEvent* e) {
	const int x = int(e->position().x());
	if (drag_ < 0) {
		setCursor(cutNear(x) > 0 ? Qt::SizeHorCursor : Qt::PointingHandCursor);
		return;
	}
	if (!data_ || data_->frames() < 256) return; // too short to move cuts in
	// a cut stays between its neighbours
	const size_t lo = cuts_[size_t(drag_) - 1] + 64,
	             hi = size_t(drag_) + 1 < cuts_.size() ? cuts_[size_t(drag_) + 1] - 64 : data_->frames() - 64;
	cuts_[size_t(drag_)] = std::clamp(frameAt(x), lo, std::max(lo, hi));
	update();
}

void ChopView::mouseReleaseEvent(QMouseEvent*) {
	if (drag_ >= 0) {
		drag_ = -1;
		emit cutsEdited();
	}
}

void ChopView::mouseDoubleClickEvent(QMouseEvent* e) {
	if (e->button() != Qt::LeftButton || int(cuts_.size()) >= dsp::D_COUNT || !data_) return;
	const size_t f = frameAt(int(e->position().x()));
	for (size_t c : cuts_)
		if (std::abs(xOf(c) - int(e->position().x())) < 6) return;
	cuts_.push_back(f);
	std::sort(cuts_.begin(), cuts_.end());
	update();
	emit cutsEdited();
}

// ---------------------------------------------------------------- the dialog

ChopDialog::ChopDialog(App* a, std::shared_ptr<const dsp::SampleData> s, const QString& soundName, QWidget* parent)
    : QDialog(parent), app(a), sound(std::move(s)) {
	setWindowTitle(tr("Chop a loop"));
	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(20, 18, 20, 14);
	v->setSpacing(12);
	auto* head = new QLabel(tr("<b style=\"font-size:16px\">Chop a loop across the drums</b><br>"
	                           "Each piece goes on its own drum pad, so you can play the loop’s sounds one by one, "
	                           "or let a pattern play them back in order at the song’s tempo."));
	head->setTextFormat(Qt::RichText);
	head->setWordWrap(true);
	v->addWidget(head);
	view = new ChopView;
	view->setSound(sound);
	v->addWidget(view, 1);

	auto* grid = new QGridLayout;
	grid->setHorizontalSpacing(12);
	grid->setVerticalSpacing(8);
	auto caption = [](const QString& t) {
		auto* l = new QLabel(t);
		l->setObjectName("cap");
		return l;
	};
	int row = 0;
	grid->addWidget(caption(tr("Cut at")), row, 0, Qt::AlignRight);
	how = new QButtonGroup(this);
	auto* atHits = new QRadioButton(tr("the hits it finds"));
	auto* atEqual = new QRadioButton(tr("equal pieces:"));
	how->addButton(atHits, 0);
	how->addButton(atEqual, 1);
	atHits->setChecked(true);
	auto* hitsRow = new QHBoxLayout;
	hitsRow->setSpacing(8);
	hitsRow->addWidget(atHits);
	auto* fewer = new QLabel(tr("fewer"));
	fewer->setObjectName("dim");
	sensitivity = new QSlider(Qt::Horizontal);
	sensitivity->setRange(0, 100);
	sensitivity->setValue(55);
	sensitivity->setFixedWidth(130);
	sensitivity->setToolTip(tr("How faint a hit may be and still get its own piece"));
	auto* more = new QLabel(tr("more"));
	more->setObjectName("dim");
	hitsRow->addSpacing(8);
	hitsRow->addWidget(fewer);
	hitsRow->addWidget(sensitivity);
	hitsRow->addWidget(more);
	hitsRow->addSpacing(18);
	hitsRow->addWidget(atEqual);
	parts = new QComboBox;
	for (int n : {2, 4, 8})
		parts->addItem(QString::number(n), n);
	parts->setCurrentIndex(2);
	parts->setToolTip(
	    tr("Cut the sound into this many pieces of the same length: good for loops that are exactly in time"));
	hitsRow->addWidget(parts);
	hitsRow->addStretch(1);
	grid->addLayout(hitsRow, row++, 1);

	grid->addWidget(caption(tr("Length")), row, 0, Qt::AlignRight);
	auto* barsRow = new QHBoxLayout;
	barsRow->setSpacing(8);
	bars = new QComboBox;
	for (int b : {1, 2, 4, 8})
		bars->addItem(b == 1 ? tr("1 bar") : tr("%1 bars").arg(b), b);
	const double secs = double(sound->frames()) / double(sound->rate);
	const int guess = samples::guessBars(secs, app->song.bpm);
	bars->setCurrentIndex(std::max(0, bars->findData(guess)));
	bars->setToolTip(
	    tr("How many bars the loop lasts. The pattern spreads the pieces over this many bars of the song."));
	barsRow->addWidget(bars);
	barsHint = new QLabel(tr("(%1 seconds long; at %2 BPM that is about %3 bars)")
	                          .arg(QLocale().toString(secs, 'f', 2))
	                          .arg(int(std::lround(app->song.bpm)))
	                          .arg(QLocale().toString(secs * app->song.bpm / 240.0, 'f', 1)));
	barsHint->setObjectName("dim");
	barsRow->addWidget(barsHint);
	barsRow->addStretch(1);
	grid->addLayout(barsRow, row++, 1);

	grid->addWidget(caption(tr("Name")), row, 0, Qt::AlignRight);
	name = new QLineEdit(soundName);
	name->setToolTip(tr("The pieces are saved in your sample library under this name, numbered 1, 2, 3 …"));
	grid->addWidget(name, row++, 1);
	grid->setColumnStretch(1, 1);
	v->addLayout(grid);

	pattern = new Switch(tr("Write a pattern that plays the pieces in their order, so the loop sounds as it was"),
	                     Switch::Beside);
	pattern->setChecked(true);
	pattern->setToolTip(tr("A drum clip at the loop: each piece starts where it was in the loop, on the nearest "
	                       "sixteenth. Clips of this drum track under it are replaced."));
	v->addWidget(pattern);
	info = new QLabel;
	info->setObjectName("dim");
	info->setWordWrap(true);
	v->addWidget(info);

	auto* buttons = new QDialogButtonBox;
	auto* go = buttons->addButton(tr("Put on the drums"), QDialogButtonBox::AcceptRole);
	go->setObjectName("primary");
	go->setDefault(true);
	buttons->addButton(QDialogButtonBox::Cancel);
	v->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, this, &ChopDialog::apply);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(how, &QButtonGroup::idClicked, this, [this] {
		edited = false;
		recut();
	});
	connect(sensitivity, &QSlider::valueChanged, this, [this] {
		how->button(0)->setChecked(true);
		edited = false;
		recut();
	});
	connect(parts, &QComboBox::activated, this, [this] {
		how->button(1)->setChecked(true);
		edited = false;
		recut();
	});
	connect(view, &ChopView::cutsEdited, this, [this] {
		edited = true;
		updateInfo();
	});
	connect(view, &ChopView::pieceClicked, this, &ChopDialog::play);
	connect(this, &QDialog::finished, this, [this] { app->previewData(nullptr); });
	resize(760, 520);
	recut();
}

void ChopDialog::recut() {
	if (edited) return;
	view->setCuts(how->checkedId() == 1 ? samples::equalCuts(sound->frames(), parts->currentData().toInt())
	                                    : samples::findHits(*sound, float(sensitivity->value()) / 100.f, dsp::D_COUNT));
	updateInfo();
}

void ChopDialog::updateInfo() {
	const int n = int(view->cuts().size());
	info->setText(tr("Pieces: %1, on drum pads 1 to %1. Click a piece to hear it; drag a line to move a cut, "
	                 "double-click to add one (up to 8), right-click a line to take it away.")
	                  .arg(n));
}

void ChopDialog::play(int piece) {
	const auto& c = view->cuts();
	if (piece < 0 || piece >= int(c.size())) return;
	auto d = std::make_shared<dsp::SampleData>(samples::slice(
	    *sound, c[size_t(piece)], size_t(piece) + 1 < c.size() ? c[size_t(piece) + 1] : sound->frames()));
	app->previewData(d);
	view->setPlaying(piece);
	const int ms = int(1000.0 * double(d->frames()) / double(d->rate));
	const int token = ++playToken; // a piece clicked later keeps its own highlight
	QTimer::singleShot(std::max(100, ms), view, [this, token] {
		if (token == playToken) view->setPlaying(-1);
	});
}

void ChopDialog::apply() {
	QString err;
	const int tid =
	    app->chopOntoDrums(*sound, view->cuts(), name->text(), bars->currentData().toInt(), pattern->isChecked(), err);
	if (tid < 0) {
		QMessageBox::warning(this, tr("Chop a loop"), err.isEmpty() ? tr("The pieces could not be made.") : err);
		return;
	}
	emit app->status(tr("The pieces of %1 are on the drums.").arg(name->text()));
	accept();
}

bool ChopDialog::chop(App* app, const QString& sampleName, QWidget* parent) {
	auto d = samples::get(sampleName);
	if (!d) {
		QMessageBox::warning(parent, tr("Chop a loop"),
		                     tr("The sound %1 could not be read.").arg(samples::displayName(sampleName)));
		return false;
	}
	ChopDialog dlg(app, d, samples::displayName(sampleName), parent);
	return dlg.exec() == QDialog::Accepted;
}

bool ChopDialog::chooseAndChop(App* app, QWidget* parent) {
	QSettings st;
	QString dir = st.value(QStringLiteral("chop/dir")).toString();
	if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
	const QString path = QFileDialog::getOpenFileName(parent, tr("Choose a loop to chop"), dir,
	                                                  tr("Sound files (*.wav *.aif *.aiff *.aifc *.WAV *.AIF *.AIFF)"));
	if (path.isEmpty()) return false;
	st.setValue(QStringLiteral("chop/dir"), QFileInfo(path).absolutePath());
	auto d = std::make_shared<dsp::SampleData>();
	QString err;
	if (!samples::readFile(path, *d, err, 60.0)) {
		QMessageBox::warning(parent, tr("Chop a loop"), err);
		return false;
	}
	ChopDialog dlg(app, d, QFileInfo(path).completeBaseName(), parent);
	return dlg.exec() == QDialog::Accepted;
}
