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
    
    def parse_value(value_str):
        """Parse a value string that could be hex ($FF), decimal (255), or binary (%11111111)"""
        value_str = value_str.strip()
        
        if value_str.startswith('$'):
            # Hex value
            return int(value_str[1:], 16)
        elif value_str.startswith('%'):
            # Binary value
            return int(value_str[1:], 2)
        elif value_str.startswith('0x') or value_str.startswith('0X'):
            # Alternative hex format
            return int(value_str, 16)
        else:
            # Decimal value
            return int(value_str)
    
    def find_variable_value(var_name, content):
        """Find the value of a variable, supporting hex, decimal, and binary formats"""
        # Try hex pattern first
        patterns = [
            rf'{var_name}\s*=\s*\$([0-9A-Fa-f]+)',  # $FF
            rf'{var_name}\s*=\s*%([01]+)',           # %11111111
            rf'{var_name}\s*=\s*(\d+)',              # 255
            rf'{var_name}\s*=\s*0x([0-9A-Fa-f]+)',  # 0xFF
        ]
        
        for pattern in patterns:
            match = re.search(pattern, content, re.IGNORECASE)
            if match:
                value_str = match.group(1)
                if '$' in pattern:
                    return int(value_str, 16)
                elif '%' in pattern:
                    return int(value_str, 2)
                elif '0x' in pattern:
                    return int(value_str, 16)
                else:
                    return int(value_str)
        
        return None
    
    def apply_conversions(content, pass_num=1, is_final_pass=False):
        """Apply all conversions in a single pass"""
        print(f"  Running pass {pass_num}...")
        
        # Pattern to match expressions like: = OAM_Y+(4*FreezeEffect_OAM_Slot)
        multiplication_pattern = r'(\w+)\s*=\s*(\w+)\+\((\d+)\*(\w+)\)'
        
        # Pattern to match multiplication in instructions like: LDY #Lives_Mario_OAM_Slot*4
        instruction_multiplication_pattern = r'(\w+)\s+#(\w+)\*(\d+)'
        
        # Pattern to match address arithmetic in instructions like: STA BufferAddr2+8 or STA BufferAddr2+$0F
        instruction_address_arithmetic_pattern = r'(\w+)\s+(\w+)\+(\$?[0-9A-Fa-f]+)'
        
        # Pattern to match bitwise OR expressions like: = Entity_MovementBits_MovingRight|Entity_MovementBits_MovingLeft
        bitwise_or_pattern = r'(\w+)\s*=\s*(\w+)\|(\w+)'
        
        # Pattern to match bitwise OR in instructions like: LDX #Entity_MovementBits_Skidding|Entity_MovementBits_MovingLeft
        instruction_bitwise_or_pattern = r'(\w+)\s+#(\w+)\|(\w+)'
        
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
            slot_value = find_variable_value(slot_var, content)
            
            if slot_value is not None:
                calculated_offset = multiplier * slot_value
                return f'{var_name} = {base_addr}+${calculated_offset:02X}'
            else:
                # If we can't find the slot value, add TODO only on final pass
                if is_final_pass:
                    return f'{var_name} = {base_addr}+{multiplier}*{slot_var} ; TODO: Calculate manually - slot value not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_instruction_multiplication(match):
            instruction = match.group(1)   # LDY
            var_name = match.group(2)      # Lives_Mario_OAM_Slot
            multiplier = int(match.group(3))  # 4
            
            # Look for the variable value in the content
            var_val = find_variable_value(var_name, content)
            
            if var_val is not None:
                # Calculate: var_val * multiplier
                result = var_val * multiplier
                return f'{instruction} #${result:02X}'
            else:
                # If we can't find the value, add TODO only on final pass
                if is_final_pass:
                    return f'{instruction} #{var_name}*{multiplier} ; TODO: Calculate manually - variable value not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_instruction_address_arithmetic(match):
            instruction = match.group(1)   # STA
            base_addr = match.group(2)     # BufferAddr2
            offset_str = match.group(3)    # 8 or $0F
            
            # Parse the offset value (could be hex or decimal)
            if offset_str.startswith('$'):
                offset_val = int(offset_str[1:], 16)
            else:
                offset_val = int(offset_str)
            
            # Look for the base address value in the content
            base_val = find_variable_value(base_addr, content)
            
            if base_val is not None:
                # Calculate: base_val + offset_val
                result = base_val + offset_val
                return f'{instruction} ${result:04X}'
            else:
                # If we can't find the base value, add TODO only on final pass
                if is_final_pass:
                    return f'{instruction} {base_addr}+{offset_str} ; TODO: Calculate manually - base address not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_bitwise_or(match):
            var_name = match.group(1)
            left_var = match.group(2)
            right_var = match.group(3)
            
            # Look for both variable values in the content
            left_val = find_variable_value(left_var, content)
            right_val = find_variable_value(right_var, content)
            
            if left_val is not None and right_val is not None:
                # Calculate bitwise OR
                result = left_val | right_val
                return f'{var_name} = ${result:02X}'
            else:
                # If we can't find the values, add TODO only on final pass
                if is_final_pass:
                    return f'{var_name} = {left_var}|{right_var} ; TODO: Calculate manually - variable values not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_instruction_bitwise_or(match):
            instruction = match.group(1)   # LDX
            left_var = match.group(2)      # Entity_MovementBits_Skidding
            right_var = match.group(3)     # Entity_MovementBits_MovingLeft
            
            # Look for both variable values in the content
            left_val = find_variable_value(left_var, content)
            right_val = find_variable_value(right_var, content)
            
            if left_val is not None and right_val is not None:
                # Calculate bitwise OR
                result = left_val | right_val
                return f'{instruction} #${result:02X}'
            else:
                # If we can't find the values, add TODO only on final pass
                if is_final_pass:
                    return f'{instruction} #{left_var}|{right_var} ; TODO: Calculate manually - variable values not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_complex_bitwise(match):
            directive = match.group(1)
            left_var = match.group(2)      # MAPPER
            shift_amount = int(match.group(3))  # 4
            mask_value = int(match.group(4), 16)  # $F0
            right_var = match.group(5)     # MIRRORING
            
            # Look for variable values in the content
            left_val = find_variable_value(left_var, content)
            right_val = find_variable_value(right_var, content)
            
            if left_val is not None and right_val is not None:
                # Calculate: MAPPER<<4&$F0|MIRRORING
                shifted = left_val << shift_amount
                masked = shifted & mask_value
                result = masked | right_val
                
                return f'DB ${result:02X}'
            else:
                # If we can't find the values, add TODO only on final pass
                if is_final_pass:
                    return f'DB {left_var}<<{shift_amount}&${mask_value:02X}|{right_var} ; TODO: Calculate manually - variable values not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_simple_bitwise_and(match):
            directive = match.group(1)
            var_name = match.group(2)      # MAPPER
            mask_value = int(match.group(3), 16)  # $F0
            
            # Look for variable value in the content
            var_val = find_variable_value(var_name, content)
            
            if var_val is not None:
                # Calculate: MAPPER & $F0
                result = var_val & mask_value
                return f'DB ${result:02X}'
            else:
                # If we can't find the value, add TODO only on final pass
                if is_final_pass:
                    return f'DB {var_name}&${mask_value:02X} ; TODO: Calculate manually - variable value not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        def replace_instruction_xor(match):
            instruction = match.group(1)   # AND
            hex_value = int(match.group(2), 16)  # $FF
            var_name = match.group(3)      # Entity_MovementBits_MovingHorz
            
            # Look for variable value in the content
            var_val = find_variable_value(var_name, content)
            
            if var_val is not None:
                # Calculate: $FF ^ var_val
                result = hex_value ^ var_val
                return f'{instruction} #${result:02X}'
            else:
                # If we can't find the value, add TODO only on final pass
                if is_final_pass:
                    return f'{instruction} #${hex_value:02X}^{var_name} ; TODO: Calculate manually - variable value not found'
                else:
                    # Leave unchanged for next pass
                    return match.group(0)
        
        # Apply all conversions
        new_content = content
        
        # Apply the multiplication conversion
        new_content = re.sub(multiplication_pattern, replace_multiplication, new_content)
        
        # Apply the instruction multiplication conversion
        new_content = re.sub(instruction_multiplication_pattern, replace_instruction_multiplication, new_content)
        
        # Apply the instruction address arithmetic conversion
        new_content = re.sub(instruction_address_arithmetic_pattern, replace_instruction_address_arithmetic, new_content)
        
        # Apply the bitwise OR conversion (variable assignments)
        new_content = re.sub(bitwise_or_pattern, replace_bitwise_or, new_content)
        
        # Apply the instruction bitwise OR conversion
        new_content = re.sub(instruction_bitwise_or_pattern, replace_instruction_bitwise_or, new_content)
        
        # Apply the complex bitwise conversion
        new_content = re.sub(complex_bitwise_pattern, replace_complex_bitwise, new_content)
        
        # Apply the simple bitwise AND conversion
        new_content = re.sub(simple_bitwise_and_pattern, replace_simple_bitwise_and, new_content)
        
        # Apply the instruction XOR conversion
        new_content = re.sub(instruction_xor_pattern, replace_instruction_xor, new_content)
        
        return new_content
    
    # Process the main file and all includes
    print("Processing includes and reading files...")
    all_content = process_file(input_file)
    
    # Apply conversions with multiple passes
    print("Applying conversions with multiple passes...")
    current_content = all_content
    
    for pass_num in range(1, 5):  # 4 passes
        is_final_pass = (pass_num == 4)
        new_content = apply_conversions(current_content, pass_num, is_final_pass)
        
        # Check if anything changed
        if new_content == current_content:
            print(f"  No changes in pass {pass_num}, stopping early.")
            break
        
        current_content = new_content
    
    # Additional ASM6 to x816 syntax conversions
    print("Applying final syntax conversions...")
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
        current_content = re.sub(pattern, replacement, current_content, flags=re.IGNORECASE)
    
    # Write the result
    with open(output_file, 'w') as f:
        f.write(current_content)
    
    print(f"Conversion complete! Output written to {output_file}")
    if flatten_includes:
        print("All .include files have been flattened into the output file.")
    else:
        print(".include statements converted to INCLUDE (use --no-flatten to disable flattening)")
    
    # Count remaining TODOs
    todo_count = current_content.count('TODO:')
    if todo_count > 0:
        print(f"Warning: {todo_count} 'TODO' comments remain for manual fixes.")
    else:
        print("All conversions completed successfully!")

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
