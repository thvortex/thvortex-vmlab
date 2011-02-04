// =====================================================================
// VMLAB user-defined components API include file
// Release 3.15, October 2009
// AMcTools. Code released under the LGPL terms
// =====================================================================

#if !defined(_BLACKBOX)
#define _BLACKBOX

#define RELEASE_CODE 2 // VMLAB release < 3.15 will flag incompatibility

#if !defined(RC_INVOKED)

#if !defined(__cplusplus)
#error "Please, compile as C++, not as C"
#endif

#if !defined(__WIN32__) && !defined(_WIN32)
#error "Please, use a Win32 compiler"
#endif

#if !defined(__DLL__) && !defined(_DLL)
#error "Please, compile as DLL, not as EXE"
#endif

#if defined(IS_PERIPHERAL) && defined(IS_DUMMY_PERIPHERAL)
#error "Sorry, define only one: IS_PERIPHERAL or IS_DUMMY_PERIPHERAL"
#endif

#if defined(IS_PERIPHERAL) || defined(IS_DUMMY_PERIPHERAL)
#define IS_MICRO_STUFF
#endif

// Macros for static variables declaration and access
//
#if defined(IS_MICRO_STUFF)
   #define DECLARE_VAR \
   namespace PRIVATE { \
      static struct Tag_Variables { \
	     void *operator new[](size_t pBytes); \
         void operator delete[](void *pMemory); \
	     WORD8 _registers[_nOfRegisters];    // Register array placeholder	  
#else
   #define DECLARE_VAR \
   namespace PRIVATE { \
      static struct Tag_Variables { \
	     void *operator new[](size_t pBytes); \
         void operator delete[](void *pMemory); \
         unsigned long ______dummy;          // Dummy variable to avoid error
		 
#endif // IS_MICRO_STUFF	  

#ifndef STANDARD_ALLOC      // By default, use special memory allocation to catch access violation errors

#define END_VAR \
   } *_Variables = NULL;\
   void _Alloc_var() { \
      _Variables = new struct Tag_Variables [_N_instances]; \
   }\
   void _Free_var() {delete [] _Variables;}\
   void *Tag_Variables:: operator new[](size_t pBytes) {\
     DWORD dummy;\
     char *memory;\
     size_t total = (pBytes + 0xFFF) / 0x1000 * 0x1000;\
     size_t align = (total - pBytes) / 0x10 * 0x10;\
     memory = (char *) VirtualAlloc(NULL, total + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);\
     VirtualProtect(memory + total, 0x1000, PAGE_NOACCESS, &dummy);\
     return memory ? (memory + align) : NULL;\
  }\
  void Tag_Variables:: operator delete[](void *pMemory) {\
     LONG_PTR page = (LONG_PTR) pMemory & ~((LONG_PTR) 0xFFF);\
     VirtualFree((LPVOID) page, 0, MEM_RELEASE);\
  }\
}

#else                   // Use standard memory allocation

#define END_VAR \
   } *_Variables = NULL;\
   void _Alloc_var() { \
      _Variables = new struct Tag_Variables [_N_instances]; \
	  for(int i = 0; i < _N_instances; i++) {\
         for(int j = 0; j < sizeof(struct Tag_Variables); j++) \
		    ((char *)&(_Variables[i]))[j] = 0;\
	  }\
   }\
   void _Free_var() {delete [] _Variables;}\
   void *Tag_Variables:: operator new[](size_t pBytes) {\
      return ::new char[pBytes];\
   }\
   void Tag_Variables:: operator delete[](void *pMemory) {\
      ::delete[] pMemory;\
   }\
}

#endif

#define VAR(a) (PRIVATE::_Variables[PRIVATE::_Instance_index].a)

// Typedefs & other defines for hardware 
//
typedef unsigned long LOGIC;
typedef unsigned long EDGE;
typedef unsigned long PIN;
typedef unsigned long PORT;
typedef const void * ELEMENT;
typedef int GADGET;
typedef unsigned long CYCLES;
typedef unsigned long ADDRESS;

#define RISE 1     // Signal transitions, etc
#define FALL 2
#define UNKNOWN 2
#define TOGGLE 3

#define PORT_OK         0  // Return codes for port handling
#define PORT_NOT_OUTPUT 1
#define PORT_NOT_OWNER  2
#define PORT_INVALID    3

#define FORCE_NONE            0   // Options for TAKEOVER_PORT
#define FORCE_OUTPUT          1
#define FORCE_INPUT           2
#define TOP_OWNER          0x10   // To combine by ORing wiht previous

#define ATTRI_DISABLE_DIGITAL    0x0001      // Port attributes, combine by ORing
#define ATTRI_OPEN_DRAIN         0x0002      //
#define ATTRI_PULLUP_1K          0x0004      // Pullup not yet implemented!
#define ATTRI_PULLUP_10K         0x0008      // 
#define ATTRI_PULLUP_100K        0x0010      //
#define ATTRI_FUTURE_TBD         0x0020      //

#define INFO_RAM_SIZE         1   // Parameters for GET_MICRO_INFO()
#define INFO_FLASH_SIZE       2
#define INFO_EEPROM_SIZE      3
#define INFO_PROGRAM_COUNTER  4
#define INFO_CPU_CYCLES       5
#define INFO_ADDR_MODE        6

#define DATA_EEPROM   1  // Parameters for GET_MICRO_DATA( )
#define DATA_RAM      2
#define DATA_FLASH    3
#define DATA_REGISTER 4

#if !defined(_WORD8) // Added to allow external include
namespace PRIVATE {
   // Forward declarations needed by typecasts and class definitions
   template <typename T, unsigned int M> class WORDX;
   template <typename T> class WORDN;
   template <typename Tl, typename Tw> class WORDHL;
   template<typename T> class WORDLE;
   template<typename T> class WORDBE;
}

