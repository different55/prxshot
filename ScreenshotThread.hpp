/*
 *  prxshot module
 *
 *  Copyright (C) 2011  Codestation
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCREENSHOTTHREAD_H_
#define SCREENSHOTTHREAD_H_

#include "Thread.hpp"
#include "PspUtils.hpp"
#include "Screenshot.hpp"
#include "PbpBlock.hpp"
#include "Settings.hpp"

class ScreenshotThread: public Thread {
public:
    ScreenshotThread(int args, void *argp);
    ~ScreenshotThread();
protected:
    int run();
private:
    char *createScreenshotDir(const char *gameid);
    void prepareDirectory();
    bool migrated;
    char *argp;
    char *shot_path;
    Screenshot *screen;
    PspUtils psp;
    PbpBlock *pbp;
    Settings *settings;
};

#endif /* SCREENSHOTTHREAD_H_ */