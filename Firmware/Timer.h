// (c) 2013 Artjom Eske
// This code is licensed under MIT license (see LICENSE.txt for details)

#pragma once

class SoftwareTimer
{
public:
    SoftwareTimer(){}

    void Start(){
        start = millis();
        IsRunning = true;
    }
    uint64_t ElapsedTime(){
        return millis() - start;
    }

    void Stop() {
        IsRunning = false;
    }

    bool IsRunning = false;

private:
    uint64_t start = 0;
};

struct Offset // Offsets der Registeradressen
{
    const uint8_t ctrla     = 0x00;
    const uint8_t ctrlb     = 0x01;
    const uint8_t evctrl    = 0x04;
    const uint8_t intctrl   = 0x05;
    const uint8_t intflags  = 0x06;
    const uint8_t status    = 0x07;
    const uint8_t dbgctrl   = 0x08;
    const uint8_t temp      = 0x09;
    const uint8_t cnt       = 0x0A;
    const uint8_t ccmp      = 0x0C;
}; 
Offset addrOffset;


struct Address // Basis der Registeradressen
{
    uint16_t  baseAddr;
};

Address baseAddr[4] 
{
// Base Address  
    {0x0A80},   // TCB0
    {0x0A90},   // TCB1   
    {0x0AA0},   // TCB2
    {0x0AB0},   // TCB3
};


using Register8  = volatile uint8_t *;
using Register16 = volatile uint16_t *;
  
Register8  regCTRLA    (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.ctrla); }
Register8  regCTRLB    (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.ctrlb); }
Register8  regEVCTRL   (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.evctrl); }
Register8  regINTCTRL  (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.intctrl); }
Register8  regINTFLAGS (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.intflags); }
Register8  regSTATUS   (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.status); }
Register8  regDBGCTRL  (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.dbgctrl); }
Register8  regTEMP     (const uint8_t n) { return (Register8)  (baseAddr[n].baseAddr + addrOffset.temp); }
Register16 regCNT      (const uint8_t n) { return (Register16) (baseAddr[n].baseAddr + addrOffset.cnt); }
Register16 regCCMP     (const uint8_t n) { return (Register16) (baseAddr[n].baseAddr + addrOffset.ccmp); }

template<const uint8_t n> 
class HardwareTimer{

private:
    using Register8  = volatile uint8_t *;
    using Register16 = volatile uint16_t *;
    const uint8_t CLKSELmask {TCB_CLKSEL1_bm | TCB_CLKSEL0_bm};
    const uint8_t CNTMODEmask {0x07};
  
public:
    HardwareTimer() = default;
    volatile uint16_t cnt {0};
    volatile uint16_t ccmp {0};
    volatile bool finish {false};  
    
    void Enable()             { *regCTRLA(n) = *regCTRLA(n)  | TCB_ENABLE_bm; }  
    void Disable()            { *regCTRLA(n) = *regCTRLA(n)  & ~TCB_ENABLE_bm; } 
    void SetCLKDIV1()         { *regCTRLA(n) = *regCTRLA(n)  & ~CLKSELmask; }
    void SetCLKDIV2()         { *regCTRLA(n) = (*regCTRLA(n) & ~CLKSELmask) | TCB_CLKSEL_CLKDIV2_gc; }
    void SetCLKTCA()          { *regCTRLA(n) = (*regCTRLA(n) & ~CLKSELmask) | TCB_CLKSEL_CLKTCA_gc; }
    void EnableSYNCUPD()      { *regCTRLA(n) = *regCTRLA(n)  | TCB_SYNCUPD_bm; }
    void DisableSYNCUPD()     { *regCTRLA(n) = *regCTRLA(n)  & ~TCB_SYNCUPD_bm; }
    void EnableRUNSTDBY()     { *regCTRLA(n) = *regCTRLA(n)  | TCB_RUNSTDBY_bm; }
    void DisableRUNSTDBY()    { *regCTRLA(n) = *regCTRLA(n)  & ~TCB_RUNSTDBY_bm; }
    
