#pragma once
#include "obj/Object.h"
#include "ui/PanelDir.h"
#include "utl/FilePath.h"
#include "utl/Str.h"

class DirLoader;

class UIPanel : public virtual Hmx::Object {
public:
    enum State {
        kUnloaded = 0,
        kUp = 1,
        kDown = 2,
    };

    UIPanel();
    OBJ_CLASSNAME(UIPanel);
    OBJ_SET_TYPE(UIPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~UIPanel();
    virtual void Load(BinStream &bs) { Hmx::Object::Load(bs); }
    virtual void SetTypeDef(DataArray *);
    virtual class ObjectDir *DataDir();

    virtual void Draw();
    virtual void Enter();
    virtual bool Entering() const;
    virtual void Exit();
    virtual bool Exiting() const;
    virtual bool Unloading() const;
    virtual void Poll();
    virtual void SetPaused(bool paused) { mPaused = paused; }
    virtual void FocusIn() {}
    virtual void FocusOut() {}
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();
    virtual void FinishLoad();

    void CheckLoad();
    void CheckUnload();
    bool CheckIsLoaded();
    void SetLoadedDir(class PanelDir *, bool);
    void UnsetLoadedDir();
    UIComponent *FocusComponent();
    void SetFocusComponent(UIComponent *);
    DataNode OnLoad(DataArray *);

    State GetState() const { return mState; }
    bool Paused() const { return mPaused; }
    bool Showing() const { return mShowing; }
    void SetShowing(bool b) { mShowing = b; }
    bool IsReferenced() const { return mLoadRefs != 0; }
    bool ForceExit() const { return mForceExit; }
    PanelDir *LoadedDir() const { return mDir; }
    int LoadRefs() const { return mLoadRefs; }

    class PanelDir *mDir; // 0x8
    DirLoader *mLoader; // 0xc
    class String mFocusName; // 0x10
    /** The panel's current state. */
    mutable State mState; // 0x1c
    bool mLoaded; // 0x20
    bool mPaused; // 0x21
    bool mShowing; // 0x22
    bool mForceExit; // 0x23
    /** The number of refs to this loaded UIPanel. */
    int mLoadRefs; // 0x24
    FilePath mFilePath; // 0x28
    /** This panel's ID. */
    int mPanelId; // 0x34

    static int sMaxPanelId;
    NEW_OBJ(UIPanel);
    static void Init() { REGISTER_OBJ_FACTORY(UIPanel); }
};
