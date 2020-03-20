# ParanoiaBox
The ParanoiaBox by Daniel L. Marks

The ParanoiaBox is a standalone open hardware/software encryption device based on the STM32F103CBT6 processor.  On this device, messages may be composed, encrypted, decrypted, and viewed.  The device is an experiment in encryption minimalism, presenting a minimal attack surface by using a small microcontroller rather than a full personal computer with a complex operating system, while providing the functionality needed to transact encrypted messages between humans through insecure networks.  The device is connected to a NTSC television and a PS/2 keyboard for user input and control.  There are two micro SD card slots, one for ciphertext and the other for plaintext used to transact files.  There is a hardware random number generator based on avalanche noise generated in two transistors.  A picture of the complete PCB of the device can be seen below:

!(images/pcb.jpg)

At the top of the board is a RCA composite video connector.  At the right is a USB B connector that can be used to program the microcontroller.  At the lower right is a female PS/2 keyboard connector.  At the bottom are two SD card slots.  Finally, at the left is a barrel jack for 7 to 12 volt DC input power.  Except for the micro SD card slots, the PCB is all through assembly so it can be assembled more easily using simple tools.

The hardware is licensed under the Creative Commons CC-BY-SA 4.0 license.  The software written by D. L. Marks is licensed under the zlib license.  All other libraries are licensed under the licenses as described in the corresponding source files.  This hardware and software is provided under these licenses to be used at your own risk.  

THE HARDWARE AND SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE HARDWARE AND SOFTWARE OR THE USE OR OTHER DEALINGS IN THE HARDWARE AND SOFTWARE.

The project uses Advanced Encryption Standard AES256 as a symmetric encryption.  In addition, asymmetric encryption based on Elliptic Curve Diffie-Helman (ECDH) Curve 25519 is also available.  Blake2s 256-bit hashes are used as a hash function for message authentication codes (MACs) as well as in a key derivation function.  The keys are encrypted using a passphrase and AES256 and stored in the flash memory of the microcontroller.  The encrypted data and public key certificates are base64 encoded so that they have be transmitted through e-mail as text files.

The project uses the stm32duino libraries located here:

https://github.com/rogerclarkmelbourne/Arduino_STM32

All other Arduino libraries are provided in the libraries directory in this project, including those that provide the FAT fs, NTSC video output, PS/2 keyboard input, and cryptography, which is based on a very slightly modified version of 

https://github.com/rweather/arduinolibs

At present, only the Curve25519 class is modified to use a different random number source to generated keys.  Here are some screenshots directly of the TV screen.  This is the main menu:

!(images/screenshot1.jpg)

This is the built-in full-screen editor used to compose short messages:

!(images/screenshot2.jpg)

This is the screen from which the AES 256 symmetric, ECDH25519 private or public keys are selected:

!(images/screenshot3.jpg)

To encrypt/decrypt using AES, a AES key is selected.  To encrypt/decrypt using ECDH25519, a private and public key pair are selected.