// Publicly visible primitive WORD classes for user components
typedef PRIVATE::WORDX<PRIVATE::WORDN<UCHAR>, 0xFF> WORD8;
typedef PRIVATE::WORDX<PRIVATE::WORDN<USHORT>, 0xFFFF> WORD16;
typedef PRIVATE::WORDX<PRIVATE::WORDN<UINT>, 0xFFFFFFFF> WORD32;

// Publicly visible little-endian WORD classes for user components
typedef PRIVATE::WORDX<PRIVATE::WORDHL<USHORT, PRIVATE::WORDLE<WORD8> >, 0xFFFF> WORD16LE;
typedef PRIVATE::WORDX<PRIVATE::WORDHL<UINT, PRIVATE::WORDLE<WORD16LE> >, 0xFFFFFFFF> WORD32LE;
typedef PRIVATE::WORDX<PRIVATE::WORDHL<UINT, PRIVATE::WORDLE<WORD16> >, 0xFFFFFFFF> WORD32LE16;

// Publicly visible big-endian WORD classes for user components
typedef PRIVATE::WORDX<PRIVATE::WORDHL<USHORT, PRIVATE::WORDBE<WORD8> >, 0xFFFF> WORD16BE;
typedef PRIVATE::WORDX<PRIVATE::WORDHL<UINT, PRIVATE::WORDBE<WORD16BE> >, 0xFFFFFFFF> WORD32BE;
typedef PRIVATE::WORDX<PRIVATE::WORDHL<UINT, PRIVATE::WORDBE<WORD16> >, 0xFFFFFFFF> WORD32BE16;

#endif // WORD8

// Pin declaration macros
//

#define DECLARE_PINS

#define END_PINS \
namespace PRIVATE {\
   PIN_ENTRY _PINLIST_END("}", sizeof("}"));\
}\
void GetPins(const char **pBegin, const char **pEnd) {\
   PRIVATE::PIN_ENTRY::GetPins(pBegin, pEnd);\
}

#define _PIN_ENTRY(name, value, tmask, tstr) \
namespace PRIVATE {\
   PIN_ENTRY _PIN_##name("@" #name, sizeof("@" #name));\
   PIN_ENTRY _INDEX_##name("@" #value, sizeof("@" #value));\
   PIN_ENTRY _TYPE_##name(tstr, sizeof(tstr));\
}\
const PIN name = value;

// Helper class for pin declarations
//

namespace PRIVATE {
   class PIN_ENTRY {
      private:
         const char *text;
         PIN_ENTRY *next;
         
         static int length;
         static char *buffer;         
         static PIN_ENTRY *last;
      public:
         PIN_ENTRY(const char *t, int size) : text(t), next(NULL)
         {            
            if (last) last->next = this;
            last = this;
            length += size;
         }
         ~PIN_ENTRY()
         {
            if(buffer == NULL) return;
            delete[] buffer;
            buffer = NULL;
         }         
         static void GetPins(const char **pBegin, const char **pEnd);
   };

   int PIN_ENTRY::length = 0;
   char *PIN_ENTRY::buffer = NULL;
   PIN_ENTRY *PIN_ENTRY::last = NULL;

   PIN_ENTRY _PIN_LISTBEGIN("{", sizeof("{"));

   void PIN_ENTRY::GetPins(const char **pBegin, const char **pEnd)
   {
      if(buffer == NULL) buffer = new char[length];
      if(buffer == NULL) { *pBegin = *pEnd = NULL; return; }
      char *dst = buffer;

      for(PIN_ENTRY *pin = &_PIN_LISTBEGIN; pin; pin = pin->next, *dst++ = '\0')
         for(const char *src = pin->text; *src; src++) *dst++ = *src;
         
      *pBegin = buffer;
      *pEnd = buffer + length - 2;
   }
}

// Macros for register mapping (only for micro stuff)
//
#if defined(IS_MICRO_STUFF)

typedef int REGISTER_ID;
typedef int INTERRUPT_ID;

#define DECLARE_REGISTERS \
enum { 

#define END_REGISTERS \
, _nOfRegisters };

#define REG(a) (PRIVATE::_Variables[PRIVATE::_Instance_index]._registers[a])


#define REGISTERS_VIEW \
int GetRegisterInfo(int pIndex, WORD8 **pWord8, \
   const char **b7, const char **b6, const char **b5, const char **b4,\
   const char **b3, const char **b2, const char **b1, const char **b0) \
{

#define DISPLAY(a, c, d, e, f, g, h, i, j, k)\
   if(pIndex == a) {\
      *pWord8 = PRIVATE::_Variables ? &REG(a) : NULL;\
      *b7=""#d""; *b6=""#e""; *b5=""#f""; *b4=""#g""; *b3=""#h""; *b2=""#i""; *b1=""#j""; *b0=""#k""; \
      return c; \
   }

#define HIDDEN(a) DISPLAY(a, 0, *, *, *, *, *, *, *, *)

#define END_VIEW \
return -1; \
}

#define FOREACH_REGISTER(a) for(int a = 0; a < _nOfRegisters; a++)

#define DECLARE_INTERRUPTS enum {
#define END_INTERRUPTS };


#endif // IS_MICRO_STUFF

