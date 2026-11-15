// Chop a loop: a drum loop (or any sound) is cut into pieces at its hits, each piece goes on a drum pad, and a
// pattern can play them back in order, so the loop follows the song's tempo and its sounds can be played one by one.
#pragma once
#include "dsp.h"
#include <QDialog>
#include <QWidget>
#include <memory>
#include <vector>

class App;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

// The sound drawn as a wave, with the cuts as lines over it. Click a piece to hear it, drag a line to move a cut,
// double-click to add one, right-click a line to take it away.
class ChopView : public QWidget {
    Q_OBJECT
public:
    explicit ChopView(QWidget* parent = nullptr);
    void setSound(std::shared_ptr<const dsp::SampleData> d);
    void setCuts(const std::vector<size_t>& cuts);
    const std::vector<size_t>& cuts() const { return cuts_; }
    void setPlaying(int piece) { playing_ = piece; update(); }
    QSize sizeHint() const override { return {680, 170}; }
    QSize minimumSizeHint() const override { return {320, 120}; }
signals:
    void cutsEdited();          // by hand
    void pieceClicked(int i);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    int xOf(size_t frame) const;
    size_t frameAt(int x) const;
    int cutNear(int x) const;    // index of the cut line under x (never the first, at the start), or -1
    int pieceAt(int x) const;
    void buildPeaks();
    std::shared_ptr<const dsp::SampleData> data_;
    std::vector<size_t> cuts_;
    std::vector<std::pair<float, float>> peaks_;   // lowest and highest value under each pixel column
    int drag_ = -1, playing_ = -1;
};

class ChopDialog : public QDialog {
    Q_OBJECT
public:
    // name: what the sound is called, for the pieces' names
    ChopDialog(App* app, std::shared_ptr<const dsp::SampleData> sound, const QString& name, QWidget* parent);
    // asks for a sound file, then opens the dialog for it; true if pieces were put on the drums
    static bool chooseAndChop(App* app, QWidget* parent);
    static bool chop(App* app, const QString& sampleName, QWidget* parent);   // a sound from the sample library
private:
    void recut();               // the cuts again, from the settings
    void updateInfo();
    void play(int piece);
    void apply();
    App* app;
    std::shared_ptr<const dsp::SampleData> sound;
    ChopView* view;
    QButtonGroup* how;
    QSlider* sensitivity;
    QComboBox *parts, *bars;
    QLineEdit* name;
    QCheckBox* pattern;
    QLabel *info, *barsHint;
    bool edited = false;        // the cuts were moved by hand: the settings no longer redo them
    int playToken = 0;
};
