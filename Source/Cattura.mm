#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreAudio/CoreAudio.h>
#if __has_include(<CoreAudio/CATapDescription.h>)
  #import <CoreAudio/CATapDescription.h>
  #import <CoreAudio/AudioHardwareTapping.h>
  #define OB_TAPS 1
#else
  #define OB_TAPS 0
#endif
#include "Cattura.h"
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <atomic>
#include <vector>

namespace {
AudioObjectID gTap = kAudioObjectUnknown;
AudioObjectID gAgg = kAudioObjectUnknown;
AudioDeviceIOProcID gProc = nullptr;
std::atomic<bool> gOn { false };
double gSrIn = 48000.0, gFr = 0.0;
constexpr int kAn = 1 << 16;
std::vector<float> gL(kAn), gR(kAn);
std::atomic<int> gW { 0 }, gRd { 0 };
AudioObjectID gRipristina = kAudioObjectUnknown;   // macOS vecchi: l'uscita vera da rimettere

// arriva l'audio del Mac: nell'anello
OSStatus letturaTap(AudioObjectID, const AudioTimeStamp*, const AudioBufferList* in,
                    const AudioTimeStamp*, AudioBufferList*, const AudioTimeStamp*, void*) {
    if (in == nullptr || in->mNumberBuffers == 0) return noErr;
    const AudioBuffer& b0 = in->mBuffers[0];
    const bool sep = in->mNumberBuffers >= 2;
    const int ch = sep ? 1 : (int) (b0.mNumberChannels > 0 ? b0.mNumberChannels : 1);
    const int n = (int) (b0.mDataByteSize / sizeof(float)) / ch;
    const float* l = (const float*) b0.mData;
    const float* r = sep ? (const float*) in->mBuffers[1].mData : (ch >= 2 ? l + 1 : l);
    int w = gW.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) { gL[(size_t) w] = l[i * ch]; gR[(size_t) w] = r[i * ch]; w = (w + 1) & (kAn - 1); }
    gW.store(w, std::memory_order_release);
    return noErr;
}

// la presa ZITTISCE l'uscita normale: se resta appesa il Mac e' muto. Si smonta sempre.
void smonta() {
    gOn.store(false);
    if (gRipristina != kAudioObjectUnknown) {             // rimette l'uscita del Mac com'era
        AudioObjectPropertyAddress ad = { kAudioHardwarePropertyDefaultOutputDevice,
                                          kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectSetPropertyData(kAudioObjectSystemObject, &ad, 0, nullptr, sizeof(gRipristina), &gRipristina);
        gRipristina = kAudioObjectUnknown;
    }
    if (gProc != nullptr && gAgg != kAudioObjectUnknown) {
        AudioDeviceStop(gAgg, gProc); AudioDeviceDestroyIOProcID(gAgg, gProc); gProc = nullptr; }
    if (gAgg != kAudioObjectUnknown) { AudioHardwareDestroyAggregateDevice(gAgg); gAgg = kAudioObjectUnknown; }
   #if OB_TAPS
    if (gTap != kAudioObjectUnknown) {
        if (@available(macOS 14.2, *)) AudioHardwareDestroyProcessTap(gTap);
        gTap = kAudioObjectUnknown;
    }
   #endif
}
void alSegnale(int s) { smonta(); signal(s, SIG_DFL); raise(s); }
void alla_fine() { smonta(); }
void armaLeDifese() {
    static bool fatto = false;
    if (fatto) return;
    fatto = true;
    atexit(alla_fine);
    for (int s : { SIGTERM, SIGINT, SIGHUP, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT })
        signal(s, alSegnale);
}

AudioObjectID uscitaPredefinita() {
    AudioObjectID d = kAudioObjectUnknown; UInt32 sz = sizeof(d);
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice,
                                     kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &sz, &d);
    return d;
}
NSString* uidDi(AudioObjectID dev) {
    CFStringRef uid = nullptr; UInt32 sz = sizeof(uid);
    AudioObjectPropertyAddress a = { kAudioDevicePropertyDeviceUID,
                                     kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData(dev, &a, 0, nullptr, &sz, &uid) != noErr || uid == nullptr) return nil;
    return (__bridge NSString*) uid;
}
AudioObjectID daPid(pid_t pid) {
    AudioObjectID o = kAudioObjectUnknown; UInt32 sz = sizeof(o);
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyTranslatePIDToProcessObject,
                                     kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, sizeof(pid), &pid, &sz, &o) != noErr)
        return kAudioObjectUnknown;
    return o;
}
// fuori dalla presa: noi stessi e ogni altro OptiBob (Player compreso), sennò si innesca un giro chiuso
NSArray* processiOptiBob() {
    NSMutableArray* fuori = [NSMutableArray array];
    AudioObjectID me = daPid(getpid());
    if (me != kAudioObjectUnknown) [fuori addObject:@(me)];
    for (NSRunningApplication* app in [[NSWorkspace sharedWorkspace] runningApplications]) {
        NSString* b = app.bundleIdentifier;
        if (b == nil || ! [b hasPrefix:@"com.optibob"] || app.processIdentifier == getpid()) continue;
        AudioObjectID o = daPid(app.processIdentifier);
        if (o != kAudioObjectUnknown) [fuori addObject:@(o)];
    }
    return fuori;
}
} // namespace

