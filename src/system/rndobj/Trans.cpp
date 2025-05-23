#include "rndobj/Trans.h"
#include "math/Rot.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/ObjPtr_p.h"
#include "obj/Object.h"
#include "obj/PropSync_p.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/TransAnim.h"
#include "rndobj/Utl.h"
#include "utl/Std.h"
#include <algorithm>
#include "utl/Symbols.h"

INIT_REVS(RndTransformable)
Plane RndTransformable::sShadowPlane;

void RndTransformable::Init() {
    Register();
    DataArray *cfg = SystemConfig("rnd");
    cfg->FindData("shadow_plane", sShadowPlane, true);
}

RndTransformable::RndTransformable()
    : mParent(this), mTarget(this), mConstraint(kNone), mPreserveScale(0) {
    mLocalXfm.Reset();
    mWorldXfm.Reset();
    mCache = new DirtyCache();
    mCache->Set((u32)mCache);
}

RndTransformable::~RndTransformable() {
    if (mParent) {
        RemoveSwap(mParent->mChildren, this);
        RemoveSwap(mParent->mCache->mChildren, mCache);
    }
    FOREACH (it, mChildren) {
        (*it)->mParent = nullptr;
        (*it)->mCache->Set(0);
        (*it)->SetDirty();
    }
    delete mCache;
}

void RndTransformable::TransformTransAnims(const Transform &tf) {
    FOREACH_OBJREF (it, this) {
        RndTransAnim *trans = dynamic_cast<RndTransAnim *>((*it)->RefOwner());
        if (trans && trans->Trans() == this) {
            TransformKeys(trans, tf);
        }
    }
}

void RndTransformable::SetTransParent(RndTransformable *newParent, bool recalcLocal) {
    MILO_ASSERT(newParent != this, 0xBB);
    if (mParent == newParent)
        SetDirty();
    else {
        if (recalcLocal) {
            Transform tf48;
            Transform tf78;
            if (mParent)
                tf48 = mParent->WorldXfm();
            else
                tf48.Reset();
            if (newParent)
                tf78 = newParent->WorldXfm();
            else
                tf78.Reset();
            Invert(tf78, tf78);
            Multiply(tf48, tf78, tf78);
            Multiply(mLocalXfm, tf78, mLocalXfm);
            TransformTransAnims(tf78);
        }
        if (mParent) {
            RemoveSwap(mParent->mChildren, this);
            RemoveSwap(mParent->mCache->mChildren, mCache);
        }
        mParent = newParent;
        unsigned int newflags = newParent ? (unsigned int)newParent->mCache : 0;
        mCache->Set(newflags);
        if (newParent) {
            newParent->mChildren.push_back(this);
            newParent->mCache->mChildren.push_back(mCache);
        }
        SetDirty();
    }
}

void RndTransformable::Replace(Hmx::Object *from, Hmx::Object *to) {
    Hmx::Object::Replace(from, to);
    if (mParent == from) {
        SetTransParent(dynamic_cast<RndTransformable *>(to), false);
    }
}

void DirtyCache::SetDirty_Force() {
    SetLastBit(1);
    if (!mChildren.empty()) {
        for (std::vector<DirtyCache *>::iterator it = mChildren.begin();
             it != mChildren.end();
             it++) {
            (*it)->SetDirty();
        }
    }
}

void RndTransformable::SetWorldXfm(const Transform &tf) {
    mWorldXfm = tf;
    mCache->SetLastBit(0);
    UpdatedWorldXfm();
    FOREACH (it, mChildren) {
        (*it)->SetDirty();
    }
}

void RndTransformable::SetWorldPos(const Vector3 &vec) {
    mWorldXfm.v = vec;
    UpdatedWorldXfm();
    FOREACH (it, mChildren) {
        (*it)->SetDirty();
    }
}

Transform &RndTransformable::WorldXfm_Force() {
    START_AUTO_TIMER("updateworldxfm");
    mCache->SetLastBit(0);
    if (!mParent) {
        mWorldXfm = mLocalXfm;
    } else if (mConstraint == kParentWorld) {
        mWorldXfm = mParent->WorldXfm();
    } else if (mConstraint == kLocalRotate) {
        Multiply(mLocalXfm.v, mParent->WorldXfm(), mWorldXfm.v);
        mWorldXfm.m = mLocalXfm.m;
    } else {
        Multiply(mLocalXfm, mParent->WorldXfm(), mWorldXfm);
    }
    if (HasDynamicConstraint())
        ApplyDynamicConstraint();
    else
        UpdatedWorldXfm();
    return mWorldXfm;
}

