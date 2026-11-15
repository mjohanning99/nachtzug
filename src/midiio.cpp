#include "midiio.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#elif defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#else
#include <alsa/asoundlib.h>
#include <poll.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
// ---------------------------------------------------------------- macOS: CoreMIDI

struct MidiIO::Impl {
    MIDIClientRef client = 0;
    MIDIPortRef inPort = 0, outPort = 0;
    MIDIEndpointRef dest = 0;
    std::vector<MIDIEndpointRef> listening;
    Receive receive;
    MidiParser parser;
    std::mutex mx, cbMx;   // cbMx: held while a message is handed over, so the receiver can be swapped safely
    QString err;
    static QString nameOf(MIDIEndpointRef e) {
        CFStringRef s = nullptr;
        if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &s) != noErr || !s) return QString();
        const QString out = QString::fromCFString(s);
        CFRelease(s);
        return out;
    }
    static void readProc(const MIDIPacketList* list, void* self, void*) {
        auto* d = static_cast<Impl*>(self);
        const MIDIPacket* p = &list->packet[0];
        for (UInt32 i = 0; i < list->numPackets; i++) {
            std::lock_guard<std::mutex> g(d->cbMx);
            if (d->receive) d->parser.feed(p->data, int(p->length), [d](const uint8_t* b, int n) { d->receive(b, n); });
            p = MIDIPacketNext(p);
        }
    }
};

MidiIO::MidiIO() : d(std::make_unique<Impl>()) {
    if (MIDIClientCreate(CFSTR("Nachtzug"), nullptr, nullptr, &d->client) != noErr) { d->err = QStringLiteral("missing"); return; }
    MIDIInputPortCreate(d->client, CFSTR("Nachtzug in"), &Impl::readProc, d.get(), &d->inPort);
    MIDIOutputPortCreate(d->client, CFSTR("Nachtzug out"), &d->outPort);
}
MidiIO::~MidiIO() { if (d->client) MIDIClientDispose(d->client); }
bool MidiIO::ok() const { return d->client != 0; }
QString MidiIO::error() const { return d->err; }
QStringList MidiIO::inputs() const { QStringList l; for (ItemCount i = 0; i < MIDIGetNumberOfSources(); i++) l << Impl::nameOf(MIDIGetSource(i)); return l; }
QStringList MidiIO::outputs() const { QStringList l; for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); i++) l << Impl::nameOf(MIDIGetDestination(i)); return l; }
bool MidiIO::listen(const QString& name, Receive onMessage) {
    if (!ok()) return false;
    for (MIDIEndpointRef e : d->listening) MIDIPortDisconnectSource(d->inPort, e);
    d->listening.clear();
    { std::lock_guard<std::mutex> g(d->cbMx); d->receive = std::move(onMessage); }
    if (name.isEmpty()) return true;
    bool any = false;
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); i++) {
        MIDIEndpointRef e = MIDIGetSource(i);
        if (name != QLatin1String("*") && Impl::nameOf(e) != name) continue;
        if (MIDIPortConnectSource(d->inPort, e, nullptr) == noErr) { d->listening.push_back(e); any = true; }
    }
    return any || name == QLatin1String("*");
}
bool MidiIO::sendTo(const QString& name) {
    std::lock_guard<std::mutex> g(d->mx);
    d->dest = 0;
    if (name.isEmpty()) return true;
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); i++) if (Impl::nameOf(MIDIGetDestination(i)) == name) d->dest = MIDIGetDestination(i);
    return d->dest != 0;
}
bool MidiIO::sending() const { return d->dest != 0; }
void MidiIO::send(const uint8_t* bytes, int length) {
    std::lock_guard<std::mutex> g(d->mx);
    if (!d->dest || length <= 0) return;
    Byte buf[256];
    auto* list = reinterpret_cast<MIDIPacketList*>(buf);
    MIDIPacket* p = MIDIPacketListInit(list);
    p = MIDIPacketListAdd(list, sizeof(buf), p, 0, ByteCount(length), bytes);
    if (p) MIDISend(d->outPort, d->dest, list);
}

#elif defined(_WIN32)
// ---------------------------------------------------------------- Windows: winmm

struct MidiIO::Impl {
    std::vector<HMIDIIN> ins;
    HMIDIOUT out = nullptr;
    Receive receive;
    std::mutex mx, cbMx;
    static QString inName(UINT i) { MIDIINCAPSW c{}; return midiInGetDevCapsW(i, &c, sizeof(c)) == MMSYSERR_NOERROR ? QString::fromWCharArray(c.szPname) : QString(); }
    static QString outName(UINT i) { MIDIOUTCAPSW c{}; return midiOutGetDevCapsW(i, &c, sizeof(c)) == MMSYSERR_NOERROR ? QString::fromWCharArray(c.szPname) : QString(); }
    static void CALLBACK inProc(HMIDIIN, UINT msg, DWORD_PTR self, DWORD_PTR p1, DWORD_PTR) {
        if (msg != MIM_DATA) return;
        auto* d = reinterpret_cast<Impl*>(self);
        const uint8_t b[3] = {uint8_t(p1 & 0xff), uint8_t((p1 >> 8) & 0xff), uint8_t((p1 >> 16) & 0xff)};
        std::lock_guard<std::mutex> g(d->cbMx);
        if (d->receive) d->receive(b, b[0] >= 0xf8 ? 1 : MidiParser::expected(b[0]));
    }
    void closeIns() { for (HMIDIIN h : ins) { midiInStop(h); midiInClose(h); } ins.clear(); }
};

