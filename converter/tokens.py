"""
Token definitions for 6502 Assembly Lexer
"""

from enum import IntEnum

class TokenType(IntEnum):
    # Special tokens
    EOF = 0
    ERROR = 1
    
    # Basic tokens
    DIRECTIVE = 3
    NAME = 4
    LABEL = 5
    DATABYTES = 6
    DATAWORDS = 7
    HEXCONST = 8
    BINCONST = 9
    DECCONST = 10
    
    # 6502 Instructions
    LDA = 11
    LDX = 12
    LDY = 13
    STA = 14
    STX = 15
    STY = 16
    TAX = 17
    TAY = 18
    TXA = 19
    TYA = 20
    TSX = 21
    TXS = 22
    PHA = 23
    PHP = 24
    PLA = 25
    PLP = 26
    AND = 27
    EOR = 28
    ORA = 29
    BIT = 30
    ADC = 31
    SBC = 32
    CMP = 33
    CPX = 34
    CPY = 35
    INC = 36
    INX = 37
    INY = 38
    DEC = 39
    DEX = 40
    DEY = 41
    ASL = 42
    LSR = 43
    ROL = 44
    ROR = 45
    JMP = 46
    JSR = 47
    RTS = 48
    BCC = 49
    BCS = 50
    BEQ = 51
    BMI = 52
    BNE = 53
    BPL = 54
    BVC = 55
    BVS = 56
    CLC = 57
    CLD = 58
    CLI = 59
    CLV = 60
    SEC = 61
    SED = 62
    SEI = 63
    BRK = 64
    NOP = 65
    RTI = 66
    
    # Special characters
    EQUALS = 67     # '='
    COMMA = 68      # ','
    HASH = 69       # '#'
    PLUS = 70       # '+'
    MINUS = 71      # '-'
    LESS = 72       # '<'
    GREATER = 73    # '>'
    LPAREN = 74     # '('
    RPAREN = 75     # ')'
    X_REG = 76      # 'x'
    Y_REG = 77      # 'y'

class Token:
    def __init__(self, token_type: TokenType, value: str = "", line: int = 0, column: int = 0):
        self.type = token_type
        self.value = value
        self.line = line
        self.column = column
    
    def __repr__(self):
        return f"Token({self.type.name}, '{self.value}', {self.line}:{self.column})"

# Instruction mapping
INSTRUCTION_MAP = {
    'lda': TokenType.LDA,
    'ldx': TokenType.LDX,
    'ldy': TokenType.LDY,
    'sta': TokenType.STA,
    'stx': TokenType.STX,
    'sty': TokenType.STY,
    'tax': TokenType.TAX,
    'tay': TokenType.TAY,
    'txa': TokenType.TXA,
    'tya': TokenType.TYA,
    'tsx': TokenType.TSX,
    'txs': TokenType.TXS,
    'pha': TokenType.PHA,
    'php': TokenType.PHP,
    'pla': TokenType.PLA,
    'plp': TokenType.PLP,
    'and': TokenType.AND,
    'eor': TokenType.EOR,
    'ora': TokenType.ORA,
    'bit': TokenType.BIT,
    'adc': TokenType.ADC,
    'sbc': TokenType.SBC,
    'cmp': TokenType.CMP,
    'cpx': TokenType.CPX,
    'cpy': TokenType.CPY,
    'inc': TokenType.INC,
    'inx': TokenType.INX,
    'iny': TokenType.INY,
    'dec': TokenType.DEC,
    'dex': TokenType.DEX,
    'dey': TokenType.DEY,
    'asl': TokenType.ASL,
    'lsr': TokenType.LSR,
    'rol': TokenType.ROL,
    'ror': TokenType.ROR,
    'jmp': TokenType.JMP,
    'jsr': TokenType.JSR,
    'rts': TokenType.RTS,
    'bcc': TokenType.BCC,
    'bcs': TokenType.BCS,
    'beq': TokenType.BEQ,
    'bmi': TokenType.BMI,
    'bne': TokenType.BNE,
    'bpl': TokenType.BPL,
    'bvc': TokenType.BVC,
    'bvs': TokenType.BVS,
    'clc': TokenType.CLC,
    'cld': TokenType.CLD,
    'cli': TokenType.CLI,
    'clv': TokenType.CLV,
    'sec': TokenType.SEC,
    'sed': TokenType.SED,
    'sei': TokenType.SEI,
    'brk': TokenType.BRK,
    'nop': TokenType.NOP,
    'rti': TokenType.RTI,
}