bool cattura_avvia(std::string& errore) {
   #if OB_TAPS
    if (gOn.load()) return true;
    if (@available(macOS 14.4, *)) {
        smonta();
        armaLeDifese();
        CATapDescription* d = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:processiOptiBob()];
        d.name = @"OptiBob Live";
        d.privateTap = YES;
        d.muteBehavior = CATapMuted;      // il suono passa SOLO da OptiBob, niente doppio
        if (AudioHardwareCreateProcessTap(d, &gTap) != noErr) {
            gTap = kAudioObjectUnknown;
            errore = "OptiBob Live needs permission to use the sound your Mac plays.\n\n"
                     "System Settings > Privacy & Security > Screen & System Audio Recording: "
                     "turn on OptiBob Live, then quit and reopen it.";
            return false;
        }
        NSDictionary* agg = @{
            @kAudioAggregateDeviceNameKey:         [NSString stringWithFormat:@"OptiBob Live %d", getpid()],
            @kAudioAggregateDeviceUIDKey:          [NSString stringWithFormat:@"com.optibob.live.presa.%@", [[NSUUID UUID] UUIDString]],
            @kAudioAggregateDeviceIsPrivateKey:    @YES,
            @kAudioAggregateDeviceIsStackedKey:    @NO,
            @kAudioAggregateDeviceTapAutoStartKey: @YES,
            @kAudioAggregateDeviceSubDeviceListKey: @[],
            @kAudioAggregateDeviceTapListKey:      @[ @{ @kAudioSubTapUIDKey: [d.UUID UUIDString] } ],
        };
        if (AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef) agg, &gAgg) != noErr) {
            gAgg = kAudioObjectUnknown; smonta();
            errore = "Could not read the Mac's sound. Close every OptiBob window and try again."; return false;
        }
        Float64 sr = 48000.0; UInt32 sz = sizeof(sr);
        AudioObjectPropertyAddress asr = { kAudioDevicePropertyNominalSampleRate,
                                           kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectGetPropertyData(gAgg, &asr, 0, nullptr, &sz, &sr);
        gSrIn = sr > 0 ? sr : 48000.0; gFr = 0.0;
        gW.store(0); gRd.store(0);
        if (AudioDeviceCreateIOProcID(gAgg, letturaTap, nullptr, &gProc) != noErr
            || AudioDeviceStart(gAgg, gProc) != noErr) {
            smonta(); errore = "Could not start reading the Mac's sound."; return false;
        }
        gOn.store(true);
        return true;
    }
   #endif
    errore = "Taking the Mac's sound needs macOS 14.4 or later.";
    return false;
}

void cattura_ferma()   { smonta(); }
bool cattura_attiva()  { return gOn.load(); }

