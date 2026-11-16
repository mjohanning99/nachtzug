// MIDI in and out on each system's own MIDI service: the ALSA sequencer on Linux, CoreMIDI on macOS, the
// multimedia MIDI functions on Windows. Raw messages only; what they mean is up to Midi (midi.h).
#pragma once
#include <QString>
#include <QStringList>
#include <cstdint>
#include <functional>
#include <memory>

class MidiIO {
public:
	// called on a thread of its own with one complete message (note, controller, clock, start, stop …)
	using Receive = std::function<void(const uint8_t* bytes, int length)>;
	MidiIO();
	~MidiIO();
	bool ok() const;             // the system's MIDI service could be reached
	QString error() const;       // why not: "access" (not allowed: on Linux, not in the audio group) or "missing"
	QStringList inputs() const;  // names of the devices that send MIDI (keyboards, controllers)
	QStringList outputs() const; // names of the devices that take MIDI
	// listen to one input by name, or to every input when name is "*"; an empty name stops listening
	bool listen(const QString& name, Receive onMessage);
	// send to one output by name; an empty name stops sending
	bool sendTo(const QString& name);
	void send(const uint8_t* bytes, int length); // to the chosen output; safe from any thread
	bool sending() const;

private:
	struct Impl;
	std::unique_ptr<Impl> d;
};

// Splits a stream of MIDI bytes into whole messages, for services that hand over bytes in batches.
class MidiParser {
public:
	template <class F> void feed(const uint8_t* b, int n, F&& out) {
		for (int i = 0; i < n; i++) {
			const uint8_t x = b[i];
			if (x >= 0xf8) {
				out(&x, 1);
				continue;
			} // clock, start, stop: may come anywhere
			if (x & 0x80) {
				if (x == 0xf0) {
					sysex_ = true;
					continue;
				}
				if (x == 0xf7) {
					sysex_ = false;
					continue;
				}
				sysex_ = false;
				msg_[0] = x;
				len_ = 1;
				need_ = expected(x);
				if (need_ == 1) {
					out(msg_, 1);
					len_ = 0;
				}
				continue;
			}
			if (sysex_) continue;
			if (len_ == 0) {
				if (!(msg_[0] & 0x80) || msg_[0] >= 0xf0) continue;
				len_ = 1;
				need_ = expected(msg_[0]);
			} // running status
			msg_[len_++] = x;
			if (len_ == need_) {
				out(msg_, len_);
				len_ = 0;
			} // the status stays for running status
		}
	}
	static int expected(uint8_t status) {
		switch (status & 0xf0) {
		case 0xc0:
		case 0xd0:
			return 2;
		case 0xf0:
			return status == 0xf2 ? 3 : (status == 0xf1 || status == 0xf3) ? 2 : 1;
		default:
			return 3;
		}
	}

private:
	uint8_t msg_[3] = {0, 0, 0};
	int len_ = 0, need_ = 3;
	bool sysex_ = false;
};
