// OptiBob Live su Windows: prende il suono del PC senza driver (WASAPI "process loopback",
// Windows 10 2004 e successivi). Cattura tutto quello che suonano gli altri programmi,
// ESCLUSO OptiBob stesso (cosi' non c'e' rientro anche se esce sulle stesse casse).
// Stessa interfaccia della versione Mac (Cattura.h): anello + cattura_leggi.
#ifdef _WIN32
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <objidl.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include "Cattura.h"

#ifndef VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK
 #define VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK L"VAD\\Process_Loopback"
#endif
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "ole32.lib")

namespace {
constexpr int kAn = 1 << 16;
std::vector<float> gL(kAn), gR(kAn);
std::atomic<int> gW { 0 }, gRd { 0 };
std::atomic<bool> gOn { false }, gStop { false };
double gSrIn = 48000.0, gFr = 0.0;
std::thread gThr;

struct Fatto : public IActivateAudioInterfaceCompletionHandler, public IAgileObject {
    LONG rif = 1; HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HRESULT esito = E_FAIL; IAudioClient* client = nullptr;
    ~Fatto() { if (ev) CloseHandle(ev); }
    STDMETHODIMP QueryInterface(REFIID r, void** p) override {
        if (r == __uuidof(IUnknown) || r == __uuidof(IActivateAudioInterfaceCompletionHandler))
            *p = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
        else if (r == __uuidof(IAgileObject)) *p = static_cast<IAgileObject*>(this);
        else { *p = nullptr; return E_NOINTERFACE; }
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG) InterlockedIncrement(&rif); }
    STDMETHODIMP_(ULONG) Release() override { const LONG n = InterlockedDecrement(&rif); if (n == 0) delete this; return (ULONG) n; }
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override {
        HRESULT hrA = E_FAIL; IUnknown* u = nullptr;
        esito = op->GetActivateResult(&hrA, &u);
        if (SUCCEEDED(esito)) esito = hrA;
        if (SUCCEEDED(esito) && u != nullptr) esito = u->QueryInterface(__uuidof(IAudioClient), (void**) &client);
        if (u) u->Release();
        SetEvent(ev); return S_OK;
    }
};

// il giro di cattura, nel suo thread. pronto: 1 ok, -1 errore
void giro(std::atomic<int>* pronto, std::string* errore) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    AUDIOCLIENT_ACTIVATION_PARAMS ap {};
    ap.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    ap.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();
    ap.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT pv {}; pv.vt = VT_BLOB; pv.blob.cbSize = sizeof(ap); pv.blob.pBlobData = (BYTE*) &ap;

    auto* h = new Fatto();
    IActivateAudioInterfaceAsyncOperation* op = nullptr;
    HRESULT hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &pv, h, &op);
    if (SUCCEEDED(hr)) { WaitForSingleObject(h->ev, 5000); hr = h->esito; }
    IAudioClient* cl = SUCCEEDED(hr) ? h->client : nullptr;
    if (op) op->Release();
    h->Release();

    WAVEFORMATEX wf {}; wf.wFormatTag = WAVE_FORMAT_PCM; wf.nChannels = 2; wf.nSamplesPerSec = 48000;
    wf.wBitsPerSample = 16; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = 48000 * 4;
    HANDLE evDati = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    IAudioCaptureClient* cap = nullptr;
    if (cl != nullptr) {
        hr = cl->Initialize(AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                            2000000, 0, &wf, nullptr);
        if (SUCCEEDED(hr)) hr = cl->SetEventHandle(evDati);
        if (SUCCEEDED(hr)) hr = cl->GetService(__uuidof(IAudioCaptureClient), (void**) &cap);
        if (SUCCEEDED(hr)) hr = cl->Start();
    }
    if (cl == nullptr || FAILED(hr) || cap == nullptr) {
        *errore = "OptiBob Live could not take the sound of this PC (it needs Windows 10 version 2004 or later).\n\n"
                  "Click Options (top left) and choose as Input the device your playout software uses.";
        pronto->store(-1);
        if (cap) cap->Release(); if (cl) cl->Release(); CloseHandle(evDati); CoUninitialize();
        return;
    }
    gSrIn = 48000.0; gFr = 0.0; gW.store(0); gRd.store(0);
    gOn.store(true); pronto->store(1);

    while (! gStop.load()) {
        WaitForSingleObject(evDati, 100);
        UINT32 n = 0;
        while (SUCCEEDED(cap->GetNextPacketSize(&n)) && n > 0) {
            BYTE* d = nullptr; UINT32 fr = 0; DWORD fl = 0;
            if (FAILED(cap->GetBuffer(&d, &fr, &fl, nullptr, nullptr))) break;
            const auto* s = (const int16_t*) d;
            int w = gW.load(std::memory_order_relaxed);
            for (UINT32 i = 0; i < fr; ++i) {
                const bool zitto = (fl & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                gL[(size_t) w] = zitto ? 0.0f : s[2 * i]     * (1.0f / 32768.0f);
                gR[(size_t) w] = zitto ? 0.0f : s[2 * i + 1] * (1.0f / 32768.0f);
                w = (w + 1) & (kAn - 1);
            }
            gW.store(w, std::memory_order_release);
            cap->ReleaseBuffer(fr);
        }
    }
    gOn.store(false);
    cl->Stop(); cap->Release(); cl->Release(); CloseHandle(evDati);
    CoUninitialize();
}
} // namespace

bool cattura_avvia(std::string& errore) {
    if (gOn.load()) return true;
    cattura_ferma();
    gStop.store(false);
    std::atomic<int> pronto { 0 };
    std::string err;
    gThr = std::thread(giro, &pronto, &err);
    for (int i = 0; i < 80 && pronto.load() == 0; ++i) Sleep(50);
    if (pronto.load() == 1) return true;
    gStop.store(true);
    if (gThr.joinable()) gThr.join();
    errore = err.empty() ? std::string("OptiBob Live could not take the sound of this PC.") : err;
    return false;
}
void cattura_ferma() { gStop.store(true); if (gThr.joinable()) gThr.join(); gOn.store(false); }
bool cattura_attiva() { return gOn.load(); }
std::string cattura_uscitaMac() { return {}; }   // su Windows si resta sull'uscita predefinita

bool cattura_leggi(float* L, float* R, int n, double srOut) {
    if (! gOn.load() || srOut <= 0.0) return false;
    int r = gRd.load(std::memory_order_relaxed);
    const int w = gW.load(std::memory_order_acquire);
    const int tieni = n * 2 + 960;
    if (((w - r) & (kAn - 1)) > tieni + 8192) r = (w - tieni) & (kAn - 1);
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
#endif
