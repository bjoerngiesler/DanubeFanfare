#include <Arduino.h>
#include <LibBB.h>
#include "DFPHandler.h"
#include "Player.h"
#include "FileManager.h"
#include "StateManager.h"

DFPHandler DFPHandler::inst;

static const uint8_t VERSION = 0xff;

std::map<DFPCmdCode, std::function<void(const DFPCmd& cmd)>> DFPHandler::callbackMap_ = {
    { CMD_NEXT, [](const DFPCmd& cmd)->void { StateManager::inst.next(); }},
    { CMD_PREV, [](const DFPCmd& cmd)->void { StateManager::inst.previous(); }},
    { CMD_PLAY, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdPlay(cmd); }},
    { CMD_INC_VOL, [](const DFPCmd& cmd)->void { Player::inst.setOutputVolume(Player::inst.outputVolume() + 0.033); }},
    { CMD_DEC_VOL, [](const DFPCmd& cmd)->void { Player::inst.setOutputVolume(Player::inst.outputVolume() - 0.033); }},
    { CMD_RESET, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdReset(cmd); }},
    { CMD_PAUSE, [](const DFPCmd& cmd)->void { Player::inst.pausePlayback(!Player::inst.isPaused()); }},
    { CMD_VOLUME, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdSetVolume(cmd); }},
    { CMD_STOP, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdStopPlayback(cmd); }},
    { CMD_PLAY_FOLDER, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdPlayFolder(cmd); }},
    { CMD_GET_U_FILES, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdGetUFiles(cmd); }},
    { CMD_GET_FOLDER_FILES, [](const DFPCmd& cmd)->void { DFPHandler::inst.cmdGetFolderFiles(cmd); }},
};

bb::Result DFPHandler::initialize(HardwareSerial& ser, unsigned int bps) {
    ser_ = ser;
    bps_ = bps;
    Runloop::runloop.addStreamCallback(&ser_, Runloop::STREAM_READ, [](Stream* str)->void{DFPHandler::inst.handle(str);});
    return Subsystem::initialize("dfp", "DFP Protocol Handler", "");
}

Result DFPHandler::start(ConsoleStream* stream) {
    ser_.begin(bps_, SERIAL_8N1, RX, TX);
    return Subsystem::start(stream);
}

Result DFPHandler::step() {
    // handle();
    return RES_OK;
}

void DFPHandler::handle(Stream* stream) {
    DFPCmd cmd;
    if(!ser_.available()) {
        bb::printf("Nothing available\n");
        return;
    }

    while(ser_.available()) {
        if(readDFPCmd(cmd) == false) {
            bb::printf("read failed\n");
            continue;
        }
        //bb::printf("DFPCmd: %02x Para1: %0x Para2: %0x Wants feedback: %d\n", cmd.cmd, cmd.para1, cmd.para2, cmd.feedback);
        if(callbackMap_.find(cmd.cmd) != callbackMap_.end()) {
            callbackMap_[cmd.cmd](cmd);
        } else {
            bb::printf("Unknown / unhandled DFPCmd 0x%02x\n", cmd.cmd);
        }
    }
}

Result DFPHandler::stop(ConsoleStream *stream) {
    return Subsystem::stop(stream);
}

Result DFPHandler::setBps(unsigned int bps) {
    if(bps_ == bps) return RES_OK;
    ser_.begin(bps);
    bps_ = bps;

    // Serial comm is 1 start bit, then for 8N1 8 bytes and 1 stop bit ==> 10 bits per
    // transferred byte, so for n bps we get n/10 bytes per second or 
    // 1000000/(n/10) microseconds per byte. We take twice that as our max timeout.
    timeoutUS_ = 2*(1000000 / (bps_/10));
    return RES_OK;
}

bool DFPHandler::waitAvailable(uint8_t& byte) {
    unsigned long timeout = timeoutUS_;
    while(!ser_.available() && timeout > 0) {
        timeout--;
        delayMicroseconds(1);
    }
    if(!ser_.available()) {
        return false;
    }
    byte = ser_.read();
    return true;
}

