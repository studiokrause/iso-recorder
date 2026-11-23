#pragma once
#include <QDialog>
#include <QList>

#include "RemoteControl.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTableWidget;
class QTimer;

// Ustawienia programu: domyslne parametry nagrywania dla nowych kanalow, karta dzwiekowa odsluchu,
// szablon nazwy folderu dla "Split to new folder".
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr, RemoteControl* remote = nullptr);

private:
    void save();
    void fillMidiTable();
    void startLearn();
    void stopLearn();

    QLineEdit* m_dir;
    QLineEdit* m_template;
    QComboBox* m_container;
    QCheckBox* m_growing;
    QComboBox* m_split;
    QComboBox* m_encoder;
    QComboBox* m_format;
    QComboBox* m_fps;
    QComboBox* m_codec;
    QComboBox* m_videoRate;
    QComboBox* m_audioRate;
    QComboBox* m_monitor;
    QLineEdit* m_folderTemplate;
    QComboBox* m_rename;
    QCheckBox* m_osc;
    QSpinBox* m_oscPort;
    QCheckBox* m_reply;
    QLineEdit* m_fbHost;
    QSpinBox* m_fbPort;
    QCheckBox* m_midi;
    QComboBox* m_midiIn;
    QComboBox* m_midiOut;
    QSpinBox* m_midiChannel;
    RemoteControl* m_remote = nullptr;
    QList<RemoteControl::MidiBinding> m_bindings;      // edytowana kopia przypisan MIDI
    QTableWidget* m_midiTable = nullptr;
    QPushButton* m_learnButton = nullptr;
    QTimer* m_learnTimer = nullptr;
};

// Opis zmiennych szablonu nazwy (podpowiedz przy etykietach "Name template").
QString nameTemplateHelp();
// Wspolne listy dla okna Settings i zakladki Output.
void fillContainerCombo(QComboBox* combo);                       // MKV, MPEG-TS, MP4, MOV
void fillCodecCombo(QComboBox* combo);                           // kodeki dostepne na tym komputerze
void fillEncoderCombo(QComboBox* combo, const QString& codec);   // Auto + enkodery kodeka + kopia strumienia
void enableContainersFor(QComboBox* combo, const QString& codec);   // wylacza kontenery, ktore nie przyjma kodeka
QString containerHelp();
QString growingHelp();
void updateGrowing(QCheckBox* box, const QString& container);     // aktywny tylko dla MP4 i MOV
