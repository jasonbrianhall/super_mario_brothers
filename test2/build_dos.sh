#!/bin/bash
# Clean build script for SMB DOS with Allegro 4

DJGPP_IMAGE="djfdyuruiry/djgpp"
BUILD_DIR="build/dos"
USER_ID=$(id -u)
GROUP_ID=$(id -g)

case "${1:-dos}" in

"setup")
    echo "Setting up build environment..."
    docker pull $DJGPP_IMAGE

    mkdir -p $BUILD_DIR

    # Download CSDPMI
    if [ ! -d "$BUILD_DIR/csdpmi" ]; then
        echo "Downloading CSDPMI..."
        cd $BUILD_DIR
        curl -L -o csdpmi7b.zip http://na.mirror.garr.it/mirrors/djgpp/current/v2misc/csdpmi7b.zip
        unzip -o -q csdpmi7b.zip -d csdpmi
        rm csdpmi7b.zip
        cd ../..
    fi

    echo "Setup complete"
    ;;

"allegro")
    echo "Building Allegro 4 for DJGPP..."

    ./build_dos.sh setup

    if [ ! -d "$BUILD_DIR/source-install" ]; then
        echo "Downloading DJGPP-compatible Allegro 4 source..."
        cd $BUILD_DIR
        curl -L -o allegro-4.2.3.1-xc.tar.gz https://github.com/superjamie/allegro-4.2.3.1-xc/archive/refs/heads/master.tar.gz
        cd ../..

        echo "Building Allegro 4 in container..."
        docker run --rm \
            -v $(pwd)/$BUILD_DIR:/workspace:z \
            -w /workspace \
            --user root \
            $DJGPP_IMAGE \
            /bin/bash -c "
                tar xzf allegro-4.2.3.1-xc.tar.gz
                cd allegro-4.2.3.1-xc-master

                chmod +x xmake.sh
                ./xmake.sh lib

                mkdir -p /workspace/source-install/include
                mkdir -p /workspace/source-install/lib

                # Copy headers (preserve include/allegro.h structure)
                cp -r include /workspace/source-install/

                # Copy library
                cp lib/djgpp/liballeg.a /workspace/source-install/lib/

                chown -R $USER_ID:$GROUP_ID /workspace
                echo 'Allegro 4 built successfully with DJGPP cross-compilation fork'
            "
    else
        echo "Allegro 4 already built"
    fi
    ;;

"dos")
    echo "Building SMB virtualizer for DOS..."

    ./build_dos.sh allegro

    if [ ! -f "source/Main.cpp" ]; then
        echo "ERROR: source/Main.cpp not found!"
        exit 1
    fi

    echo "Compiling with DJGPP and Allegro 4..."
    docker run --rm \
        -v $(pwd):/src:z \
        -u $USER_ID:$GROUP_ID \
        $DJGPP_IMAGE \
        /bin/sh -c "
            cd /src &&
            echo 'Checking Allegro headers and libraries...' &&
            ls -l $BUILD_DIR/source-install/include/allegro.h || echo 'Header not found' &&
            ls -l $BUILD_DIR/source-install/lib/liballeg.a || echo 'Library not found' &&
            echo 'Compiling DOS executable with Allegro 4...' &&
            g++ source/SDL.cpp \
                source/Main.cpp \
                source/Configuration.cpp \
                source/Emulation/APU.cpp \
                source/Emulation/Controller.cpp \
                source/Emulation/MemoryAccess.cpp \
                source/Emulation/PPU.cpp \
                source/SMB/SMB.cpp \
                source/SMB/SMBData.cpp \
                source/SMB/SMBEngine.cpp \
                source/Util/Video.cpp \
                source/Util/VideoFilters.cpp \
                source/SMBRom.cpp \
                -I$BUILD_DIR/source-install/include \
                -I/src/third_party/boost \
                -L$BUILD_DIR/source-install/lib \
                -o $BUILD_DIR/smb.exe \
                -O2 -lalleg -lm -fpermissive -w &&
            echo 'Converting to COFF format...' &&
            exe2coff $BUILD_DIR/smb.exe &&
            echo 'Creating final DOS executable with DPMI stub...' &&
            cat $BUILD_DIR/csdpmi/bin/CWSDSTUB.EXE $BUILD_DIR/smb > $BUILD_DIR/smb.exe &&
            echo 'DOS build complete!'
        "

    echo "Looking for DPMI files..."
    ls -la $BUILD_DIR/csdpmi/ 2>/dev/null || echo "CSDPMI directory not found"
    find $BUILD_DIR -name "CWSDPMI.EXE" -o -name "cwsdpmi.exe" -o -name "CWSDSTUB.EXE" 2>/dev/null

    DPMI_FILE=$(find $BUILD_DIR -name "CWSDPMI.EXE" -o -name "cwsdpmi.exe" 2>/dev/null | head -1)
    if [ -n "$DPMI_FILE" ]; then
        cp "$DPMI_FILE" $BUILD_DIR/
        echo "Copied DPMI server: $(basename "$DPMI_FILE")"
    else
        echo "Warning: DPMI server not found - DOS executable may need DPMI host"
    fi

    echo ""
    echo "🎮 SMB DOS Emulator built with Allegro 4!"
    echo "📁 Files:"
    ls -la $BUILD_DIR/*.exe $BUILD_DIR/*.EXE 2>/dev/null || true
    ;;

"run")
    if [ ! -f "$BUILD_DIR/smb.exe" ]; then
        echo "SMB virtualizer not built yet. Building..."
        ./build_dos.sh dos
    fi

    echo "Running SMB virtualizer in DOSBox..."
    cd $BUILD_DIR && dosbox smb.exe
    ;;

"clean")
    echo "Cleaning build files..."
    rm -rf $BUILD_DIR
    echo "Clean complete"
    ;;

*)
    echo "Usage: $0 [setup|allegro|dos|run|clean]"
    ;;

esac

