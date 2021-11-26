#include "Arduino.h"
#include <EEPROM.h>
#define SCPI_BUFFER_LENGTH 254
#define SCPI_MAX_TOKENS 25
#define SCPI_ARRAY_SIZE 30
#define SCPI_MAX_COMMANDS 20
#include "Vrekrer_scpi_parser_v2.h"
#include <util/delay_basic.h>

#define NOP __asm__ __volatile__ ("nop\n\t")
#define MILIS_DELAY_CYCLES 15977ul
#define MIN_CYCLES_DELAY 17
#define DELAY_OFFSET 61
//TODO: move DELAY_OFFSET to eeprom
#define MAX_PUlSE_SEQ_LEN 50
#define SEQ_ERR_NAN 1
#define SEQ_ERR_TIME 2
#define SEQ_ERR_CH 4
#define SEQ_ERR_MEM 8
#define PORTS_AVAILABLE 0x1FFC
#define CYCLES_S F_CPU
#define CYCLES_MS (F_CPU / 1000ul)
#define CYCLES_US (F_CPU / 1000000ul)
#define EEPROM_TIME_UNIT_IDX 128

inline void delayMilis(uint16_t& ms) __attribute__((always_inline));
inline void delayCycles(uint16_t& cycles)  __attribute__((always_inline));
void printError(Stream& interface = Serial);

uint16_t    val[MAX_PUlSE_SEQ_LEN];
uint16_t    milisDelays[MAX_PUlSE_SEQ_LEN];
// uint32_t    cyclesDelays[MAX_PUlSE_SEQ_LEN];
uint8_t     delay3Cycles[MAX_PUlSE_SEQ_LEN];
uint16_t    delay4Cycles[MAX_PUlSE_SEQ_LEN];
SCPI_Parser parser;
int32_t     times[MAX_PUlSE_SEQ_LEN];
uint16_t    switches[MAX_PUlSE_SEQ_LEN];
uint16_t    seqLen;
int         seqError;
uint16_t    startVal;
uint32_t    timeInputMp;
char        unitPref;

void setup()
{
  //default all available digital ports to outputs
  DDRD |= PORTS_AVAILABLE;
  DDRB |= (PORTS_AVAILABLE >> 8);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);  
  seqLen = 0;  
  //timeInputMp= 1;
  recallTimeInputMp();
  parser.RegisterCommand(F("*IDN?"), &identify);
  parser.RegisterCommand(F("*RST"), &fullReset);
  //OUTPut:[X]ON <port>, {port}
  parser.RegisterCommand(F("OUTPut:ON"), &outputOn);
  parser.RegisterCommand(F("OUTPut:XON"), &outputOn);
  //OUTPut:OFF {port}
  parser.RegisterCommand(F("OUTPut:OFF"), &outputOff);
  //PULSe:RUN
  parser.RegisterCommand(F("PULSe:RUN"), &pulseRun);
  //PULSe[?]:[(ADD)|XADD] <port>, <time>, {time}
  parser.RegisterCommand(F("PULSe"), &pulseAdd);
  parser.RegisterCommand(F("PULSe?"), &pulseQuery);
  parser.RegisterCommand(F("PULSe:ADD"), &pulseAdd);
  parser.RegisterCommand(F("PULSe:XADD"), &pulseAdd);
  //PULSe:RESet [port]
  parser.RegisterCommand(F("PULSe:RESet"), &pulseReset);
  //SYSTem:UNIT[?] <cycle|us|ms|s>
  parser.RegisterCommand(F("SYSTem:UNIT"), &setUnit);
  parser.RegisterCommand(F("SYSTem:UNIT?"), &getUnit);
  //SYSTem:UNIT:<STORe|RECall>
  parser.RegisterCommand(F("SYSTem:UNIT:STORe"), &storeUnit);
  parser.RegisterCommand(F("SYSTem:UNIT:RECall"), &recallUnit);
  //PULSe:CALibrate:MILis:[ONE|TWO] [port=2]"
  parser.RegisterCommand(F("PULSe:CALibrate:MILis:ONE"), &calibMilis);
  parser.RegisterCommand(F("PULSe:CALibrate:MILis:TWO"), &calibMilis);
  Serial.begin(115200);
}

