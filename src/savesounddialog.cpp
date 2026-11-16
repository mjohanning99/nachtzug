// The "Save sound…" dialog: see savesounddialog.h.
#include "savesounddialog.h"
#include "widgets.h"
#include <QDialogButtonBox>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SaveSoundDialog::SaveSoundDialog(const Patch& patch, int group, QWidget* parent) : QDialog(parent) {
	setWindowTitle(tr("Save sound"));
	sound.patch = patch;
	sound.group = std::clamp(group, 0, mysounds::GroupCount - 1);
	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(22, 18, 22, 16);
	v->setSpacing(6);
	auto caption = [&](const QString& text) {
		auto* l = new QLabel(text);
		l->setObjectName("cap");
		v->addWidget(l);
	};
	caption(tr("Name"));
	name = new QLineEdit;
	// a ready-made sound changed by hand gets a name of its own to start from
	const QString start = patch.name.trimmed();
	bool already = false; // one of your own already keeps its name
	for (const mysounds::Sound& m : mysounds::all())
		already = already || m.name == start;
	name->setText(start.isEmpty() ? tr("My sound") : already ? start : tr("%1 (mine)").arg(ui(start)));
	name->selectAll();
	name->setMinimumWidth(320);
	v->addWidget(name);
	v->addSpacing(8);

	caption(tr("Group"));
	auto* keys = new QHBoxLayout;
	keys->setSpacing(0);
	auto* only = new QButtonGroup(this); // one choice among the group keys only
	for (int g = 0; g < mysounds::GroupCount; g++) {
		auto* k = new QPushButton(ui(mysounds::groupName(g)));
		k->setObjectName(g == 0 ? "segL" : g == mysounds::GroupCount - 1 ? "segR" : "segM");
		k->setCheckable(true);
		only->addButton(k);
		k->setChecked(g == sound.group);
		k->setMinimumWidth(84);
		groupKeys[g] = k;
		keys->addWidget(k);
	}
	keys->addStretch(1);
	v->addLayout(keys);
	v->addSpacing(8);

	caption(tr("Words to find it by (optional)"));
	words = new QLineEdit;
	words->setPlaceholderText(tr("for example: dark, squelchy"));
	words->setToolTip(tr("A few words, separated by commas; the sound library finds the sound by them"));
	v->addWidget(words);
	v->addSpacing(10);

	star = new Switch(tr("Show it in the preset list (starred)"), Switch::Beside);
	star->setChecked(true);
	star->setToolTip(tr("Starred sounds are listed on top of the preset list, in every song"));
	v->addWidget(star);
	v->addSpacing(8);

	auto* buttons = new QDialogButtonBox;
	auto* save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
	save->setObjectName("primary");
	save->setDefault(true);
	buttons->addButton(QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, this, [this] { trySave(); });
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	v->addWidget(buttons);
}

void SaveSoundDialog::trySave() {
	sound.name = name->text().trimmed();
	for (int g = 0; g < mysounds::GroupCount; g++)
		if (groupKeys[g]->isChecked()) sound.group = g;
	sound.words.clear();
	for (const QString& w : words->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
		if (!w.trimmed().isEmpty()) sound.words << w.trimmed();
	sound.starred = star->isChecked();
	QString error;
	if (!mysounds::save(sound, &error)) {
		QMessageBox::warning(this, windowTitle(), error);
		return;
	}
	accept();
}