MidiIO::MidiIO() : d(std::make_unique<Impl>()) {}
MidiIO::~MidiIO() { d->closeIns(); if (d->out) midiOutClose(d->out); }
bool MidiIO::ok() const { return true; }
QString MidiIO::error() const { return QString(); }
QStringList MidiIO::inputs() const { QStringList l; for (UINT i = 0; i < midiInGetNumDevs(); i++) l << Impl::inName(i); return l; }
QStringList MidiIO::outputs() const { QStringList l; for (UINT i = 0; i < midiOutGetNumDevs(); i++) l << Impl::outName(i); return l; }
bool MidiIO::listen(const QString& name, Receive onMessage) {
    d->closeIns();
    { std::lock_guard<std::mutex> g(d->cbMx); d->receive = std::move(onMessage); }
    if (name.isEmpty()) return true;
    bool any = false;
    for (UINT i = 0; i < midiInGetNumDevs(); i++) {
        if (name != QLatin1String("*") && Impl::inName(i) != name) continue;
        HMIDIIN h = nullptr;
        if (midiInOpen(&h, i, DWORD_PTR(&Impl::inProc), DWORD_PTR(d.get()), CALLBACK_FUNCTION) == MMSYSERR_NOERROR) { midiInStart(h); d->ins.push_back(h); any = true; }
    }
    return any || name == QLatin1String("*");
}
bool MidiIO::sendTo(const QString& name) {
    std::lock_guard<std::mutex> g(d->mx);
    if (d->out) { midiOutClose(d->out); d->out = nullptr; }
    if (name.isEmpty()) return true;
    for (UINT i = 0; i < midiOutGetNumDevs(); i++)
        if (Impl::outName(i) == name && midiOutOpen(&d->out, i, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) return true;
    return false;
}
bool MidiIO::sending() const { return d->out != nullptr; }
void MidiIO::send(const uint8_t* b, int n) {
    std::lock_guard<std::mutex> g(d->mx);
    if (!d->out || n <= 0) return;
    DWORD m = b[0];
    if (n > 1) m |= DWORD(b[1]) << 8;
    if (n > 2) m |= DWORD(b[2]) << 16;
    midiOutShortMsg(d->out, m);
}

#else
// ---------------------------------------------------------------- Linux: the ALSA sequencer

struct MidiIO::Impl {
    snd_seq_t* seq = nullptr;
    int client = -1, inPort = -1, outPort = -1;
    snd_midi_event_t *decoder = nullptr, *encoder = nullptr;
    std::vector<std::pair<int, int>> listening;   // (client, port)
    std::pair<int, int> target{-1, -1};
    Receive receive;
    std::mutex mx;                                 // one handle, shared by the listening thread and the senders
    std::mutex cbMx;                               // held while a message is handed over, so the receiver can be swapped safely
    std::thread th;
    std::atomic<bool> run{false};
    int wake[2] = {-1, -1};
    QString err;

    // every port of other programs and devices with the given abilities, as (name, client, port)
    struct PortRef { QString name; int client, port; };
    std::vector<PortRef> ports(unsigned caps) {
        std::vector<PortRef> out;
        std::lock_guard<std::mutex> g(mx);
        snd_seq_client_info_t* ci; snd_seq_port_info_t* pi;
        snd_seq_client_info_alloca(&ci); snd_seq_port_info_alloca(&pi);
        snd_seq_client_info_set_client(ci, -1);
        while (snd_seq_query_next_client(seq, ci) >= 0) {
            const int c = snd_seq_client_info_get_client(ci);
            if (c == client || c == SND_SEQ_CLIENT_SYSTEM) continue;
            snd_seq_port_info_set_client(pi, c);
            snd_seq_port_info_set_port(pi, -1);
            while (snd_seq_query_next_port(seq, pi) >= 0) {
                if ((snd_seq_port_info_get_capability(pi) & caps) != caps) continue;
                if (!(snd_seq_port_info_get_type(pi) & (SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_SYNTH | SND_SEQ_PORT_TYPE_HARDWARE))) continue;
                const QString cn = QString::fromUtf8(snd_seq_client_info_get_name(ci)), pn = QString::fromUtf8(snd_seq_port_info_get_name(pi));
                out.push_back({pn.startsWith(cn) ? pn : cn + QStringLiteral(": ") + pn, c, snd_seq_port_info_get_port(pi)});
            }
        }
        return out;
    }
    void loop() {
        const int nfd = snd_seq_poll_descriptors_count(seq, POLLIN);
        std::vector<pollfd> fds(size_t(nfd) + 1);
        snd_seq_poll_descriptors(seq, fds.data(), unsigned(nfd), POLLIN);
        fds[size_t(nfd)] = {wake[0], POLLIN, 0};
        uint8_t buf[64];
        while (run.load()) {
            if (poll(fds.data(), fds.size(), 500) <= 0) continue;
            if (fds[size_t(nfd)].revents) break;
            for (;;) {
                snd_seq_event_t* ev = nullptr;
                long n = 0;
                {
                    std::lock_guard<std::mutex> g(mx);
                    if (snd_seq_event_input(seq, &ev) < 0 || !ev) break;
                    snd_midi_event_reset_decode(decoder);
                    n = snd_midi_event_decode(decoder, buf, sizeof(buf), ev);
                }
                std::lock_guard<std::mutex> cg(cbMx);
                if (n > 0 && receive && buf[0] != 0xf0) receive(buf, int(n));
            }
        }
    }
};

static void quietAlsa(const char*, int, const char*, int, const char*, ...) {}

MidiIO::MidiIO() : d(std::make_unique<Impl>()) {
    snd_lib_error_set_handler(&quietAlsa);   // a missing sequencer is reported in the settings, not on the terminal
    const int r = snd_seq_open(&d->seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
    if (r < 0) {
        d->seq = nullptr;
        d->err = r == -EACCES ? QStringLiteral("access") : QStringLiteral("missing");
        return;
    }
    snd_seq_set_client_name(d->seq, "Nachtzug");
    d->client = snd_seq_client_id(d->seq);
    d->inPort = snd_seq_create_simple_port(d->seq, "Nachtzug in", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    d->outPort = snd_seq_create_simple_port(d->seq, "Nachtzug out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    snd_midi_event_new(256, &d->decoder); snd_midi_event_new(256, &d->encoder);
    snd_midi_event_no_status(d->decoder, 1);   // every message whole, without running status
    if (pipe(d->wake) == 0) { d->run = true; d->th = std::thread([this] { d->loop(); }); }
}
MidiIO::~MidiIO() {
    if (d->run.exchange(false) && d->wake[1] >= 0) { const char c = 1; (void)!write(d->wake[1], &c, 1); }
    if (d->th.joinable()) d->th.join();
    for (int fd : d->wake) if (fd >= 0) close(fd);
    if (d->decoder) snd_midi_event_free(d->decoder);
    if (d->encoder) snd_midi_event_free(d->encoder);
    if (d->seq) snd_seq_close(d->seq);
}
bool MidiIO::ok() const { return d->seq != nullptr; }
QString MidiIO::error() const { return d->err; }
QStringList MidiIO::inputs() const { QStringList l; if (ok()) for (auto& p : d->ports(SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) l << p.name; return l; }
QStringList MidiIO::outputs() const { QStringList l; if (ok()) for (auto& p : d->ports(SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) l << p.name; return l; }
bool MidiIO::listen(const QString& name, Receive onMessage) {
    if (!ok()) return false;
    const auto all = d->ports(SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ);
    std::lock_guard<std::mutex> g(d->mx);
    for (auto [c, p] : d->listening) snd_seq_disconnect_from(d->seq, d->inPort, c, p);
    d->listening.clear();
    { std::lock_guard<std::mutex> cg(d->cbMx); d->receive = std::move(onMessage); }
    if (name.isEmpty()) return true;
    bool any = false;
    for (const auto& p : all) {
        // "*": every device, but not ALSA's own loop-back port, which would echo what Nachtzug sends
        if (name == QLatin1String("*") ? p.name.contains(QLatin1String("Midi Through")) : p.name != name) continue;
        if (snd_seq_connect_from(d->seq, d->inPort, p.client, p.port) >= 0) { d->listening.push_back({p.client, p.port}); any = true; }
    }
    return any || name == QLatin1String("*");
}
bool MidiIO::sendTo(const QString& name) {
    if (!ok()) return false;
    const auto all = d->ports(SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE);
    std::lock_guard<std::mutex> g(d->mx);
    if (d->target.first >= 0) snd_seq_disconnect_to(d->seq, d->outPort, d->target.first, d->target.second);
    d->target = {-1, -1};
    if (name.isEmpty()) return true;
    for (const auto& p : all)
        if (p.name == name && snd_seq_connect_to(d->seq, d->outPort, p.client, p.port) >= 0) { d->target = {p.client, p.port}; return true; }
    return false;
}
bool MidiIO::sending() const { return d->target.first >= 0; }
void MidiIO::send(const uint8_t* b, int n) {
    if (!ok() || n <= 0) return;
    std::lock_guard<std::mutex> g(d->mx);
    if (d->target.first < 0) return;
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_midi_event_reset_encode(d->encoder);
    if (snd_midi_event_encode(d->encoder, b, n, &ev) <= 0 || ev.type == SND_SEQ_EVENT_NONE) return;
    snd_seq_ev_set_source(&ev, d->outPort);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_event_output_direct(d->seq, &ev);
}
#endif
