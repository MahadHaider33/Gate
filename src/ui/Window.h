#pragma once
#include "app/Preferences.h"
#include "audio/Engine.h"
#include "ui/Theme.h"
#include "ui/LayoutBatch.h"
#include "ui/ScrollMotion.h"
#include <dwrite.h>
#include <shellapi.h>
#include <commctrl.h>
#include <array>
#include <list>
namespace gate {
class Window {
public:
    explicit Window(HINSTANCE instance);
    int run(int show);
private:
    static constexpr float headerHeight=104,contentHeight=547;
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK contentProcedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK controlProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    LRESULT message(UINT,WPARAM,LPARAM);
    void createControls();
    void layout();
    void positionControls();
    void scrollTo(float position,bool animate=true);
    void animateScroll();
    void stopScroll();
    void moveScroll(float position);
    float maxScroll() const;
    void paint();
    void paintContent();
    void paintControl(HWND,HDC);
    float sliderThumbX(HWND) const;
    void invalidateSliderValue(HWND);
    bool beginControlPaint(HDC,const RECT&);
    void endControlPaint();
    void label(ID2D1RenderTarget*,const std::wstring&,D2D1_RECT_F,float,D2D1_COLOR_F,bool=false,DWRITE_TEXT_ALIGNMENT=DWRITE_TEXT_ALIGNMENT_LEADING,bool=false);
    void drawEmoji(ID2D1RenderTarget*,const std::wstring&,D2D1_RECT_F,float);
    void drawItem(const DRAWITEMSTRUCT& item);
    void refreshStatus();
    void populate(HWND combo,const std::vector<Device>& list,const std::wstring& id);
    void syncTest();
    void updateTheme();
    void hide();
    void trayMenu();
    void updateTimer();
    void save();
    void flushSave();
    enum class Page { Microphone, Voice, Soundboard, Media };
    void createMediaPage();
    void refreshApps();
    void syncMedia();
    void layoutMediaPage(LayoutBatch& batch);
    void paintMediaPage(ID2D1RenderTarget* target);
    bool mediaCommand(unsigned id,unsigned notification);
    bool paintMediaControl(HWND control,ID2D1RenderTarget* target,float width,float height);
    bool isMediaFader(HWND control) const;
    void applyMixLevels();
    void invalidateMediaMeters();
    std::wstring mediaAppName() const;
    HWND mediaNav_{},mediaApp_{},mediaRemove_{},mediaRefresh_{},mediaStart_{},mediaChange_{},mediaVolume_{},mediaMicVolume_{},mediaMicMute_{},mediaMute_{};
    std::vector<HWND> mediaControls_;
    std::vector<AudioApp> mediaApps_;
    unsigned mediaVolumePercent_=80;
    unsigned mediaMicPercent_=100;
    bool mediaMicMuted_=false,mediaMuted_=false;
    void setPage(Page page);
    void createVoicePages();
    void layoutVoicePages(LayoutBatch& batch);
    void paintVoicePages(ID2D1RenderTarget* target);
    void rebuildClips();
    void selectClip(int index);
    void commitClipName();
    void openEmojiPicker();
    void filterEmojis();
    void chooseEmoji();
    static LRESULT CALLBACK emojiProcedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK editorProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    void importClip();
    bool featureCommand(unsigned id,unsigned notification);
    bool featureSlider(HWND control);
    static constexpr int voiceHotkeyId=1,clipHotkeyFirst=1000;
    void registerVoiceShortcut();
    void updateVoiceShortcut();
    void endShortcutCapture(bool restore=true);
    bool captureShortcut(MSG& message);
    void toggleVoiceShortcut();
    void registerClipShortcut(size_t index);
    void updateClipShortcut();
    void triggerClip(size_t index);
    bool acceptShortcut(uint32_t value);
    void advanceShortcut();
    void confirmShortcut();
    void refreshShortcutEditor();
    std::wstring shortcutPromptText() const;
    void measureShortcutMessage();
    float shortcutExtraHeight(bool voice) const;
    GlobalShortcut voiceHotkey_;
    HWND voiceShortcut_{},clearVoiceShortcut_{};
    bool capturingVoiceShortcut_=false;
    enum class ShortcutPrompt {None, TypingKey, Replace};
    ShortcutPrompt shortcutPrompt_=ShortcutPrompt::None;
    uint32_t pendingShortcut_=0;
    int pendingShortcutOwner_=-2,approvedShortcutOwner_=-2;
    bool typingShortcutApproved_=false;
    HWND voiceShortcutConfirm_{},voiceShortcutCancel_{},clipShortcutConfirm_{},clipShortcutCancel_{};
    float shortcutMessageHeight_=0,shortcutMeasuredWidth_=0;
    std::wstring shortcutMeasuredText_;
    std::wstring voiceShortcutError_;
    struct ClipHotkey {GlobalShortcut key;std::wstring error;};
    std::vector<std::unique_ptr<ClipHotkey>> clipHotkeys_;
    HWND clipShortcut_{},clearClipShortcut_{};
    int capturingClipShortcut_=-1;
    void updateFeatureTheme();
    float pageHeight() const;
    unsigned gridColumns() const;
    float clipEditorTop() const;
    bool sideInspector() const;
    float soundGridWidth() const;
    unsigned voiceColumns() const;
    float voiceSettingsTop() const;
    float customControlTop(unsigned index) const;
    void drawIcon(ID2D1RenderTarget* target,unsigned icon,float x,float y,float size,D2D1_COLOR_F color);
    bool isSlider(HWND control) const;
    bool isToggle(HWND control) const;
    bool isTile(HWND control) const;
    bool tileSelected(HWND control) const;
    Page page_=Page::Microphone;
    std::vector<HWND> voiceControls_,soundControls_,clipTiles_,clipSettings_;
    std::array<HWND,voiceCount> voiceTiles_{};
    std::array<HWND,voiceControlCount> customSliders_{};
    HWND customReset_{},customBack_{};
    bool customEditor_=false;
    HWND soundNav_{},effectToggle_{},intensity_{},hearMyself_{};
    HWND import_{},stopSounds_{},hearSounds_{},clipName_{},clipEmoji_{},closeClip_{},removeClip_{};
    HWND emojiPopup_{},emojiSearch_{},emojiList_{};
    HIMAGELIST emojiImages_{};
    unsigned emojiCategory_=0;
    int selectedClip_=-1;
    std::wstring importMessage_;
    std::vector<std::wstring> pendingDeletes_;

