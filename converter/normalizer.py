#!/usr/bin/env python3

import sys
import re

def normalize_ca65(input_file, output_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    variables = []
    output_lines = []
    
    i = 0
    while i < len(lines):
        current_line = lines[i].strip()
        
        # Check if current line is a label
        if current_line.endswith(':'):
            label_name = current_line[:-1]  # Remove the colon
            
            # Check if next line exists and is a .byte directive
            if i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                if next_line.startswith('.byte'):
                    # Extract the byte value and comment
                    parts = next_line.split(';', 1)
                    byte_part = parts[0].strip()
                    comment = ';' + parts[1] if len(parts) > 1 else ''
                    
                    # Get the value after .byte
                    byte_value = byte_part.split()[1]
                    
                    # Create variable
                    var_line = f"{label_name} = {byte_value}"
                    if comment:
                        var_line += f" {comment}"
                    
                    variables.append(var_line)
                    
                    # Skip both lines
                    i += 2
                    continue
        
        # Strip z: and a: prefixes and line numbers, then keep the line
        cleaned_line = lines[i]
        # Strip z: and a: prefixes
        cleaned_line = re.sub(r'\b[za]:', '', cleaned_line)
        # Strip line numbers at beginning of line
        cleaned_line = re.sub(r'^\d+\s+', '', cleaned_line)
        
        output_lines.append(cleaned_line)
        i += 1
    
    # Write the output file
    with open(output_file, 'w') as f:
        # Write variables first
        if variables:
            f.write("; Variables\n")
            for var in variables:
                f.write(var + "\n")
            f.write("\n")
        
        # Write the rest of the code
        for line in output_lines:
            f.write(line)
    
    print(f"Converted {len(variables)} labels to variables")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py input.asm output.asm")
        sys.exit(1)
    
    normalize_ca65(sys.argv[1], sys.argv[2])
