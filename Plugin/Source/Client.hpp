/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Client_hpp
#define Client_hpp

#include <JuceHeader.h>

#include "Message.hpp"
#include "ServerPlugin.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"
#include "Metrics.hpp"
#include "ImageReader.hpp"

#include <boost/lockfree/spsc_queue.hpp>

class AudioGridderAudioProcessor;

namespace e47 {

class Client : public Thread, public LogTag, public MouseListener, public KeyListener {
  public:
    Client(AudioGridderAudioProcessor* processor);
    ~Client() override;

    struct Parameter {
        int idx = -1;
        String name;
        float defaultValue = 0.0f;
        AudioProcessorParameter::Category category = AudioProcessorParameter::genericParameter;
        String label;
        int numSteps = 0x7fffffff;
        bool isBoolean = false;
        bool isDiscrete = false;
        bool isMeta = false;
        bool isOrientInv = false;
        StringArray allValues;

        int automationSlot = -1;
        float currentValue = 0.0f;

        NormalisableRange<double> range;

        static Parameter fromJson(const json& j) {
            Parameter p;
            p.idx = j["idx"].get<int>();
            p.name = j["name"].get<std::string>();
            p.defaultValue = j["defaultValue"].get<float>();
            p.category = j["category"].get<AudioProcessorParameter::Category>();
            p.label = j["label"].get<std::string>();
            p.numSteps = j["numSteps"].get<int>();
            p.isBoolean = j["isBoolean"].get<bool>();
            p.isDiscrete = j["isDiscrete"].get<bool>();
            p.isMeta = j["isMeta"].get<bool>();
            p.isOrientInv = j["isOrientInv"].get<bool>();
            String val;
            if (j.find("minValue") != j.end()) {
                val = j["minValue"].get<std::string>();
                if (val.containsOnly("0123456789-.")) {
                    p.range.start = val.getFloatValue();
                }
            }
            if (j.find("maxValue") != j.end()) {
                val = j["maxValue"].get<std::string>();
                if (val.containsOnly("0123456789-.")) {
                    p.range.end = val.getFloatValue();
                }
            }
            if (p.range.start >= p.range.end) {
                p.range.start = 0.0;
                p.range.end = 1.0;
            }
            if (j.find("allValues") != j.end()) {
                for (auto& s : j["allValues"]) {
                    p.allValues.add(s.get<std::string>());
                }
            }
            if (p.allValues.size() > 2) {
                p.range.start = 0.0f;
                p.range.end = p.allValues.size() - 1;
                p.range.interval = 1.0 / p.allValues.size();
            } else if (p.isDiscrete) {
                p.range.interval = 1.0 / p.numSteps;
                if (p.numSteps == 2) {
                    p.isBoolean = true;
                }
            }
            if (j.find("automationSlot") != j.end()) {
                p.automationSlot = j["automationSlot"].get<int>();
            }
            if (j.find("currentValue") != j.end()) {
                p.currentValue = j["currentValue"].get<float>();
            } else {
                p.currentValue = p.defaultValue;
            }
            return p;
        }

        json toJson() const {
            json j = {{"idx", idx},
                      {"name", name.toStdString()},
                      {"defaultValue", defaultValue},
                      {"currentValue", currentValue},
                      {"category", category},
                      {"label", label.toStdString()},
                      {"numSteps", numSteps},
                      {"isBoolean", isBoolean},
                      {"isDiscrete", isDiscrete},
                      {"isMeta", isMeta},
                      {"isOrientInv", isOrientInv},
                      {"minValue", String(range.start).toStdString()},
                      {"maxValue", String(range.end).toStdString()},
                      {"automationSlot", automationSlot}};
            j["allValues"] = json::array();
            for (auto& s : allValues) {
                j["allValues"].push_back(s.toStdString());
            }
            return j;
        }

        float getValue() const { return (float)range.convertFrom0to1(currentValue); }

        void setValue(float val) { currentValue = (float)range.convertTo0to1(val); }
    };

