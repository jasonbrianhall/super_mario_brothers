#!/bin/bash
# Clean build script for SMB DOS with Allegro 4 (modular compilation)

DJGPP_IMAGE="djfdyuruiry/djgpp"
BUILD_DIR="build/dos"
OBJ_DIR="$BUILD_DIR/obj"
USER_ID=$(id -u)
GROUP_ID=$(id -g)

SOURCE_FILES=(
    source/SDL.cpp
    source/Configuration.cpp
    source/Emulation/APU.cpp
    source/Emulation/Controller.cpp
    source/Emulation/MemoryAccess.cpp
    source/Emulation/PPU.cpp
    source/SMB/SMB.cpp
    source/SMB/SMBData.cpp
    source/SMB/SMBEngine.cpp
    source/Util/Video.cpp
    source/Util/VideoFilters.cpp
    source/SMBRom.cpp
    source/Main.cpp
)

case "${1:-dos}" in

"setup")
    echo "Setting up build environment..."
    docker pull $DJGPP_IMAGE
    mkdir -p "$BUILD_DIR" "$OBJ_DIR"

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
                mkdir -p /workspace/source-install/include /workspace/source-install/lib
                cp -r include /workspace/source-install/
                cp lib/djgpp/liballeg.a /workspace/source-install/lib/
                chown -R $USER_ID:$GROUP_ID /workspace
                echo 'Allegro 4 built successfully'
            "
    else
        echo "Allegro 4 already built"
    fi
    ;;

"dos")
    echo "Building SMB virtualizer for DOS..."
    ./build_dos.sh allegro

    echo "Compiling each source file..."
    for src in "${SOURCE_FILES[@]}"; do
        obj="$OBJ_DIR/$(basename "${src%.cpp}.o")"
        echo "Compiling $src -> $obj"
        docker run --rm \
            -v $(pwd):/src:z \
            -u $USER_ID:$GROUP_ID \
            $DJGPP_IMAGE \
            /bin/sh -c "
                g++ -c /src/$src \
                    -I/src/$BUILD_DIR/source-install/include \
                    -o /src/$obj \
                    -O2 -fpermissive -w
            "
    done

    echo "Linking object files:"
    for src in "${SOURCE_FILES[@]}"; do
        obj="$OBJ_DIR/$(basename "${src%.cpp}.o")"
        echo "  $obj"
    done


    docker run --rm \
        -v $(pwd):/src:z \
        -u $USER_ID:$GROUP_ID \
        $DJGPP_IMAGE \
        /bin/sh -c "
            g++ $OBJ_LIST \
                -lalleg -lm \
                -L/src/$BUILD_DIR/source-install/lib \
                -o /src/$BUILD_DIR/smb.exe
        "

    echo "Converting to COFF and adding DPMI stub..."
    docker run --rm \
        -v $(pwd):/src:z \
        -u $USER_ID:$GROUP_ID \
        $DJGPP_IMAGE \
        /bin/sh -c "
            exe2coff /src/$BUILD_DIR/smb.exe &&
            cat /src/$BUILD_DIR/csdpmi/bin/CWSDSTUB.EXE /src/$BUILD_DIR/smb > /src/$BUILD_DIR/smb.exe
        "

    echo "DOS build complete!"
    ;;

"run")
    if [ ! -f "$BUILD_DIR/smb.exe" ]; then
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

