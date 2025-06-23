#!/usr/bin/env python3
"""
6502 Assembly Code Generator - Main Application with Debug Parser

Converts 6502 assembly code to C++ code for game engines.
This is a Python port of the original C++ version.

Usage: python main.py <INPUT_ASM_FILE> <OUTPUT_DIRECTORY>
"""

import sys
import os
from pathlib import Path

from lexer import Lexer
from parser import Parser  # This will use your debug parser
from ast_nodes import cleanup_ast
from translator import Translator
from util import clear_comments

def create_output_directory(output_dir: str):
    """Create output directory if it doesn't exist"""
    Path(output_dir).mkdir(parents=True, exist_ok=True)

def write_output_files(translator: Translator, output_dir: str):
    """Write all generated files to the output directory"""
    output_path = Path(output_dir)
    
    # Write source file
    source_file = output_path / "SMB.cpp"
    with open(source_file, 'w') as f:
        f.write(translator.get_source_output())
    
    # Write data file
    data_file = output_path / "SMBData.cpp"
    with open(data_file, 'w') as f:
        f.write(translator.get_data_output())
    
    # Write data header file
    data_header_file = output_path / "SMBDataPointers.hpp"
    with open(data_header_file, 'w') as f:
        f.write(translator.get_data_header_output())
    
    # Write constants header file
    constants_header_file = output_path / "SMBConstants.hpp"
    with open(constants_header_file, 'w') as f:
        f.write(translator.get_constant_header_output())
    
    print(f"Generated files in {output_dir}:")
    print("  SMB.cpp")
    print("  SMBData.cpp")
    print("  SMBDataPointers.hpp")
    print("  SMBConstants.hpp")

def main():
    """Main function with CA65 support and debug parser"""
    if len(sys.argv) < 3:
        print("usage: python main.py <INPUT ASM FILE> <OUTPUT DIRECTORY> [CONFIG DIRECTORY] [-ca65] [-debug]")
        print("  -ca65: Use CA65 assembly format instead of original format")
        print("  -debug: Enable detailed debug output from parser")
        print("Examples:")
        print("  python main.py game.asm output_dir")
        print("  python main.py game.asm output_dir -ca65")
        print("  python main.py game.asm output_dir my_config -ca65 -debug")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_dir = sys.argv[2]
    
    # Parse optional arguments
    config_dir = None
    is_ca65 = False
    debug_mode = False
    
    # Check all remaining arguments
    for i in range(3, len(sys.argv)):
        arg = sys.argv[i]
        if arg == "-ca65":
            is_ca65 = True
        elif arg == "-debug":
            debug_mode = True
        elif not arg.startswith('-'):
            config_dir = arg
    
    # Set default config directory based on format
    if config_dir is None:
        config_dir = "ca65_indirect_config" if is_ca65 else "indirect_jump_config"
    
    # Check if input file exists
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    try:
        # Clear any previous state
        clear_comments()
        
        # Read input file
        with open(input_file, 'r') as f:
            input_text = f.read()
        
        print(f"Debug mode: {'ON' if debug_mode else 'OFF'}")
        
        # Lexical analysis
        print(f"Performing lexical analysis ({'CA65' if is_ca65 else 'Original'} format)...")
        lexer = Lexer(input_text)
        tokens = lexer.tokenize()
        print(f"Generated {len(tokens)} tokens")
        
        # Show first few tokens in debug mode
        if debug_mode:
            print(f"\nFirst 10 tokens:")
            for i, token in enumerate(tokens[:10]):
                print(f"  [{i}] {token.type.name}: '{token.value}' (line {token.line})")
            print()
        
        # Parsing with debug support
        print("Parsing...")
        parser = Parser(tokens)
        
        # Set debug mode
        parser.debug_mode = debug_mode
        
        # Provide source lines for better debugging
        source_lines = input_text.split('\n')
        parser.set_source_lines(source_lines)
        
        # Parse with error recovery
        try:
            ast_root = parser.parse()
            print(f"Parsed {len(ast_root.children)} top-level nodes")
        except Exception as e:
            print(f"Parser error: {e}")
            if debug_mode:
                import traceback
                traceback.print_exc()
            
            # Try to continue with partial AST if available
            if hasattr(parser, 'root') and parser.root:
                ast_root = parser.root
                print(f"Using partial AST with {len(ast_root.children)} nodes")
            else:
                print("Could not recover from parser error")
                sys.exit(1)
        
        # Cleanup AST (reverse lists, etc.)
        print("Cleaning up AST...")
        cleanup_ast(ast_root)
        
        # Translation
        print("Translating to C++...")
        if config_dir:
            print(f"Using config directory: {config_dir}")
        
        # PASS is_ca65 FLAG TO TRANSLATOR
        translator = Translator(input_file, ast_root, config_dir, is_ca65)
        
        # Create output directory
        create_output_directory(output_dir)
        
        # Write output files
        write_output_files(translator, output_dir)
        
        print("Translation completed successfully!")
        if config_dir:
            analyzer_script = "ca65_analyzer.py" if is_ca65 else "rom_analyzer.py"
            print(f"\nIf you encounter indirect jump errors, run: python {analyzer_script} {input_file}")
            print(f"Then edit the files in {config_dir}:")
            print("  - *_targets.txt (for indirect jump targets)")
            if not is_ca65:
                print("  - data_labels.txt (for labels that should generate data pointers)")
                print("  - code_labels.txt (for labels used in goto statements)")
                print("  - alias_labels.txt (for simple label aliases)")
        
    except Exception as e:
        print(f"Error: {e}")
        if debug_mode:
            import traceback
            traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