    void SetModeINT()         { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_INT_gc; }
    void SetModeTIMEOUT()     { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_TIMEOUT_gc; }
    void SetModeCAPT()        { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_CAPT_gc; }
    void SetModeFRQ()         { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_FRQ_gc; }
    void SetModePW()          { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_PW_gc; }
    void SetModeFRQPW()       { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_FRQPW_gc; }
    void SetModeSINGLE()      { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_SINGLE_gc; }
    void SetModePWM8()        { *regCTRLB(n) = (*regCTRLB(n) & ~CNTMODEmask) | TCB_CNTMODE_PWM8_gc; }
    void EnableCCMPEN()       { *regCTRLB(n) = *regCTRLB(n)  | TCB_CCMPEN_bm; }
    void DisableCCMPEN()      { *regCTRLB(n) = *regCTRLB(n)  & ~TCB_CCMPEN_bm; }
    void EnableCCMPINIT()     { *regCTRLB(n) = *regCTRLB(n)  | TCB_CCMPINIT_bm; }
    void DisableCCMPINIT()    { *regCTRLB(n) = *regCTRLB(n)  & ~TCB_CCMPINIT_bm; }
    void EnableASYNC()        { *regCTRLB(n) = *regCTRLB(n)  | TCB_ASYNC_bm; }
    void DisableASYNC()       { *regCTRLB(n) = *regCTRLB(n)  & ~TCB_ASYNC_bm; }

    void EnableEventInput()   { *regEVCTRL(n) = *regEVCTRL(n) | TCB_CAPTEI_bm; }
    void DisableEventInput()  { *regEVCTRL(n) = *regEVCTRL(n) & ~TCB_CAPTEI_bm; }
    void EnableEventEdge()    { *regEVCTRL(n) = *regEVCTRL(n) | TCB_EDGE_bm; }    
    void DisableEventEdge()   { *regEVCTRL(n) = *regEVCTRL(n) & ~TCB_EDGE_bm; }
    void EnableNoiseFilter()  { *regEVCTRL(n) = *regEVCTRL(n) | TCB_FILTER_bm; }
    void DisableNoiseFilter() { *regEVCTRL(n) = *regEVCTRL(n) & ~TCB_FILTER_bm; }
    
    void EnableCAPTInterrupt()  { *regINTCTRL(n) = *regINTCTRL(n) | TCB_CAPT_bm; }  
    void DisableCAPTInterrupt() { *regINTCTRL(n) = *regINTCTRL(n) & ~TCB_CAPT_bm; }  
    
    void DeleteFlagCAPT()     { *regINTFLAGS(n) = *regINTFLAGS(n) | TCB_CAPT_bm; } 
    
    void Reset()
    {
      *regCTRLA(n) = 0;
      *regCTRLB(n) = 0;
      *regEVCTRL(n) = 0;
      *regINTCTRL(n) = 0;
      DeleteFlagCAPT();
      *regCNT(n) = 0;
      *regCCMP(n) = 0;
    } 
    
    bool IsRunning() { return *regSTATUS(n); }
    
    void SetCounter(const uint16_t var) { *regCNT(n) = var;  }
    void SetCompare(const uint16_t var) { *regCCMP(n) = var; }
    
    uint16_t ReadCounter() { return *regCNT(n); }
    uint16_t ReadCompare() { return *regCCMP(n); }

    // for Debugging and Tests etc.
    Register8 GetRegCTRLA()    { return (Register8) regCTRLA(n); }
    Register8 GetRegCTRLB()    { return (Register8) regCTRLB(n); }
    Register8 GetRegEVCTRL()   { return (Register8) regEVCTRL(n); }
    Register8 GetRegINTCTRL()  { return (Register8) regINTCTRL(n); }
    Register8 GetRegINTFLAGS() { return (Register8) regINTFLAGS(n); }
    Register8 GetRegSTATUS()   { return (Register8) regSTATUS(n); }
    uint16_t GetAddrBase()                 { return baseAddr[n].baseAddr; }
    uint8_t  GetAddrOffset(Register8 reg)  { return static_cast<uint8_t> (reinterpret_cast<uint16_t>(reg) - getAddrBase() ); }
    uint16_t GetAddrAbsolut(Register8 reg) { return reinterpret_cast<uint16_t>(reg); } 
};