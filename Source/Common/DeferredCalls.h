//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Todo: improve deferred calls

#pragma once

#include "Common.h"

#include "EngineBase.h"
#include "ScriptSystem.h"

DECLARE_EXCEPTION(DeferredCallException);

struct DeferredCall
{
    ident_t Id {};
    tick_t FireTime {};
    tick_t Delay {};
    ScriptFunc<void> EmptyFunc {};
    ScriptFunc<void, any_t> AnyFunc {};
    ScriptFunc<void, vector<any_t>> AnyArrayFunc {};
    vector<any_t> FuncValue {};
    bool Repeating {};
};

class DeferredCallManager
{
public:
    DeferredCallManager() = delete;
    explicit DeferredCallManager(FOEngineBase* engine);
    DeferredCallManager(const DeferredCallManager&) = delete;
    DeferredCallManager(DeferredCallManager&&) noexcept = delete;
    auto operator=(const DeferredCallManager&) = delete;
    auto operator=(DeferredCallManager&&) noexcept = delete;
    virtual ~DeferredCallManager() = default;

    [[nodiscard]] auto IsDeferredCallPending(ident_t id) const noexcept -> bool;

    auto AddDeferredCall(uint delay, bool repeating, ScriptFunc<void> func) -> ident_t;
    auto AddDeferredCall(uint delay, bool repeating, ScriptFunc<void, any_t> func, any_t value) -> ident_t;
    auto AddDeferredCall(uint delay, bool repeating, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values) -> ident_t;
    auto CancelDeferredCall(ident_t id) -> bool;
    void ProcessDeferredCalls();

protected:
    virtual auto GetNextCallId() -> ident_t;
    virtual void OnDeferredCallRemoved(const DeferredCall& call);

    FOEngineBase* _engine;
    list<DeferredCall> _deferredCalls {};

private:
    auto AddDeferredCall(uint delay, DeferredCall& call) -> ident_t;
    auto RunDeferredCall(DeferredCall& call) const -> bool;

    uint64 _idCounter {};
};
