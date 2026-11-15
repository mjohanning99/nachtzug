// Settings: the program's look and language, the sound card (output and input), and the sample library.
#pragma once
#include <QDialog>
#include <functional>

class App;
class QComboBox;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QTabWidget;
class QCheckBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    // what the main window does when a setting changes
    struct Hooks {
        std::function<void(int)> theme;          // colour scheme index
        std::function<void(bool)> simpleSound;   // the Sound tab shows the simple page
        std::function<void(bool)> pads;          // on-screen keys under the notes
        std::function<void()> restart;           // start the program again (asks to save first)
    };
    SettingsDialog(App* app, const Hooks& hooks, QWidget* parent, int page = 0);
    enum Page { General, Audio, Samples, MidiPage };
private:
    QWidget* generalPage();
    QWidget* audioPage();
    QWidget* samplesPage();
    QWidget* midiPage();
    void fillAudio();
    void applyAudio();
    void fillSamples(const QString& select = QString());
    QString currentSample() const;
    App* app;
    Hooks hooks;
    QTabWidget* tabs;
    QComboBox *driver, *output, *rate, *buffer, *input;
    QLabel* audioState;
    QTreeWidget* sampleList;
    QPushButton *playBtn, *renameBtn, *removeBtn, *chopBtn;
    bool filling = false;
};

// Records a sample from the sound card's input into the library.
class RecordDialog : public QDialog {
    Q_OBJECT
public:
    RecordDialog(App* app, QWidget* parent);
    QString savedName() const { return saved; }
private:
    void start();
    void stop();
    void save();
    App* app;
    QComboBox* input;
    QPushButton *recBtn, *saveBtn;
    QCheckBox* trim;
    class QLineEdit* name;
    QLabel *timeLabel, *hint;
    class LevelBar* level;
    std::vector<float> take;
    int takeRate = 48000;
    bool recording = false;
    QString saved;
};
