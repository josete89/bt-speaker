# bt-speaker

The project plays music received by Bluetooth from any A2DP Bluetooth source.

It will resample it to 96 kHZ and pass through an equalizer with the following modes:

* Pop
* Soft
* Rock
* Dance
* Classic 

## Build

```
$ export ADF_PATH=~/esp/esp-adf
$ . ~/esp/esp-idf/export.sh
$ idf.py build
$ idf.py -p /dev/tty.usbserial-1410 flash monitor
```

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example:

- Connect with Bluetooth on your smartphone to the audio board identified as "ESP-ADF-SPEAKER"
- Play some audio from the smartphone and it will be transmitted over Bluetooth to the audio bard.
- The audio playback may be controlled from the smartphone, as well as from the audio board. The following controls may be used:

    |   Smartphone   | Audio Board |
    |:--------------:|:-----------:|
    |   Play Music   |    [Play]   |
    |   Stop Music   |    [Set]    |
    |   Next Song    |    [Vol+]   |
    | Previous Song  |    [Vol-]   |