// I/O and port definitions. Ports are defined only in peripherals
//
//#if !defined(IS_MICRO_STUFF) // Extended geral pins support 
#if !defined(IS_MICRO_STUFF) || defined(IS_PERIPHERAL) 
#define ANALOG_IN(name, index)   _PIN_ENTRY(name, index, 0x0400, "@AI")
#define ANALOG_OUT(name, index)  _PIN_ENTRY(name, index, 0x0800, "@AO")
#define DIGITAL_IN(name, index)  _PIN_ENTRY(name, index, 0x0100, "@DI")
#define DIGITAL_OUT(name, index) _PIN_ENTRY(name, index, 0x0200, "@DO")
#define DIGITAL_BID(name, index) _PIN_ENTRY(name, index, 0x0200, "@DB")
#endif

#ifdef IS_PERIPHERAL 
#define MICRO_PORT(name, index)  _PIN_ENTRY(name, index, 0x0200, "@MP")
#endif

#define KEEP_VOLTAGE 1.0E24

#define USE_WINDOW(n) int GetTheWindow() { return n; }

// DLL private variables
//
namespace PRIVATE {
   static void (*_Print)(const char *) = NULL;
   static void (*_Set_logic)(ELEMENT, PIN, LOGIC, double) = NULL;
   static void (*_Set_voltage)(ELEMENT, PIN, double) = NULL;
   static LOGIC (*_Get_logic)(ELEMENT, PIN) = NULL;
   static void (*_Set_drive)(ELEMENT, PIN, BOOL) = NULL;
   static double (*_Get_voltage)(ELEMENT, PIN) = NULL;
   static void (*_Remind_me)(ELEMENT, double, int) = NULL;
   static HWND (*_Get_handle)(ELEMENT, int);
   static double (*_Get_param)(ELEMENT, unsigned long) = NULL;
   static void (*_Break)(ELEMENT, const char *) = NULL;
   static const char * (*_Get_instance)(ELEMENT) = NULL;
   static void (*_Trace)(ELEMENT, BOOL) = NULL;
   
   static int (*_Takeover_port)(ELEMENT, PORT, BOOL, UINT) = NULL;
   static int (*_Set_port)(ELEMENT, PORT, LOGIC) = NULL;
   static void (*_Set_interrupt_flag)(ELEMENT, int, int, int) = NULL;
   static void (*_Remind_me2)(ELEMENT, CYCLES, int) = NULL;
   static BOOL (*_Set_interrupt_vectors)(ELEMENT, ADDRESS) = NULL;
   static int  (*_Get_fuse)(ELEMENT, const char *) = NULL;
   static int _Version = 0;
   static BOOL (*_Set_clock)(ELEMENT, double, int) = NULL;
   static double (*_Get_clock)(ELEMENT) = NULL;
   static double (*_Get_temp)() = NULL;
   
   static void (*_Notify)(const char *, int) = NULL;
   static void (*_Set_interrupt_enable)(ELEMENT, int, BOOL) = NULL;
   static void (*_Warning)(ELEMENT, const char *, int, int) = NULL;
   
   static WORD8 * (*_Get_micro_data)(ELEMENT, int, ADDRESS) = NULL;      
   static int (*_Get_micro_info)(ELEMENT, int) = NULL;
   static BOOL (*_Get_drive)(ELEMENT, PIN) = NULL;        
   static BOOL (*_Set_port_attri)(ELEMENT, PORT, UINT, UINT); 

   static int _Create_calls = 0;
   static int _Instance_index = 0;
   static int _N_instances = 0;
   static ELEMENT _Element = NULL;
   static double _Power = 0.0;
   static double _Temp = 0.0;  // Keept to accept old DLLs. New TEMP() uses function, instead

   void _Set_pin_bounds();
   void _Alloc_var();
   void _Free_var();
}

// Exported functions
//
#define DLL_EXPORT extern "C" __declspec(dllexport)

//  Private callback functions
//
DLL_EXPORT unsigned long InitDll(
	ELEMENT, void *, void *, void *, void *, void *, void *,
    void *, void *, void *, void *, void *, void *,

    void *, void *, void *, void *, void *, void *, int, 
    void *, void *, void *, void *, void *, void *, void *, 
	void *, void *, void *
);   
DLL_EXPORT void SetInstance(ELEMENT, int);
DLL_EXPORT void StartSimulation(double, double);
DLL_EXPORT const char *Create();
DLL_EXPORT void Destroy();
DLL_EXPORT void GetPins(const char **, const char **);
DLL_EXPORT int GetTheWindow();

// User callback functions
//
DLL_EXPORT void On_simulation_end();
DLL_EXPORT void On_digital_in_edge(PIN, EDGE, double);
DLL_EXPORT double On_voltage_ask(PIN, double);
DLL_EXPORT void On_time_step(double);
DLL_EXPORT void On_remind_me(double, int);
DLL_EXPORT void On_gadget_notify(GADGET, int);
DLL_EXPORT void On_window_init(HWND);
DLL_EXPORT void On_update_tick(double);
DLL_EXPORT void On_break(BOOL);

// New callback functions for micro peripherals definition
//
#ifdef IS_MICRO_STUFF
DLL_EXPORT int GetRegisterInfo(int, WORD8 **,
   const char **, const char **, const char **, const char **,  // Internal, not for
   const char **, const char **, const char **, const char **   // user interface
);

#ifdef IS_PERIPHERAL
DLL_EXPORT void On_port_edge(PORT, EDGE, double);               // On_port_edge() works in a different
#else                                                           // way in the dummy peripheral
DLL_EXPORT void On_port_edge(const char *, int, EDGE, double);  // 
#endif

DLL_EXPORT int  On_instruction(int pCode);
DLL_EXPORT void On_interrupt_start(INTERRUPT_ID);
DLL_EXPORT WORD8 *On_register_read(REGISTER_ID);
DLL_EXPORT void On_register_write(REGISTER_ID, WORD8);
DLL_EXPORT void On_reset(int pMode);
DLL_EXPORT void On_sleep(int pMode);
DLL_EXPORT void On_notify(int pWhat);
DLL_EXPORT void On_clock_change(double pValue);
#endif

