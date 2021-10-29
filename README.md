# Arduino pulse generator
Arduino pulse generator uses single-cycle accurate delays and direct port writing to achieve very precise pulses. Pulses sequences are programmed using SCPI communication (thanks to [Vrekrer SCPI parser](https://github.com/Vrekrer/Vrekrer_scpi_parser)) with some extra utilities like changing output states or setting a time unit. Because Arduino is a very simple machine, there are however some [limitations](#limitations).
## Installation
Just download the code, open `pulsegen.ino` with Arduino IDE and upload the sketch to your board. Use the serial monitor to verify successful upload.
## SCPI commands
`<foo>` – mandatory part, `[bar]` – optional part, `{val}` – optional one or more arguments, `foo|bar|val` – pick one, `[(foo)|bar]` or `[arg=2]` – default if ommited.

- `*IDN?`  
  Identify command. Should return something like `mymail@gmail.com,Arduino pulse generator,#86,v0.2`.

- `*RST`  
Reset Arduino to the initial state (as if it was powered off and on).

- `OUTPut:[X]ON <port>, {port}`  
Turn specified output port(s) on. The `XON` variant can be used to ensure that all other outputs are off.  
:warning: Ports 0 and 1 are used for communication and are excluded from use.

- `OUTPut:OFF {port}`  
Turn specified output ports off. If used without arguments, turns off all channels.

- `SYSTem:UNIT[?] <cycle|us|ms|s>`  
Set (query) the unit used when specifying time. 

- `SYSTem:UNIT:<STORe|RECall>`  
Store (recall) a time unit in (from) EEPROM. The stored unit is recalled by default when powering on Arduino.

- `PULSe[?]:[(ADD)|XADD] <port>, <time>, {time}`  
 Add a pulses sequence for a specified port. A port's state will be switched at specified times. For convenience ordering is not important and negative times can be used. Also, `PULSe:ADD` is equivalent to `PULSe`. For debugging purposes use `PULSe?` to query the current pulses sequence.  
 :warning: Note that an initial state of a port determines polarity of a pulse sequence. It's advised to use `PULSe:OFF` or  `PULSe:XON` to set proper starting states before starting the sequence.

- `PULSe:RESet [channel]`  
Remove a channel from a pulses sequence. If no channel is specified, clear the entire sequence. 

- `PULSe:RUN`  
Execute the pulses sequence.  
:warning: Because interrupts are turned off during a pulses sequence, all commands sent to Arduino during that time will be discarded.

- `PULSe:CALibrate:MILis:<ONE|TWO> [port=2]`  
  Set special calibration pulse. The first one is the longest pulse that doesn't use milisecond delay, while the second one is the shortest that uses it. Thus, with proper [calibration](#long-pulses), time difference between these two pulses should be exactly of one clock cycle.

:information_source: SCPI parser is case insensitive either when using short or long command form. You can write `PULSe:RUN`, `pulse:RUN`, `puls:run`, or even `pULs:rUn`.

## Limitations
- Time between output changes must be larger then approximately 4 μs, due to minimal run time of control loop. However, arbitrary number of ports can be switched at the same time.
- Because ports 8-13 are controlled by a different register then ports 0-7, timing of pulses programmed for ports 8-13 will be off by a single clock cycle (for typical 16 kHz clock, it is equivalent to 62.5 ns).
- Depending on a board used and the version of a compiler, timing between consecutive output changes can be off by a constant value. Check the next section for instructions on how to fix it.
- Absolute timing of pulses can vary, as Arduino's clock is never a perfect 16 kHz. Clock frequency can also drift with temperature.
- Length of commands is limited by size of Arduino buffers. By default, single message shouldn't be longer than 254 characters and number of parameters shouldn't exceed 15. There is also a limit of 50 steps in a pulses sequence. This limits can be increased by changing appropriate constants, however, increased memory usage can lead to instability.

## Checking delays' calibration
### Short pulses
To verify short pulses timing, execute command:
```
*RST; syst:unit us; puls 2, 0, 10; puls:run
```
and verify on an oscilloscope that the pulse from port 2 took exactly 10 μs. If there is some offset, divide it by clock period (62.5 ns) and increase (if the pulse comes too late) or decrease (if it comes too early) the `DELAY_OFFSET` constant (line 11 in `pulsegen.ino`) by the calculated value. After recompilation and upload of the updated sketch, repeat above steps to verify successful correction.

### Long pulses
For pulses longer than milisecond, timing of a milisecond delay must be verified. To do that first execute a following command:
```
*RST; puls:cal:mil:one; puls:run;
```
and zoom into the end of the pulse on port 2. Then execute a second command:
```
*RST; puls:cal:mil:two; puls:run;
```
The second pulse should be longer than the first one by a single clock cycle. If its shorter, `MILIS_DELAY_CYCLES` constant should be increased, otherwise decreased. Repeat until the difference between pulses' length is exactly of one clock cycle.
## Examples
Work in progress...