void RndTransformable::ApplyDynamicConstraint() {
    if (mConstraint == kTargetWorld) {
        mWorldXfm = mTarget->WorldXfm();
    } else if (mConstraint == kShadowTarget) {
        Transform tf40;
        Transpose(mTarget->WorldXfm(), tf40);
        Multiply(mWorldXfm, tf40, mWorldXfm);
        Plane pl50;
        Multiply(sShadowPlane, tf40, pl50);
        float planeB = pl50.b;
        tf40.m.Set(1, -pl50.a / planeB, 0, 0, 0, 0, 0, -pl50.c / planeB, 1);
        tf40.v.Set(0, -pl50.d / pl50.b, 0);
        Multiply(mWorldXfm, tf40, mWorldXfm);
        Multiply(mWorldXfm, mTarget->WorldXfm(), mWorldXfm);
    } else if (RndCam::sCurrent) {
        Vector3 v60;
        const Transform &currentWorld = RndCam::sCurrent->WorldXfm();
        if (mPreserveScale) {
            MakeScale(mWorldXfm.m, v60);
        }
        switch (mConstraint) {
        case kFastBillboardXYZ:
            mWorldXfm.m = currentWorld.m;
            break;
        case kBillboardXYZ:
            Subtract(mWorldXfm.v, currentWorld.v, mWorldXfm.m.y);
            mWorldXfm.m.z = currentWorld.m.z;
            Normalize(mWorldXfm.m, mWorldXfm.m);
            break;
        case kBillboardZ:
            Subtract(mWorldXfm.v, currentWorld.v, mWorldXfm.m.y);
            if (mPreserveScale)
                Normalize(mWorldXfm.m.z, mWorldXfm.m.z);
            Cross(mWorldXfm.m.y, mWorldXfm.m.z, mWorldXfm.m.x);
            Normalize(mWorldXfm.m.x, mWorldXfm.m.x);
            Cross(mWorldXfm.m.z, mWorldXfm.m.x, mWorldXfm.m.y);
            break;
        case kBillboardXZ:
            Subtract(mWorldXfm.v, currentWorld.v, mWorldXfm.m.y);
            Normalize(mWorldXfm.m.y, mWorldXfm.m.y);
            Cross(mWorldXfm.m.y, mWorldXfm.m.z, mWorldXfm.m.x);
            Normalize(mWorldXfm.m.x, mWorldXfm.m.x);
            Cross(mWorldXfm.m.x, mWorldXfm.m.y, mWorldXfm.m.z);
            break;
        case kLookAtTarget:
            Subtract(mTarget->WorldXfm().v, mWorldXfm.v, mWorldXfm.m.y);
            Normalize(mWorldXfm.m, mWorldXfm.m);
            break;
        }
        if (mPreserveScale)
            Scale(v60, mWorldXfm.m, mWorldXfm.m);
    }
    mCache->SetLastBit(1);
}

void RndTransformable::SetTransConstraint(
    Constraint cst, RndTransformable *t, bool preserveScale
) {
    MILO_ASSERT(t != this, 0x1C1);
    mConstraint = cst;
    mPreserveScale = preserveScale;
    mTarget = t;
    SetDirty();
}

namespace {
    bool HorizontalCmp(const RndTransformable *t1, const RndTransformable *t2) {
        return t1->mLocalXfm.v[0] < t2->mLocalXfm.v[0];
    }

    bool VerticalCmp(const RndTransformable *t1, const RndTransformable *t2) {
        return t1->mLocalXfm.v[2] > t2->mLocalXfm.v[2];
    }
}

void RndTransformable::DistributeChildren(bool b, float f) {
    std::vector<RndTransformable *> vec;
    FOREACH (it, mChildren) {
        vec.push_back(*it);
    }
    int count = vec.size();
    if (count < 2)
        return;
    else {
        if (b)
            std::sort(vec.begin(), vec.end(), HorizontalCmp);
        else {
            std::sort(vec.begin(), vec.end(), VerticalCmp);
            f *= -1.0f;
        }
    }
    int idx = b ? 0 : 2;
    float at = vec[0]->mLocalXfm.v[idx];

    for (int i = 1; i < count; i++) {
        Transform t = vec[i]->mLocalXfm;
        t.v[idx] = f * i + at;
        vec[i]->SetLocalXfm(t);
    }
}