// Parameters for On_reset(...) and On_instructio(...) CFs
//
#define RESET_UNKNOWN  0         // Codes for reset used in On_reset() and RESET()
#define RESET_POWERON  1
#define RESET_EXTERNAL 2
#define RESET_BROWNOUT 3
#define RESET_WATCHDOG 4

#define SLEEP_DENIED  1          // Codes for sleep mode, On_sleep()	
#define SLEEP_EXIT    2
#define SLEEP_IDLE 3
#define SLEEP_NOISE_REDUCTION 4
#define SLEEP_POWERDOWN 5
#define SLEEP_POWERSAVE 6
#define SLEEP_STANDBY 7
#define SLEEP_DONE 0

#define INSTR_SLEEP 1      // Code for instructions handled with On_instruction()
#define INSTR_SPM   2
#define INSTR_LPM   3
#define INSTR_WDR   4

#define SPM_WRITE_BUFFER 0x01   // SPM instruction actions
#define SPM_ERASE_PAGE 0x03
#define SPM_WRITE_PAGE 0x05
#define SPM_DENIED 0

#define IV_STANDARD_RESET 0    // For IVSEL handling
#define IV_BOOT_RESET 1

#define FLAG_CLEAR 0   // Action parameters for Set_interrupt_flag
#define FLAG_SET   1
#define FLAG_UNLOCK 2
#define FLAG_LOCK 3
//#define FLAG_ENABLE  // Integrate also the enable/disable here ???
//#define FLAG_DISABLE
#define _RESET_ID 1024  // Internal flag to code reset action	

// Warning categories, for WARNING() function
//
#define CAT_MEMORY   1    // Memory access related errors
#define CAT_UART     2    // UART related errors
#define CAT_ADC      3    // ...
#define CAT_WATCHDOG 4
#define CAT_STACK    5
#define CAT_EEPROM   6
#define CAT_SPI      7
#define CAT_TWI      8
#define CAT_TIMER    9
#define CAT_CPU     10
#define CAT_PORT    11   // ...
#define CAT_COMP    12   // Analog comparator related errors

// Warning flags, depending on the peripheral type
//
#define WARN_MISC 0x10000000  // Miscellaneous, not assigned to any particular type

// Data space (memory) access troubles
//
#define WARN_MEMORY_READ_INVALID   0x00001000  // Reading from an invalid/unimplemnted address
#define WARN_MEMORY_WRITE_INVALID  0x00002000  // Writing to   "     "          "         "
#define WARN_MEMORY_WRITE_X_IO     0x00004000  // Writing unknown bits (X) to I/O register
#define WARN_MEMORY_INDEX_X        0x00008000  // X bits in index register
#define WARN_MEMORY_INDEX_IO       0x00010000  // Access to I/O area with indexed addressing
#define WARN_MEMORY_INDEX_INVALID  0x00020000  // Index pointing to invalid address

// General codes, meaning applies to several peripherals
//
#define WARN_READ_OVERRUN    0x00000010   // Read faster than allowed
#define WARN_WRITE_OVERRUN   0x00000020   // Write faster than allowed
#define WARN_READ_BUSY       0x00000040   // Read peripheral while busy
#define WARN_WRITE_BUSY      0x00000080   // Write to peripheral while busy
#define WARN_PARAM_BUSY      0x00000100   // Changing parameters/modes while busy
#define WARN_PARAM_RESERVED  0x00000200   // Programming reserved of unsupported modes
#define WARN_PARAM_BITRATE   0x00000400   // Programmed baud/bit-rate not good

// ADC specific
//
#define WARN_ADC_CLOCK       0x00001000   // Invalid clock
#define WARN_ADC_REFERENCE   0x00002000   // Invalid reference
#define WARN_ADC_SHORT       0x00004000   // ADC short through wrong multiplexing
#define WARN_ADC_CHANNEL     0x00008000   // Invalid channel selected
#define WARN_ADC_POWDOWN     0x00010000   // ADC in powerdown
#define WARN_ADC_UNSTABLE    0x00020000   // Conversion unstable for some reason

// UARTs specific
//
#define WARN_UART_FRAMING    0x00001000
#define WARN_UART_BAUDRATE   0x00002000

// Watchdog specific
//
#define WARN_WATCHDOG_SUSPICIOUS_USE 0x00001000

// EEPROM Specific
//
#define WARN_EEPROM_ADDRES_OUTSIDE     0x00001000   // Address outside range
#define WARN_EEPROM_DANGER             0x00002000   // Dangerous operation
#define WARN_EEPROM_SIMULTANEOUS_RW    0x00004000   // Simultaneous R/W

// Timers specific
//
#define WARN_TIMERS_OUTPUT     0x00001000       // Wrong configuration for cap/com signal
#define WARN_TIMERS_16BIT_READ     0x00002000   // Wrong sequence for 16 bits read operation
#define WARN_TIMERS_16BIT_WRITE    0x00004000   // Wrong sequence for 16 bits write operation


// Mandatory callbacks, called from private handlers
//
void On_simulation_begin();
const char *On_create();
void On_destroy();