    std::atomic_int NUM_OF_BUFFERS{DEFAULT_NUM_OF_BUFFERS};
    std::atomic_int LOAD_PLUGIN_TIMEOUT{DEFAULT_LOAD_PLUGIN_TIMEOUT};

    void run() override;

    void setServer(const ServerInfo& srv);
    String getServerHost();
    String getServerHostAndID();
    int getServerPort();
    int getServerID();
    int getChannelsIn() const { return m_channelsIn; }
    int getChannelsOut() const { return m_channelsOut; }
    double getSampleRate() const { return m_rate; }
    int getSamplesPerBlock() const { return m_samplesPerBlock; }
    int getLatencySamples() const { return m_latency + NUM_OF_BUFFERS * m_samplesPerBlock; }

    bool isReady(int timeout = 1000);
    bool isReadyLockFree();
    void init(int channelsIn, int channelsOut, double rate, int samplesPerBlock, bool doublePrecission);

    void reconnect() { m_needsReconnect = true; }
    void close();

    bool audioLock() {
        int retry = 10;
        bool locked;
        while ((locked = m_audioMtx.try_lock()) == false && --retry > 0) {
            sleep(1);
        }
        if (locked) {
            if (isReadyLockFree()) {
                return true;
            }
            m_audioMtx.unlock();
        } else {
            logln("warning: failed to lock audio stream, this will cause audio stutter");
        }
        return false;
    }

    void audioUnlock() { m_audioMtx.unlock(); }

    void sendAudioMessage(AudioBuffer<float>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        m_audioStreamerF->send(buffer, midi, posInfo);
    }

    void sendAudioMessage(AudioBuffer<double>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        m_audioStreamerD->send(buffer, midi, posInfo);
    }

    void readAudioMessage(AudioBuffer<float>& buffer, MidiBuffer& midi) { m_audioStreamerF->read(buffer, midi); }

    void readAudioMessage(AudioBuffer<double>& buffer, MidiBuffer& midi) { m_audioStreamerD->read(buffer, midi); }

    const auto& getPlugins() const { return m_plugins; }
    Image getPluginScreen();  // create copy
    void setPluginScreen(std::shared_ptr<Image> img, int w, int h);

    using ScreenUpdateCallback = std::function<void(std::shared_ptr<Image>, int, int)>;
    void setPluginScreenUpdateCallback(ScreenUpdateCallback fn);

    using OnConnectCallback = std::function<void()>;
    void setOnConnectCallback(OnConnectCallback fn);

    using OnCloseCallback = std::function<void()>;
    void setOnCloseCallback(OnCloseCallback fn);

    bool addPlugin(String id, StringArray& presets, Array<Parameter>& params, String settings = "");
    void delPlugin(int idx);
    void editPlugin(int idx);
    void hidePlugin();
    MemoryBlock getPluginSettings(int idx);
    void setPluginSettings(int idx, String settings);
    void bypassPlugin(int idx);
    void unbypassPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    std::vector<ServerPlugin> getRecents();
    void setPreset(int idx, int preset);

    float getParameterValue(int idx, int paramIdx);
    void setParameterValue(int idx, int paramIdx, float val);

    struct ParameterResult {
        int idx;
        float value;
    };

    Array<ParameterResult> getAllParameterValues(int idx, int count);

    void updateScreenCaptureArea(int val);

    void rescan(bool wipe = false);

    void updateCPULoad();
    float getCPULoad() const { return m_srvLoad; }

