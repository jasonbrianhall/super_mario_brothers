"""
First Pass Label Classification for 6502 Assembly
Determines if labels are code labels or data labels
"""

import re
from typing import Dict, List, Tuple, Set
from enum import Enum

class LabelType(Enum):
    CODE = "code"
    DATA = "data"
    ALIAS = "alias"
    UNKNOWN = "unknown"

class FirstPassClassifier:
    def __init__(self, assembly_text: str):
        self.assembly_text = assembly_text
        self.lines = assembly_text.split('\n')
        self.labels: Dict[str, LabelType] = {}
        self.label_line_map: Dict[str, int] = {}
        self.data_indicators = {'.db', '.dw', '.byte', '.word'}
        self.instruction_keywords = self._get_6502_instructions()
    
    def _get_6502_instructions(self) -> Set[str]:
        """Get set of all 6502 instruction mnemonics"""
        return {
            'lda', 'ldx', 'ldy', 'sta', 'stx', 'sty',
            'tax', 'tay', 'txa', 'tya', 'tsx', 'txs',
            'pha', 'php', 'pla', 'plp',
            'and', 'eor', 'ora', 'bit',
            'adc', 'sbc', 'cmp', 'cpx', 'cpy',
            'inc', 'inx', 'iny', 'dec', 'dex', 'dey',
            'asl', 'lsr', 'rol', 'ror',
            'jmp', 'jsr', 'rts', 'rti',
            'bcc', 'bcs', 'beq', 'bmi', 'bne', 'bpl', 'bvc', 'bvs',
            'clc', 'cld', 'cli', 'clv', 'sec', 'sed', 'sei',
            'brk', 'nop'
        }
    
    def classify_all_labels(self) -> Dict[str, LabelType]:
        """Perform first pass classification of all labels"""
        # First, find all labels and their locations
        self._find_all_labels()
        
        # Then classify each label based on what follows it
        for label_name, line_num in self.label_line_map.items():
            self.labels[label_name] = self._classify_single_label(label_name, line_num)
        
        return self.labels
    
    def _find_all_labels(self):
        """Find all labels in the assembly code"""
        label_pattern = re.compile(r'^([a-zA-Z_][a-zA-Z0-9_]*):')
        
        for line_num, line in enumerate(self.lines):
            line = line.strip()
            match = label_pattern.match(line)
            if match:
                label_name = match.group(1)
                self.label_line_map[label_name] = line_num
    
    def _classify_single_label(self, label_name: str, start_line: int) -> LabelType:
        """Classify a single label based on what follows it"""
        # Look at the next several lines after the label
        content_analysis = self._analyze_content_after_label(start_line)
        
        # Decision logic
        if content_analysis['has_data_directives'] and not content_analysis['has_instructions']:
            return LabelType.DATA
        elif content_analysis['has_instructions']:
            return LabelType.CODE
        elif content_analysis['points_to_other_label']:
            return LabelType.ALIAS
        elif content_analysis['has_data_directives']:
            return LabelType.DATA
        else:
            # Default to CODE for labels that don't clearly indicate data
            return LabelType.CODE
    
    def _analyze_content_after_label(self, start_line: int) -> Dict[str, bool]:
        """Analyze content following a label to determine its type"""
        analysis = {
            'has_instructions': False,
            'has_data_directives': False,
            'points_to_other_label': False,
            'line_count_analyzed': 0
        }
        
        # Look at up to 10 lines after the label
        max_lines_to_check = min(10, len(self.lines) - start_line - 1)
        
        for i in range(1, max_lines_to_check + 1):
            line_num = start_line + i
            if line_num >= len(self.lines):
                break
                
            line = self.lines[line_num].strip()
            
            # Skip empty lines and comments
            if not line or line.startswith(';'):
                continue
            
            # If we hit another label, stop analyzing
            if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*:', line):
                break
            
            analysis['line_count_analyzed'] += 1
            
            # Check for data directives
            if self._line_contains_data_directive(line):
                analysis['has_data_directives'] = True
            
            # Check for instructions
            if self._line_contains_instruction(line):
                analysis['has_instructions'] = True
            
            # Check if it points to another label (simple heuristic)
            if self._line_points_to_label(line):
                analysis['points_to_other_label'] = True
            
            # If we've found clear evidence, we can stop early
            if analysis['has_instructions'] or analysis['has_data_directives']:
                break
        
        return analysis
    
    def _line_contains_data_directive(self, line: str) -> bool:
        """Check if line contains data directive like .db or .dw"""
        # Remove comments
        line = line.split(';')[0].strip()
        
        # Check for data directives
        tokens = line.lower().split()
        if tokens and tokens[0] in self.data_indicators:
            return True
        
        return False
    
    def _line_contains_instruction(self, line: str) -> bool:
        """Check if line contains a 6502 instruction"""
        # Remove comments
        line = line.split(';')[0].strip()
        
        # Extract the first token (should be instruction)
        tokens = line.lower().split()
        if tokens and tokens[0] in self.instruction_keywords:
            return True
        
        return False
    
    def _line_points_to_label(self, line: str) -> bool:
        """Simple heuristic to check if line points to another label"""
        # This is a simple check - could be made more sophisticated
        line = line.split(';')[0].strip()
        
        # Look for patterns like "jmp LabelName" or "jsr LabelName"
        tokens = line.lower().split()
        if len(tokens) >= 2 and tokens[0] in ['jmp', 'jsr']:
            target = tokens[1]
            # Check if target looks like a label (not a hex address, etc.)
            if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', target):
                return True
        
        return False
    
    def print_classification_report(self):
        """Print a report of the classification results"""
        print("Label Classification Report:")
        print("=" * 50)
        
        code_labels = []
        data_labels = []
        alias_labels = []
        unknown_labels = []
        
        for label, label_type in self.labels.items():
            if label_type == LabelType.CODE:
                code_labels.append(label)
            elif label_type == LabelType.DATA:
                data_labels.append(label)
            elif label_type == LabelType.ALIAS:
                alias_labels.append(label)
            else:
                unknown_labels.append(label)
        
        print(f"CODE Labels ({len(code_labels)}):")
        for label in sorted(code_labels):
            print(f"  {label}")
        
        print(f"\nDATA Labels ({len(data_labels)}):")
        for label in sorted(data_labels):
            print(f"  {label}")
        
        if alias_labels:
            print(f"\nALIAS Labels ({len(alias_labels)}):")
            for label in sorted(alias_labels):
                print(f"  {label}")
        
        if unknown_labels:
            print(f"\nUNKNOWN Labels ({len(unknown_labels)}):")
            for label in sorted(unknown_labels):
                print(f"  {label}")
        
        print(f"\nTotal Labels: {len(self.labels)}")

def test_classifier():
    """Test the classifier with sample assembly code"""
    sample_code = """
; Sample 6502 Assembly Code
Start:
    lda #$00
    sta $0200
    jmp Loop

Loop:
    inx
    bne Loop
    rts

DataTable:
    .db $01, $02, $03, $04
    .db $05, $06, $07, $08

MessageText:
    .db "HELLO WORLD", $00

SoundData:
    .dw $1000, $2000, $3000

AliasDirect:
    jmp SomeOtherPlace

MixedSection:
    lda DataTable
    .db $FF

CodeOnly:
    pha
    pla
    rts
"""
    
    classifier = FirstPassClassifier(sample_code)
    labels = classifier.classify_all_labels()
    classifier.print_classification_report()
    
    return labels

if __name__ == "__main__":
    # Run test
    test_classifier()
