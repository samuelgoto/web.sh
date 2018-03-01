default: all

all:
	g++  main.cc -o web.sh  \
        -Wl,--start-group $(V8)/out.gn/x64.release/obj/{libv8_{base,libbase,external_snapshot,libplatform,libsampler},third_party/icu/libicu{uc,i18n},src/inspector/libinspector}.a -Wl,--end-group \
        -lrt -ldl -pthread -std=c++0x \
        -I. -I $(V8) -I $(V8)/include \