void loop()
{
  parser.ProcessInput(Serial, "\n");
}

void recallTimeInputMp(){
  unitPref = EEPROM[EEPROM_TIME_UNIT_IDX];
  if(unitPref == 's')
    timeInputMp = CYCLES_S;
  else if(unitPref == 'm')
    timeInputMp = CYCLES_MS;
  else if(unitPref == 'u')
    timeInputMp = CYCLES_US;
  else if(unitPref == 'c')
    timeInputMp = 1;
  else{
    //default to ms
    unitPref = 'm';
    timeInputMp = CYCLES_MS;
  }
}


void storeUnit(SCPI_C commands, SCPI_P parameters, Stream& interface){
  (void) commands;
  (void) interface;
  (void) parameters;
  EEPROM[EEPROM_TIME_UNIT_IDX] = unitPref;
}

void recallUnit(SCPI_C commands, SCPI_P parameters, Stream& interface){
  (void) commands;
  (void) interface;
  (void) parameters;
  recallTimeInputMp();
}

void setUnit(SCPI_C commands, SCPI_P parameters, Stream& interface){
  (void) commands;
  (void) interface;

  if(parameters.Size()!=1)
    return;
  if(strcmp_P(parameters[0], PSTR("s")) == 0)
    timeInputMp = CYCLES_S;
  else if(strcmp_P(parameters[0], PSTR("ms")) == 0)
    timeInputMp = CYCLES_MS;
  else if(strcmp_P(parameters[0], PSTR("us")) == 0)
    timeInputMp = CYCLES_US;
  else if(strcmp_P(parameters[0], PSTR("cycle")) == 0)
    timeInputMp = 1;
  else
    return;
}

void getUnit(SCPI_C commands, SCPI_P parameters, Stream& interface){
  (void) commands;
  (void) parameters;

  if(timeInputMp == CYCLES_S)
    interface.println(F("s"));  
  else if(timeInputMp == CYCLES_MS)
    interface.println(F("ms"));
  else if(timeInputMp == CYCLES_US)
    interface.println(F("us"));
  else if(timeInputMp == 1)
    interface.println(F("cycle"));
  else
    interface.println(F("WARNING: unknown time unit"));
}

void identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void) commands;
  (void) parameters;
  (void) interface;
  interface.println(F("franciszek\152ura\163\100gmail\056com,Arduino pulse generator,#86,v0.3"));
}

void resetOutputs(){
  PORTD &= ~(PORTS_AVAILABLE);
  PORTB &= ~(PORTS_AVAILABLE >> 8);
}

void fullReset(SCPI_C commands, SCPI_P parameters, Stream& interface){
  (void) commands;
  (void) parameters;
  (void) interface;

  resetSeq();
  recallTimeInputMp();
  resetOutputs();
}

void resetSeq(){  
  seqLen = 0;
  seqError = 0;
}

void pulseAdd(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void)  commands;
  (void)  interface;

  if(parameters.Size() == 0)
    return;
  int ch = parseChannel(parameters[0]);
  if(ch < 0){
    seqError |= SEQ_ERR_CH;
    return;
  }
  if(strcasecmp_P(commands.Last(), PSTR("xadd")) == 0)
    resetChannel(ch);
  if(seqError)
    return;
  for(int i = 1; i < parameters.Size(); ++i){
    int32_t time = parseTime(parameters[i]);
    // Serial.print(F("Parsed time: "));
    // Serial.println(time);
    if(seqError)
      return;
    else
      seqAdd(ch, time);
  }
}


int32_t parseTime(const char * str){
  int32_t intPart;
  int64_t fracPart;
  char *  end;
  char *  beg;

  intPart = strtol(str, &end, 10);
  intPart *= timeInputMp;
  if(end[0] == '\0' || ((end[0] == '.') && (end[1] == '\0')))
    return intPart;
  if(end[0] != '.' || end[1] == '-'){
    seqError |= SEQ_ERR_NAN;
    return 0;
  }
  beg = end + 1;
  fracPart = strtol(beg, &end, 10);
  if(end[0] != '\0' || fracPart < 0){
    seqError |= SEQ_ERR_NAN;
    return 0;
  }
  uint32_t power = 1;
  for(int i = 0; i < (end - beg); ++i)
    power *= 10;
  fracPart *= timeInputMp;
  uint32_t fracPart32 = (fracPart + (power / 2)) / power;
  return (intPart + fracPart32);
}

