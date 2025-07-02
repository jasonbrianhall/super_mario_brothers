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
    
    if [ ! -d "$BUILD_DIR/allegro4-install" ]; then
        echo "Downloading DJGPP-compatible Allegro 4 allegro4..."
        cd $BUILD_DIR
        
        # Use the DJGPP cross-compilation fork instead
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
                
                # Build using the xmake script (DJGPP-specific)
                chmod +x xmake.sh
                ./xmake.sh lib
                
                # Create install directory structure
                mkdir -p /workspace/allegro4-install/include
                mkdir -p /workspace/allegro4-install/lib
                
                # Copy headers
                cp -r include/* /workspace/allegro4-install/include/
                
                # Copy library
                cp lib/djgpp/liballeg.a /workspace/allegro4-install/lib/
                
                chown -R $USER_ID:$GROUP_ID /workspace
                echo 'Allegro 4 built successfully with DJGPP cross-compilation fork'
            "
    else
        echo "Allegro 4 already built"
    fi
    ;;

"dos")
    echo "Building SMB virtualizer for DOS..."
    
    # Ensure Allegro is built
    ./build_dos.sh allegro
    
    # Check allegro4 files
    if [ ! -f "allegro4/dos_main.cpp" ]; then
        echo "ERROR: allegro4/dos_main.cpp not found!"
        exit 1
    fi
    
    # Build with Allegro 4
    echo "Compiling with DJGPP and Allegro 4..."
    docker run --rm \
        -v $(pwd):/src:z \
        -u $USER_ID:$GROUP_ID \
        $DJGPP_IMAGE \
        /bin/sh -c "
            cd /src && 
            echo 'Checking available libraries...' &&
            find $BUILD_DIR/allegro4-install -name '*.a' 2>/dev/null || echo 'No .a files found' &&
            echo 'Compiling DOS executable with Allegro 4...' &&
            g++ -s allegro4/dos_main.cpp \
                allegro4/Configuration.cpp \
                allegro4/Emulation/APU.cpp \
                allegro4/Emulation/Controller.cpp \
                allegro4/Emulation/MemoryAccess.cpp \
                allegro4/Emulation/PPU.cpp \
                allegro4/SMB/SMB.cpp \
                allegro4/SMB/SMBData.cpp \
                allegro4/SMB/SMBEngine.cpp \
                allegro4/Util/Video.cpp \
                allegro4/Util/VideoFilters.cpp \
                allegro4/SMBRom.cpp \
                -I$BUILD_DIR/allegro4-install/include \
                -L$BUILD_DIR/allegro4-install/lib \
                -o $BUILD_DIR/smb.exe \
                -O2 -lalleg -lm -fpermissive -w &&
            echo 'Converting to COFF format...' &&
            exe2coff $BUILD_DIR/smb.exe &&
            echo 'Creating final DOS executable with DPMI stub...' &&
            cat $BUILD_DIR/csdpmi/bin/CWSDSTUB.EXE $BUILD_DIR/smb > $BUILD_DIR/smb.exe &&
            echo 'DOS build complete!'
        "
    
    # Copy DPMI server (find it wherever it is)
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
