# Arduino pulse generator
Arduino pulse generator uses single-cycle accurate delays and direct port writing to achieve very precise pulses. Pulse sequences are programmed using SCPI communication (thanks to [Vrekrer SCPI parser](https://github.com/Vrekrer/Vrekrer_scpi_parser)) with some extra utilities like changing output states or setting time unit. Because Arduino is a very simple machine, there are however some [limitations](#limitations).
## Instalation
Just download code, open `pulsegen.ino` with Arduino IDE and upload sketch to your board. Use serial monitor to verify successful upload.
## SCPI commands

- `*IDN?`
Identify command. Should return something like `mymail@gmail.com,Arduino pulse generator,#86,v0.2`.

- `*RST`
Reset Arduino to the initial state (as if it was powered off and on).

- `OUTPut:[X]ON <port>, {port}`
Turn specified output port(s) on. `XON` variant can be used to ensure that all other outputs are off.
:warning: Ports 0 and 1 are used for communication and are excluded from use.

- `OUTPut:OFF {port}`
Turn specified output port(s) off. If used without arguments, turns off all channels.

- `SYSTem:UNIT[?] <cycle|us|ms|s>`
Set (query) unit used when specifying time. 

- `SYSTem:UNIT[:STORe|RECall]`
Store (recall) time unit in (from) EEPROM. Stored unit is recalled by default when powering on Arduino.

- `PULSe[?][:(ADD)|XADD] <port>, <time>, {time}`
 Add pulse sequence for specified port. Port's state will be switched at specified times. For convenience ordering is not important and negative times can be used. Also, `PULSe:ADD` is equivalent to `PULSe`. For debugging purposes use `PULSe?` to query current pulse sequence.
 :warning: Note that initial state of a port determines polarity of a pulse sequence. It's advised to use `PULSe:OFF` or  `PULSe:XON` to set proper starting states before starting the sequence.

- `PULSe:RESet [channel]`
Remove channel from a pulse sequence. If no channel is specified, clear entire sequence. 

- `PULSe:RUN`
Start pulse sequence. 
:warning: Because interrupts are turned off during pulse sequence, all commands sent to Arduino during that time will be discarded.

:information_source: SCPI parser is case insensitive either when using short or long command form. You can write `PULSe:RUN`, `pulse:RUN`, `puls:run`, or even `pULs:rUn`.

## Limitations
- Time between output changes must be larger then approximately 4 μs, due to minimal run time of control loop. However, arbitrary number of ports can be switched at the same time.
- Because ports 8-13 are controlled by a different register then ports 0-7, timing of pulses programmed for ports 8-13 will be off by single clock cycle (for typical 16 kHz clock, it is equivalent to 62.5 ns).
- Depending on a board used and the version of a compiler, timing between consecutive output changes can be off by a constant value. Check the next section for instructions on how to fix it.
- Absolute timing of pulses can vary, as Arduino's clock is never a perfect 16 kHz. Clock frequency can also drift with temperature.

## Checking delays
Work in progress...

## Examples
Work in progress...
