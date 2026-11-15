// Renders text the way Nachtzug's widgets do: the spec is painted inside a QWidget's paintEvent and the widget is
// grabbed, exactly the path of the program's own --screenshot (QPainter on a widget, QFont pixel sizes, the
// platform's FreeType hinting and its subpixel antialiasing, which Qt only uses when painting widgets).
// usage: QT_QPA_PLATFORM=offscreen qtrender spec.txt out.png
// spec lines:
//   font <path>                                     load an application font
//   canvas <w> <h> <#bg>
//   text <x> <y> <family> <weight> <px> <spacing> <#rgb> <text...>     (y = baseline)
//   ctext <cx> <y> <family> <weight> <px> <spacing> <#rgb> <text...>   (centred on cx, y = baseline)
//   box <x> <y> <w> <h> <family> <weight> <px> <spacing> <#rgb> <text...>  (centred in the box, like a button)
//   rect <x> <y> <w> <h> <#rgb>
//   rrect <x> <y> <w> <h> <radius> <#fill> <#border|none> [borderwidth]
//   ellipse <cx> <cy> <rx> <ry> <#fill>
//   image <x> <y> <path>                            draw a picture (1:1)
//   feat <tag> <value> | nofeat                      OpenType features for the following text
//   scale <s>                                        painter scale for what follows (coordinates in scaled units)
//   gray                                             grayscale antialiasing: paint into a QImage instead
#include <QApplication>
#include <QWidget>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QTextStream>
#include <QFontMetricsF>
#include <cstdio>

static void replay(QPainter* p, const QStringList& lines) {
    p->setRenderHint(QPainter::TextAntialiasing, true);
    QList<QPair<QByteArray, int>> feats;
    for (const QString& line : lines) {
        const QStringList w = line.split(' ');
        const QString cmd = w[0];
        if (cmd == "rect") {
            p->fillRect(QRect(w[1].toInt(), w[2].toInt(), w[3].toInt(), w[4].toInt()), QColor(w[5]));
        } else if (cmd == "rrect") {
            p->save(); p->setRenderHint(QPainter::Antialiasing, true);
            p->setBrush(QColor(w[6]));
            if (w[7] == "none") p->setPen(Qt::NoPen); else p->setPen(QPen(QColor(w[7]), w.size() > 8 ? w[8].toDouble() : 1.0));
            p->drawRoundedRect(QRectF(w[1].toDouble(), w[2].toDouble(), w[3].toDouble(), w[4].toDouble()), w[5].toDouble(), w[5].toDouble());
            p->restore();
        } else if (cmd == "ellipse") {
            p->save(); p->setRenderHint(QPainter::Antialiasing, true); p->setPen(Qt::NoPen); p->setBrush(QColor(w[5]));
            p->drawEllipse(QPointF(w[1].toDouble(), w[2].toDouble()), w[3].toDouble(), w[4].toDouble());
            p->restore();
        } else if (cmd == "image") {
            p->drawImage(QPointF(w[1].toDouble(), w[2].toDouble()), QImage(w.mid(3).join(' ')));
        } else if (cmd == "feat") {
            feats.append({w[1].toLatin1(), w[2].toInt()});
        } else if (cmd == "nofeat") {
            feats.clear();
        } else if (cmd == "scale") {
            p->resetTransform();
            const double sc = w[1].toDouble();
            if (sc != 1.0) p->scale(sc, sc);
        } else if (cmd == "text" || cmd == "box" || cmd == "ctext") {
            const int o = cmd == "box" ? 2 : 0;
            QString fam = w[3 + o]; fam.replace('_', ' ');
            QFont font(fam);
            font.setPixelSize(w[5 + o].toInt());
            font.setWeight(QFont::Weight(w[4 + o].toInt()));
            const double sp = w[6 + o].toDouble();
            if (sp != 0) font.setLetterSpacing(QFont::AbsoluteSpacing, sp);
            for (const auto& f : feats) font.setFeature(QFont::Tag::fromString(QString::fromLatin1(f.first)).value(), quint32(f.second));
            // Qt's layout cache does not tell fonts apart by their features alone; a negligible spacing does
            if (!feats.isEmpty() && sp == 0) font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0001);
            p->setFont(font);
            p->setPen(QColor(w[7 + o]));
            const QString text = w.mid(8 + o).join(' ');
            if (cmd == "text") p->drawText(QPointF(w[1].toDouble(), w[2].toDouble()), text);
            else if (cmd == "ctext") {
                const double adv = QFontMetricsF(font).horizontalAdvance(text);
                p->drawText(QPointF(w[1].toDouble() - adv / 2, w[2].toDouble()), text);
            } else p->drawText(QRect(w[1].toInt(), w[2].toInt(), w[3].toInt(), w[4].toInt()), Qt::AlignCenter, text);
        }
    }
}

class Canvas : public QWidget {
public:
    QStringList lines; QColor bg;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), bg);
        replay(&p, lines);
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc < 3) { fprintf(stderr, "usage: qtrender spec.txt out.png\n"); return 1; }
    QFile f(QString::fromLocal8Bit(argv[1]));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 2;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    Canvas c;
    bool gray = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty() || line.startsWith('#')) continue;
        const QStringList w = line.split(' ');
        if (w[0] == "font") {
            if (QFontDatabase::addApplicationFont(line.mid(5)) < 0) fprintf(stderr, "could not load %s\n", qPrintable(line.mid(5)));
        } else if (w[0] == "gray") {
            gray = true;
        } else if (w[0] == "canvas") {
            c.resize(w[1].toInt(), w[2].toInt()); c.bg = QColor(w[3]);
        } else c.lines << line;
    }
    QImage out;
    if (gray) {
        out = QImage(c.size(), QImage::Format_RGB32); out.fill(c.bg);
        QPainter p(&out); replay(&p, c.lines);
    } else out = c.grab().toImage();
    return out.save(QString::fromLocal8Bit(argv[2])) ? 0 : 3;
}
