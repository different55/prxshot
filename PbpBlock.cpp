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

#include <string.h>
#include <malloc.h>
#include "libcpp.h"
#include "PbpBlock.hpp"
#include "PspUtils.hpp"
#include "logger.h"

#define PSCM_FILE "/PSCM.DAT"
#define PSCM_SIZE 236
#define PSCM_PAIRS 3
#define PSCM_KEYS_SIZE 32
#define XMB_FILE "xmb.sfo"
#define XMB_ICON0 "default_icon0.png"

#define PARENTAL_LEVEL "PARENTAL_LEVEL"
#define VERSION "VERSION"
#define TITLE "TITLE"
#define GAME_ID "GAME_ID"

#define SFO_PATH "disc0:/PSP_GAME/PARAM.SFO"
#define ICON0_PATH "disc0:/PSP_GAME/ICON0.PNG"
#define PIC1_PATH "disc0:/PSP_GAME/PIC1.PNG"

PbpBlock::PbpBlock(const char *plugin_path) {
    init(plugin_path, NULL);
}

PbpBlock::PbpBlock(const char *plugin_path, const char *path) {
    init(plugin_path, path);
}

void PbpBlock::init(const char *plugin_path, const char *path) {
    sfo_path = plugin_path;
    is_created = false;
    header = new pbp_header;
    sfo = new SfoBlock();
    stop_func = NULL;
    pbp_filename = path ? strdup(path) : NULL;
}

void PbpBlock::outputDir(const char *path) {
    outfile = strjoin(path, PSCM_FILE);
}

int PbpBlock::run() {
    kprintf("Started PbpBlock thread\n");
    SceIo fdo, fdi;
    kprintf("opening %s\n", outfile);
    if(!fdo.open(outfile, SceIo::FILE_RDWR))
        return 1;
    kprintf("opening %s\n", pbp_filename);
    if(pbp_filename && (!fdi.open(pbp_filename, SceIo::FILE_READ)))
        return 1;
    SfoBlock *pscm = generatePSCM(sfo);
    fdo.write(header, sizeof(pbp_header));
    pscm->save(&fdo);
    SceSize size = header->icon1_offset - header->icon0_offset;
    kprintf("pbp: %s, sfo: %s, pic0 size: %i bytes\n", pbp_filename, sfo_path, size);
    if(!pbp_filename || size == 0) {
        // XMB, UMD/ISO or PBP without icon0 detected
        if(PspUtils::applicationType() == PspUtils::VSH || pbp_filename) {
            // XMB or PBP without icon0 detected
            char *icon0_file = strjoin(sfo_path, XMB_ICON0);
            if(!fdi.open(icon0_file, SceIo::FILE_READ)) {
                size = 0;
            } else {
                size = fdi.size();
            }
            free(icon0_file);
        } else {
            // UMD/ISO detected
            fdi.open(ICON0_PATH, SceIo::FILE_READ);
            size = fdi.size();
        }
    } else {
        // PBP with an icon0 detected
        fdi.seek(header->icon0_offset, SceIo::FILE_SET);
    }
    header->icon0_offset = header->sfo_offset + PSCM_SIZE;
    header->icon1_offset = header->icon0_offset + size;
    header->pic0_offset = header->icon0_offset + size;
    if(size)
        appendData(&fdo, &fdi, size);
    //TODO. read settings to see if using pic1 is allowed
    /*if(!file) {
        fdi.close();
        if(fdi.open(PIC1_PATH, SceIo::FILE_READ))
            size = fdi.size();
        else
            size = 0;
    } else {
        fdi.seek(header->pic1_offset, SceIo::FILE_SET);
        size = header->snd0_offset - header->pic1_offset;
    }*/
    size = 0;
    header->pic1_offset = header->pic0_offset;
    header->snd0_offset = header->pic1_offset + size;
    header->psp_offset = header->pic1_offset + size;
    header->psar_offset = header->pic1_offset + size;
    if(size)
        appendData(&fdo, &fdi, size);
    if(!pbp_filename)
        fdi.close();
    fdo.rewind();
    fdo.write(header, sizeof(pbp_header));
    fdo.close();
    fdi.close();
    delete pscm;
    is_created = true;
    if(stop_func)
        stop_func();
    return 0;
}

bool PbpBlock::load() {
    SceIo fd;
    kprintf("PbpBlock::load, %s\n", pbp_filename);
    if(pbp_filename && !fd.open(pbp_filename, SceIo::FILE_READ))
        return false;
    if(pbp_filename) {
        fd.read(header, sizeof(pbp_header));
        sfo->load(&fd, header->icon0_offset - header->sfo_offset);
        fd.close();
    } else {
        SceSize size;
        if(PspUtils::applicationType() == PspUtils::VSH) {
            char *sfo_file = strjoin(sfo_path, XMB_FILE);
            kprintf("Loading: %s\n", sfo_file);
            size = sfo->load(sfo_file);
            free(sfo_file);
        } else {
            kprintf("Loading: %s\n", SFO_PATH);
            size = sfo->load(SFO_PATH);
        }
        if(!size) {
            return false;
        }
        memcpy(header->id, "\0PBP", 4);
        header->version = 0x10000;
        header->sfo_offset = sizeof(pbp_header);
        header->icon0_offset = header->sfo_offset + size;
    }
    is_created = false;
    return true;
}

void PbpBlock::appendData(SceIo *out, SceIo *in, size_t size) {
    int read = BUFFER_SIZE;
    char *buffer = new char[BUFFER_SIZE];
    while(size) {
        read = in->read(buffer, read);
        if(read <= 0) break;
        out->write(buffer, read);
        size -= read;
        read = size < BUFFER_SIZE ? size : BUFFER_SIZE;
    }
    delete buffer;
}

SfoBlock *PbpBlock::generatePSCM(SfoBlock *sfo) {
    int value;
    kprintf("Generating PSCM\n");
    SfoBlock *pscm = new SfoBlock();
    pscm->prepare(PSCM_SIZE, PSCM_PAIRS, PSCM_KEYS_SIZE);
    if(!sfo->getIntValue(PARENTAL_LEVEL, &value))
        value = 1;
    pscm->setIntValue(PARENTAL_LEVEL, value);
    const char *title = sfo->getStringValue(TITLE);
    if(!title) {
        // must not happen, invalid SFO
        title = "Game / Homebrew";
    }
    pscm->setStringValue(TITLE, title);
    if(!sfo->getIntValue(VERSION, &value))
        value = 0x10000;
    pscm->setIntValue(VERSION, value);
    return pscm;
}

PbpBlock::~PbpBlock() {
    free(pbp_filename);
    free(outfile);
    delete header;
    delete sfo;
}