void seqAdd(uint8_t ch, int32_t time){
  int pos = 0;
  uint16_t mask = 1 << ch;

  // Serial.print(ch);
  // Serial.print(": ");
  // Serial.println(time);
  pos = findPosInSeq(time);
  if(pos < 0)
    return;
  if(pos != (int)seqLen && times[pos] == time){
    switches[pos] ^= mask;
    if(switches[pos] == 0)
      removeFromSeq(pos);
  }
  else
    insertInSeq(time, mask, pos);  
}

int parseChannel(const char * str){
  char *  end;
  int     ch;
  
  ch = strtol(str, &end, 10);
  if((*end != '\0') || (ch < 0) || (ch > 15))
    return -1;
  if(((1 << ch) & PORTS_AVAILABLE) == 0){
    return -1;
  }
  return ch;
}

int findPosInSeq(int32_t time){
  unsigned int i = 0;
  if(seqLen == 0)
    return 0;
  while((i < seqLen) && (times[i] < time))
    ++i;
  if((i > 0) && ((time - times[i-1]) < DELAY_OFFSET)){
    seqError |= SEQ_ERR_TIME;
    return -1;
  }
  if((i < seqLen) && ((times[i] - time) < DELAY_OFFSET) && (times[i] != time)){
    seqError |= SEQ_ERR_TIME;
    return -1;
  }
  return i;
}

int insertInSeq(int32_t time, uint16_t val, unsigned int i){
  if((i > seqLen) || seqLen == MAX_PUlSE_SEQ_LEN){
    seqError |= SEQ_ERR_MEM;
    return -1;
  }
  for(unsigned int j = seqLen; j > i; --j){
    times[j] = times[j - 1];
    switches[j] = switches[j - 1];
  }
  ++seqLen;
  times[i] = time;
  switches[i] = val;
  return 0;
}

int removeFromSeq(unsigned int i){
  if(i >= seqLen){
    seqError |= SEQ_ERR_MEM;
    return -1;
  }
  for(unsigned int j = i; j < (seqLen - 1); ++j){
    times[j] = times[j + 1];
    switches[j] = switches[j + 1];
  }
  --seqLen;
  return 0;
}

void pulseReset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void)  commands;
  (void)  interface;
  int     ch;

  if(parameters.Size() == 0){
    resetSeq();
    return;
  }
  for(int i = 0; i < parameters.Size(); ++i){
    ch = parseChannel(parameters[i]);
    if(ch > 0){
      resetChannel(ch);
    }
  }
}

void resetChannel(int ch){
  uint16_t mask = 1 << ch;
  for(unsigned int i = 0; i < seqLen; ++i){
    switches[i] &= ~mask;
    if(switches[i] == 0)
      removeFromSeq(i--);
  }
}

void printStatus(Stream& interface, bool detailed){
  if(seqError)
    printError(interface);
  else if(detailed){
    if(seqLen == 0){
      interface.println(F("WARNING: Nothing to run"));
    }
    else{
      interface.println(F("Cycle,Mask"));
      for(unsigned int i = 0; i < seqLen; ++i){
        interface.print(F(";"));
        interface.print(times[i]);
        interface.print(F(","));
        interface.print(switches[i], BIN);
      }
      interface.println("");
    }
  }
  else
    interface.println("OK");
}

