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

#pragma once

#include "Common.h"

#include "Geometry.h"

FO_BEGIN_NAMESPACE();

class LineTracer final
{
public:
    explicit LineTracer(mpos start_hex, mpos target_hex, float32 dir_angle_offset, msize map_size);
    explicit LineTracer(mpos start_hex, float32 dir_angle, int32 dist, msize map_size);
    LineTracer(const LineTracer&) = delete;
    LineTracer(LineTracer&&) noexcept = default;
    auto operator=(const LineTracer&) -> LineTracer& = delete;
    auto operator=(LineTracer&&) noexcept -> LineTracer& = delete;
    ~LineTracer() = default;

    auto GetNextHex(mpos& hex) const -> uint8;
    void GetNextSquare(mpos& pos);

private:
    void TraceInit(mpos start_hex, mpos target_hex, float32 dir_angle_offset);

    msize _mapSize {};
    float32 _dirAngle {};
    uint8 _dirLeft {};
    uint8 _dirRight {};
    float32 _xStart {};
    float32 _yStart {};
    float32 _x {};
    float32 _y {};
    float32 _dx {};
    float32 _dy {};
};

FO_END_NAMESPACE();
