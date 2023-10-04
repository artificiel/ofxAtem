#pragma once

#include "BMDSwitcherAPI.h"
#include <atomic>
#include <map>
#include <string>

namespace ofxAtem {
    class Controller;
    
    class SwitcherMonitor : public IBMDSwitcherCallback
    {
        inline static const std::map<BMDSwitcherEventType, std::string> kSwitcherEventType =
        {
            { bmdSwitcherEventTypeVideoModeChanged ,                "Video Mode Changed"},
            { bmdSwitcherEventTypeMethodForDownConvertedSDChanged,  "Method for DownConverter Changed"},
            { bmdSwitcherEventTypeDownConvertedHDVideoModeChanged, "DownConverter HD Video Mode Changed"},
            { bmdSwitcherEventTypeMultiViewVideoModeChanged, "MultiVideo Mode Changed"},
            { bmdSwitcherEventTypePowerStatusChanged, "Power Status Changed"},
            { bmdSwitcherEventTypeDisconnected, "Disconnected"},
            { bmdSwitcherEventType3GSDIOutputLevelChanged        , "3G SDI Output Level Changed"},
            { bmdSwitcherEventTypeTimeCodeChanged                   , "TimeCode Changed"},
            { bmdSwitcherEventTypeTimeCodeLockedChanged          , "Timecode Locked Changed"},
            { bmdSwitcherEventTypeTimeCodeModeChanged         , "Timecode Mode Changed"},
            { bmdSwitcherEventTypeSuperSourceCascadeChanged, "SuperSource Cascade Changed"},
            { bmdSwitcherEventTypeAutoVideoModeChanged, "Auto Video Mode Changed"},
            { bmdSwitcherEventTypeAutoVideoModeDetectedChanged , "AutoVideo Mode Detected Changed"}
        };
    public:
        SwitcherMonitor(Controller* uiDelegate) :	mUiDelegate(uiDelegate), mRefCount(1){};
        
        auto getEventTypeName(BMDSwitcherEventType event) {
            if (kSwitcherEventType.count(event)) {
                return kSwitcherEventType.at(event);
            } else {
                return std::string{"unknown event type"};
            }
        }
        
    protected:
        virtual ~SwitcherMonitor(){};
        
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
        ULONG STDMETHODCALLTYPE AddRef(void) override;
        ULONG STDMETHODCALLTYPE Release(void) override;
        HRESULT STDMETHODCALLTYPE Notify (BMDSwitcherEventType eventType,  BMDSwitcherVideoMode coreVideoMode) override;

    private:
        Controller*	mUiDelegate;
        std::atomic<std::int32_t> mRefCount;
    };
}