BEGIN_COPYS(RndTransformable)
    COPY_VIRTUAL_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndTransformable)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mWorldXfm)
        COPY_MEMBER(mLocalXfm)
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mPreserveScale)
            COPY_MEMBER(mConstraint)
            COPY_MEMBER(mTarget)
        } else if (mConstraint == c->mConstraint) {
            COPY_MEMBER(mTarget)
        }
        SetTransParent(c->mParent, false);
    END_COPYING_MEMBERS
END_COPYS

void RndTransformable::Print() {
    TextStream *ts = &TheDebug;
    *ts << "   localXfm: " << mLocalXfm << "\n";
    *ts << "   worldXfm: " << mWorldXfm << "\n";
    *ts << "   constraint: " << mConstraint << "\n";
    *ts << "   preserveScale: " << mPreserveScale << "\n";
    *ts << "   parent: " << mParent << "\n";
}

void RndTransformable::Highlight() {
    UtilDrawAxes(WorldXfm(), 3.0f, Hmx::Color(1, 1, 1, 1));
}

SAVE_OBJ(RndTransformable, 586)

// retail: https://decomp.me/scratch/PrWG3
BEGIN_LOADS(RndTransformable)
    LOAD_REVS(bs)
    ASSERT_REVS(9, 0)
    if (ClassName() == StaticClassName())
        LOAD_SUPERCLASS(Hmx::Object)
    if (gLoadingProxyFromDisk) {
        Transform t;
        bs >> t >> t;
    } else {
        bs >> mLocalXfm >> mWorldXfm;
    }
    if (gRev < 9) {
        ObjPtrList<RndTransformable> l(this);
        bs >> l;
        FOREACH (it, l) {
            (*it)->SetTransParent(this, false);
        }
    }
    switch (gRev) {
    default:
        bs >> (int &)mConstraint;
        break;
    case 7:
    case 8:
        bs >> (int &)mConstraint;
        unsigned short svar2 = mConstraint;
        if (mConstraint == 4) {
            mConstraint = 0;
        } else if (mConstraint == 2 || mConstraint == 3 || svar2 == 4) {
            mConstraint += 1;
        }
        break;
    case 6:
        bs >> (int &)mConstraint;
        mPreserveScale = mConstraint > kTargetWorld;
        if (mConstraint > 9) {
            mConstraint -= 5;
        } else if (mConstraint > 2) {
            mConstraint -= 1;
        } else if (mConstraint == 2) {
            mConstraint = 2;
        }
        break;
    case 3:
    case 4:
    case 5:
        int unkb0;
        bs >> unkb0;
        mPreserveScale = unkb0 >> 7 & 1;

        switch (unkb0) {
        case 0x4:
        case 0x84:
            mConstraint = 5;
            break;
        case 0x8:
        case 0x88:
            mConstraint = 6;
            break;
        case 0x10:
        case 0x90:
            mConstraint = 7;
            break;
        case 0x20:
        case 0xA0:
            mConstraint = 8;
            break;
        case 0x40:
            mConstraint = 1;
            break;
        default:
            mConstraint = 0;
            break;
        }
        break;
    case 1:
    case 2:
        uint numb4;
        bs >> numb4;
        int sp80[6] = { 0, 0, 0, 5, 6, 7 };
        if (numb4 >= 0x18) {
            mConstraint = 0;
        } else {
            mConstraint = sp80[numb4];
        }
        break;
    case 0:
        break;
    }
    if (gRev != 0 && gRev < 7) {
        Vector3 v;
        bs >> v;
        if (!v.IsZero()) {
            MILO_LOG("Transform origin no longer supported\n");
        }
    }
    if (gRev == 2 || gRev == 3 || gRev == 4) {
        bool b3u;
        bs >> b3u;
    }
    if (gRev == 6 || gRev == 7) {
        Sphere s;
        bs >> s;
        RndDrawable *d = dynamic_cast<RndDrawable *>(this);
        if (d)
            d->SetSphere(s);
    }
    if (gRev > 5) {
        if (gLoadingProxyFromDisk) {
            ObjPtr<RndTransformable> tPtr(this);
            tPtr.Load(bs, false, 0);
        } else
            bs >> mTarget;
    }
    if (gRev > 6)
        bs >> mPreserveScale;
    if (gRev > 8) {
        ObjPtr<RndTransformable> tPtr(this);
        if (!gLoadingProxyFromDisk) {
            bs >> tPtr;
            SetTransParent(tPtr, false);
        } else
            tPtr.Load(bs, false, 0);
    } else if (gRev > 6) {
        ObjPtr<RndTransformable> tPtr(this);
        bs >> tPtr;
        if (tPtr != this) {
            SetTransParent(tPtr, false);
            mConstraint = 2;
        }
    } else if (gRev == 6 && mConstraint == 2) {
        SetTransParent(mTarget, false);
    }
