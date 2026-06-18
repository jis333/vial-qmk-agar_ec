## Make agar mini and vonvert to uf2
```
./go.sh
```

## go.sh
```
python3 ./util/uf2conv.py ydkb_evoli_agar_mini_ec_vial.bin -f 0x9d5becf1 -b 0x08004000 -c -o ydkb_evoli_agar_mini_ec_vial.uf2
```

## Make

```
make ydkb/evoli:xxx_vial
```

xxx means agar_ec or agar_mini_ec ...

## Convert BIN to UF2

```
uf2conv.py ydkb_evoli_xxx_vial.bin -b 0x8004000 -c -f 0x9d5becf1 -o xxx.uf2
```