// dal giro audio di JUCE: pesca n campioni dall'anello, con la conversione di frequenza
bool cattura_leggi(float* L, float* R, int n, double srOut) {
    if (! gOn.load() || srOut <= 0.0) return false;
    int r = gRd.load(std::memory_order_relaxed);
    const int w = gW.load(std::memory_order_acquire);
    const int tieni = n * 2 + 512;                       // ritardo che teniamo
    if (((w - r) & (kAn - 1)) > tieni + 8192) r = (w - tieni) & (kAn - 1);   // troppo indietro: si recupera
    const double passo = gSrIn / srOut;
    double fr = gFr;
    for (int i = 0; i < n; ++i) {
        if (((w - r) & (kAn - 1)) < 2) { L[i] = 0.0f; R[i] = 0.0f; continue; }
        const int r2 = (r + 1) & (kAn - 1);
        L[i] = (float) (gL[(size_t) r] + (gL[(size_t) r2] - gL[(size_t) r]) * fr);
        R[i] = (float) (gR[(size_t) r] + (gR[(size_t) r2] - gR[(size_t) r]) * fr);
        fr += passo;
        while (fr >= 1.0) { fr -= 1.0; r = (r + 1) & (kAn - 1); }
    }
    gFr = fr;
    gRd.store(r, std::memory_order_relaxed);
    return true;
}

std::string cattura_uscitaMac() {
    const AudioObjectID d = uscitaPredefinita();
    if (d == kAudioObjectUnknown) return {};
    CFStringRef nome = nullptr; UInt32 sz = sizeof(nome);
    AudioObjectPropertyAddress a = { kAudioObjectPropertyName,
                                     kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData(d, &a, 0, nullptr, &sz, &nome) != noErr || nome == nullptr) return {};
    std::string r = [(__bridge NSString*) nome UTF8String];
    CFRelease(nome);
    return r;
}

// ---------------------------------------------------------------- macOS prima del 14.4: ponte BlackHole
namespace {
std::string nomeDi(AudioObjectID d) {
    CFStringRef n = nullptr; UInt32 sz = sizeof(n);
    AudioObjectPropertyAddress a = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData(d, &a, 0, nullptr, &sz, &n) != noErr || n == nullptr) return {};
    std::string r = [(__bridge NSString*) n UTF8String]; CFRelease(n); return r;
}
bool haUscite(AudioObjectID d) {
    UInt32 sz = 0;
    AudioObjectPropertyAddress a = { kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };
    return AudioObjectGetPropertyDataSize(d, &a, 0, nullptr, &sz) == noErr && sz > 0;
}
std::vector<AudioObjectID> tutti() {
    UInt32 sz = 0;
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &sz) != noErr) return {};
    std::vector<AudioObjectID> v(sz / sizeof(AudioObjectID));
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &sz, v.data());
    return v;
}
}

bool ponte_blackhole(std::string& uscitaVera) {
    AudioObjectID bh = kAudioObjectUnknown;
    for (auto d : tutti()) if (nomeDi(d).rfind("BlackHole", 0) == 0 && haUscite(d)) { bh = d; break; }
    if (bh == kAudioObjectUnknown) return false;
    AudioObjectID vera = uscitaPredefinita();
    if (vera == bh || vera == kAudioObjectUnknown) {        // gia' su BlackHole (es. dopo un crash): prende un'uscita vera
        vera = kAudioObjectUnknown;
        for (auto d : tutti()) {
            const auto n = nomeDi(d);
            if (d != bh && haUscite(d) && n.rfind("BlackHole", 0) != 0 && n.find("OptiBob") == std::string::npos) { vera = d; break; }
        }
        if (vera == kAudioObjectUnknown) return false;
    }
    uscitaVera = nomeDi(vera);
    gRipristina = vera;
    armaLeDifese();
    AudioObjectPropertyAddress ad = { kAudioHardwarePropertyDefaultOutputDevice,
                                      kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioObjectSetPropertyData(kAudioObjectSystemObject, &ad, 0, nullptr, sizeof(bh), &bh);
    return true;
}
std::string ponte_nome() {
    for (auto d : tutti()) { const auto n = nomeDi(d); if (n.rfind("BlackHole", 0) == 0 && haUscite(d)) return n; }
    return {};
}
