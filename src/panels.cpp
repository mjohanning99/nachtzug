// The panels: two filters on the whole program. One paints the screwed well under every section and unit and
// rounds the drop-down lists and menus; the other lets a section fold shut to a long key with a summary.
// And tidyForm(), so a form looks the same on every system.
//
// What's here: tidyForm(), makeFoldable(), setFolded(), installWells(); installWells() is called once at start-up,
// makeFoldable() by each panel for its sections.
// Changes in the song file: none (a section's folded state is a setting).
#include "widgets.h"
#include "model.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QPointer>
#include <QSettings>
#include <QFrame>
#include <QGroupBox>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMenu>
#include <QStyledItemDelegate>
#include <algorithm>
#include <functional>

namespace theme {

void tidyForm(QFormLayout* form) {
	form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form->setRowWrapPolicy(QFormLayout::DontWrapRows);
}

namespace {
// a drop-down's items as the style sheet draws them, and a separator as a thin rule like the menus' (a plain styled
// delegate would draw it as an empty item, a gap that looks like something is missing)
class ListDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	static bool separator(const QModelIndex& i) {
		return i.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator");
	}
	void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override {
		if (i.data(LIST_INDENT_ROLE).toBool()) { // under a heading: set in, in the list only
			QStyleOptionViewItem opt = o;
			initStyleOption(&opt, i);
			opt.text = indent() + opt.text;
			const QWidget* w = opt.widget;
			(w ? w->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &opt, p, w);
			return;
		}
		if (!separator(i)) {
			QStyledItemDelegate::paint(p, o, i);
			return;
		}
		const int y = o.rect.center().y();
		p->fillRect(QRect(o.rect.left() + 8, y, o.rect.width() - 16, 1), gridBar);
	}
	QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override {
		if (separator(i)) return QSize(1, 9); // as QMenu::separator: a line with 4 pixels above and below
		QSize s = QStyledItemDelegate::sizeHint(o, i);
		if (i.data(LIST_INDENT_ROLE).toBool()) s.rwidth() += o.fontMetrics.horizontalAdvance(indent());
		return s;
	}
	static QString indent() { return QStringLiteral("   "); }
};

class WellPainter : public QObject {
public:
	using QObject::QObject;
	bool eventFilter(QObject* o, QEvent* e) override {
		if (e->type() == QEvent::Polish) {
			round(o);
			if (auto* g = qobject_cast<QGroupBox*>(o)) fitKnobs(g);
		}
		if (e->type() == QEvent::Paint && o->property("nzRound").toBool() && o->inherits("QComboBoxPrivateContainer")) {
			// Qt paints this window's background its own way, square, whatever the style sheet says: so here
			auto* w = static_cast<QWidget*>(o);
			QPainter p(w);
			p.setRenderHint(QPainter::Antialiasing);
			p.setPen(QPen(rule, 1));
			p.setBrush(well);
			p.drawRoundedRect(QRectF(w->rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
			return true;
		}
		if (e->type() != QEvent::Paint || !tactile || !o->isWidgetType()) return false;
		auto* w = static_cast<QWidget*>(o);
		if (qobject_cast<QGroupBox*>(w)) {
			if (w->property("folded").toBool()) return false; // folded shut: only its name and summary
			QPainter p(w);
			drawWell(p, QRectF(w->rect()).adjusted(0, GROUP_TOP, 0, 0));
		} else if (qobject_cast<QFrame*>(w)) {
			const QString& n = w->objectName();
			if (n == QLatin1String("unit") || n == QLatin1String("strip")) {
				QPainter p(w);
				drawWell(p, QRectF(w->rect()), w->property("sel").toBool());
			}
		}
		return false;
	}

private:
	// a section whose knobs' names all fit on one line gives them room for one line only: an empty second line under
	// every knob would lift them above the middle of the section. One long name keeps two lines for all of them, so
	// the knobs of a section stay level.
	static void fitKnobs(QGroupBox* g) {
		const QList<Knob*> knobs = g->findChildren<Knob*>();
		for (Knob* k : knobs)
			k->ensurePolished(); // the section is polished before its knobs: their font is only right after this
		const bool one =
		    !knobs.isEmpty() && std::all_of(knobs.begin(), knobs.end(), [](Knob* k) { return k->nameFitsOneLine(); });
		for (Knob* k : knobs)
			k->setOneLine(one);
	}
	// a drop-down's list and a menu are windows of their own, square unless they are see-through outside their
	// rounded shape: then the style sheet's border-radius shows, as on the boxes and keys they open from
	static void round(QObject* o) {
		QWidget* w = nullptr;
		if (auto* box = qobject_cast<QComboBox*>(o)) {
			if (!dynamic_cast<ListDelegate*>(box->itemDelegate()))
				box->setItemDelegate(new ListDelegate(box)); // so the style sheet's item rules apply
			w = box->view()->window();                       // the list's window, made here and not shown yet
		} else if (auto* menu = qobject_cast<QMenu*>(o))
			w = menu;
		if (!w || w->property("nzRound").toBool() || w->testAttribute(Qt::WA_WState_Created)) return;
		w->setProperty("nzRound", true);
		w->setWindowFlag(Qt::FramelessWindowHint, true);
		w->setWindowFlag(Qt::NoDropShadowWindowHint, true); // a square shadow would show behind the round corners
		w->setAttribute(Qt::WA_TranslucentBackground, true);
		if (auto* frame = qobject_cast<QFrame*>(w)) frame->setFrameShape(QFrame::NoFrame);
	}
};
} // namespace

// ---- folding sections
namespace {
class Folder : public QObject {
public:
	Folder(QGroupBox* g, QString key, std::function<QString()> summary)
	    : QObject(g), g_(g), key_(std::move(key)), summary_(std::move(summary)) {
		base_ = g->title();
		setObjectName(QStringLiteral("nzFolder"));
		g->installEventFilter(this);
		g->setMouseTracking(true);
	}
	bool folded() const { return folded_; }
	void set(bool on) {
		if (on == folded_) return;
		folded_ = on;
		if (on) {
			hidden_.clear();
			for (QObject* o : g_->children())
				if (auto* w = qobject_cast<QWidget*>(o); w && !w->isWindow() && w->isVisibleTo(g_)) {
					hidden_.push_back(w);
					w->hide();
				}
			// as wide as the name and its summary, but never wider than there is room for: in a narrow column (the
			// inspector) the summary shortens instead, and only the name has to fit
			const QFontMetrics fm(g_->font());
			const int name = fm.horizontalAdvance(base_) + 3 * base_.size() + 34;
			g_->setFixedHeight(32); // a long key: press it to open
			g_->setMinimumWidth(name);
			// in a row (the Sound tab) it gives the room it no longer needs to its neighbours; in a column (the
			// inspector) it keeps the column's width, so every summary ends at the same edge
			const QWidget* p = g_->parentWidget();
			const bool inColumn = p && qobject_cast<QVBoxLayout*>(p->layout()) && p->layout()->indexOf(g_) >= 0;
			g_->setMaximumWidth(inColumn ? QWIDGETSIZE_MAX
			                             : name + std::min(220, fm.horizontalAdvance(summary()) + 24) + 30);
		} else {
			for (QWidget* w : hidden_)
				if (w) w->show();
			hidden_.clear();
			g_->setMinimumSize(0, 0);
			g_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		}
		g_->setProperty("folded", on);
		g_->setTitle((on ? QStringLiteral("▸ ") : QStringLiteral("▾ ")) + base_);
		if (!key_.isEmpty()) QSettings().setValue(QStringLiteral("fold/") + key_, on);
		g_->updateGeometry();
		if (QWidget* p = g_->parentWidget()) {
			if (p->layout()) p->layout()->invalidate();
			p->updateGeometry();
		}
	}
	QString summary() const {
		if (summary_) return summary_();
		QStringList parts;
		for (QComboBox* c : g_->findChildren<QComboBox*>())
			if (c->count()) parts << c->currentText().trimmed();
		if (parts.isEmpty())
			for (Knob* k : g_->findChildren<Knob*>()) {
				if (parts.size() == 3) break;
				parts << QStringLiteral("%1 %2").arg(k->name(), k->valueText());
			}
		return parts.join(QStringLiteral(" · "));
	}
	bool eventFilter(QObject*, QEvent* e) override {
		switch (e->type()) {
		case QEvent::MouseButtonPress: {
			auto* m = static_cast<QMouseEvent*>(e);
			if (m->button() == Qt::LeftButton && (folded_ || m->position().y() < GROUP_TOP)) { // folded: the whole key
				set(!folded_);
				return true;
			}
			break;
		}
		case QEvent::MouseMove: {
			auto* m = static_cast<QMouseEvent*>(e);
			const bool over = folded_ || m->position().y() < GROUP_TOP;
			if (over != hot_) {
				hot_ = over;
				g_->update(); // a folded key lights up
				if (over)
					g_->setCursor(theme::cursor(QStringLiteral("finger"), Qt::PointingHandCursor));
				else
					g_->unsetCursor();
			}
			break;
		}
		case QEvent::Leave:
			if (hot_) {
				hot_ = false;
				g_->unsetCursor();
				g_->update();
			}
			break;
		case QEvent::Paint:
			if (folded_) { // a long key, pressed to open: ▸, the name, and a summary of the settings inside
				QPainter p(g_);
				p.setRenderHint(QPainter::Antialiasing);
				const QRectF r = QRectF(g_->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
				QLinearGradient kg(0, r.top(), 0, r.bottom());
				kg.setColorAt(0, hot_ ? wellHi.lighter(106) : wellHi.lighter(103));
				kg.setColorAt(0.86, mix(wellHi, rule, 0.18));
				kg.setColorAt(0.88, rule.darker(125));
				kg.setColorAt(1, rule.darker(125));
				p.setPen(QPen(rule, 1));
				p.setBrush(kg);
				p.drawRoundedRect(r, 5, 5);
				const double x0 = 12, cy = r.center().y() - 1.5, h = r.height() - 3;
				QPolygonF tri;
				tri << QPointF(x0, cy - 4.5) << QPointF(x0 + 6, cy) << QPointF(x0, cy + 4.5);
				p.setPen(Qt::NoPen);
				p.setBrush(ink);
				p.drawPolygon(tri);
				const QFont tf = g_->font();
				p.setFont(tf);
				p.setPen(ink);
				const int tx = int(x0) + 14, tw = QFontMetrics(tf).horizontalAdvance(base_);
				p.drawText(QRectF(tx, 0, tw + 4, h), Qt::AlignVCenter | Qt::AlignLeft, base_);
				QFont sf = tf;
				sf.setPixelSize(11);
				sf.setWeight(QFont::Normal);
				p.setFont(sf);
				p.setPen(dim);
				const int sx = tx + tw + 14, room = std::max(0, int(r.width()) - 12 - sx);
				p.drawText(QRectF(sx, 0, room, h), Qt::AlignVCenter | Qt::AlignRight,
				           QFontMetrics(sf).elidedText(summary(), Qt::ElideRight, room));
				return true; // instead of the section's own title
			}
			break;
		default:
			break;
		}
		return false;
	}

private:
	QGroupBox* g_;
	QString key_, base_;
	std::function<QString()> summary_;
	std::vector<QPointer<QWidget>> hidden_;
	bool folded_ = false, hot_ = false;
};
} // namespace

void makeFoldable(QGroupBox* g, const QString& key, std::function<QString()> summary) {
	if (!g || g->findChild<QObject*>(QStringLiteral("nzFolder"), Qt::FindDirectChildrenOnly)) return;
	auto* f = new Folder(g, key, std::move(summary));
	g->setTitle(QStringLiteral("▾ ") + g->title());
	g->setToolTip(g->toolTip().isEmpty() ? QCoreApplication::translate(
	                                           "Model", "Click the name to fold this section shut or open it again")
	                                     : g->toolTip());
	if (!key.isEmpty() && QSettings().value(QStringLiteral("fold/") + key, false).toBool())
		QTimer::singleShot(0, g, [f] { f->set(true); }); // once the section is filled
}

void setFolded(QGroupBox* g, bool folded) {
	if (auto* f =
	        g ? static_cast<Folder*>(g->findChild<QObject*>(QStringLiteral("nzFolder"), Qt::FindDirectChildrenOnly))
	          : nullptr)
		f->set(folded);
}

void installWells() {
	static bool done = false;
	if (done || !qApp) return;
	done = true;
	qApp->installEventFilter(new WellPainter(qApp));
}

} // namespace theme
