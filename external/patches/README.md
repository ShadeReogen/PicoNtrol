To apply the patches, do:

```
cd ${BLUEPAD32_SRC}/external/btstack
git apply < ../patches/0002-l2cap-allow-incoming-connection-with-not-enough.patch
```

And after applying the patch, you have to install btstack. E.g:

```
cd ${BLUEPAD32_SRC}/external/btstack/port/esp32
IDF_PATH=../../../../src ./integrate_btstack.py
```
