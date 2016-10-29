# Development Notes

Compile

```
pio ci --lib . --board uno --project-conf platformio.ini examples\scope_fixed_duration.ino
```

Compile and upload

```
pio ci --lib . --board uno --project-conf platformio.ini examples\scope_fixed_duration.ino
```

Put `targets = upload` into `platformio.ini` in case you want to have immediate upload.

Inspect assembly

First instruct pio to use specific build dir (must exist)
```
pio ci --lib . --board uno --build-dir BUILD_DIR --keep-build-dir examples\scope_fixed_duration.ino 
```

Update env path 
```
set PATH=%userprofile%\.platformio\packages\toolchain-atmelavr\bin;%PATH%
```

Dump disassembly
```
avr-objdump -x -g -S BUILD_DIR/.pioenvs/uno/firmware.elf > out.txt
```



