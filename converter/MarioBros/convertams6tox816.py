import re
import argparse
import sys
import os

def convert_asm6_to_x816(input_file, output_file, flatten_includes=True):
    """Convert ASM6 assembly to x816 by fixing multiplication expressions"""
    
    def process_file(file_path, processed_files=None, base_dir=None):
        """Process a single file and return its content"""
        if processed_files is None:
            processed_files = set()
        if base_dir is None:
            base_dir = os.path.dirname(os.path.abspath(file_path))
            
        # Resolve absolute path to avoid processing the same file twice
        abs_path = os.path.abspath(file_path)
        
        if abs_path in processed_files:
            return f"; File {file_path} already included\n"
            
        processed_files.add(abs_path)
        
        try:
            with open(file_path, 'r') as f:
                content = f.read()
        except FileNotFoundError:
            return f"; ERROR: Could not find file {file_path}\n"
        
        processed_content = ""
        
        # Process line by line to handle includes
        for line_num, line in enumerate(content.split('\n'), 1):
            # Check for include directives (both .include and INCLUDE)
            include_match = re.match(r'\s*(?:\.include|include)\s+"([^"]+)"', line, re.IGNORECASE)
            if not include_match:
                include_match = re.match(r'\s*(?:\.include|include)\s+([^\s;]+)', line, re.IGNORECASE)
            
            if include_match:
                include_file = include_match.group(1)
                # Remove quotes if present
                include_file = include_file.strip('"\'')
                
                # Resolve relative path from the directory of the current file
                current_dir = os.path.dirname(abs_path)
                include_path = os.path.join(current_dir, include_file)
                
                if flatten_includes:
                    # Replace the include line with the file contents
                    processed_content += f"\n; ===== Contents of {include_file} =====\n"
                    processed_content += process_file(include_path, processed_files, base_dir)
                    processed_content += f"; ===== End of {include_file} =====\n\n"
                else:
                    # Convert .include to INCLUDE for x816 if not flattening
                    line = re.sub(r'\.include', 'INCLUDE', line, flags=re.IGNORECASE)
                    processed_content += line + '\n'
            else:
                processed_content += line + '\n'
        
        return processed_content
    
    # Process the main file and all includes
    all_content = process_file(input_file)
    
    # Pattern to match expressions like: = OAM_Y+(4*FreezeEffect_OAM_Slot)
    # Captures the base address, multiplier, and variable name
    pattern = r'(\w+)\s*=\s*(\w+)\+\((\d+)\*(\w+)\)'
    
    def replace_multiplication(match):
        var_name = match.group(1)
        base_addr = match.group(2) 
        multiplier = int(match.group(3))
        slot_var = match.group(4)
        
        # Look for the slot value definition in the content
        slot_pattern = rf'{slot_var}\s*=\s*(\d+)'
        slot_match = re.search(slot_pattern, all_content)
        
        if slot_match:
            slot_value = int(slot_match.group(1))
            calculated_offset = multiplier * slot_value
            return f'{var_name} = {base_addr}+${calculated_offset:02X}'
        else:
            # If we can't find the slot value, leave a comment for manual fix
            return f'{var_name} = {base_addr}+{multiplier}*{slot_var} ; TODO: Calculate manually - slot value not found'
    
    # Apply the multiplication conversion
    converted_content = re.sub(pattern, replace_multiplication, all_content)
    
    # Additional ASM6 to x816 syntax conversions
    conversions = [
        (r'\.org\b', 'ORG'),
        (r'\.db\b', 'DB'),
        (r'\.dw\b', 'DW'),
        (r'\.ds\b', 'DS'),
        (r'\.ascii\b', 'DB'),
        (r'\.byte\b', 'DB'),
        (r'\.word\b', 'DW'),
    ]
    
    for pattern, replacement in conversions:
        converted_content = re.sub(pattern, replacement, converted_content, flags=re.IGNORECASE)
    
    # Write the result
    with open(output_file, 'w') as f:
        f.write(converted_content)
    
    print(f"Conversion complete! Output written to {output_file}")
    if flatten_includes:
        print("All .include files have been flattened into the output file.")
    else:
        print(".include statements converted to INCLUDE (use --no-flatten to disable flattening)")
    print("Look for any 'TODO' comments for manual fixes needed.")

# Example usage:
if __name__ == "__main__":
    # Set up command line argument parsing
    parser = argparse.ArgumentParser(description='Convert ASM6 assembly to x816 by fixing multiplication expressions')
    parser.add_argument('input', help='Input ASM6 file to convert')
    parser.add_argument('output', help='Output x816 file to create')
    parser.add_argument('--no-flatten', action='store_true', 
                       help='Keep .include statements instead of flattening files (convert .include to INCLUDE)')
    
    args = parser.parse_args()
    
    try:
        convert_asm6_to_x816(args.input, args.output, not args.no_flatten)
    except FileNotFoundError:
        print(f"Error: Input file '{args.input}' not found!")
        sys.exit(1)
    except Exception as e:
        print(f"Error during conversion: {e}")
        sys.exit(1)