    // MouseListener
    void mouseMove(const MouseEvent& event) override;
    void mouseEnter(const MouseEvent& event) override;
    void mouseExit(const MouseEvent& /* event */) override {}
    void mouseDown(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;
    void mouseDoubleClick(const MouseEvent& event) override;
    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;

    void sendMouseEvent(MouseEvType ev, Point<float> p, bool isShiftDown, bool isCtrlDown, bool isAltDown,
                        const MouseWheelDetails* wheel = nullptr);

    // KeyListener
    bool keyPressed(const KeyPress& kp, Component* originatingComponent) override;

  private:
    AudioGridderAudioProcessor* m_processor;
    std::mutex m_srvMtx;
    String m_srvHost = "";
    int m_srvPort = DEFAULT_SERVER_PORT;
    int m_srvId = 0;
    float m_srvLoad = 0.0f;
    bool m_needsReconnect = false;
    double m_rate = 0;
    bool m_doublePrecission = false;

    std::atomic_int m_channelsIn{0};
    std::atomic_int m_channelsOut{0};
    std::atomic_int m_samplesPerBlock{0};
    std::atomic_int m_latency{0};

    std::atomic_bool m_ready{false};
    std::atomic_bool m_error{false};

    std::mutex m_clientMtx;
    int m_clientMtxId = 0;
    std::unique_ptr<StreamingSocket> m_cmd_socket;
    std::unique_ptr<StreamingSocket> m_screen_socket;
    std::vector<ServerPlugin> m_plugins;

    class ScreenReceiver : public Thread, public LogTagDelegate {
      public:
        ScreenReceiver(Client* clnt, StreamingSocket* sock) : Thread("ScreenWorker"), m_client(clnt), m_socket(sock) {
            setLogTagSource(clnt);
            m_imgReader.setLogTagSource(clnt);
        }
        ~ScreenReceiver() {
            signalThreadShouldExit();
            waitForThreadAndLog(m_client, this, 1000);
        }
        void run();

      private:
        Client* m_client;
        StreamingSocket* m_socket;
        std::shared_ptr<Image> m_image;
        ImageReader m_imgReader;
    };

    friend ScreenReceiver;

    std::unique_ptr<ScreenReceiver> m_screenWorker;
    std::shared_ptr<Image> m_pluginScreen;
    ScreenUpdateCallback m_pluginScreenUpdateCallback;
    std::mutex m_pluginScreenMtx;

    OnConnectCallback m_onConnectCallback;
    OnCloseCallback m_onCloseCallback;

    enum LockID {
        NOLOCK,
        SETPLUGINSCREENUPDATECALLBACK,
        SETONCONNECTCALLBACK,
        SETONCLOSECALLBACK,
        INIT1,
        INIT2,
        CLOSE,
        ADDPLUGIN,
        DELPLUGIN,
        EDITPLUGIN,
        HIDEPLUGIN,
        GETPLUGINSETTINGS,
        SETPLUGINSETTINGS,
        BYPASSPLUGIN,
        UNBYPASSPLUGIN,
        EXCHANGEPLUGINS,
        GETRECENTS,
        SETPRESET,
        GETPARAMETERVALUE,
        SETPARAMETERVALUE,
        GETALLPARAMETERVALUES,
        SENDMOUSEEVENT,
        KEYPRESSED,
        UPDATESCREENCAPTUREAREA,
        RESCAN,
        UPDATECPULOAD,
        GETLOADEDPLUGINSSTRING
    };

    struct LockByID {
        Client& client;
        LockID lockid;
        LockByID(Client& c, LockID id, bool enforce = true) : client(c), lockid(id) {
            if (enforce) {
                lock();
            } else {
                tryLock();
            }
        }
        ~LockByID() { unlock(); }
        inline void lock() {
            client.m_clientMtx.lock();
            client.m_clientMtxId = lockid;
        }
        inline void tryLock() {
            if (client.m_clientMtx.try_lock()) {
                client.m_clientMtxId = lockid;
            }
        }
        inline void unlock() {
            client.m_clientMtxId = NOLOCK;
            client.m_clientMtx.unlock();
        }
    };

    void quit();
    void init();

    StreamingSocket* accept(StreamingSocket& sock) const;

    String getLoadedPluginsString();

#include "AudioStreamer.hpp"

    std::mutex m_audioMtx;
    std::unique_ptr<AudioStreamer<float>> m_audioStreamerF;
    std::unique_ptr<AudioStreamer<double>> m_audioStreamerD;

    bool audioConnectionOk() {
        std::lock_guard<std::mutex> lock(m_audioMtx);
        return (nullptr != m_audioStreamerF && m_audioStreamerF->isOk()) ||
               (nullptr != m_audioStreamerD && m_audioStreamerD->isOk());
    }
};

}  // namespace e47

#endif /* Client_hpp */