END_LOADS

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(RndTransformable)
    HANDLE(copy_local_to, OnCopyLocalTo)
    HANDLE(set_constraint, OnSetTransConstraint)
    HANDLE(set_local_rot, OnSetLocalRot)
    HANDLE(set_local_rot_index, OnSetLocalRotIndex)
    HANDLE(set_local_rot_mat, OnSetLocalRotMat)
    HANDLE(set_local_pos, OnSetLocalPos)
    HANDLE(set_local_pos_index, OnSetLocalPosIndex)
    HANDLE(get_local_rot, OnGetLocalRot)
    HANDLE(get_local_rot_index, OnGetLocalRotIndex)
    HANDLE(get_local_pos, OnGetLocalPos)
    HANDLE(get_local_pos_index, OnGetLocalPosIndex)
    HANDLE(set_local_scale, OnSetLocalScale)
    HANDLE(set_local_scale_index, OnSetLocalScaleIndex)
    HANDLE(get_local_scale, OnGetLocalScale)
    HANDLE(get_local_scale_index, OnGetLocalScaleIndex)
    HANDLE_ACTION(normalize_local, Normalize(mLocalXfm.m, mLocalXfm.m))
    HANDLE(get_world_forward, OnGetWorldForward)
    HANDLE(get_world_pos, OnGetWorldPos)
    HANDLE(get_world_rot, OnGetWorldRot)
    HANDLE_ACTION(
        set_trans_parent,
        SetTransParent(
            _msg->Obj<RndTransformable>(2), _msg->Size() > 3 ? _msg->Int(3) != 0 : false
        )
    )
    HANDLE_EXPR(trans_parent, (Hmx::Object *)mParent)
    HANDLE_ACTION(reset_xfm, DirtyLocalXfm().Reset())
    HANDLE_ACTION(
        distribute_children, DistributeChildren(_msg->Int(2) != 0, _msg->Float(3))
    )
    HANDLE(get_children, OnGetChildren)
    HANDLE_VIRTUAL_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x357)
END_HANDLERS
#pragma pop

DataNode RndTransformable::OnCopyLocalTo(const DataArray *da) {
    DataArray *arr = da->Array(2);
    for (int i = arr->Size() - 1; i >= 0; i--) {
        RndTransformable *t = arr->Obj<RndTransformable>(i);
        t->SetLocalXfm(LocalXfm());
    }
    return 0;
}

DataNode RndTransformable::OnGetWorldForward(const DataArray *da) {
    *da->Var(2) = WorldXfm().m.y.x;
    *da->Var(3) = WorldXfm().m.y.y;
    *da->Var(4) = WorldXfm().m.y.z;
    return 0;
}

DataNode RndTransformable::OnGetWorldPos(const DataArray *da) {
    *da->Var(2) = WorldXfm().v.x;
    *da->Var(3) = WorldXfm().v.y;
    *da->Var(4) = WorldXfm().v.z;
    return 0;
}

DataNode RndTransformable::OnGetLocalPos(const DataArray *da) {
    *da->Var(2) = LocalXfm().v.x;
    *da->Var(3) = LocalXfm().v.y;
    *da->Var(4) = LocalXfm().v.z;
    return 0;
}

DataNode RndTransformable::OnGetLocalPosIndex(const DataArray *a) {
    MILO_ASSERT(a->Int(2) < 3, 896);
    return LocalXfm().v[a->Int(2)];
}

DataNode RndTransformable::OnGetLocalRot(const DataArray *da) {
    Vector3 v40;
    Hmx::Matrix3 m34(LocalXfm().m);
    Normalize(m34, m34);
    MakeEuler(m34, v40);
    v40 *= RAD2DEG;
    *da->Var(2) = v40.x;
    *da->Var(3) = v40.y;
    *da->Var(4) = v40.z;
    return 0;
}

DataNode RndTransformable::OnGetLocalRotIndex(const DataArray *a) {
    MILO_ASSERT(a->Int(2) < 3, 0x395);
    Vector3 v1c;
    Vector3 v28;
    MakeEulerScale(LocalXfm().m, v1c, v28);
    v1c *= RAD2DEG;
    return v1c[a->Int(2)];
}

DataNode RndTransformable::OnGetWorldRot(const DataArray *da) {
    Vector3 v20;
    MakeEuler(WorldXfm().m, v20);
    v20 *= RAD2DEG;
    *da->Var(2) = v20.x;
    *da->Var(3) = v20.y;
    *da->Var(4) = v20.z;
    return 0;
}

