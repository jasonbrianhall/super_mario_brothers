"""
CA65-Specific ROM Analyzer for Indirect Jump Detection
Handles CA65 assembly format and syntax patterns
"""

import re
from typing import Dict, Set, List, Tuple
from pathlib import Path
import json

class CA65IndirectJumpAnalyzer:
    def __init__(self, assembly_file: str):
        self.assembly_file = assembly_file
        self.lines = []
        self.indirect_jumps: Dict[str, Set[str]] = {}
        self.jump_variables: Set[str] = set()
        self.address_to_label: Dict[str, str] = {}
        self.label_to_address: Dict[str, str] = {}
        self.zero_page_vars: Dict[str, str] = {}  # CA65 zero page variables
        
        self._load_assembly()
        self._build_label_maps()
        self._find_zero_page_vars()
    
    def _load_assembly(self):
        """Load assembly file and split into lines"""
        try:
            with open(self.assembly_file, 'r') as f:
                self.lines = f.readlines()
            print(f"Loaded {len(self.lines)} lines from {self.assembly_file}")
        except FileNotFoundError:
            print(f"Error: Could not find file {self.assembly_file}")
            return
    
    def _build_label_maps(self):
        """Build label maps for CA65 format"""
        # CA65 label patterns:
        # label_name:
        # @local_label:
        # ::global_label:
        
        label_patterns = [
            re.compile(r'^([a-zA-Z_][a-zA-Z0-9_]*):'),  # standard labels
            re.compile(r'^(@[a-zA-Z_][a-zA-Z0-9_]*):'), # local labels
            re.compile(r'^(::?[a-zA-Z_][a-zA-Z0-9_]*):'), # global labels
        ]
        
        # Also look for address comments
        address_comment_pattern = re.compile(r';\s*\$([A-Fa-f0-9]+)')
        
        current_address = None
        
        for line_num, line in enumerate(self.lines):
            line_stripped = line.strip()
            
            # Extract address from comment if present
            addr_match = address_comment_pattern.search(line)
            if addr_match:
                current_address = addr_match.group(1).upper()
            
            # Check for labels
            for pattern in label_patterns:
                match = pattern.match(line_stripped)
                if match:
                    label_name = match.group(1)
                    if current_address:
                        self.address_to_label[current_address] = label_name
                        self.label_to_address[label_name] = current_address
                    break
        
        print(f"Found {len(self.address_to_label)} labels")
    
    def _find_zero_page_vars(self):
        """Find zero page variable definitions in CA65"""
        # Look for patterns like:
        # var_name = $xx
        # .define var_name $xx
        # var_name: .res 1
        
        zp_patterns = [
            re.compile(r'^([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*\$([0-9A-Fa-f]+)'),
            re.compile(r'^\.define\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+\$([0-9A-Fa-f]+)'),
            re.compile(r'^([a-zA-Z_][a-zA-Z0-9_]*):.*\.res\s+\d+.*;\s*\$([0-9A-Fa-f]+)'),
        ]
        
        for line_num, line in enumerate(self.lines):
            line_stripped = line.strip()
            
            for pattern in zp_patterns:
                match = pattern.match(line_stripped)
                if match:
                    var_name = match.group(1)
                    address = match.group(2).upper()
                    self.zero_page_vars[var_name] = address
                    print(f"Found zero page var: {var_name} = ${address}")
                    break
    
    def find_indirect_jumps(self):
        """Find indirect jumps in CA65 format"""
        # CA65 indirect jump patterns:
        # jmp (variable)
        # jmp (zp_var)
        # jmp ($1234)
        
        indirect_jump_pattern = re.compile(r'jmp\s+\(([^)]+)\)', re.IGNORECASE)
        
        for line_num, line in enumerate(self.lines):
            match = indirect_jump_pattern.search(line)
            if match:
                operand = match.group(1).strip()
                
                # Clean up operand - remove $ prefix if present
                if operand.startswith('$'):
                    operand = operand[1:]
                
                self.jump_variables.add(operand)
                if operand not in self.indirect_jumps:
                    self.indirect_jumps[operand] = set()
                
                print(f"Found indirect jump: JMP ({operand}) at line {line_num + 1}")
    
    def analyze_jump_targets(self):
        """Analyze CA65 assembly for jump targets"""
        for jump_var in self.jump_variables:
            print(f"\nAnalyzing targets for {jump_var}...")
            targets = self._find_ca65_targets(jump_var)
            self.indirect_jumps[jump_var].update(targets)
            print(f"Found {len(targets)} targets: {sorted(targets)}")
    
    def _find_ca65_targets(self, jump_var: str) -> Set[str]:
        """Find targets using CA65-specific patterns"""
        targets = set()
        
        # Method 1: Direct stores to variable
        targets.update(self._find_ca65_stores(jump_var))
        
        # Method 2: 16-bit address loads (CA65 style)
        targets.update(self._find_ca65_address_loads(jump_var))
        
        # Method 3: Table-based loads
        targets.update(self._find_ca65_table_loads(jump_var))
        
        # Method 4: Immediate 16-bit stores
        targets.update(self._find_ca65_immediate_stores(jump_var))
        
        return targets
    
    def _find_ca65_stores(self, jump_var: str) -> Set[str]:
        """Find STA instructions in CA65 format"""
        targets = set()
        
        # CA65 addressing modes:
        # sta variable
        # sta variable+1  (high byte)
        # sta $1234
        # sta zp_var
        
        for line_num, line in enumerate(self.lines):
            # Look for STA to our variable
            if re.search(rf'\bsta\s+{re.escape(jump_var)}\b', line, re.IGNORECASE):
                print(f"    Found STA to {jump_var} at line {line_num + 1}: {line.strip()}")
                
                # Look for preceding LDA
                target = self._find_ca65_preceding_load(line_num)
                if target:
                    targets.add(target)
                    print(f"      -> Target: {target}")
            
            # Also check for high byte stores (variable+1)
            elif re.search(rf'\bsta\s+{re.escape(jump_var)}\+1\b', line, re.IGNORECASE):
                print(f"    Found STA to {jump_var}+1 at line {line_num + 1}: {line.strip()}")
                # This is high byte - look for corresponding low byte
                target = self._find_ca65_word_assignment(line_num, jump_var)
                if target:
                    targets.add(target)
        
        return targets
    
    def _find_ca65_preceding_load(self, store_line: int) -> str:
        """Find preceding LDA in CA65 format"""
        for i in range(1, min(11, store_line + 1)):
            line = self.lines[store_line - i].strip()
            
            # CA65 immediate load patterns:
            patterns = [
                r'lda\s+#\$([0-9A-Fa-f]+)',           # lda #$1234
                r'lda\s+#<([a-zA-Z_][a-zA-Z0-9_]*)',  # lda #<label
                r'lda\s+#>([a-zA-Z_][a-zA-Z0-9_]*)',  # lda #>label
                r'lda\s+#([0-9]+)',                   # lda #123 (decimal)
            ]
            
            for pattern in patterns:
                match = re.search(pattern, line, re.IGNORECASE)
                if match:
                    value = match.group(1)
                    # If it's a label, try to resolve to address
                    if value in self.label_to_address:
                        return self.label_to_address[value]
                    # If it's hex, return as-is
                    try:
                        int(value, 16)
                        return value.upper()
                    except ValueError:
                        return f"LABEL:{value}"
        
        return None
    
    def _find_ca65_address_loads(self, jump_var: str) -> Set[str]:
        """Find 16-bit address loads in CA65"""
        targets = set()
        
        # Look for patterns like:
        # lda #<label_name
        # sta jump_var
        # lda #>label_name
        # sta jump_var+1
        
        for line_num, line in enumerate(self.lines):
            # Look for low byte immediate
            low_match = re.search(r'lda\s+#<([a-zA-Z_][a-zA-Z0-9_]*)', line, re.IGNORECASE)
            if low_match:
                label_name = low_match.group(1)
                
                # Check if this gets stored to our variable
                if self._ca65_stores_to_var_nearby(line_num, jump_var, 5):
                    if label_name in self.label_to_address:
                        targets.add(self.label_to_address[label_name])
                    else:
                        targets.add(f"LABEL:{label_name}")
                    print(f"    Found 16-bit load of {label_name}")
        
        return targets
    
    def _find_ca65_table_loads(self, jump_var: str) -> Set[str]:
        """Find table-based loads in CA65"""
        targets = set()
        
        # Look for indexed loads from tables:
        # lda table,x
        # sta jump_var
        # lda table+1,x  
        # sta jump_var+1
        
        for line_num, line in enumerate(self.lines):
            # Look for indexed table access
            table_match = re.search(r'lda\s+([a-zA-Z_][a-zA-Z0-9_]*),x', line, re.IGNORECASE)
            if table_match:
                table_name = table_match.group(1)
                
                if self._ca65_stores_to_var_nearby(line_num, jump_var, 3):
                    targets.add(f"TABLE:{table_name}")
                    print(f"    Found table load from {table_name}")
        
        return targets
    
    def _find_ca65_immediate_stores(self, jump_var: str) -> Set[str]:
        """Find immediate 16-bit stores using CA65 .word directive"""
        targets = set()
        
        # Look for patterns where addresses are stored directly
        # This might appear in initialization code
        
        for line_num, line in enumerate(self.lines):
            # Look for .word directives that might initialize jump tables
            if re.search(r'\.word\s+', line, re.IGNORECASE):
                word_match = re.search(r'\.word\s+([a-zA-Z_][a-zA-Z0-9_]*)', line, re.IGNORECASE)
                if word_match:
                    label_name = word_match.group(1)
                    if label_name in self.label_to_address:
                        targets.add(self.label_to_address[label_name])
                        print(f"    Found .word directive: {label_name}")
        
        return targets
    
    def _find_ca65_word_assignment(self, high_byte_line: int, jump_var: str) -> str:
        """Find complete word assignment when we see high byte store"""
        # Look backwards for low byte assignment
        for i in range(1, 6):
            if high_byte_line - i < 0:
                break
            
            line = self.lines[high_byte_line - i].strip()
            
            # Look for low byte store to same variable
            if re.search(rf'\bsta\s+{re.escape(jump_var)}\b', line, re.IGNORECASE):
                # Found low byte store, now look for the label being loaded
                for j in range(1, 6):
                    if high_byte_line - i - j < 0:
                        break
                    
                    prev_line = self.lines[high_byte_line - i - j].strip()
                    low_match = re.search(r'lda\s+#<([a-zA-Z_][a-zA-Z0-9_]*)', prev_line, re.IGNORECASE)
                    if low_match:
                        label_name = low_match.group(1)
                        if label_name in self.label_to_address:
                            return self.label_to_address[label_name]
                        else:
                            return f"LABEL:{label_name}"
        
        return None
    
    def _ca65_stores_to_var_nearby(self, line_num: int, jump_var: str, search_range: int) -> bool:
        """Check if variable is stored to nearby (CA65 format)"""
        patterns = [
            rf'\bsta\s+{re.escape(jump_var)}\b',
            rf'\bsta\s+{re.escape(jump_var)}\+1\b',
        ]
        
        start = line_num
        end = min(len(self.lines), line_num + search_range + 1)
        
        for i in range(start, end):
            line = self.lines[i]
            for pattern in patterns:
                if re.search(pattern, line, re.IGNORECASE):
                    return True
        
        return False
    
    def generate_ca65_manual_guide(self, config_path: Path):
        """Generate CA65-specific manual analysis guide"""
        guide_file = config_path / "CA65_MANUAL_ANALYSIS_GUIDE.txt"
        
        with open(guide_file, 'w') as f:
            f.write("CA65 MANUAL ANALYSIS GUIDE FOR INDIRECT JUMPS\n")
            f.write("="*50 + "\n\n")
            
            f.write("CA65 Assembly Format - Patterns to Look For:\n\n")
            
            f.write("1. DIRECT STORES:\n")
            for jump_var in self.jump_variables:
                f.write(f"   grep -B5 'sta {jump_var}' {self.assembly_file}\n")
                f.write(f"   grep -B5 'sta {jump_var}+1' {self.assembly_file}\n")
            
            f.write("\n2. 16-BIT ADDRESS LOADS:\n")
            f.write(f"   grep -A10 'lda #<' {self.assembly_file}\n")
            f.write(f"   grep -A10 'lda #>' {self.assembly_file}\n")
            
            f.write("\n3. TABLE LOADS:\n")
            f.write(f"   grep -A5 'lda.*,x' {self.assembly_file}\n")
            f.write(f"   grep -A5 'lda.*,y' {self.assembly_file}\n")
            
            f.write("\n4. INITIALIZATION SECTIONS:\n")
            f.write(f"   grep -B5 -A5 '.word' {self.assembly_file}\n")
            
            f.write("\n5. JUMP ENGINE PATTERNS:\n")
            f.write(f"   grep -B5 -A5 'jmp (' {self.assembly_file}\n")
            
            f.write("\nCA65 SYNTAX NOTES:\n")
            f.write("- Labels: label_name:\n")
            f.write("- Zero page: variable = $xx\n")
            f.write("- Low byte: #<label\n")
            f.write("- High byte: #>label\n")
            f.write("- Indexed: table,x or table,y\n")
            f.write("- Comments: ; comment\n")
            
            f.write(f"\nSPECIFIC COMMANDS FOR YOUR FILE:\n")
            for jump_var in self.jump_variables:
                f.write(f"\nFor {jump_var}:\n")
                f.write(f"  grep -n -B3 -A1 'sta {jump_var}' {self.assembly_file}\n")
                f.write(f"  grep -n -B3 -A1 'sta {jump_var}+1' {self.assembly_file}\n")
                f.write(f"  grep -n -C5 '{jump_var}' {self.assembly_file}\n")
        
        print(f"Generated CA65 manual guide: {guide_file}")
    
    def run_analysis(self, config_dir: str = "ca65_indirect_config"):
        """Run CA65-specific analysis"""
        print("Starting CA65 indirect jump analysis...")
        
        self.find_indirect_jumps()
        
        if not self.jump_variables:
            print("No indirect jumps found.")
            return
        
        self.analyze_jump_targets()
        
        # Generate config files
        config_path = Path(config_dir)
        config_path.mkdir(exist_ok=True)
        
        self._generate_json_config(config_path)
        self._generate_individual_configs(config_path)
        self._generate_summary(config_path)
        self.generate_ca65_manual_guide(config_path)
        
        print(f"\nCA65 analysis complete! Check '{config_dir}' directory.")
        print("Use the grep commands in CA65_MANUAL_ANALYSIS_GUIDE.txt to find targets manually.")
    
    # Include the config generation methods from the previous analyzer
    def _generate_json_config(self, config_path: Path):
        config_data = {}
        for jump_var, targets in self.indirect_jumps.items():
            config_data[jump_var] = {
                "targets": list(targets),
                "resolved_targets": {}
            }
            for target in targets:
                if target.startswith(("TABLE:", "LABEL:")):
                    config_data[jump_var]["resolved_targets"][target] = "NEEDS_MANUAL_ANALYSIS"
                elif target in self.address_to_label:
                    config_data[jump_var]["resolved_targets"][target] = self.address_to_label[target]
        
        config_file = config_path / "indirect_jumps.json"
        with open(config_file, 'w') as f:
            json.dump(config_data, f, indent=2)
        print(f"Generated {config_file}")
    
    def _generate_individual_configs(self, config_path: Path):
        for jump_var, targets in self.indirect_jumps.items():
            clean_name = jump_var.replace('$', 'var_').replace('_var_', 'var_')
            config_file = config_path / f"{clean_name}_targets.txt"
            
            with open(config_file, 'w') as f:
                f.write(f"# CA65 Indirect jump targets for {jump_var}\n")
                f.write("# Format: ADDRESS:LABEL_NAME (one per line)\n")
                f.write("# Lines starting with # are comments\n")
                f.write("#\n")
                f.write("# CA65 specific commands to find targets:\n")
                f.write(f"# grep -B5 'sta {jump_var}' {self.assembly_file}\n")
                f.write(f"# grep -B5 'sta {jump_var}+1' {self.assembly_file}\n")
                f.write("#\n")
                
                for target in sorted(targets):
                    if target.startswith(("TABLE:", "LABEL:")):
                        f.write(f"# {target}  # NEEDS MANUAL ANALYSIS\n")
                    else:
                        label_name = self.address_to_label.get(target, f"label_{target.lower()}")
                        f.write(f"{target}:{label_name}\n")
                
                if not targets:
                    f.write("# No targets found automatically\n")
                    f.write("# Add targets manually in format: ADDRESS:LABEL\n")
                    f.write("# Example for CA65:\n")
                    f.write("# C35E:jump_engine\n")
                    f.write("# F51C:mode_handler\n")
            
            print(f"Generated {config_file}")
    
    def _generate_summary(self, config_path: Path):
        summary_file = config_path / "ca65_indirect_jumps_summary.txt"
        
        with open(summary_file, 'w') as f:
            f.write("CA65 Indirect Jump Analysis Summary\n")
            f.write("=" * 50 + "\n\n")
            
            f.write(f"Assembly file: {self.assembly_file}\n")
            f.write(f"Format: CA65\n")
            f.write(f"Total lines: {len(self.lines)}\n")
            f.write(f"Labels found: {len(self.address_to_label)}\n")
            f.write(f"Zero page vars: {len(self.zero_page_vars)}\n")
            f.write(f"Indirect jumps: {len(self.jump_variables)}\n\n")
            
            if self.zero_page_vars:
                f.write("ZERO PAGE VARIABLES:\n")
                for var, addr in self.zero_page_vars.items():
                    f.write(f"  {var} = ${addr}\n")
                f.write("\n")
            
            for jump_var, targets in self.indirect_jumps.items():
                f.write(f"INDIRECT JUMP: {jump_var}\n")
                f.write(f"  Auto-detected targets: {len(targets)}\n")
                
                if targets:
                    f.write("  Targets:\n")
                    for target in sorted(targets):
                        f.write(f"    {target}\n")
                else:
                    f.write("  *** NEEDS MANUAL ANALYSIS ***\n")
                f.write("\n")
            
            f.write("NEXT STEPS:\n")
            f.write("1. Run the grep commands in CA65_MANUAL_ANALYSIS_GUIDE.txt\n")
            f.write("2. Look for 'sta variable' and 'sta variable+1' patterns\n")
            f.write("3. Find the preceding 'lda #<label' and 'lda #>label' instructions\n")
            f.write("4. Update the individual config files with found targets\n")
        
        print(f"Generated {summary_file}")

def main():
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python ca65_analyzer.py <ca65_assembly_file> [config_directory]")
        print("Example: python ca65_analyzer.py duckhunt.asm ca65_config")
        sys.exit(1)
    
    assembly_file = sys.argv[1]
    config_dir = sys.argv[2] if len(sys.argv) > 2 else "ca65_indirect_config"
    
    analyzer = CA65IndirectJumpAnalyzer(assembly_file)
    analyzer.run_analysis(config_dir)

if __name__ == "__main__":
    main()