// Interface Functions (IF)
//
inline double POWER() {return PRIVATE::_Power;}
//inline double TEMP() {return PRIVATE::_Temp;} // Modified allow dynamic temp change, but _Temp variable kept for compatibility of old DLLs
inline double TEMP() {return (*PRIVATE::_Get_temp)();}
inline const char * GET_INSTANCE() {return (*PRIVATE::_Get_instance)(PRIVATE::_Element); }
inline HWND GET_HANDLE(int pId) {return (*PRIVATE::_Get_handle)(PRIVATE::_Element, pId);}
inline void BREAK(const char *pMessage = NULL) {(*PRIVATE::_Break)(PRIVATE::_Element, pMessage);}
inline void PRINT(const char *text) {(*PRIVATE::_Print)(text);}
inline void REMIND_ME(double delay, int param = 0) {(*PRIVATE::_Remind_me)(PRIVATE::_Element, delay, param);}
inline void SET_LOGIC(PIN pin, LOGIC value, double delay = 0) {(*PRIVATE::_Set_logic)(PRIVATE::_Element, pin, value, delay);}
inline void SET_VOLTAGE(PIN pin, double value) {(*PRIVATE::_Set_voltage)(PRIVATE::_Element, pin, value);}
inline LOGIC GET_LOGIC(PIN pin) {return (*PRIVATE::_Get_logic)(PRIVATE::_Element, pin); }
inline double GET_VOLTAGE(PIN pin) {return (*PRIVATE::_Get_voltage)(PRIVATE::_Element, pin);}
inline void TRACE(BOOL enable) {(*PRIVATE::_Trace)(PRIVATE::_Element, enable); }
inline void SET_DRIVE(PIN pin, BOOL enable) {(*PRIVATE::_Set_drive)(PRIVATE::_Element, pin, enable);}
inline double GET_CLOCK() {return (*PRIVATE::_Get_clock)(PRIVATE::_Element);}; // Requested to be available by everyone
inline BOOL GET_DRIVE(PIN pin) {return (*PRIVATE::_Get_drive)(PRIVATE::_Element, pin);}


// New functions to define micro  peripherals
//
#ifdef IS_MICRO_STUFF
#ifdef IS_PERIPHERAL
inline int SET_PORT(PORT port, LOGIC value) {return (*PRIVATE::_Set_port)(PRIVATE::_Element, port, value);}
inline BOOL SET_PORT_ATTRI(PORT port, UINT add, UINT remove = 0) {return (*PRIVATE::_Set_port_attri)(PRIVATE::_Element, port, add, remove);}
//inline int SET_PORT_ATTRI(PORT port, int value) {return (*PRIVATE::_Set_port)(PRIVATE::_Element, port, value << 16);}
inline int TAKEOVER_PORT(PORT port, BOOL status, UINT options = 0) {return (*PRIVATE::_Takeover_port)(PRIVATE::_Element, port, status, options);}
#endif
inline void SET_INTERRUPT_FLAG(int id, int action) {(*PRIVATE::_Set_interrupt_flag)(PRIVATE::_Element, id, action, 0);}  // 0 = aux for future options
inline void RESET(int action) {(*PRIVATE::_Set_interrupt_flag)(PRIVATE::_Element, _RESET_ID, action, 0);}
inline void REMIND_ME2(UINT nCycles, int param = 0) {(*PRIVATE::_Remind_me2)(PRIVATE::_Element, nCycles, param);}
inline BOOL SET_INTERRUPT_VECTORS(ADDRESS addr) {return (*PRIVATE::_Set_interrupt_vectors)(PRIVATE::_Element, addr);}
inline int  GET_FUSE(const char *name) {return (*PRIVATE::_Get_fuse)(PRIVATE::_Element, name);}
inline int  VERSION() {return PRIVATE::_Version;}
inline BOOL SET_CLOCK(double value, int rCycles = 0) {return (*PRIVATE::_Set_clock)(PRIVATE::_Element, value, rCycles);};
inline void NOTIFY(const char *instance, int aux) {(*PRIVATE::_Notify)(instance, aux);};
inline void SET_INTERRUPT_ENABLE(int id, BOOL value) {(*PRIVATE::_Set_interrupt_enable)(PRIVATE::_Element, id, value);};
inline void WARNING(const char *text, int cat, int flags) {(*PRIVATE::_Warning)(PRIVATE::_Element, text, cat, flags);};
inline WORD8 * GET_MICRO_DATA(int what, ADDRESS addr) { return (*PRIVATE::_Get_micro_data)(PRIVATE::_Element, what, addr);}
inline int GET_MICRO_INFO(int what) {return (*PRIVATE::_Get_micro_info)(PRIVATE::_Element, what);}

#else
inline double GET_PARAM(unsigned long index) {return (*PRIVATE::_Get_param)(PRIVATE::_Element, index);}
#endif      // this function is not allowed in PERIPHERAL MODE, use VERSION()


unsigned long InitDll(ELEMENT pElement, void *setLogic, void *setVoltage, void *getLogic,
   void *getVoltage, void *pPrint, void *pRemindMe, void *getParam, void *pBreak,
   void *pGetHandle, void *pGetInstance, void *pTrace, void *pSetDrive,
  
  // New IF introduced for rel 3.15
  // 
  void *pSetInterruptFlag, void *pSetPort, void *pRemindMe2, 
  void *pSetInterruptVectors, void *pGetFuse, void *pTakeoverPort, int pVersion, void *pSetClock,
  void *pNotify, void *pSetInterruptEnable, void *pWarning, void *pGetClock, void *pGetTemp,
  void *pGetData, void *pGetMicroInfo, void *pGetDrive, void *pSetPortAttri
)