    float scale() const {return dpi_/96.f;}
    int px(float v) const {return int(std::lround(v*scale()));}
    float top(float y) const {return y-float(scroll_);}
    HWND hwnd_{},content_{},input_{},listener_{},suppression_{},gate_{},strength_{},threshold_{},test_{},microphoneNav_{},voiceNav_{},setup_{},tooltip_{},hoverControl_{};
    HINSTANCE instance_;
    HFONT font_{},boldFont_{};
    HBRUSH fieldBrush_{};
    unsigned dpi_=96;
    float scroll_=0,scrollTarget_=0,viewportHeight_=0,contentWidth_=0;
    ScrollMotion scrollMotion_;
    HANDLE scrollClock_{};
    double scrollTick_=0;
    struct ScrollControl {HWND control;int x,y;};
    std::vector<ScrollControl> scrollControls_;
    bool scrolling_=false,draggingScroll_=false;
    float scrollGrab_=0;
    float sliderGrab_=0;
    float width_=1100,height_=710,sidebar_=240;
    bool paused_=false,testPending_=false,testRequested_=false,timerActive_=false;
    bool soundboardPrepared_=false;
    Preferences preferences_;
    std::vector<SoundClip> savedClips_;
    Engine engine_;
    EngineStatus state_;
    Theme theme_;
    ComPtr<ID2D1Factory> factory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<ID2D1HwndRenderTarget> contentTarget_;
    ComPtr<ID2D1DCRenderTarget> controlTarget_;
    ComPtr<IDWriteFactory> textFactory_;
    std::array<std::array<ComPtr<IDWriteTextFormat>,2>,65> textFormats_{};
    struct CachedText {
        std::wstring text;
        float width,height,size;
        bool bold,wrap;
        DWRITE_TEXT_ALIGNMENT alignment;
        ComPtr<IDWriteTextLayout> layout;
    };
    // Small LRU, bounded even while resizing or importing large soundboards.
    std::list<CachedText> textLayouts_;
    std::list<CachedText> emojiLayouts_;
    NOTIFYICONDATAW tray_{};
};
}
