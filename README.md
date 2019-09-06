# ASM: Assembler for a simple 16-bit 2-address processor

2 pass assembler for a 16-bit 2-address processor which is described in accompanying file 'system.pdf'. All interaction is done throguh command line when executing compiled executible. User should provide a source file which is formatted according to system guides. Assembler then generates object file which is formatted similarly to ELF and can later be linked to form an executable. Project was executed as a semestral work for university course of system programming.  

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes under **Linux**.
See deployment for notes on how to deploy the project on a live system.

### Prerequisites

GCC 5+, GNU Make

### Installing and executing

1. Start by cloning this git repository

```
git clone https://github.com/aleksailic/ASM
```
2. Change your path to newly created folder

```
cd ASM
```
3. Build project using make

```
make
```
4. Execute the program (-h is help flag and will give further instructions)
```
./assembler -h
```

## Contributing

Feel free to submit a pull request or contact me at [aleksa.d.ilic@gmail.com](mailto:aleksa.d.ilic@gmail.com). 

## License

Project is licensed under the MPL 2.0 License - see the [LICENSE](LICENSE) file for details.

## Resources
* [System programming course website ](http://si3ss.etf.rs/)