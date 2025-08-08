# A Practical Implementation and Evaluation of In-Network Caching on Constrained IEEE 802.15.4 IoT Hardware 

This is the home to the source code of the firmware and helper scripts to configure, run, and gather data from experiments mentioned in the paper `A Practical Implementation and Evaluation of In-Network Caching on Constrained IEEE 802.15.4 IoT Hardware`.

This work uses a custom version of RIOT OS, particularly the IPv6 forwarding flow is modified. Said custom RIOT OS is included in this repo as a submodule. Please run `git submodule update --recursive --init` before anything else.

# How to build
- For ease of compiling for different boards and flashing, one can use our helper program `makescript.sh`. 
- Usage: in the application subfolder, `./makescript.sh --board iotlab-m3 --fitiot` will compile the program for the Fit-IoT iotlab-m3 board, which is the board used in our paper. The compiled binary will be in the `./bin/` folder.
- This script can also build the app for other boards. The application is only tested on the `iotlab-m3` and the `seeedstudio-xiao-nrf52840` targets. 

# How to flash
- After booking nodes in the Fit-IoT lab one can then call `iotlab-node -fl ./bin/iotlab-m3/cache_perf.elf` to flash all of the booked nodes with the compiled binary. 

# How to run
- `automator.py` TODO

# Citing our work
- Please use the following Bibtex for any citations:
```@INPROCEEDINGS{Yilm2512:Practical,

AUTHOR="Isikcan Yilmaz and Jonas Schulz and Siddharth Das and Ricardo J. B. Pousa
and Leonardo Gonzalez and Juan A. Cabrera and Patrick Seeling and Frank
H.P. Fitzek",

TITLE="A Practical Implementation and Evaluation of {In-Network} Caching on
Constrained {IEEE} {802.15.4} {IoT} Hardware",

BOOKTITLE="2025 IEEE Globecom Workshops (GC Wkshps): Workshop on Towards Integrated
IoT and Ground-Air-Space Networks: Bridging Global Connectivity (GC Wkshps
2025-IGASN BGC)",

ADDRESS="Taipei, Taiwan",

PAGES="5.86",

KEYWORDS="In-Network Caching; Internet of Things; 802.15.4; Hardware Testbed;
Wireless Sensor Network",

}```


