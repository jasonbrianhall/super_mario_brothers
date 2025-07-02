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
        unzip -o -q csdpmi7b.zip
        rm csdpmi7b.zip
        cd ../..
    fi
    
    echo "Setup complete"
    ;;

"allegro")
    echo "Building Allegro 4 for DJGPP..."
    
    ./build_dos.sh setup
    
    if [ ! -d "$BUILD_DIR/allegro4-install" ]; then
        echo "Downloading Allegro 4 source..."
        cd $BUILD_DIR
        curl -L -o allegro-4.4.3.1.tar.gz https://github.com/liballeg/allegro5/releases/download/4.4.3.1/allegro-4.4.3.1.tar.gz
        cd ../..
        
        echo "Building Allegro 4 in container..."
        docker run --rm \
            -v $(pwd)/$BUILD_DIR:/workspace:z \
            -w /workspace \
            --user root \
            $DJGPP_IMAGE \
            /bin/bash -c "
                tar xzf allegro-4.4.3.1.tar.gz
                cd allegro-4.4.3.1
                chmod +x configure
                ./configure --host=i586-pc-msdosdjgpp --enable-static --disable-shared --prefix=/workspace/allegro4-install
                make
                make install
                chown -R $USER_ID:$GROUP_ID /workspace
                echo 'Allegro 4 built successfully'
            "
    else
        echo "Allegro 4 already built"
    fi
    ;;

"dos")
    echo "Building SMB emulator for DOS..."
    
    # Ensure Allegro is built
    ./build_dos.sh allegro
    
    # Check source files
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
            echo 'Compiling DOS executable with Allegro 4...' &&
            g++ -s allegro4/dos_main.cpp \
                -I $BUILD_DIR/allegro4-install/include \
                -L $BUILD_DIR/allegro4-install/lib \
                -o $BUILD_DIR/smb.exe \
                -O2 -lalleg -lm &&
            echo 'Converting to COFF format...' &&
            exe2coff $BUILD_DIR/smb.exe &&
            echo 'Creating final DOS executable with DPMI stub...' &&
            cat $BUILD_DIR/csdpmi/bin/CWSDSTUB.EXE $BUILD_DIR/smb > $BUILD_DIR/smb.exe &&
            echo 'DOS build complete!'
        "
    
    # Copy DPMI server
    cp $BUILD_DIR/csdpmi/bin/CWSDPMI.EXE $BUILD_DIR/
    
    echo ""
    echo "🎮 SMB DOS Emulator built with Allegro 4!"
    echo "📁 Files:"
    ls -la $BUILD_DIR/*.exe $BUILD_DIR/*.EXE 2>/dev/null || true
    ;;

"run")
    if [ ! -f "$BUILD_DIR/smb.exe" ]; then
        echo "SMB emulator not built yet. Building..."
        ./build_dos.sh dos
    fi
    
    echo "Running SMB emulator in DOSBox..."
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
