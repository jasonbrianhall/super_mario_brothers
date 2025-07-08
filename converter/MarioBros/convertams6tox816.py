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
    multiplication_pattern = r'(\w+)\s*=\s*(\w+)\+\((\d+)\*(\w+)\)'
    
    # Pattern to match bitwise OR expressions like: = Entity_MovementBits_MovingRight|Entity_MovementBits_MovingLeft
    bitwise_or_pattern = r'(\w+)\s*=\s*(\w+)\|(\w+)'
    
    # Pattern to match complex bitwise expressions in DB statements like: DB MAPPER<<4&$F0|MIRRORING
    complex_bitwise_pattern = r'(DB|\.byte)\s+(\w+)<<(\d+)&\$([0-9A-Fa-f]+)\|(\w+)'
    
    # Pattern to match simple bitwise AND expressions like: DB MAPPER&$F0
    simple_bitwise_and_pattern = r'(DB|\.byte)\s+(\w+)&\$([0-9A-Fa-f]+)'
    
    # Pattern to match bitwise XOR in instructions like: AND #$FF^Entity_MovementBits_MovingHorz
    instruction_xor_pattern = r'(\w+)\s+#\$([0-9A-Fa-f]+)\^(\w+)'
    
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
    
    def replace_bitwise_or(match):
        var_name = match.group(1)
        left_var = match.group(2)
        right_var = match.group(3)
        
        # Look for both variable values in the content
        left_pattern = rf'{left_var}\s*=\s*\$([0-9A-Fa-f]+)'
        right_pattern = rf'{right_var}\s*=\s*\$([0-9A-Fa-f]+)'
        
        left_match = re.search(left_pattern, all_content)
        right_match = re.search(right_pattern, all_content)
        
        # Also try decimal patterns
        if not left_match:
            left_pattern = rf'{left_var}\s*=\s*(\d+)'
            left_match = re.search(left_pattern, all_content)
            
        if not right_match:
            right_pattern = rf'{right_var}\s*=\s*(\d+)'
            right_match = re.search(right_pattern, all_content)
        
        if left_match and right_match:
            # Parse values (handle both hex and decimal)
            left_val_str = left_match.group(1)
            right_val_str = right_match.group(1)
            
            # Check if hex or decimal
            if left_val_str.upper().startswith('0X') or any(c in left_val_str.upper() for c in 'ABCDEF'):
                left_val = int(left_val_str, 16)
            else:
                left_val = int(left_val_str)
                
            if right_val_str.upper().startswith('0X') or any(c in right_val_str.upper() for c in 'ABCDEF'):
                right_val = int(right_val_str, 16)
            else:
                right_val = int(right_val_str)
            
            # Calculate bitwise OR
            result = left_val | right_val
            return f'{var_name} = ${result:02X}'
        else:
            # If we can't find the values, leave a comment for manual fix
            return f'{var_name} = {left_var}|{right_var} ; TODO: Calculate manually - variable values not found'
    
    def replace_complex_bitwise(match):
        directive = match.group(1)
        left_var = match.group(2)      # MAPPER
        shift_amount = int(match.group(3))  # 4
        mask_value = int(match.group(4), 16)  # $F0
        right_var = match.group(5)     # MIRRORING
        
        # Look for variable values in the content
        left_pattern = rf'{left_var}\s*=\s*\$([0-9A-Fa-f]+)'
        right_pattern = rf'{right_var}\s*=\s*\$([0-9A-Fa-f]+)'
        
        left_match = re.search(left_pattern, all_content)
        right_match = re.search(right_pattern, all_content)
        
        # Also try decimal patterns
        if not left_match:
            left_pattern = rf'{left_var}\s*=\s*(\d+)'
            left_match = re.search(left_pattern, all_content)
            
        if not right_match:
            right_pattern = rf'{right_var}\s*=\s*(\d+)'
            right_match = re.search(right_pattern, all_content)
        
        if left_match and right_match:
            # Parse values (handle both hex and decimal)
            left_val_str = left_match.group(1)
            right_val_str = right_match.group(1)
            
            # Check if hex or decimal
            if left_val_str.upper().startswith('0X') or any(c in left_val_str.upper() for c in 'ABCDEF'):
                left_val = int(left_val_str, 16)
            else:
                left_val = int(left_val_str)
                
            if right_val_str.upper().startswith('0X') or any(c in right_val_str.upper() for c in 'ABCDEF'):
                right_val = int(right_val_str, 16)
            else:
                right_val = int(right_val_str)
            
            # Calculate: MAPPER<<4&$F0|MIRRORING
            # Step 1: left_val << shift_amount
            shifted = left_val << shift_amount
            # Step 2: shifted & mask_value
            masked = shifted & mask_value
            # Step 3: masked | right_val
            result = masked | right_val
            
            return f'DB ${result:02X}'
        else:
            # If we can't find the values, leave a comment for manual fix
            return f'DB {left_var}<<{shift_amount}&${mask_value:02X}|{right_var} ; TODO: Calculate manually - variable values not found'
    
    def replace_simple_bitwise_and(match):
        directive = match.group(1)
        var_name = match.group(2)      # MAPPER
        mask_value = int(match.group(3), 16)  # $F0
        
        # Look for variable value in the content
        var_pattern = rf'{var_name}\s*=\s*\$([0-9A-Fa-f]+)'
        var_match = re.search(var_pattern, all_content)
        
        # Also try decimal pattern
        if not var_match:
            var_pattern = rf'{var_name}\s*=\s*(\d+)'
            var_match = re.search(var_pattern, all_content)
        
        if var_match:
            # Parse value (handle both hex and decimal)
            var_val_str = var_match.group(1)
            
            # Check if hex or decimal
            if var_val_str.upper().startswith('0X') or any(c in var_val_str.upper() for c in 'ABCDEF'):
                var_val = int(var_val_str, 16)
            else:
                var_val = int(var_val_str)
            
            # Calculate: MAPPER & $F0
            result = var_val & mask_value
            
            return f'DB ${result:02X}'
        else:
            # If we can't find the value, leave a comment for manual fix
            return f'DB {var_name}&${mask_value:02X} ; TODO: Calculate manually - variable value not found'
    
    def replace_instruction_xor(match):
        instruction = match.group(1)   # AND
        hex_value = int(match.group(2), 16)  # $FF
        var_name = match.group(3)      # Entity_MovementBits_MovingHorz
        
        # Look for variable value in the content
        var_pattern = rf'{var_name}\s*=\s*\$([0-9A-Fa-f]+)'
        var_match = re.search(var_pattern, all_content)
        
        # Also try decimal pattern
        if not var_match:
            var_pattern = rf'{var_name}\s*=\s*(\d+)'
            var_match = re.search(var_pattern, all_content)
        
        if var_match:
            # Parse value (handle both hex and decimal)
            var_val_str = var_match.group(1)
            
            # Check if hex or decimal
            if var_val_str.upper().startswith('0X') or any(c in var_val_str.upper() for c in 'ABCDEF'):
                var_val = int(var_val_str, 16)
            else:
                var_val = int(var_val_str)
            
            # Calculate: $FF ^ var_val
            result = hex_value ^ var_val
            
            return f'{instruction} #${result:02X}'
        else:
            # If we can't find the value, leave a comment for manual fix
            return f'{instruction} #${hex_value:02X}^{var_name} ; TODO: Calculate manually - variable value not found'
    
    # Apply the multiplication conversion
    converted_content = re.sub(multiplication_pattern, replace_multiplication, all_content)
    
    # Apply the bitwise OR conversion
    converted_content = re.sub(bitwise_or_pattern, replace_bitwise_or, converted_content)
    
    # Apply the complex bitwise conversion
    converted_content = re.sub(complex_bitwise_pattern, replace_complex_bitwise, converted_content)
    
    # Apply the simple bitwise AND conversion
    converted_content = re.sub(simple_bitwise_and_pattern, replace_simple_bitwise_and, converted_content)
    
    # Apply the instruction XOR conversion
    converted_content = re.sub(instruction_xor_pattern, replace_instruction_xor, converted_content)
    
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