DataNode RndTransformable::OnSetLocalPos(const DataArray *da) {
#ifdef MILO_DEBUG
    SetLocalPos(da->Float(2), da->Float(3), da->Float(4));
#else
    SetLocalPos(Vector3(da->Float(2), da->Float(3), da->Float(4)));
#endif
    return 0;
}

DataNode RndTransformable::OnSetLocalPosIndex(const DataArray *a) {
    MILO_ASSERT(a->Int(2) < 3, 943);
    Vector3 v28(LocalXfm().v);
    v28[a->Int(2)] = a->Float(3);
    SetLocalPos(v28);
    return 0;
}

void RndTransformable::SetLocalRot(Vector3 v) {
    v *= DEG2RAD;
    Hmx::Matrix3 m;
    MakeRotMatrix(v, m, true);
    SetLocalRot(m);
}

DataNode RndTransformable::OnSetLocalRot(const DataArray *da) {
    SetLocalRot(Vector3(da->Float(2), da->Float(3), da->Float(4)));
    return 0;
}

DataNode RndTransformable::OnSetLocalRotIndex(const DataArray *a) {
    MILO_ASSERT(a->Int(2) < 3, 0x3C8);
    Vector3 v5c;
    Vector3 v68;
    MakeEulerScale(LocalXfm().m, v5c, v68);
    v5c[a->Int(2)] = a->Float(3) * DEG2RAD;
    Hmx::Matrix3 m50;
    MakeRotMatrix(v5c, m50, true);
    Scale(v68, m50, m50);
    SetLocalRot(m50);
    return 0;
}

DataNode RndTransformable::OnSetLocalRotMat(const DataArray *da) {
    Hmx::Matrix3 m(
        da->Float(2),
        da->Float(3),
        da->Float(4),
        da->Float(5),
        da->Float(6),
        da->Float(7),
        da->Float(8),
        da->Float(9),
        da->Float(10)
    );
    SetLocalRot(m);
    return 0;
}

DataNode RndTransformable::OnSetLocalScale(const DataArray *da) {
    SetLocalScale(this, Vector3(da->Float(2), da->Float(3), da->Float(4)));
    return 0;
}

DataNode RndTransformable::OnSetLocalScaleIndex(const DataArray *a) {
    MILO_ASSERT(a->Int(2) < 3, 0x3E6);
    Vector3 v28;
    MakeScale(LocalXfm().m, v28);
    v28[a->Int(2)] = a->Float(3);
    SetLocalScale(this, v28);
    return 0;
}

DataNode RndTransformable::OnGetLocalScale(const DataArray *da) {
    Vector3 v20;
    MakeScale(LocalXfm().m, v20);
    *da->Var(2) = v20.x;
    *da->Var(3) = v20.y;
    *da->Var(4) = v20.z;
    return 0;
}

DataNode RndTransformable::OnGetLocalScaleIndex(const DataArray *a) {
    MILO_ASSERT(a->Int(2) < 3, 0x3FE);
    Vector3 v28;
    MakeScale(LocalXfm().m, v28);
    return v28[a->Int(2)];
}

DataNode RndTransformable::OnSetTransConstraint(const DataArray *da) {
    RndTransformable *trans = 0;
    if (da->Size() > 3)
        trans = da->Obj<RndTransformable>(3);
    SetTransConstraint((Constraint)da->Int(2), trans, false);
    return 0;
}

DataNode RndTransformable::OnGetChildren(const DataArray *da) {
    DataArray *arr = new DataArray((int)mChildren.size());
    int idx = 0;
    FOREACH (it, mChildren) {
        arr->Node(idx++) = *it;
    }
    DataNode ret(arr, kDataArray);
    arr->Release();
    return ret;
}

BEGIN_PROPSYNCS(RndTransformable)
    SYNC_PROP_SET(
        trans_parent, mParent, SetTransParent(_val.Obj<RndTransformable>(), true)
    )
    SYNC_PROP_SET(
        trans_constraint,
        mConstraint,
        SetTransConstraint((Constraint)_val.Int(), mTarget, mPreserveScale)
    )
    SYNC_PROP_SET(
        trans_target,
        mTarget.Ptr(),
        SetTransConstraint(
            (Constraint)mConstraint, _val.Obj<RndTransformable>(), mPreserveScale
        )
    )
    SYNC_PROP_SET(
        preserve_scale,
        mPreserveScale,
        SetTransConstraint((Constraint)mConstraint, mTarget, _val.Int())
    )
END_PROPSYNCS
