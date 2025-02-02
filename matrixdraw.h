//+--------------------------------------------------------------------------
//
// File:        SocketServer.h
//
// NightDriverPi - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
//
// Description:
//
//    Hosts a socket server on port 49152 to receive LED data from the master
//
// History:     Aug-14-2024     Davepl      Created from NightDriverStrip
//---------------------------------------------------------------------------
#pragma once

#include "led-matrix.h"
#include "ledbuffer.h"
#include <thread>
#include <chrono>

using rgb_matrix::Canvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

extern volatile bool interrupt_received;

class MatrixDraw
{
    static double _FPS;

  protected:
	
    // DrawFrame
    //
    // Sends a frame's worth of color data to the matrix and then swaps it on the next VSync
	
    static void DrawFrame(std::unique_ptr<LEDBuffer> & buffer, RGBMatrix & matrix)
    {
        static double lastTime = 0.0;
        double currentTime = CAppTime::CurrentTime();
        double delta = currentTime - lastTime + DBL_EPSILON;        // Add epsilon to avoid divide by zero
        lastTime = currentTime;
        _FPS =  1.0 / delta;

        // TODO: This code could center a smaller buffer on the matrix or scale it up to fill the matrix
        //       if the matrix is larger than the frame, but for now, we just require that the frames being
        //       sent are the same size as the matrix.

        if (buffer->ColorData().size() != (size_t) matrix.width() * matrix.height())
            throw std::runtime_error("Size mismatch between data and matrix.");
            
        for (int x = 0; x < matrix.width(); x++)
        {
            for (int y = 0; y < matrix.height(); y++)
            {
                CRGB color = buffer->ColorData()[y * matrix.width() + x];
                matrix.SetPixel(matrix.width() - 1 - x, y, color.r, color.g, color.b);
            }
        }
        //matrix.SwapOnVSync(nullptr);

    }

  public:

    // RunDrawLoop
    // 
    // Loops looking for frames that have matured on the buffer manager, then drawing them on the matrix as they do

    static bool RunDrawLoop(LEDBufferManager & bufferManager, RGBMatrix & matrix)
    {
        // If set to true, this will cause backlogged frames to be discarded.  If false, they will be drawn
        // as fast as possible to catch up to the current time

        constexpr auto burnExtraFrames = false;
        constexpr auto kMaximumWait = 10000.0;  // How long to wait when no frames available in the buffer

        while (!interrupt_received)
        {
            // There may be a slight race condition here, where the oldest buffer is popped and then
            // replaced by another before we wind up grabbing it, but that's not a big deal.  It'd be
            // serious if were were popping thhe last buffer, but the optional<> nature of the return
            // value means we can handle that case just fine.

            while (bufferManager.AgeOfOldestBuffer() <= 0)
            {
                std::optional<std::unique_ptr<LEDBuffer>> buffer = bufferManager.PopOldestBuffer();
                if (!buffer.has_value())
                    continue;

                if (burnExtraFrames && bufferManager.AgeOfOldestBuffer() <= 0)
                    continue;

                DrawFrame(buffer.value(), matrix);
            }
            int64_t delay = std::min(kMaximumWait, bufferManager.AgeOfOldestBuffer() * 1000000L);
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::microseconds(delay));
        }
	    return true;
    }

    static double FPS()
    {
        return _FPS;
    }
};
