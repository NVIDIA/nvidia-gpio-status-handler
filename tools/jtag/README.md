JTAG (Joint Test Action Group) is a standard interface used for testing and programming electronic devices, particularly integrated circuits (ICs). 
The JTAG interface provides a way to access the internal components of a device and perform a variety of tasks, such as testing the device's functionality,
programming the device's memory, and debugging the device's firmware.

The JTAG interface consists of a set of pins on a device's physical package that are used to communicate with the device's internal components. 
These pins include TDI (Test Data In), TDO (Test Data Out), TCK (Test Clock), and TMS (Test Mode Select). 
The TDI pin is used to input test data into the device, while the TDO pin is used to output test data from the device. 
The TCK pin provides a clock signal to synchronize the transfer of data, and the TMS pin is used to control the device's test mode.
In the picture below we see example of HW connections for JTAG interface:

             +--------+             +----------+                   +----------+
 			 |        | TMS ------> |          | TMS --------->    |          |
			 | Master | TCK ------> | Device 1 |   TCK ------>     | Device 2 |   
             |        | TDI ------> |          | TDO ------> TDI   |          |  TDO -----| 
             |        |             +----------+                   +----------+           |
             |        |  TDO <------------------------------------------------------------|
             +--------+

JTAG interface defines 2 regsiter IR (instruction register), and DR(Data Register).
IR is used to share commands to the chip over JTAG while DR is used to send and receive data.
JTAG specification follows strict JTAG State machine. If you are interested you can see the diagram at:
https://www.allaboutcircuits.com/technical-articles/jtag-test-access-port-tap-state-machine/
Shift-DR is the state where the data is shifted in/out from the DR register. TLR (Test-Logic-Reset) is the inital state of JTAG
state machine.
Each device has unique IDCODE. Per JTAG spec once the device is in TLR the DR will have 32-bit IDCODE in it.
Another important information is that once IR registers are filled with 1's the device will be in BYPASS state.
In this state the size of the DR register of the device iin BYPASS state is 1. So the test will do 3 things:

1. Shift 1024 bits of data through IR registers. It will recognize the pattern coming back and this will help it determine
   total size of IR register.
2. Populate IR registers with all 1's. Look at the length of DR register with the same algorithm that is used to determine the
   length of the IR. For an example in upper drawing, filling all IR's with 1's will show the DR register of length 2. This
   tells the test that there are 2 devices on JTAG chain
3. Once the test knows the number of devices on the JTAG chain it will set the JTAG state machine back to ShiftDR through TLR. This
   way we know that IDCODE is in the DR register for each device. Now the test will read 32bit * NUMBER_OF_DEVICES, and will compare
   idcodes that came back with the idcode given to the test.

This test will not work correctly if there are 2 Devices of different type on the same JTAG chain. For an example, if on the same JTAG
chain there is an FPGA and a CPU, idcode's will not be the same, so the test will fail at all times since it expects that the devices on the
JTAG chain have the same idcode.

Example of executing the test:
root@ranger:~# ./jtag_test -j /dev/jtag0 -i 0x318a0dd
0

if you would like to see the details of what it does:
./jtag_test -j /dev/jtag0 -i 0x318a0dd -v
Found number of devices:1
Found ir length devices:10
idcode:0x318a0dd
0