//******************************************************************************
// DLL initializer from VMLAB. Returns instance counter and release
{
   if(PRIVATE::_N_instances++ == 0) {
	  PRIVATE::_Element = pElement;
      PRIVATE::_Set_logic = (void(*)(ELEMENT, PIN, LOGIC, double))setLogic;
      PRIVATE::_Set_voltage = (void(*)(ELEMENT, PIN, double))setVoltage;
      PRIVATE::_Get_logic = (LOGIC (*)(ELEMENT, PIN))getLogic;
      PRIVATE::_Get_voltage = (double (*)(ELEMENT, PIN))getVoltage;
      PRIVATE::_Print = (void (*)(const char *))pPrint;
      PRIVATE::_Remind_me = (void (*)(ELEMENT, double, int))pRemindMe;
      PRIVATE::_Get_param = (double (*)(ELEMENT, unsigned long))getParam;
      PRIVATE::_Break = (void (*)(ELEMENT, const char *))pBreak;
      PRIVATE::_Get_handle = (HWND (*)(ELEMENT, int))pGetHandle;
      PRIVATE::_Get_instance = (const char * (*)(ELEMENT)) pGetInstance;
      PRIVATE::_Trace = (void (*)(ELEMENT, BOOL)) pTrace;
      PRIVATE::_Set_drive = (void (*)(ELEMENT, PIN, BOOL)) pSetDrive;
	  
//    New IFs; will come as NULL if not allowed 

	  PRIVATE::_Set_interrupt_flag = (void (*)(ELEMENT, int, int, int)) pSetInterruptFlag;
      PRIVATE::_Set_port = (int (*)(ELEMENT, PIN, LOGIC)) pSetPort;
	  PRIVATE::_Remind_me2 = (void (*)(ELEMENT, CYCLES, int)) pRemindMe2;
	  PRIVATE::_Set_interrupt_vectors = (BOOL (*)(ELEMENT, ADDRESS)) pSetInterruptVectors;
	  PRIVATE::_Get_fuse = (int (*)(ELEMENT, const char *)) pGetFuse;
      PRIVATE::_Takeover_port = (int (*)(ELEMENT, PORT, BOOL, UINT)) pTakeoverPort;
      PRIVATE::_Version = pVersion;
	  PRIVATE::_Set_clock = (BOOL (*)(ELEMENT, double, int)) pSetClock;
	  PRIVATE::_Get_clock = (double (*)(ELEMENT)) pGetClock;	  
	  PRIVATE::_Notify = (void (*)(const char *, int)) pNotify;
	  PRIVATE::_Set_interrupt_enable = (void (*)(ELEMENT, int, BOOL)) pSetInterruptEnable;     
	  PRIVATE::_Warning = (void (*)(ELEMENT, const char *, int, int)) pWarning;
	  PRIVATE::_Get_temp = (double (*)())pGetTemp;
	  PRIVATE::_Get_micro_data = (WORD8 * (*)(ELEMENT, int, ADDRESS)) pGetData;
	  PRIVATE::_Get_micro_info =  (int (*)(ELEMENT, int)) pGetMicroInfo;	  	  
	  PRIVATE::_Get_drive = (BOOL (*)(ELEMENT, PIN)) pGetDrive;
	  PRIVATE::_Set_port_attri = (BOOL (*)(ELEMENT, PORT, UINT, UINT)) pSetPortAttri;
   }
   return MAKELONG(PRIVATE::_N_instances, RELEASE_CODE);
}

void SetInstance(ELEMENT pElement, int pInstance)
// **********************************************
{
	PRIVATE::_Element = pElement;
	PRIVATE::_Instance_index = pInstance;
}

void StartSimulation(double pPower, double pTemp)
// **********************************************
{
   PRIVATE::_Power = pPower;
   PRIVATE::_Temp = pTemp;  // Kept for compatibility. New DLLs use _Get_temp() (dynamic)
   On_simulation_begin();
}


const char *Create()
//******************
{
   if(PRIVATE::_Create_calls++ == 0) {
      PRIVATE::_Alloc_var();
   }
   return On_create();
}

void Destroy()
//************
{
   On_destroy();
   if(--PRIVATE::_Create_calls == 0) {
      PRIVATE::_Free_var();
   }
}

#if !defined(_WORD8_LINKED) 

// Implementation of simplified standalone version of WORD8 class if not linked
// ============================================================================
// 

