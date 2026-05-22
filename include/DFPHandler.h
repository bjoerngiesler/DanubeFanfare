#if !defined(DFPHANDLER_H)
#define DFPHANDLER_H

#include <LibBB.h>
#include <sys/types.h>
#include <map>
#include <functional>

enum DFPCmdCode {
    CMD_NEXT             = 0x01, // done, untested
    CMD_PREV             = 0x02, // done, untested
    CMD_PLAY             = 0x03, // Play given track
    CMD_INC_VOL          = 0x04, // done, untested
    CMD_DEC_VOL          = 0x05, // done, untested
    CMD_VOLUME           = 0x06, // done
    CMD_EQ               = 0x07,
    CMD_PLAYBACK_MODE    = 0x08,
    CMD_PLAYBACK_SRC     = 0x09,
    CMD_STANDBY          = 0x0a, // will not implement
    CMD_NORMAL           = 0x0b, // will not implement - Come out of standby mode
    CMD_RESET            = 0x0c, // done
    CMD_PLAYBACK         = 0x0d, 
    CMD_PAUSE            = 0x0e, // done
    CMD_PLAY_FOLDER      = 0x0f, // done
    CMD_VOL_ADJUST       = 0x10, // will not implement
    CMD_REPEAT_PLAY      = 0x11,
    CMD_USE_MP3_FOLDER   = 0x12, // ??
    CMD_INSERT_ADVERT    = 0x13, // This interrupts the current track with an inserted one
    CMD_SPEC_TRACK_3000  = 0x14, // Play from specific folder with up to 3000 files
    CMD_STOP_ADVERT      = 0x15, // This stops the interrupt and resumes playback on the original
    CMD_STOP             = 0x16, // done
    CMD_REPEAT_FOLDER    = 0x17,
    CMD_RANDOM_ALL       = 0x18,
    CMD_REPEAT_CURRENT   = 0x19,
    CMD_SET_DAC          = 0x20, // ??

    CMD_GET_STAY_1       = 0x3c, // ?? According to original data sheet
    CMD_GET_STAY_2       = 0x3d, // ?? According to original data sheet
    CMD_GET_STAY_3       = 0x3e, // ?? According to original data sheet
    CMD_GET_SEND_INIT    = 0x3f,
    CMD_GET_RETRANSMIT   = 0x40,
    CMD_GET_REPLY        = 0x41, // ??
    CMD_GET_STATUS       = 0x42,
    CMD_GET_VOLUME       = 0x43,
    CMD_GET_EQ           = 0x44,
    CMD_GET_MODE         = 0x45,
    CMD_GET_VERSION      = 0x46,
    CMD_GET_TF_FILES     = 0x47,
    CMD_GET_U_FILES      = 0x48, // done
    CMD_GET_FLASH_FILES  = 0x49,
    CMD_KEEP_ON          = 0x4a,
    CMD_GET_TF_TRACK     = 0x4b,
    CMD_GET_U_TRACK      = 0x4c,
    CMD_GET_FLASH_TRACK  = 0x4d,
    CMD_GET_FOLDER_FILES = 0x4e, // done
    CMD_GET_FOLDERS      = 0x4f
};

enum Equalizer {
    EQ_NORMAL  = 0,
    EQ_POP     = 1,
    EQ_ROCK    = 2,
    EQ_JAZZ    = 3,
    EQ_CLASSIC = 4,
    EQ_BASE    = 5
};

enum PlayMode {
    MODE_REPEAT        = 0,
    MODE_FOLDER_REPEAT = 1,
    MODE_SINGLE_REPEAT = 2,
    MODE_RANDOM        = 3
};

enum PlaySource {
    SRC_U     = 1,
    SRC_TF    = 2,
    SRC_AUX   = 3,
    SRC_SLEEP = 4,
    SRC_FLASH = 5
};

struct PACKED_ATTR DFPCmd {
    DFPCmdCode cmd            : 8;
    bool feedback             : 8;
    uint8_t para1             : 8;
    uint8_t para2             : 8;
    mutable uint16_t checksum : 16;
    uint16_t calcChecksum(bool swap=false) const;
};

using namespace bb;

class DFPHandler: public Subsystem {
public:
    static DFPHandler inst;

    Result initialize(HardwareSerial& ser = Serial1, unsigned int bps=9600);
    Result start(ConsoleStream* stream = nullptr);
    Result step();
    Result stop(ConsoleStream *stream = nullptr);

    void handle(Stream* stream);

    Result setBps(unsigned int bps);

    void cmdReset(const DFPCmd& cmd);
    void cmdSetVolume(const DFPCmd& cmd);
    void cmdStopPlayback(const DFPCmd& cmd);
    void cmdPlayFolder(const DFPCmd& cmd);
    void cmdGetUFiles(const DFPCmd& cmd);
    void cmdGetFolderFiles(const DFPCmd& cmd);
    void cmdPlay(const DFPCmd& cmd);

    bool sendCommand(const DFPCmd& cmd);
protected:
    static std::map<DFPCmdCode, std::function<void(const DFPCmd& cmd)>> callbackMap_;
    bool waitAvailable(uint8_t& byte);
    bool readDFPCmd(DFPCmd& DFPCmd);

    unsigned int bps_ = 9600;
    HardwareSerial& ser_ = Serial1;
    unsigned long timeoutUS_ = 2*(1000000 / (bps_/10));
};

#endif // DFPHANDLER_H