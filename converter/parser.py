"""
Recursive Descent Parser for 6502 Assembly - DEBUG VERSION WITH DETAILED ERROR REPORTING
"""

from typing import List, Optional
from tokens import Token, TokenType
from ast_nodes import *

class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0
        self.current_token = tokens[0] if tokens else None
        self.source_lines = []  # Will be populated with source file content
        self.debug_mode = True
        
        # Try to load source file for better debugging
        self.load_source_file()
    
    def load_source_file(self):
        """Load source file content for debugging"""
        try:
            # Try to find the source file name from the first token or context
            # For now, we'll just use a placeholder that can be set externally
            pass
        except:
            pass
    
    def set_source_lines(self, lines: List[str]):
        """Set source lines for debugging purposes"""
        self.source_lines = lines
    
    def get_source_line(self, line_num: int) -> str:
        """Get the actual source line for debugging"""
        if self.source_lines and 1 <= line_num <= len(self.source_lines):
            return self.source_lines[line_num - 1].strip()
        return f"<line {line_num} not available>"
    
    def debug_print_token_context(self, message: str):
        """Print detailed debugging information about current token and context"""
        if not self.debug_mode:
            return
            
        print(f"\n=== PARSER DEBUG: {message} ===")
        
        if not self.current_token:
            print("Current token: None (EOF)")
            return
        
        print(f"Current token: {self.current_token.type.name} = '{self.current_token.value}'")
        print(f"Line {self.current_token.line}, Column {self.current_token.column}")
        print(f"Source line: {self.get_source_line(self.current_token.line)}")
        
        # Show a few tokens around the current position for context
        print(f"\nToken context (position {self.pos}):")
        start = max(0, self.pos - 2)
        end = min(len(self.tokens), self.pos + 3)
        
        for i in range(start, end):
            token = self.tokens[i]
            marker = " --> " if i == self.pos else "     "
            print(f"{marker}[{i}] {token.type.name}: '{token.value}' (line {token.line})")
        
        print("=" * 50)
    
    def error(self, message: str):
        self.debug_print_token_context(f"ERROR - {message}")
        if self.current_token:
            raise SyntaxError(f"Parse error at line {self.current_token.line}: {message}")
        else:
            raise SyntaxError(f"Parse error: {message}")
    
    def advance(self):
        """Move to the next token"""
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
            self.current_token = self.tokens[self.pos]
        else:
            self.current_token = None
    
    def expect(self, token_type: TokenType) -> Token:
        """Expect a specific token type and advance"""
        if not self.current_token or self.current_token.type != token_type:
            self.error(f"Expected {token_type.name}, got {self.current_token.type.name if self.current_token else 'EOF'}")
        
        token = self.current_token
        self.advance()
        return token
    
    def match(self, token_type: TokenType) -> bool:
        """Check if current token matches type without advancing"""
        return self.current_token and self.current_token.type == token_type
    
    def parse(self) -> RootNode:
        """Enhanced parser with better label recovery"""
        root = RootNode()
        parse_attempts = 0
        max_attempts = len(self.tokens) * 2
    
        while self.current_token and self.current_token.type != TokenType.EOF:
            parse_attempts += 1
            if parse_attempts > max_attempts:
                break
            
            old_pos = self.pos
        
            try:
                if self.match(TokenType.DIRECTIVE):
                    self.parse_directive()
                
                elif self.match(TokenType.NAME):
                    decl = self.parse_declaration()
                    if decl:
                        root.children.append(decl)
                        decl.parent = root
                    else:
                        # If not a declaration, might be a missed label
                        # Check if next token is colon
                        if (self.pos < len(self.tokens) - 1 and 
                            self.tokens[self.pos + 1].type == TokenType.COLON):
                            # Create label from NAME + COLON
                            name_token = self.current_token
                            self.advance()  # consume NAME    
                            self.advance()  # consume COLON
                        
                            label = LabelNode(f"{name_token.value}:", None)
                            label.line_number = name_token.line
                        
                            # Try to parse content after label
                            content = self.parse_label_content()
                            if content:
                                label.child = content
                                content.parent = label
                        
                            root.children.append(label)
                            label.parent = root
                            print(f"Recovered missed label: {name_token.value}")
                        else:
                            # Skip unknown NAME token
                            self.advance()
                        
                elif self.match(TokenType.LABEL):
                    section = self.parse_section()
                    if section:
                        root.children.append(section)
                        section.parent = root
                    else:
                        # Create empty label if parsing failed
                        label_token = self.current_token
                        empty_label = LabelNode(label_token.value, None)
                        empty_label.line_number = label_token.line
                        root.children.append(empty_label)
                        empty_label.parent = root
                        self.advance()
                        print(f"Created empty label: {label_token.value}")
                    
                else:
                    # Unknown token - skip it
                    if self.current_token:
                        print(f"Skipping unknown token: {self.current_token.type} '{self.current_token.value}' at line {self.current_token.line}")
                    self.advance()
                
                # Safety: ensure we always advance
                if self.pos == old_pos and self.current_token:
                    self.advance()
                
            except Exception as e:
                print(f"Parse error at line {self.current_token.line if self.current_token else 'EOF'}: {e}")
                # Skip to next likely parsing point
                while (self.current_token and 
                       self.current_token.type not in [TokenType.LABEL, TokenType.DIRECTIVE, TokenType.EOF]):
                    self.advance()
    
        print(f"Parser created {len(root.children)} root children")
        return root

    def parse_label_content(self):
        """Parse content after a label with better error recovery"""
        content_list = None
        last_item = None
    
        while (self.current_token and 
               self.current_token.type not in [TokenType.LABEL, TokenType.EOF]):
        
            try:
                item = None
            
                if self.match(TokenType.INSTRUCTION):
                    item = self.parse_instruction()
                elif self.match(TokenType.DATA8):
                    item = self.parse_data8()
                elif self.match(TokenType.DATA16):
                    item = self.parse_data16()
                else:
                    # Unknown content - skip
                    self.advance()
                    continue
                
                if item:
                    # Add to list
                    list_node = ListNode(item)
                    if content_list is None:
                        content_list = list_node
                        last_item = list_node
                    else:
                        last_item.next = list_node
                        last_item = list_node
                    
            except Exception as e:
                print(f"Error parsing label content: {e}")
                # Skip this item and continue
                self.advance()
                continue
    
        return content_list
    
    def parse_directive(self):
        """Parse a directive (currently just consume it)"""
        directive_token = self.current_token
        self.advance()  # consume directive
        
        if self.debug_mode:
            print(f"  -> Directive: {directive_token.value}")
        
        if self.current_token and self.current_token.type in [TokenType.HEXCONST, TokenType.BINCONST, TokenType.DECCONST]:
            if self.debug_mode:
                print(f"  -> Directive argument: {self.current_token.value}")
            self.advance()  # consume constant
    
    def parse_declaration(self) -> Optional[DeclNode]:
        """Parse a declaration: NAME '=' expr"""
        if not self.match(TokenType.NAME):
            return None
        
        name_token = self.current_token
        self.advance()
        
        if not self.match(TokenType.EQUALS):
            # This isn't a declaration, backtrack
            if self.debug_mode:
                print(f"  -> NAME '{name_token.value}' not followed by '=', not a declaration")
            self.pos -= 1
            self.current_token = self.tokens[self.pos]
            return None
        
        self.advance()  # consume '='
        
        expr = self.parse_expression()
        if not expr:
            self.error("Expected expression after '='")
        
        decl = DeclNode(name_token.value, expr)
        decl.line_number = name_token.line
        return decl
    
    def parse_section(self) -> Optional[LabelNode]:
        """Parse a section: LABEL code or LABEL section"""
        if not self.match(TokenType.LABEL):
            return None
        
        label_token = self.current_token
        self.advance()
        
        # Try to parse another section (nested labels)
        if self.match(TokenType.LABEL):
            if self.debug_mode:
                print(f"  -> Found nested label, parsing recursively")
            child_section = self.parse_section()
            if child_section:
                label_node = LabelNode(label_token.value, child_section)
                label_node.line_number = label_token.line
                child_section.parent = label_node
                return label_node
        
        # Parse code block
        if self.debug_mode:
            print(f"  -> Parsing code block for label {label_token.value}")
        code = self.parse_code()
        if code:
            label_node = LabelNode(label_token.value, code)
            label_node.line_number = label_token.line
            code.parent = label_node
            return label_node
        
        # Even if no code follows, create the label node
        if self.debug_mode:
            print(f"  -> No code found after label, creating empty label node")
        label_node = LabelNode(label_token.value, None)
        label_node.line_number = label_token.line
        return label_node
    
    def parse_code(self) -> Optional[ListNode]:
        """Parse a code block (list of instructions and data) - DEBUG VERSION"""
        items = []
        code_attempts = 0
        max_code_attempts = 1000  # Reasonable limit for code block parsing
        
        while (self.current_token and 
               self.current_token.type not in [TokenType.EOF, TokenType.LABEL]):
            
            code_attempts += 1
            if code_attempts > max_code_attempts:
                self.debug_print_token_context("INFINITE LOOP IN PARSE_CODE - ABORTING")
                break
                
            old_pos = self.pos
            
            # Try to parse instruction
            inst = self.parse_instruction()
            if inst:
                items.append(inst)
                if self.debug_mode and len(items) % 100 == 0:  # Progress indicator
                    print(f"    -> Parsed {len(items)} code items so far...")
                continue
            
            # Try to parse data
            data = self.parse_data()
            if data:
                items.append(data)
                if self.debug_mode and len(items) % 100 == 0:  # Progress indicator
                    print(f"    -> Parsed {len(items)} code items so far...")
                continue
            
            # If we can't parse either, provide detailed debugging info
            if self.pos == old_pos:
                self.debug_print_token_context("CANNOT PARSE AS INSTRUCTION OR DATA")
                
                # Show what instruction types we tried to match
                if self.current_token:
                    token_type = self.current_token.type
                    print(f"Token type {token_type.name} is not recognized as:")
                    print("  - Instruction types: LDA, LDX, STA, JMP, etc.")
                    print("  - Data types: DATABYTES (.db), DATAWORDS (.dw)")
                    print("  - End markers: LABEL, EOF")
                    print("Forcing advance to prevent infinite loop...")
                
                self.advance()  # Force advancement to prevent infinite loop
            
            # Double-check we're making progress
            if self.pos == old_pos:
                self.debug_print_token_context("STILL STUCK AFTER FORCED ADVANCE")
                if self.current_token:
                    self.advance()
                break
        
        if self.debug_mode and items:
            print(f"  -> Parsed code block with {len(items)} items")
        
        if not items:
            return None
        
        # Create linked list in forward order - maintain source code order
        head = None
        tail = None
        
        for item in items:
            list_node = ListNode()
            list_node.value = item
            item.parent = list_node
            list_node.next = None
            
            if head is None:
                head = list_node
                tail = list_node
            else:
                tail.next = list_node
                tail = list_node
        
        return head
    
    def parse_data(self) -> Optional[AstNode]:
        """Parse data declaration: .db or .dw followed by data list"""
        if self.match(TokenType.DATABYTES):
            data_token = self.current_token
            if self.debug_mode:
                print(f"    -> Parsing .db at line {data_token.line}")
            self.advance()
            dlist = self.parse_data_list()
            if dlist:
                data_node = AstNode(AstType.AST_DATA8)
                data_node.line_number = data_token.line
                data_node.value = dlist
                dlist.parent = data_node
                return data_node
        elif self.match(TokenType.DATAWORDS):
            data_token = self.current_token
            if self.debug_mode:
                print(f"    -> Parsing .dw at line {data_token.line}")
            self.advance()
            dlist = self.parse_data_list()
            if dlist:
                data_node = AstNode(AstType.AST_DATA16)
                data_node.line_number = data_token.line
                data_node.value = dlist
                dlist.parent = data_node
                return data_node
        
        return None
    
    def parse_data_list(self) -> Optional[ListNode]:
        """Parse a comma-separated list of expressions"""
        expressions = []
        
        expr = self.parse_expression()
        if not expr:
            if self.debug_mode:
                print(f"      -> No expression found for data list")
            return None
        
        expressions.append(expr)
        
        while self.match(TokenType.COMMA):
            self.advance()  # consume ','
            expr = self.parse_expression()
            if not expr:
                if self.debug_mode:
                    print(f"      -> No expression after comma, ending data list")
                break
            expressions.append(expr)
        
        if self.debug_mode:
            print(f"      -> Data list has {len(expressions)} expressions")
        
        # Create linked list in forward order
        head = None
        tail = None
        
        for expr in expressions:
            list_node = ListNode()
            list_node.value = expr
            expr.parent = list_node
            list_node.next = None
            
            if head is None:
                head = list_node
                tail = list_node
            else:
                tail.next = list_node
                tail = list_node
        
        return head
    
    def parse_instruction(self) -> Optional[InstructionNode]:
        """Parse a 6502 instruction with debugging"""
        if not self.current_token:
            return None
        
        token = self.current_token
        
        # Instructions that take operands
        if token.type in [TokenType.LDA, TokenType.LDX, TokenType.LDY, 
                         TokenType.STA, TokenType.STX, TokenType.STY,
                         TokenType.AND, TokenType.EOR, TokenType.ORA, TokenType.BIT,
                         TokenType.ADC, TokenType.SBC, TokenType.CMP, TokenType.CPX, TokenType.CPY,
                         TokenType.INC, TokenType.DEC, TokenType.JMP]:
            if self.debug_mode:
                print(f"    -> Parsing instruction {token.type.name} at line {token.line}")
            self.advance()
            operand = self.parse_instruction_expression()
            inst = InstructionNode(token.type.value, operand)
            inst.line_number = token.line
            return inst
        
        # ASL/LSR/ROL/ROR can be with or without operand
        elif token.type in [TokenType.ASL, TokenType.LSR, TokenType.ROL, TokenType.ROR]:
            if self.debug_mode:
                print(f"    -> Parsing shift instruction {token.type.name} at line {token.line}")
            self.advance()
            operand = self.parse_instruction_expression()  # Optional
            inst = InstructionNode(token.type.value, operand)
            inst.line_number = token.line
            return inst
        
        # Branch instructions take NAME operands
        elif token.type in [TokenType.JSR, TokenType.BCC, TokenType.BCS, TokenType.BEQ, 
                           TokenType.BMI, TokenType.BNE, TokenType.BPL, TokenType.BVC, TokenType.BVS]:
            if self.debug_mode:
                print(f"    -> Parsing branch/jump instruction {token.type.name} at line {token.line}")
            self.advance()
            if not self.match(TokenType.NAME):
                self.error(f"Expected NAME after {token.type.name}")
            name_token = self.current_token
            self.advance()
            inst = InstructionNode(token.type.value, name_token.value)
            inst.line_number = token.line
            return inst
        
        # Instructions with no operands
        elif token.type in [TokenType.TAX, TokenType.TAY, TokenType.TXA, TokenType.TYA,
                           TokenType.TSX, TokenType.TXS, TokenType.PHA, TokenType.PHP,
                           TokenType.PLA, TokenType.PLP, TokenType.INX, TokenType.INY,
                           TokenType.DEX, TokenType.DEY, TokenType.RTS, TokenType.CLC,
                           TokenType.CLD, TokenType.CLI, TokenType.CLV, TokenType.SEC,
                           TokenType.SED, TokenType.SEI, TokenType.BRK, TokenType.NOP, TokenType.RTI]:
            if self.debug_mode:
                print(f"    -> Parsing no-operand instruction {token.type.name} at line {token.line}")
            self.advance()
            inst = InstructionNode(token.type.value, None)
            inst.line_number = token.line
            return inst
        
        return None
    
    def parse_const(self) -> Optional[str]:
        """Parse a constant (hex, binary, or decimal)"""
        if self.match(TokenType.HEXCONST):
            value = self.current_token.value
            self.advance()
            return value
        elif self.match(TokenType.BINCONST):
            value = self.current_token.value
            self.advance()
            return value
        elif self.match(TokenType.DECCONST):
            value = self.current_token.value
            self.advance()
            return value
        return None
    
    def parse_expression(self) -> Optional[AstNode]:
        """Parse an expression"""
        return self.parse_additive_expression()
    
    def parse_additive_expression(self) -> Optional[AstNode]:
        """Parse addition and subtraction"""
        left = self.parse_primary_expression()
        if not left:
            return None
        
        while self.current_token and self.current_token.type in [TokenType.PLUS, TokenType.MINUS]:
            op_token = self.current_token
            self.advance()
            right = self.parse_primary_expression()
            if not right:
                self.error(f"Expected expression after '{op_token.value}'")
            
            if op_token.type == TokenType.PLUS:
                left = BinaryNode(AstType.AST_ADD, left, right)
            else:
                left = BinaryNode(AstType.AST_SUBTRACT, left, right)
        
        return left
    
    def parse_primary_expression(self) -> Optional[AstNode]:
        """Parse primary expressions"""
        if self.match(TokenType.NAME):
            name_token = self.current_token
            self.advance()
            return AstNode(AstType.AST_NAME, name_token.value)
        
        const = self.parse_const()
        if const:
            return AstNode(AstType.AST_CONST, const)
        
        if self.match(TokenType.HASH):
            self.advance()  # consume '#'
            expr = self.parse_expression()
            if not expr:
                self.error("Expected expression after '#'")
            return UnaryNode(AstType.AST_IMMEDIATE, expr)
        
        if self.match(TokenType.LESS):
            self.advance()  # consume '<'
            expr = self.parse_expression()
            if not expr:
                self.error("Expected expression after '<'")
            return UnaryNode(AstType.AST_LOBYTE, expr)
        
        if self.match(TokenType.GREATER):
            self.advance()  # consume '>'
            expr = self.parse_expression()
            if not expr:
                self.error("Expected expression after '>'")
            return UnaryNode(AstType.AST_HIBYTE, expr)
        
        if self.match(TokenType.LPAREN):
            self.advance()  # consume '('
            expr = self.parse_expression()
            if not expr:
                self.error("Expected expression after '('")
            self.expect(TokenType.RPAREN)
            return UnaryNode(AstType.AST_INDIRECT, expr)
        
        return None
    
    def parse_instruction_expression(self) -> Optional[AstNode]:
        """Parse instruction operand expression (can have indexing)"""
        expr = self.parse_expression()
        if not expr:
            return None
        
        # Check for indexing
        if self.match(TokenType.COMMA):
            self.advance()  # consume ','
            if self.match(TokenType.X_REG):
                self.advance()
                return UnaryNode(AstType.AST_INDEXED_X, expr)
            elif self.match(TokenType.Y_REG):
                self.advance()
                return UnaryNode(AstType.AST_INDEXED_Y, expr)
            else:
                self.error("Expected 'x' or 'y' after ','")
        
        return expr