namespace PRIVATE {

template <typename T, unsigned int M>
class WORDX : public T
//*********
// Generic class to model any numeric type that can have undefined bit status
// The template parameter "T" specifies the base class which controls actual
// data layout in memory. The base class also provides the public d() and x()
// methods used by this class to access the data and known bit flags. Finally,
// the base class provides a "TYPE" typedef representing the primitive data
// type (i.e. unsigned char) which this class represents. All intermediate
// bitwise operations are carried out as WORD32 instances to avoid bit
// truncation. When smaller values are converted to larger ones (such as the
// intermediate WORD32), the upper bits are set to a known state and a value
// of 0. Template parameter "M" is a bitmask corresponding to the primitive
// size of the WORDX instance. For exmaple WORD8::MASK == 0xFF.
{
public:
   // When converting from this WORDX instance to a larger bit-width, this
   // mask is used to set the upper bits of the larger bit-width WORDX to
   // a known state (and 0 data value). For example, if a WORD8 is converted
   // to a WORD32, then the high order bits 8-31 in the WORD32 are set to
   // a known zero state.
   static const unsigned int MASK;

   inline WORDX() : T(0, 0) {}
   //**************************************
   // Default constructor. All bits set to UNKNOWN.

   inline WORDX(typename T::TYPE pInteger) : T(-1, pInteger) {}
   //**************************************
   // Constructor, upon an integer. Bits are known.
   
   inline WORDX(typename T::TYPE pDefined, typename T::TYPE pData) : T(pDefined, pData) {}
   //**************************************
   // Constructor, upon two values

   template<typename U, unsigned int N>
   inline operator WORDX<U, N> () const
   //******************************
   // Generic type cast operator to other WORDX bit widths/layouts
   {
      return WORDX<U, N>(~MASK | x(), d());
   }

   LOGIC get_bit(UINT pBit) const
   //******************************
   // Gets the pBit position as a logic value
   {
      TYPE mask = 1 << pBit;

      if(x() & mask) {
         return (d() & mask) ? 1 : 0;
      } else {
         return UNKNOWN;
      } 
   }

   void toggle_bit(UINT pBit)
   //********************************
   // Toggles a given bit. Does not change x value
   {
      d(d() ^ (1 << pBit));
   }

   void set_bit(UINT pBit, LOGIC pValue)
   //*******************************************
   // Sets the given bit in pBit to the given LOGIC parameter
   {
      TYPE mask = 1 << pBit;

      if(pValue == 0) {
         x(x() | mask);
         d(d() & ~mask);
      } else if(pValue == 1) {
         x(x() | mask);
         d(d() | mask);
      } else if(pValue == UNKNOWN) {
         x(x() & ~mask);
      }
   }

   int get_field(UINT pMsb, UINT pLsb) const
   //*****************************************
   // Extract a n-bits field from pLSB to pMSB.
   {
      TYPE data = d() >> pLsb;   
      TYPE known = x() >> pLsb;
      TYPE mask = (1 << (pMsb - pLsb + 1)) - 1;

      if(mask != (known & mask)) {
         return -1;
      }
      
      return data & mask;
   }

   void set_field(UINT pMsb, UINT pLsb, int pValue) 
   //*****************************************
   // Set a n-bits field from pLSB to pMSB to known status and
   // with pValue data. If pValue is negative the n-bits are
   // zeroed and set to UNKNOWN status.
   {
      TYPE mask = ((1 << (pMsb - pLsb + 1)) - 1) << pLsb;

      d(d() & ~mask);
      if(pValue < 0) {
         x(x() & ~mask);
      } else {   
         x(x() | mask);
         d(d() | ((pValue << pLsb) & mask));
      }
   }

   const WORD32 operator & (const WORD32 &p) const
   //*************************************
   {
      UINT x32 = ~MASK | x(); // Convert this object's x value to 32 bits
      return WORD32( (x32 & p.x()) | (~d() & x32) | (~p.d() & p.x()), d() & p.d() );
   }

   const WORD32 operator | (const WORD32 &p) const
   //*************************************
   {
      UINT x32 = ~MASK | x(); // Convert this object's x value to 32 bits
      return WORD32( (x32 & p.x()) | (d() & x32) | (p.d() & p.x()), d() | p.d() );
   }
   
   const WORD32 operator ^ (const WORD32 &p) const
   //**************************************
   {
      UINT x32 = ~MASK | x(); // Convert this object's x value to 32 bits
      return WORD32( x32 & p.x(), d() ^ p.d() );
   }

   const WORD32 operator << (int pInteger) const
   //**************************************
   {
      UINT x32 = ~MASK | x(); // Convert this object's x value to 32 bits
      UINT mask = (1 << pInteger) - 1;
      return WORD32( (x32 << pInteger) | mask, d() << pInteger );
   }

   const WORD32 operator >> (int pInteger) const
   //**************************************
   {
      UINT x32 = ~MASK | x(); // Convert this object's x value to 32 bits
      UINT mask = -1 >> pInteger;
      return WORD32( ~mask | (x32 >> pInteger), d() >> pInteger );
   }
   
   bool operator == (const WORD32 &p) const
   //**************************************
   {
      // (unsigned) stops a compiler warning about signed/unsigned comparison
      return p.d() == (unsigned)d() && known() && p.known();
   }
   
   bool operator != (const WORD32 &p) const
   //**************************************
   {  
      // (unsigned) stops a compiler warning about signed/unsigned comparison
      return p.d() != (unsigned)d() && known() && p.known();
   }
   
   inline LOGIC operator [] (UINT pIndex) const
   //**************************************
   {
      return get_bit(pIndex);
   }
   
   inline BOOL known() const
   //**************************************
   {
      return x() == ((TYPE) -1);
   }
};

// Static variable WORDX::MASK is initialized with WORDX template parameter "M"
template<typename T, unsigned int M> const unsigned int WORDX<T, M>::MASK = M;

template <typename T>
class WORDN
//*********
// Base class for WORDX to represent a "native" data layout using primitive
// types (e.g. unsigned int) to hold the data and known bit status. The
// template paramater "T" specifies the primitive type.
{
private:
	T _x;  // Says, by bit, if data is defined: 1=defined; 0=undefined
	T _d;  // The real data

protected:
   inline WORDN(T pDefined, T pData) : _x(pDefined), _d(pData) {}
   //******************************
   // Constructor used by WORDX class to initialize "x" and "d" variables
   
public:
   typedef T TYPE; // Publicly visible primitive type used by x() and d()
   
   //******************************
   // Access methods needed by WORDX class to read/write the x/d fields
   inline T d() const { return _d; }
   inline T x() const { return _x; }
   inline void d(T pInteger) { _d = pInteger; }
   inline void x(T pInteger) { _x = pInteger; }
};

template<typename T>
class WORDLE
//*********
// Helper class used by WORDHL to provide little-endian data layout. Template
// parameter "T" is a WORDX type used to create the larger WORDHL instance.
{
   public:
      typedef typename T::TYPE TYPE;
      T Low;
      T High;
};

template<typename T>
class WORDBE
//*********
// Helper class used by WORDHL to provide big-endian data layout. Template
// parameter "T" is a WORDX type used to create the larger WORDHL instance.
{
   public:
      typedef typename T::TYPE TYPE;
      T High;
      T Low;
};

template <typename Tp, typename Tw>
class WORDHL
//*********
// Base class for WORDX to represent a larger data type (e.g. 16-bit) by using
// a pair of smaller WORDX data types (typically WORD8) arranged in either a
// little or big endian fasion. An array of WORD8 datatypes can be cast to
// a pointer of this type to reinterpret the WORD8 array as a larger data type.
// This class may also be instantiated by itself, and then converted to a
// WORD8 array pointer, allowing access to the individual bytes making up the
// larger WORDHL data type. Template parameter "Tp" specifies the primitive
// type (e.g. unsigned short) of the larger data type. Parameter "Tw" is
// either the WORDLE or WORDBE class to specify the little/big endian layout
// and specify the smaller WORDX data type.
{
private:
   Tw Word; // Underlying little/big endian data layout

protected:
   inline WORDHL(Tp pDefined, Tp pData) { x(pDefined); d(pData); }
   //******************************
   // Constructor used by WORDX class to initialize "word" array

public:
   typedef Tp TYPE; // Publicly visible primitive type used by x() and d()

   inline void d(Tp pInteger)
   //**************************************
   // Integer assignments to WORDHL class are forwarded to high/low words
   {
      Word.High.d(pInteger >> (sizeof(Tw::TYPE) * 8));
      Word.Low.d(pInteger);
   }

   inline void x(Tp pInteger)
   //**************************************
   // Integer assignments to WORDHL class are forwarded to high/low words
   {
      Word.High.x(pInteger >> (sizeof(Tw::TYPE) * 8));
      Word.Low.x(pInteger);
   }
   
   inline Tp d() const
   //**************************************
   // Converting to literal integer form high/low words
   {
      return (Word.High.d() << (sizeof(Tw::TYPE) * 8)) | Word.Low.d();
   }

   inline Tp x() const
   //**************************************
   // Converting to literal integer form high/low words
   {
      return (Word.High.x() << (sizeof(Tw::TYPE) * 8)) | Word.Low.x();
   } 
};

} // namespace PRIVATE