void pulseQuery(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void) commands;
  (void) parameters;
  if((parameters.Size() == 1) && (strcasecmp_P(parameters.First(), PSTR("-l")) == 0))
    printStatus(interface, true);
  else
    printStatus(interface, false);
}
void pulseRun(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void) commands;
  (void) parameters;

  if(seqError){
    // printError(interface);
    resetSeq();
    return;
  }
  if(seqLen == 0){
    // interface.println(F("WARNING: Nothing to run"));
    return;
  }
  startVal = PORTB;
  startVal = startVal << 8;
  startVal |= PORTD;
  val[0] = startVal ^ switches[0];
  for(unsigned int i = 1; i < seqLen; ++i){
    val[i] = val[i - 1] ^ switches[i];
    uint32_t delay = times[i] - times[i-1] - DELAY_OFFSET;
    milisDelays[i-1] = delay / (F_CPU / 1000);
    // cyclesDelays[i-1] = (delay % (F_CPU / 1000)) + MIN_CYCLES_DELAY;
    uint16_t cycles =  (delay % (F_CPU / 1000)) + MIN_CYCLES_DELAY;
    delay3Cycles[i-1] = 4 - (cycles % 4);
    delay4Cycles[i-1] = (cycles / 4) - delay3Cycles[i-1];
    // cyclesDelays[i-1] = (((int32_t)delay3Cycles[i-1]) << 16) | delay4Cycles[i-1];
  }
  milisDelays[seqLen - 1] = 0;
  delay3Cycles[seqLen-1] = 1;
  delay4Cycles[seqLen-1] = 1;
  noInterrupts(); 
  for(unsigned int i = 0; i < seqLen; ++i){
    uint8_t regd = val[i];
    uint8_t regb = val[i] >> 8;
    PORTD = regd;
    PORTB = regb;
    delayMilis(milisDelays[i]);
    // delayCycles(cyclesDelays[i]);
    _delay_loop_1(delay3Cycles[i]);
    _delay_loop_2(delay4Cycles[i]);
    // _delay_loop_1(cyclesDelays[i]>>16);
    // _delay_loop_2(cyclesDelays[i]);
  }    
  interrupts();
}
/*!
This function delays the execution by (`cycles` + C) number of cycles
where C is a compiler dependent constant.
The `cycles` argument must be larger than 16.
*/
void delayCycles(uint16_t& cycles){
  uint8_t delay3Cycles = 4 - (cycles % 4);
  uint16_t delay4Cycles = (cycles / 4) - delay3Cycles;
  _delay_loop_2(delay4Cycles);
  _delay_loop_1(delay3Cycles);
}


void delayMilis(uint16_t& ms){
  while(ms > 0){
    __builtin_avr_delay_cycles(MILIS_DELAY_CYCLES);
    ms--;
  }
}

void calibMilis(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void)  interface;
  int     ch;

  ch = -1;
  if(parameters.Size() == 0)
    ch = 2;
  else if (parameters.Size() == 1)
    ch = parseChannel(parameters[0]);
  if(ch < 0)
    return;

  int32_t time = (F_CPU / 1000) + DELAY_OFFSET;
  if(strcasecmp_P(commands.Last(), PSTR("one")) == 0)
    --time;
  resetSeq();
  seqAdd(ch, 0);
  seqAdd(ch, time);
  digitalWrite(ch, 0);  
}


void outputOn(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void)  interface;
  int     ch;

  if(strcasecmp_P(commands.Last(), PSTR("xon")) == 0){
    PORTD &= ~(PORTS_AVAILABLE);
    PORTB &= ~(PORTS_AVAILABLE >> 8);
  }

  for(int i = 0; i < parameters.Size(); ++i){
    ch = parseChannel(parameters[i]);
    if(ch > 0){
      digitalWrite(ch, 1);
    }
  }
}

void outputOff(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  (void)  commands;
  (void)  interface;
  int     ch;

  if (parameters.Size() == 0)
    resetOutputs();

  for(int i = 0; i < parameters.Size(); ++i){
    ch = parseChannel(parameters[i]);
    if(ch > 0){
      digitalWrite(ch, 0);
    }
  }
}

void printError(Stream& interface){
  if(seqError & SEQ_ERR_NAN)
    interface.println(F("Error: (NAN) Something you sent was not a proper number"));
  else if (seqError & SEQ_ERR_TIME){
    interface.print(F("Error: (TIME) Two output value changes are closer than "));
    interface.print(((double)DELAY_OFFSET) / ((double)CYCLES_US));
    interface.println(" us");
  }  
  else if(seqError & SEQ_ERR_CH)
    interface.println(F("Error: (CH) Chosen channel is not available"));  
  else if(seqError & SEQ_ERR_MEM)
    interface.println(F("Error: (MEM) Somehow you exceeded available memory D:"));
}