bool DFPHandler::readDFPCmd(DFPCmd& cmd) {
    uint8_t sync, ver, len, end;

    // sync
    size_t numdropped = 0;
    while(true) {
        if(!waitAvailable(sync)) {
            bb::printf("Timeout while waiting for sync byte\n");
            return false;
        }
        if(sync == 0x7e) break;
        numdropped++;
    }
    if(numdropped) Serial.printf("*** dropped %d bytes\n", numdropped);

    if(!waitAvailable(ver)) {
        bb::printf("Timeout while waiting for version byte\n");
        return false;
    }
    if(!waitAvailable(len)) {
        bb::printf("Timeout while waiting for length byte\n");
        return false;
    }

    uint8_t* buf = new uint8_t[len];
    for(int i=0; i<len; i++) {
        if(!waitAvailable(buf[i])) {
            delete[] buf;
            return false;
        }
    }

    if(len != 6) {
        bb::printf("Unknown length\n");
        delete[] buf;
        return false;
    }

    // We silently disregard the 0xef end-of-packet flag. It's doubly redundant -
    // the len field tells us how many bytes to read, and it's always constant
    // 6 by protocol anyway. The 0xef gets kicked by the sync code at the beginning.

    cmd.cmd = (DFPCmdCode)buf[0];
    cmd.feedback = buf[1]>0;
    cmd.para1 = buf[2];
    cmd.para2 = buf[3];
    cmd.checksum = buf[4] << 8 | buf[5];
    if(cmd.calcChecksum() != cmd.checksum) {
        bb::printf("Checksum error -- should be 0x%0x but is 0x%0x\n", cmd.calcChecksum(), cmd.checksum);
        return false;
    } 

    delete[] buf;

    if(!waitAvailable(end)) {
        bb::printf("Timeout while waiting for length byte\n");
        return false;
    }
    if(end != 0xef) bb::printf("Got wrong end byte -- expected 0x%x, got 0x%x\n", 0xef, end);
    return true;
}

uint16_t DFPCmd::calcChecksum(bool swap) const {
    uint16_t cs = 1 + (0xffff - (VERSION  + 6 + cmd + feedback + para1 + para2));
    if(swap) return (cs&0xff) << 8 | (cs&0xff00) >> 8;
    else return cs;
}

void DFPHandler::cmdReset(const DFPCmd& cmd) {
    bb::printf("CMD: Reset!\n");
    StateManager::inst.stop();
}

void DFPHandler::cmdSetVolume(const DFPCmd& cmd) {
    uint16_t volume = cmd.para1 << 8 | cmd.para2;
    bb::printf("CMD: Set volume to %d\n", volume);
    float v = float(volume)/30.0f;
    Player::inst.setOutputVolume(constrain(v, 0.0f, 1.0f));
}

void DFPHandler::cmdStopPlayback(const DFPCmd& cmd) {
    bb::printf("CMD: Stop playback\n");
    StateManager::inst.stop();
}

void DFPHandler::cmdPlayFolder(const DFPCmd& cmd) {
    bb::printf("CMD: Play folder %d file %d\n", cmd.para1, cmd.para2);
    StateManager::inst.playFile(cmd.para2, cmd.para1);
}

void DFPHandler::cmdGetUFiles(const DFPCmd& cmd) {
    bb::printf("CMD: Get U Files %d / %d --> %d\n", cmd.para1, cmd.para2, FileManager::inst.numFiles());
    DFPCmd c = cmd;
    c.para2 = FileManager::inst.numFiles();
    if(sendCommand(c) == false) bb::printf("Send failed\n");
}

void DFPHandler::cmdGetFolderFiles(const DFPCmd& cmd) {
    bb::printf("CMD: Get Folder Files %d / %d --> %d\n", cmd.para1, cmd.para2, FileManager::inst.numFilesInFolder(cmd.para2));
    DFPCmd c = cmd;
    c.para2 = FileManager::inst.numFilesInFolder(cmd.para2);
    if(sendCommand(c) == false) bb::printf("Send failed\n");
}

void DFPHandler::cmdPlay(const DFPCmd& cmd) {
    bb::printf("CMD: Play %d / %d\n", cmd.para1, cmd.para2);
    String filename = FileManager::inst.filename(cmd.para2);
    if(filename == "") {
        bb::printf("No file for index %d in root\n", cmd.para2);
        return;
    }
    if(StateManager::inst.playFile(filename) == false) {
        bb::printf("Couldn't play file %d -> '%s'\n", cmd.para2, filename.c_str());
    }
    bb::printf("Success playing.\n");
}

bool DFPHandler::sendCommand(const DFPCmd& cmd) {
    cmd.checksum = cmd.calcChecksum(true);
    uint8_t buf[sizeof(cmd)+4];
    buf[0] = 0x7e;
    buf[1] = 0xff;
    buf[2] = 6;
    memcpy(&(buf[3]), (uint8_t*)&cmd, sizeof(cmd));
    buf[sizeof(buf)-1]=0xef;

    if(ser_.write(buf, sizeof(buf)) != sizeof(buf)) return false; // end byte

    return true;
}