// Allow binary operations with integer on left hand side
inline WORD32 operator | (UINT pL, const WORD32 &pR) { return pR | pL; }
inline WORD32 operator & (UINT pL, const WORD32 &pR) { return pR & pL; }
inline WORD32 operator ^ (UINT pL, const WORD32 &pR) { return pR ^ pL; }
inline BOOL operator == (UINT pL, const WORD32 &pR) { return pR == pL; }
inline BOOL operator != (UINT pL, const WORD32 &pR) { return pR != pL; }
#endif // if not _WORD8_LINKED

#endif // #ifdef(RC_INVOKED)

// MANDATORY identifier values for Windows controls in the resource file
//
#define GADGET_FIRST 100
#define GADGET0  (GADGET_FIRST + 0)
#define GADGET1  (GADGET_FIRST + 1)
#define GADGET2  (GADGET_FIRST + 2)
#define GADGET3  (GADGET_FIRST + 3)
#define GADGET4	 (GADGET_FIRST + 4)
#define GADGET5	 (GADGET_FIRST + 5)
#define GADGET6	 (GADGET_FIRST + 6)
#define GADGET7	 (GADGET_FIRST + 7)
#define GADGET8  (GADGET_FIRST + 8)
#define GADGET9  (GADGET_FIRST + 9)
#define GADGET10 (GADGET_FIRST + 10)
#define GADGET11 (GADGET_FIRST + 11)
#define GADGET12 (GADGET_FIRST + 12)
#define GADGET13 (GADGET_FIRST + 13)
#define GADGET14 (GADGET_FIRST + 14)
#define GADGET15 (GADGET_FIRST + 15)
#define GADGET16 (GADGET_FIRST + 16)
#define GADGET17 (GADGET_FIRST + 17)
#define GADGET18 (GADGET_FIRST + 18)
#define GADGET19 (GADGET_FIRST + 19)
#define GADGET20 (GADGET_FIRST + 20)
#define GADGET21 (GADGET_FIRST + 21)
#define GADGET22 (GADGET_FIRST + 22)
#define GADGET23 (GADGET_FIRST + 23)
#define GADGET24 (GADGET_FIRST + 24)
#define GADGET25 (GADGET_FIRST + 25)
#define GADGET26 (GADGET_FIRST + 26)
#define GADGET27 (GADGET_FIRST + 27)
#define GADGET28 (GADGET_FIRST + 28)
#define GADGET29 (GADGET_FIRST + 29)
#define GADGET30 (GADGET_FIRST + 30)
#define GADGET31 (GADGET_FIRST + 31)

#ifdef WIDTH    // Reported that some compiler has a predefined value for this
#undef WIDTH    // constant. What is the use?
#endif          // Keept for compatibility. To avoid problmems, width and height
#ifdef HEIGHT   // are named as W_CELL / H_CELL in the new .RC files generated
#undef HEIGHT   // automatically
#endif

#define WIDTH 251  // Assigned window width. Do not modify.
#define HEIGHT 67  // Default height; this can be modified

#ifdef IS_PERIPHERAL
#undef WIDTH    
#define WIDTH 171 
#endif

#ifdef IS_DUMMY_PERIPHERAL
#undef WIDTH
#define WIDTH 232
#endif

// MANDATORY identifiers for DIALOG IDs (Resource Compiler)
//
#define WINDOW_USER_1 20000
#define WINDOW_USER_2 20001
#define WINDOW_USER_3 20002
#define WINDOW_USER_4 20003


// Expand button and frame
//
#define EXPAND_FRAME  20706
#define EXPAND_BUTTON 771


#endif // _BLACKBOX
