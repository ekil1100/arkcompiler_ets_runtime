/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ECMASCRIPT_SERIALIZER_BASE_DESERIALIZER_H
#define ECMASCRIPT_SERIALIZER_BASE_DESERIALIZER_H

#include "ecmascript/js_serializer.h"
#include "ecmascript/serializer/serialize_data.h"

namespace panda::ecmascript {
class Heap;
class JSThread;
struct NewConstPoolInfo {
    JSPandaFile *jsPandaFile_ {nullptr};
    panda_file::File::EntityId methodId_;
    uintptr_t slotAddr_ {0U};

    NewConstPoolInfo(JSPandaFile *jsPandaFile, panda_file::File::EntityId methodId)
        : jsPandaFile_(jsPandaFile), methodId_(methodId) {}
};

struct NativeBindingInfo {
    AttachFunc af_ {nullptr};
    void *bufferPointer_ {nullptr};
    void *hint_ = {nullptr};
    void *attachData_ = {nullptr};
    ObjectSlot slot_;
    bool root_ {false};

    NativeBindingInfo(AttachFunc af, void *bufferPointer, void *hint, void *attachData, ObjectSlot slot, bool root)
        : af_(af), bufferPointer_(bufferPointer), hint_(hint), attachData_(attachData), slot_(slot), root_(root) {}
};

struct JSErrorInfo {
    uint8_t errorType_ {0};
    JSTaggedValue errorMsg_;
    ObjectSlot slot_;
    bool root_ {false};

    JSErrorInfo(uint8_t errorType, JSTaggedValue errorMsg, ObjectSlot slot, bool root)
        : errorType_(errorType), errorMsg_(errorMsg), slot_(slot), root_(root) {}
};

class BaseDeserializer {
public:
    explicit BaseDeserializer(JSThread *thread, SerializeData *data, void *hint = nullptr)
        : thread_(thread), heap_(const_cast<Heap *>(thread->GetEcmaVM()->GetHeap())), data_(data), engine_(hint) {}
    ~BaseDeserializer()
    {
        data_->ResetPosition();
        objectVector_.clear();
        regionVector_.clear();
    }

    NO_COPY_SEMANTIC(BaseDeserializer);
    NO_MOVE_SEMANTIC(BaseDeserializer);

    JSHandle<JSTaggedValue> ReadValue();

private:
    JSHandle<JSTaggedValue> DeserializeJSTaggedValue();
    uintptr_t DeserializeTaggedObject(SerializedObjectSpace space);
    void DeserializeConstPool(NewConstPoolInfo *info);
    void DeserializeNativeBindingObject(NativeBindingInfo *info);
    void DeserializeJSError(JSErrorInfo *info);
    uintptr_t RelocateObjectAddr(SerializedObjectSpace space, size_t objSize);
    JSTaggedType RelocateObjectProtoAddr(uint8_t objectType);
    void DeserializeObjectField(uintptr_t start, uintptr_t end);
    size_t ReadSingleEncodeData(uint8_t encodeFlag, uintptr_t addr, bool isRoot = false);
    void HandleNewObjectEncodeFlag(SerializedObjectSpace space, ObjectSlot slot, bool isRoot);
    void HandleMethodEncodeFlag();

    void TransferArrayBufferAttach(uintptr_t objAddr);
    void ResetNativePointerBuffer(uintptr_t objAddr, void *bufferPointer);
    void ResetMethodConstantPool(uintptr_t objAddr, ConstantPool *constpool);

    void AllocateToDifferentSpaces();
    void AllocateMultiRegion(SparseSpace *space, size_t spaceObjSize, size_t &regionIndex);
    void AllocateToOldSpace(size_t oldSpaceSize);
    void AllocateToNonMovableSpace(size_t nonMovableSpaceSize);
    void AllocateToMachineCodeSpace(size_t machineCodeSpaceSize);
    void UpdateBarrier(uintptr_t addr, ObjectSlot slot);

    bool GetAndResetWeak()
    {
        bool isWeak = isWeak_;
        if (isWeak_) {
            isWeak_ = false;
        }
        return isWeak;
    }

    bool GetAndResetTransferBuffer()
    {
        bool isTransferArrayBuffer = isTransferArrayBuffer_;
        if (isTransferArrayBuffer_) {
            isTransferArrayBuffer_ = false;
        }
        return isTransferArrayBuffer;
    }

    bool GetAndResetNeedNewConstPool()
    {
        bool needNewConstPool = needNewConstPool_;
        if (needNewConstPool_) {
            needNewConstPool_ = false;
        }
        return needNewConstPool;
    }

    bool GetAndResetIsErrorMsg()
    {
        bool isErrorMsg = isErrorMsg_;
        if (isErrorMsg_) {
            isErrorMsg_ = false;
        }
        return isErrorMsg;
    }

    void *GetAndResetBufferPointer()
    {
        if (bufferPointer_) {
            void *buffer = bufferPointer_;
            bufferPointer_ = nullptr;
            return buffer;
        }
        return nullptr;
    }

    ConstantPool *GetAndResetConstantPool()
    {
        if (constpool_) {
            ConstantPool *constpool = constpool_;
            constpool_ = nullptr;
            return constpool;
        }
        return nullptr;
    }

    void UpdateMaybeWeak(ObjectSlot slot, uintptr_t addr, bool isWeak)
    {
        isWeak ? slot.UpdateWeak(addr) : slot.Update(addr);
    }

private:
    JSThread *thread_;
    Heap *heap_;
    SerializeData* data_;
    void *engine_;
    uintptr_t oldSpaceBeginAddr_ {0};
    uintptr_t nonMovableSpaceBeginAddr_ {0};
    uintptr_t machineCodeSpaceBeginAddr_ {0};
    CVector<uintptr_t> objectVector_;
    CVector<Region *> regionVector_;
    size_t oldRegionIndex_ {0};
    size_t nonMovableRegionIndex_ {0};
    size_t machineCodeRegionIndex_ {0};
    size_t regionRemainSizeIndex_ {0};
    bool isWeak_ {false};
    bool isTransferArrayBuffer_ {false};
    bool isErrorMsg_ {false};
    void *bufferPointer_ {nullptr};
    ConstantPool *constpool_ {nullptr};
    bool needNewConstPool_ {false};
    CVector<NewConstPoolInfo *> newConstPoolInfos_;
    CVector<NativeBindingInfo *> nativeBindingInfos_;
    CVector<JSErrorInfo *> jsErrorInfos_;
    CVector<JSFunction *> concurrentFunctions_;
};
}

#endif  // ECMASCRIPT_SERIALIZER_BASE_DESERIALIZER_H